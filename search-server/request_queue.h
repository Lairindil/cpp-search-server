#pragma once

#include <vector>
#include <deque>
#include <string>

#include "search_server.h"
#include "document.h"


class RequestQueue {
public:
    explicit RequestQueue(const SearchServer& search_server);
    // сделаем "обёртки" для всех методов поиска, чтобы сохранять результаты для нашей статистики
    
    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult request;
        request.result = search_server_.FindTopDocuments(raw_query);
        ParseRequest(request);
        return request.result;
    }
    
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    
    int GetNoResultRequests() const;
    
private:
    struct QueryResult {
        bool null_result = false;
        std::vector<Document> result;
    };
    
    std::deque<QueryResult> requests_;
    const static int min_in_day_ = 1440;
    
    uint64_t current_time_ = 0;
    int null_results_ = 0;
    
    const SearchServer& search_server_;
    
    void ParseRequest(QueryResult& request);
};
