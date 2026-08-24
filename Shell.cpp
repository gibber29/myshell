#include "Shell.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <sys/wait.h>
#include <unistd.h>

void Shell::run() {
    std::string line;

    while (running_) {
        printPrompt();

        if (!readLine(line)) {
            std::cout << "\n";
            break;
        }

        std::vector<std::string> tokens = tokenize(line);

        if (tokens.empty()) {
            continue;
        }

        history_.push_back(line);

        try {
            Command command = parseRedirection(tokens);
            execute(command);
        } catch (const std::runtime_error& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
        }
    }
}

void Shell::printPrompt() const {
    std::cout << "myshell> " << std::flush;
}

bool Shell::readLine(std::string& outLine) {
    return static_cast<bool>(std::getline(std::cin, outLine));
}

void Shell::execute(const Command& command) {
    if (command.argv.empty()) {
        return;
    }

    if (isBuiltin(command.argv[0])) {
        runBuiltin(command.argv);
    } else {
        runExternal(command);
    }
}

// ---------------- Builtins ----------------

bool Shell::isBuiltin(const std::string& command) const {
    return command == "cd" || command == "exit" || command == "history";
}

void Shell::runBuiltin(const std::vector<std::string>& args) {
    const std::string& command = args[0];

    if (command == "cd") {
        builtinCd(args);
    } else if (command == "exit") {
        builtinExit(args);
    } else if (command == "history") {
        builtinHistory();
    }
}

void Shell::builtinCd(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "cd: expected an argument\n";
        return;
    }

    if (chdir(args[1].c_str()) != 0) {
        std::cerr << "cd: " << args[1] << ": " << std::strerror(errno) << "\n";
    }
}

void Shell::builtinExit(const std::vector<std::string>& /*args*/) {
    running_ = false;
}

void Shell::builtinHistory() const {
    for (size_t i = 0; i < history_.size(); ++i) {
        std::cout << i + 1 << "  " << history_[i] << "\n";
    }
}

// ---------------- External commands ----------------

void Shell::runExternal(const Command& command) {
    std::vector<char*> argv;
    argv.reserve(command.argv.size() + 1);

    for (const auto& arg : command.argv) {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }

    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "fork failed: " << std::strerror(errno) << "\n";
        return;
    }

    if (pid == 0) {
        execvp(argv[0], argv.data());

        std::cerr << command.argv[0] << ": command not found\n";
        _exit(127);
    }

    int status = 0;

    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
    }
}