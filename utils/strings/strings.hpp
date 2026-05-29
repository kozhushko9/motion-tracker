#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

namespace utils {

void replace_all(std::string& text, std::string_view token, std::string_view value);

std::string replace_tokens(std::string text,
                           const std::unordered_map<std::string, std::string>& tokens);

}  // namespace utils