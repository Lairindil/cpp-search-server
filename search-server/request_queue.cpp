#include "request_queue.h"
#include "search_server.h"
#include "document.h"


using namespace std;

RequestQueue::RequestQueue(const SearchServer& search_server)
    : search_server_(search_server) {
}

vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query, DocumentStatus status) {
    
    QueryResult request;
    request.result = search_server_.FindTopDocuments(raw_query);
    ParseRequest(request);
    return request.result;
}


vector<Document> RequestQueue::AddFindRequest(std::string_view raw_query) {
    QueryResult request;
    request.result = search_server_.FindTopDocuments(raw_query);
    ParseRequest(request);
    return request.result;
}


int RequestQueue::GetNoResultRequests() const {
    return null_results_;
}

void RequestQueue::ParseRequest(QueryResult& request) {
    ++current_time_;
    if (request.result.empty()) {
        request.null_result = true;
        ++null_results_;
    }
    requests_.push_back(request);
    
    if (current_time_ > min_in_day_) {
        if (requests_.front().null_result == true) {
            --null_results_;
        }
        requests_.pop_front();
        --current_time_;
    }
}
