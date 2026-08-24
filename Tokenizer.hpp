#pragma once

#include <string>
#include <vector>

// Splits a raw input line into whitespace-separated tokens.
//
// Deliberately minimal right now: no quoting, no escaping, no
// redirection/pipe symbols treated specially. It just gives you
// clean tokens like {"ls", "-la", "/tmp"}.
//
// This is your first real extension point. When you get to
// redirection and pipes, you'll come back here and either:
//   (a) teach this tokenizer to recognize >, >>, <, | as their own
//       tokens even when not surrounded by spaces (e.g. "ls>out.txt"),
//   (b) add a second pass (a small parser) that walks the token list
//       Tokenize() produces and splits it into a structured command
//       (argv + redirects + pipeline stages).
// (b) is the cleaner design — keep tokenizing "dumb" and put the
// meaning-assigning logic in one place.
std::vector<std::string> tokenize(const std::string& line);
