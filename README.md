myshell

A simplified Unix shell written in C++17, built directly on top of POSIX system calls (fork, execvp, pipe, dup2, waitpid) — no shortcuts, no library doing the process management for you.

Status: actively in development. See the feature checklist below for what's implemented vs. in progress. This line and the checklist get updated as each feature lands — don't claim more than what's checked.

<!-- TODO: add a demo GIF or asciinema recording here once redirection/pipes work. `asciinema rec demo.cast` is the easiest way to record a terminal session. -->
Features
 REPL loop (prompt, read, tokenize, execute)
 Command execution via fork + execvp + waitpid
 Built-ins: cd, exit, history (run in-process, not forked)
 Graceful handling of unknown commands (no shell crash on bad input)
 I/O redirection: >, >>, <
 Pipelines: cmd1 | cmd2 | ... | cmdN (arbitrary length, not just two)
 Background jobs: cmd &
 Signal handling: Ctrl+C kills the running child, not the shell itself
Build

Requires a C++17 compiler and CMake on a POSIX system (Linux/macOS).

bash
mkdir build && cd build
cmake ..
make
./myshell

Or without CMake:

bash
g++ -std=c++17 -Wall -Wextra -Isrc src/*.cpp -o myshell
Usage
myshell> pwd
/home/user/myshell
myshell> echo hello world
hello world
myshell> cd ..
myshell> history
1  pwd
2  echo hello world
3  cd ..
4  history
myshell> exit
<!-- TODO once redirection/pipes/jobs work, add usage examples for each, e.g.: myshell> ls -la > out.txt myshell> cat < out.txt | wc -l myshell> sleep 5 & -->
How it works
The fork / exec / wait pattern

Every external command follows the same three-syscall sequence, which is the mechanism underneath essentially every Unix shell:

fork() duplicates the currently running shell process. After the call returns, there are two nearly-identical processes running the same code — the parent (original shell) and the child (the copy). fork() returns twice: 0 in the child, and the child's PID in the parent. That return value is how each process knows which one it is.
execvp(), called only in the child, replaces that process's memory image (code, data, stack) with the new program named in the command. The process ID stays the same, but everything the process is running changes. If execvp succeeds, none of the shell's own code runs again in that process — it's now genuinely running ls, or cat, or whatever was requested. If it fails (e.g. the command doesn't exist), execution falls through to the error-handling code right after the call, and the child exits with _exit() rather than returning — using _exit() instead of exit() matters here, since exit() would also run cleanup/flush logic that belongs to the parent shell, which we don't want duplicated in a forked child.
waitpid(), called in the parent, blocks until the specific child process changes state (here, until it exits). This is what makes the shell's prompt come back only after the command finishes, rather than racing ahead immediately after fork().
File descriptors after fork

A forked child inherits copies of the parent's open file descriptor table — same fds pointing at the same underlying open files, but independently closeable. This matters for redirection and pipes: to redirect a child's stdout to a file, you open() the file, then dup2(fileFd, STDOUT_FILENO) in the child, after fork(), before execvp(). dup2 makes STDOUT_FILENO (fd 1) point at whatever fileFd points at; the child's subsequent writes to stdout — including everything the exec'd program writes — go to the file instead of the terminal. This has to happen after fork() (so it only affects the child, not the shell itself) and before execvp() (so the new program inherits the redirected fd when its image replaces the child's).

<!-- TODO once pipes are implemented, add a paragraph here explaining: - pipe() creates a unidirectional fd pair (read end, write end) - why BOTH the unused pipe ends must be closed in both parent and child (a stray open write end is the classic bug — the reader never sees EOF and hangs forever waiting for more input) - how an N-stage pipeline generalizes to N-1 pipes and N forks --> <!-- TODO once background jobs/signals are implemented, add a paragraph on: - why `cmd &` means "don't call waitpid immediately" - reaping finished background jobs via WNOHANG (avoiding zombies) - SIGINT/SIGCHLD handling and why signal handlers must stick to async-signal-safe operations -->
RAII file descriptor ownership

FileDescriptor (in src/FileDescriptor.hpp) wraps a raw fd so it's closed automatically when it goes out of scope — on every exit path, including early returns and thrown exceptions. Copying is disabled (a fd should have exactly one owner at a time); moving is allowed, so ownership can transfer cleanly. This is the standard C++ RAII idiom (the same pattern as std::unique_ptr or std::lock_guard) applied to a resource the standard library doesn't wrap for you — it's meant to make it structurally hard to leak a fd or leave a pipe end open by accident, which is otherwise one of the easiest bugs to introduce in a shell implementation.

Design notes
Builtins run in-process, not forked. Commands like cd need to mutate the shell's own state (its working directory via chdir()), which would be pointless in a forked child — the child's chdir() would only affect the child, and that process is about to exit. Builtins are intercepted and dispatched before any fork() call.
Tokenizing is kept separate from parsing structure. Tokenizer.cpp only splits raw input into string tokens; it doesn't know about |, >, or &. <!-- TODO: once the pipe/redirect parser exists, note here whether structure got added as a second pass over tokens, or folded into tokenizing directly, and why. -->
Testing
<!-- TODO: fill in once you've tested it. At minimum, note: - manual test cases you ran (bad command, empty input, missing redirect target, broken pipeline, etc.) - if you added GoogleTest for the tokenizer/parser, note how to run it here - if you ran it under valgrind to check for leaks, note the result -->
What I'd do next
<!-- TODO: a short, honest list of what's still missing or what you'd improve with more time — e.g. quoting/escaping in the tokenizer, `cd -`, exit codes from `exit <n>`, job control commands like `jobs`/`fg`/`bg`. Interviewers like seeing this; it shows you know the edges of what you built rather than overselling it. -->