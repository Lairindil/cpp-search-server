#pragma once

#include <vector>
#include <string_view>
#include <string>
#include <set>
#include <algorithm>
#include <functional>

std::vector<std::string_view> SplitIntoWords(std::string_view text);


template <typename StringContainer>
std::set<std::string, std::less<>> MakeUniqueNonEmptyStrings(const StringContainer& strings) {
    std::set<std::string, std::less<>> non_empty_strings;
    for (auto& sv : strings) {
        if (!sv.empty()) {
            non_empty_strings.emplace(sv);
        }
    }
    return non_empty_strings;
}