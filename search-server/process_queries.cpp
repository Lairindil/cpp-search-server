#include "process_queries.h"

#include <execution>
#include <vector>
#include <list>

using namespace std;

std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {

    vector<vector<Document>> processed_queries(queries.size());
    
    std::transform(std::execution::par,
                   queries.begin(), queries.end(), processed_queries.begin(), 
                  [&search_server](auto &querie) {
                     return search_server.FindTopDocuments(querie);
                  });
        
        return processed_queries;
    }


    vector<Document> ProcessQueriesJoined(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
        auto processed_queries = ProcessQueries(search_server, queries);
        vector<Document> temp;
        
        for (auto& proc : processed_queries) {
                for (auto& p : proc) {
                    temp.push_back(p);
                }
        }
        return temp;
    }