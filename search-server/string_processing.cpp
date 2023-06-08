#include "string_processing.h"

using namespace std;

vector<string_view> SplitIntoWords(string_view sv) {
    std::vector<std::string_view> result;
        sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
        const int64_t pos_end = sv.npos;

        while (!sv.empty()) {
            int64_t space = sv.find(' ');
            result.push_back(space == pos_end ? sv.substr(0) : sv.substr(0, space));
            sv.remove_prefix(result.back().size());
            sv.remove_prefix(std::min(sv.find_first_not_of(" "), sv.size()));
        }
    return result;
}