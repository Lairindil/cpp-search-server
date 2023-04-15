#include "search_server.h"
#include "string_processing.h"
#include "log_duration.h"

using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
   : SearchServer(
       SplitIntoWords(stop_words_text))  // Invoke delegating constructor from string container
    {
    }

void SearchServer::AddDocument(int document_id, const string& document, DocumentStatus status,
                               const vector<int>& ratings) {
    if (documents_.count(document_id) > 0) {
        throw invalid_argument ("Документ не был добавлен, так как его id совпадает с уже имеющимся"s);
    }
    if (document_id < 0) {
        throw invalid_argument ("Документ не был добавлен, так как его id отрицательный"s);
    }
    if (const auto words = SplitIntoWordsNoStop(document)) {
        const double inv_word_count = 1.0 / words->size();
        for (const string& word : *words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
            word_frequencies_[document_id][word] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ComputeAverageRating(ratings), status});
        documents_ids_.push_back(document_id);

    } else {
        throw invalid_argument ("Документ не был добавлен, так как содержит спецсимволы"s);
    }
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query, DocumentStatus status) const {
    const auto matched_documents = FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
    });{
            return matched_documents;
        }
}

vector<Document> SearchServer::FindTopDocuments(const string& raw_query) const {
    const auto matched_documents = FindTopDocuments(raw_query, DocumentStatus::ACTUAL); {
            return matched_documents;
        }
}

int SearchServer::GetDocumentCount() const {
    return static_cast<int>(documents_.size());
}

tuple<vector<string>, DocumentStatus> SearchServer::MatchDocument(const string& raw_query, int document_id) const {

    const auto query = ParseQuery(raw_query);
    vector<string> matched_words;
    for (const string& word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.push_back(word);
        }
    }
    for (const string& word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        if (word_to_document_freqs_.at(word).count(document_id)) {
            matched_words.clear();
            break;
        }
    }
    return tuple(matched_words, documents_.at(document_id).status);;
}


//int SearchServer::GetDocumentId(int index) const {
//    if (index < 0 || index > static_cast<int>(documents_ids_.size())) {
//        throw out_of_range("Индекс переданного документа выходит за пределы допустимого диапазона (0; количество документов)"s);
//    } else {
//        return documents_ids_[index];
//    }
//}

vector<int>::iterator SearchServer::begin() {
    return documents_ids_.begin();
}

vector<int>::iterator SearchServer::end() {
    return documents_ids_.end();
}

const map<string, double>& SearchServer::GetWordFrequencies(int document_id) const {
    if (word_frequencies_.count(document_id) == 0) {
        return empty_map_;
    }
    return word_frequencies_.at(document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    //do we have doc on server?
    if (count(documents_ids_.begin(), documents_ids_.end(), document_id) == 0) {
        return;
    }

    //remove from documents
    documents_.erase(document_id);
    
    //remove from documents_ids_
    auto it_to_remove = find(documents_ids_.begin(), documents_ids_.end(), document_id);
    documents_ids_.erase(it_to_remove);
    
    for(auto& [word, freq] : GetWordFrequencies(document_id)) {
        word_to_document_freqs_[word].erase(document_id);
    }
    
    //remove from word_frequencies
    word_frequencies_.erase(document_id);

}


bool SearchServer::IsValidWord(const string& word) {
        // A valid word must not contain special characters
        return none_of(word.begin(), word.end(), [](char c) {
            return c >= '\0' && c < ' ';
        });
    }

bool SearchServer::IsStopWord(const std::string& word) const {
    return stop_words_.count(word) > 0;
}

optional<vector<string>> SearchServer::SplitIntoWordsNoStop(const string& text) const {
     vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
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

SearchServer::QueryWord SearchServer::ParseQueryWord(string text) const {
    bool is_minus = false;
    // Word shouldn't be empty
    if (text[0] == '-') {
        is_minus = true;
        text = text.substr(1);
    }
    if (!IsValidWord(text)) {
        throw invalid_argument ("В поисковом запросе \""s + text + "\" есть недопустимые символы с кодами от 0 до 31"s);
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

SearchServer::Query SearchServer::ParseQuery(const string& text) const {
    Query query;
    for (const string& word : SplitIntoWords(text)) {
        const auto query_word = ParseQueryWord(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
            } else {
                    query.plus_words.insert(query_word.data);
            }
        }
    }
    return query;
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}
