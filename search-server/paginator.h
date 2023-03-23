#pragma once

#include <iostream>
#include <vector>
#include <algorithm>

template <typename Iterator>
class IteratorRange {
public:
    IteratorRange(Iterator begin, Iterator end)
        : first_(begin)
        , last_(end)
        , size_(distance(first_, last_)) {
    }
    
    auto begin() const {
        return first_;
    }
    
    auto end() const {
        return last_;
    }
    
    size_t size(){
        return size_;
    }
private:
    Iterator first_;
    Iterator last_;
    size_t size_;
};

template <typename Iterator>
class Paginator {
public:
    Paginator(Iterator begin, Iterator end, size_t page_size) {
        for(size_t documents_left = distance(begin, end); documents_left > 0;) {
            const size_t current_page_size = std::min(page_size, documents_left);
            const Iterator current_page_end = next(begin, current_page_size);
            pages_.push_back({begin, current_page_end});
            
            documents_left -= current_page_size;
            begin = current_page_end;
        }
    }
    
    auto begin() const {
        return pages_.begin();
    }

    auto end() const {
        return pages_.end();
    }
    
    size_t size() {
        return pages_.size();
    }
    
private:
    std::vector<IteratorRange<Iterator>> pages_;
};

template <typename Container>
auto Paginate(const Container& c, size_t page_size) {
    return Paginator(begin(c), end(c), page_size);
}

template <typename Iterator>
std::ostream& operator<<(std::ostream& out, const IteratorRange<Iterator> page) {
    for (auto doc = page.begin(); doc < page.end(); ++doc) {
        out << *doc;
    }
    return out;
}
