#pragma once

#include <string>
#include <vector>

struct Command {
    std::vector<std::string> argv;
    std::string outputFile;
    bool appendOutput = false;
    std::string inputFile;
};

Command parseRedirection(const std::vector<std::string>& tokens);