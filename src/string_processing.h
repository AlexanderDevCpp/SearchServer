#pragma once
#include <string>
#include <set>
#include <vector>
#include <iostream>
std::vector<std::string> SplitIntoWords(const std::string & text);
std::vector<std::string_view> SplitIntoWordsView(const std::string_view str);
template <typename StringContainer>
std::set<std::string> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string> non_empty_strings;
    for (const auto& str : strings) {
        if (!str.empty()) {
            non_empty_strings.insert(std::string{ str });
        }
    }
    return non_empty_strings;
}