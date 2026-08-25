#include<stdexcept>
#include "Parser.hpp"

Command parseRedirection(const std::vector<std::string>& tokens) {
    Command cmd;
    size_t i = 0;
    while (i < tokens.size()) {
        if (tokens[i] == ">") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("expected filename after >");
            }
            cmd.outputFile = tokens[i + 1];
            cmd.appendOutput = false;
            i += 2;
        }
        else if (tokens[i] == ">>") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("expected filename after >>");
            }
            cmd.outputFile = tokens[i + 1];
            cmd.appendOutput = true;
            i += 2;
        }
        else if (tokens[i] == "<") {
            if (i + 1 >= tokens.size()) {
                throw std::runtime_error("expected filename after <");
            }
            cmd.inputFile = tokens[i + 1];
            i += 2;
        }
        else {
            cmd.argv.push_back(tokens[i]);
            i++;
        }
    }
    return cmd;
}

Pipeline parsePipeline(const std::vector<std::string>& tokens) {
    Pipeline pipe;
    std::vector<std::string> current;

    for (size_t i = 0; i < tokens.size(); i++) {
        if (tokens[i] == "|") {
            if (current.empty()) {
                throw std::runtime_error("invalid pipe: empty command");
            }

            pipe.stages.push_back(parseRedirection(current));
            current.clear();
        } else {
            current.push_back(tokens[i]);
        }
    }

    if (current.empty()) {
        throw std::runtime_error("invalid pipe: empty command");
    }

    pipe.stages.push_back(parseRedirection(current));

    return pipe;
}



