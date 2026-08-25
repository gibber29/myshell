#include "Shell.hpp"
#include "Tokenizer.hpp"
#include "Parser.hpp"
#include "FileDescriptor.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <iostream>

#include <array>
#include <fcntl.h>
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
            Pipeline pipeline = parsePipeline(tokens);
            executePipeline(pipeline);
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

void Shell::executePipeline(const Pipeline& pipeline) {
    if (pipeline.stages.empty()) {
        return;
    }

    if (pipeline.stages.size() == 1) {
        // No pipe involved -- reuse the existing single-command path.
        execute(pipeline.stages[0]);
    } else {
        runPipeline(pipeline);
    }
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
        // ---- Output redirection ----
        if (!command.outputFile.empty()) {
            int flags = O_WRONLY | O_CREAT;
            if (command.appendOutput) {
                flags |= O_APPEND;
            } else {
                flags |= O_TRUNC;
            }
            int rawFd = open(command.outputFile.c_str(), flags, 0644);
            if (rawFd < 0) {
                std::cerr << command.outputFile << ": " << std::strerror(errno) << "\n";
                _exit(1);
            }
            FileDescriptor outFd(rawFd);
            if (dup2(outFd.get(), STDOUT_FILENO) < 0) {
                std::cerr << "dup2 failed: " << std::strerror(errno) << "\n";
                _exit(1);
            }
        }

        // ---- Input redirection ----
        if (!command.inputFile.empty()) {
            int rawFd = open(command.inputFile.c_str(), O_RDONLY);
            if (rawFd < 0) {
                std::cerr << command.inputFile << ": " << std::strerror(errno) << "\n";
                _exit(1);
            }
            FileDescriptor inFd(rawFd);
            if (dup2(inFd.get(), STDIN_FILENO) < 0) {
                std::cerr << "dup2 failed: " << std::strerror(errno) << "\n";
                _exit(1);
            }
        }

        execvp(argv[0], argv.data());
        std::cerr << command.argv[0] << ": command not found\n";
        _exit(127);
    }

    // ---- Parent process ----
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
    }

    // TODO (later): once you add background jobs (&), this is the
    // function that needs to branch — either wait here (foreground)
    // or record the pid and return immediately (background), reaping
    // it later with waitpid(pid, &status, WNOHANG).
}

void Shell::runPipeline(const Pipeline& pipeline) {
    size_t n = pipeline.stages.size();

    if (n == 0) {
        return;
    }

    // Build all N-1 pipes up front.
    std::vector<std::array<FileDescriptor, 2>> pipes;

    for (size_t i = 0; i + 1 < n; i++) {
        int fds[2];

        if (pipe(fds) < 0) {
            std::cerr << "pipe failed: "
                      << std::strerror(errno) << "\n";
            return;
        }

        // fds[0] = read end
        // fds[1] = write end
        pipes.push_back({
            FileDescriptor(fds[0]),
            FileDescriptor(fds[1])
        });
    }

    std::vector<pid_t> pids;
    pids.reserve(n);

    for (size_t i = 0; i < n; i++) {

        // Build argv for this stage.
        std::vector<char*> argv;
        argv.reserve(pipeline.stages[i].argv.size() + 1);

        for (const auto& arg : pipeline.stages[i].argv) {
            argv.push_back(const_cast<char*>(arg.c_str()));
        }

        argv.push_back(nullptr);

        pid_t pid = fork();

        if (pid < 0) {
            std::cerr << "fork failed: "
                      << std::strerror(errno) << "\n";
            return;
        }

        if (pid == 0) {
            // ---- Child for stage i ----

            // If this isn't the first stage,
            // stdin comes from the previous pipe.
            if (i > 0) {
                if (dup2(pipes[i - 1][0].get(), STDIN_FILENO) < 0) {
                    std::cerr << "dup2 stdin failed: "
                              << std::strerror(errno) << "\n";
                    _exit(1);
                }
            }

            // If this isn't the last stage,
            // stdout goes into the next pipe.
            if (i + 1 < n) {
                if (dup2(pipes[i][1].get(), STDOUT_FILENO) < 0) {
                    std::cerr << "dup2 stdout failed: "
                              << std::strerror(errno) << "\n";
                    _exit(1);
                }
            }

            // ---- Per-stage input redirection ----
            //
            // Explicit redirection should override the pipe.
            if (!pipeline.stages[i].inputFile.empty()) {
                int rawFd = open(
                    pipeline.stages[i].inputFile.c_str(),
                    O_RDONLY
                );

                if (rawFd < 0) {
                    std::cerr << pipeline.stages[i].inputFile
                              << ": "
                              << std::strerror(errno)
                              << "\n";
                    _exit(1);
                }

                FileDescriptor inFd(rawFd);

                if (dup2(inFd.get(), STDIN_FILENO) < 0) {
                    std::cerr << "dup2 input failed: "
                              << std::strerror(errno) << "\n";
                    _exit(1);
                }
            }

            // ---- Per-stage output redirection ----
            //
            // Explicit redirection should override the pipe.
            if (!pipeline.stages[i].outputFile.empty()) {
                int flags = O_WRONLY | O_CREAT;

                if (pipeline.stages[i].appendOutput) {
                    flags |= O_APPEND;
                } else {
                    flags |= O_TRUNC;
                }

                int rawFd = open(
                    pipeline.stages[i].outputFile.c_str(),
                    flags,
                    0644
                );

                if (rawFd < 0) {
                    std::cerr << pipeline.stages[i].outputFile
                              << ": "
                              << std::strerror(errno)
                              << "\n";
                    _exit(1);
                }

                FileDescriptor outFd(rawFd);

                if (dup2(outFd.get(), STDOUT_FILENO) < 0) {
                    std::cerr << "dup2 output failed: "
                              << std::strerror(errno) << "\n";
                    _exit(1);
                }
            }

            // ---- Close every pipe fd ----
            //
            // At this point STDIN/STDOUT have been redirected,
            // so the original pipe descriptors are no longer needed.
            for (auto& pipePair : pipes) {
                pipePair[0].reset();
                pipePair[1].reset();
            }

            execvp(argv[0], argv.data());

            std::cerr << pipeline.stages[i].argv[0]
                      << ": command not found\n";

            _exit(127);
        }

        // ---- Parent ----
        pids.push_back(pid);
    }

    // Parent doesn't participate in the pipeline.
    // Close every pipe fd.
    for (auto& pipePair : pipes) {
        pipePair[0].reset();
        pipePair[1].reset();
    }

    // Wait for EVERY child.
    for (pid_t pid : pids) {
        int status = 0;

        if (waitpid(pid, &status, 0) < 0) {
            std::cerr << "waitpid failed: "
                      << std::strerror(errno) << "\n";
        }
    }
}