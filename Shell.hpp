#pragma once

#include <string>
#include <vector>

class Shell {
public:
    // Runs the read-eval-print loop until the user exits.
    void run();

private:
    // ---- REPL steps ----
    bool readLine(std::string& outLine);   // false on EOF (Ctrl+D)
    void printPrompt() const;

    // ---- Execution ----
    // Takes already-tokenized argv, decides builtin vs external,
    // and runs it. This is the seam where redirection/pipes will
    // eventually plug in — right now it only handles the plain
    // "run one command, wait for it" case.
    void execute(const std::vector<std::string>& args);

    // fork -> execvp -> waitpid for a single external command.
    void runExternal(const std::vector<std::string>& args);
    void runPipeline(const Pipeline& pipeline);

    // ---- Builtins ----
    // Builtins run IN the shell process itself (never forked), because
    // their entire point is to mutate the shell's own state (cwd,
    // history, or the process's own lifetime). Add new builtins here
    // and dispatch to them from execute().
    bool isBuiltin(const std::string& command) const;
    void runBuiltin(const std::vector<std::string>& args);

    void builtinCd(const std::vector<std::string>& args);
    void builtinExit(const std::vector<std::string>& args);
    void builtinHistory() const;

    // ---- State ----
    std::vector<std::string> history_;
    bool running_ = true;
};
