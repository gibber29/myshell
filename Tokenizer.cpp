#include "Tokenizer.hpp"

#include <cctype>
#include <sstream>

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::istringstream stream(line);
    std::string token;

    // istringstream >> std::string already splits on whitespace and
    // skips leading/trailing whitespace for us. Good enough for v1.
    while (stream >> token) {
        tokens.push_back(token);
    }

    return tokens;
}
