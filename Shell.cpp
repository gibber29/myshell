#include "Shell.hpp"
#include "Tokenizer.hpp"

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
            // EOF (Ctrl+D) — behave like a real shell and exit cleanly.
            std::cout << "\n";
            break;
        }

        std::vector<std::string> args = tokenize(line);

        if (args.empty()) {
            // Just whitespace or an empty line — do nothing, loop again.
            continue;
        }

        history_.push_back(line);
        execute(args);
    }
}

void Shell::printPrompt() const {
    std::cout << "myshell> " << std::flush;
}

bool Shell::readLine(std::string& outLine) {
    return static_cast<bool>(std::getline(std::cin, outLine));
}

void Shell::execute(const std::vector<std::string>& args) {
    if (isBuiltin(args[0])) {
        runBuiltin(args);
    } else {
        runExternal(args);
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
    // TODO: handle `cd` with no args (should go to $HOME) and `cd -`
    // (should go to the previous directory) — real shells support both.
    if (args.size() < 2) {
        std::cerr << "cd: expected an argument\n";
        return;
    }

    if (chdir(args[1].c_str()) != 0) {
        std::cerr << "cd: " << args[1] << ": " << std::strerror(errno) << "\n";
    }
}

void Shell::builtinExit(const std::vector<std::string>& /*args*/) {
    // TODO: support `exit <code>` and return it via std::exit / return code.
    running_ = false;
}

void Shell::builtinHistory() const {
    for (size_t i = 0; i < history_.size(); ++i) {
        std::cout << i + 1 << "  " << history_[i] << "\n";
    }
}

// ---------------- External commands ----------------

void Shell::runExternal(const std::vector<std::string>& args) {
    // execvp needs a char* const argv[] terminated by a null pointer.
    // We build that from our std::vector<std::string> right before
    // forking — the strings themselves stay alive in `args` for the
    // lifetime of this call, so the char* pointers into them are safe.
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& arg : args) {
        // const_cast is safe here: execvp does not modify argv strings,
        // the signature is just a historical wart in the POSIX API.
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid < 0) {
        std::cerr << "fork failed: " << std::strerror(errno) << "\n";
        return;
    }

    if (pid == 0) {
        // ---- Child process ----
        // At this point the child is a near-exact copy of the shell:
        // same open fds, same memory, same everything (copy-on-write).
        // execvp REPLACES this process's image with the new program.
        // If it succeeds, none of the code after this call ever runs.
        execvp(argv[0], argv.data());

        // If we get here, execvp failed (e.g. command not found).
        // Critical: use _exit(), not exit() or return. This is still
        // a forked copy of the shell — exit() would run the shell's
        // own atexit handlers / flush buffers that belong to the
        // PARENT, which we don't want duplicated in the child.
        std::cerr << args[0] << ": command not found\n";
        _exit(127); // 127 is the conventional "command not found" code
    }

    // ---- Parent process ----
    // Block until this specific child changes state (here: exits).
    // This is what makes the prompt wait for the command to finish
    // instead of racing ahead immediately after fork().
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
    }

    // TODO (later): once you add background jobs (&), this is the
    // function that needs to branch — either wait here (foreground)
    // or record the pid and return immediately (background), reaping
    // it later with waitpid(pid, &status, WNOHANG).
}
