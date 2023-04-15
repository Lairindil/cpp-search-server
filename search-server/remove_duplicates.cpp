#include "remove_duplicates.h"
#include "search_server.h"



void RemoveDuplicates(SearchServer& search_server){

    std::map<std::set<std::string>, int> words_to_id;
    std::set<int> documents_to_remove;
    
    for (const int document_id : search_server) {
        auto check = search_server.GetWordFrequencies(document_id);
        
        std::set<std::string> unique_words;
        for (const auto& word : check) {
            unique_words.insert(word.first);
        }
        
        if (words_to_id.count(unique_words) == 0) {
            words_to_id[unique_words] = document_id;
        } else {
            auto doc_to_remove = std::max(document_id, words_to_id.at(unique_words));
            documents_to_remove.insert(doc_to_remove);
        }
    }
    
    for (int id : documents_to_remove) {
        std::cout << "Found duplicate document id "s << id << std::endl;
        
        search_server.RemoveDocument(id);
    }
    
}
