#include "search_server.h"
#include "string_processing.h"



using namespace std;

SearchServer::SearchServer(const std::string& stop_words_text)
   : SearchServer(SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
       {
       }

SearchServer::SearchServer(std::string_view stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))
       {
       }


void SearchServer::AddDocument(int document_id, std::string_view document, DocumentStatus status,
                               const vector<int>& ratings) { 
    if (documents_.count(document_id) > 0) {
        throw invalid_argument ("Документ не был добавлен, так как его id совпадает с уже имеющимся"s);
    }
    if (document_id < 0) {
        throw invalid_argument ("Документ не был добавлен, так как его id отрицательный"s);
    }
    storage_.emplace_back((std::string(document)));
    if (auto words = SplitIntoWordsNoStop(storage_.back())) {
        const double inv_word_count = 1.0 / words->size();
        
        for (std::string_view word : *words) {
            
            word_to_document_freqs_[word][document_id] += inv_word_count;
            word_frequencies_[document_id][word] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        documents_ids_.insert(document_id);

     } else {
         throw invalid_argument ("Документ не был добавлен, так как содержит спецсимволы"s);
    }
}


int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(string_view raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query);
    vector<string_view> matched_words;

        for (string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            return tie(matched_words, documents_.at(document_id).status);
        }
    }
        for (string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }

    return tie(matched_words, documents_.at(document_id).status);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const {
    return SearchServer::MatchDocument(raw_query, document_id);
}

std::tuple<std::vector<std::string_view>, DocumentStatus> SearchServer::MatchDocument(const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const {
    
    if (!documents_ids_.count(document_id)) {
        throw std::out_of_range("Отсутствует документ с указанным ID"s);
    }

    auto query = ParseQuery(raw_query, 1);
    vector<string_view> matched_words(query.plus_words.size());

    if (std::any_of(std::execution::par, query.minus_words.begin(), query.minus_words.end(), [&](auto &word){
        return word_to_document_freqs_.at(word).count(document_id);
    })) {
        matched_words.clear();
        return tie(matched_words, documents_.at(document_id).status);
    }
    
    auto last = std::copy_if(std::execution::par, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(),
        [&](auto &word) {
            return (word_to_document_freqs_.at(word).count(document_id));
        });
    matched_words.erase(last, matched_words.end());
    std::sort(std::execution::par, matched_words.begin(), matched_words.end());
    last = std::unique(std::execution::par, matched_words.begin(), matched_words.end());
    matched_words.erase(last, matched_words.end());

    return tie(matched_words, documents_.at(document_id).status);
 }

set<int>::iterator SearchServer::begin() {
    return documents_ids_.begin();
}

set<int>::iterator SearchServer::end() {
    return documents_ids_.end();
}

const map<std::string_view, double>& SearchServer::GetWordFrequencies(int document_id) const {
    static std::map<std::string_view, double> empty_map;
    if (word_frequencies_.count(document_id) == 0) {
        return empty_map;
    }
    return word_frequencies_.at(document_id);
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query) const {
    const auto matched_documents = FindTopDocuments(
        execution::seq, raw_query, DocumentStatus::ACTUAL); 
    return matched_documents;
}

vector<Document> SearchServer::FindTopDocuments(string_view raw_query, DocumentStatus status) const {
    const auto matched_documents = FindTopDocuments(
        execution::seq, raw_query, status); 
    return matched_documents;
}

void SearchServer::RemoveDocument(int document_id) {
 //do we have doc on server?
    auto it_to_remove = find(documents_ids_.begin(), documents_ids_.end(), document_id);
    if (it_to_remove == documents_ids_.end()) {
        return;
    }
    //remove from documents_ids_
    documents_ids_.erase(it_to_remove);

    //remove from documents
    documents_.erase(document_id);
    
    for(auto& [word, freq] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_[word].erase(document_id);
    }
    
    //remove from word_frequencies
    word_frequencies_.erase(document_id);

}

void SearchServer::RemoveDocument(const std::execution::sequenced_policy&, int document_id) {
    SearchServer::RemoveDocument(document_id);
}

void SearchServer::RemoveDocument(const std::execution::parallel_policy&, int document_id) {
    //do we have doc on server?
    auto it_to_remove = find(documents_ids_.begin(), documents_ids_.end(), document_id);
    if (it_to_remove == documents_ids_.end()) {
        return;
    }
    //remove from documents_ids_
    documents_ids_.erase(it_to_remove);

    //remove from documents
    documents_.erase(document_id);
    
    auto words_to_delete = GetWordFrequencies(document_id);
    vector<string_view> temp;
    temp.reserve(words_to_delete.size());
    
    transform(words_to_delete.begin(), words_to_delete.end(), temp.begin(), 
    [](auto& word) {
        return word.first;
    });

    for_each(execution::par, temp.begin(),temp.end(), 
    [this, &document_id](auto word){
        word_to_document_freqs_[word].erase(document_id);
    });

    
    //remove from word_frequencies
    word_frequencies_.erase(document_id);

}


bool SearchServer::IsValidWord(std::string_view word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

bool SearchServer::IsStopWord(std::string_view word) const {
    return stop_words_.count(word) > 0;
}

optional<vector<string_view>> SearchServer::SplitIntoWordsNoStop(std::string_view text) const {
     vector<string_view> words;
    for (string_view word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            return nullopt;
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    int rating_sum = accumulate(ratings.begin(), ratings.end(), 0);
    return rating_sum / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(std::string_view text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (!IsValidWord(text)) {
        throw invalid_argument ("В поисковом запросе \""s + std::string(text) + "\" есть недопустимые символы с кодами от 0 до 31"s);
    }
    if (text.size() == 0) {
        throw invalid_argument ("Отсутствие текста после символа «минус» в поисковом запросе"s);
    }
    if (text[0] == '-') {
        throw invalid_argument ("Наличие более чем одного минуса перед словами, которых не должно быть в искомых документах"s);
    }
    QueryWord query_word = {text, is_minus, IsStopWord(text)};
    return  query_word;
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text, bool par) const {
    Query query;
    
        for (std::string_view word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                        query.minus_words.push_back(query_word.data);
                } else {
                        query.plus_words.push_back(query_word.data);
                }
            }
        }
     if (!par) {   
            std::sort(query.minus_words.begin(), query.minus_words.end());
            auto last = std::unique(query.minus_words.begin(), query.minus_words.end());
            query.minus_words.erase(last, query.minus_words.end()); 

            std::sort(query.plus_words.begin(), query.plus_words.end());
            last = std::unique(query.plus_words.begin(), query.plus_words.end());
            query.plus_words.erase(last, query.plus_words.end());
    } 
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(std::string_view word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
