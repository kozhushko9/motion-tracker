#include "strings.hpp"

namespace utils {

void replace_all(std::string& text,
                 std::string_view token,
                 std::string_view value) {
    if (token.empty()) {
        return;
    }

    std::size_t pos = 0;

    while ((pos = text.find(token.data(), pos, token.size())) != std::string::npos) {
        text.replace(pos, token.size(), value.data(), value.size());
        pos += value.size();
    }
}

std::string replace_tokens(
    std::string text,
    const std::unordered_map<std::string, std::string>& tokens) {
    for (const auto& [token, value] : tokens) {
        replace_all(text, token, value);
    }

    return text;
}

}  // namespace utils