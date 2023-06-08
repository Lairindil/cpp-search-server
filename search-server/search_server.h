#pragma once

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <numeric>
#include <execution>
#include <functional>
#include <deque>

#include <optional>

#include "document.h"
#include "read_input_functions.h"
#include "string_processing.h"

#include "concurrent_map.h"



using namespace std::string_literals;

const int MAX_RESULT_DOCUMENT_COUNT = 5;

class SearchServer {
public:
    
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words);
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(std::string_view stop_words_text);

    void AddDocument(int document_id, std::string_view document, DocumentStatus status,
                     const std::vector<int>& ratings);
    
    //FindTopDocuments with policy
    template <typename ExecutionPolicy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const ExecutionPolicy& policy, std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(
        const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const;

    template <typename ExecutionPolicy>
    std::vector<Document> FindTopDocuments(
        const ExecutionPolicy& policy, std::string_view raw_query) const;

    //FindTopDocuments without policy
    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(std::string_view raw_query,
                                           DocumentPredicate document_predicate) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query) const;
    std::vector<Document> FindTopDocuments(std::string_view raw_query, DocumentStatus status) const;

    int GetDocumentCount() const;

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::sequenced_policy&, std::string_view raw_query, int document_id) const;
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(
        const std::execution::parallel_policy&, std::string_view raw_query, int document_id) const;
    
    std::set<int>::iterator begin();
    std::set<int>::iterator end();
    
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;
    
    void RemoveDocument(int document_id);
    void RemoveDocument(const std::execution::sequenced_policy&, int document_id);
    void RemoveDocument(const std::execution::parallel_policy&, int document_id);
    
private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };


    const std::set<std::string, std::less<>> stop_words_;
    std::deque<std::string> storage_;

    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    
    //doc_id to word/freq
    std::map<int, std::map<std::string_view, double>> word_frequencies_;
    
    std::map<int, DocumentData> documents_;
    
    std::set<int> documents_ids_;

    static bool IsValidWord(std::string_view word);
    
    bool IsStopWord(std::string_view word) const;

    std::optional<std::vector<std::string_view>> SplitIntoWordsNoStop(std::string_view text) const;

    static int ComputeAverageRating(const std::vector<int>& ratings);

    struct QueryWord {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(std::string_view text) const;

    struct Query {
            std::vector<std::string_view> plus_words;
            std::vector<std::string_view> minus_words;
        };


    Query ParseQuery(std::string_view text, bool par = 0) const;

    // Existence required
    double ComputeWordInverseDocumentFreq(std::string_view word) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const;

    template <typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(
        const Query& query, DocumentPredicate document_predicate) const; 
    
};


template <typename StringContainer>
SearchServer::SearchServer(const StringContainer& stop_words)
    : stop_words_(MakeUniqueNonEmptyStrings(stop_words)) {

        if (!none_of(stop_words.begin(), stop_words.end(), 
            [](std::string_view word) { return !IsValidWord(word);}))
        {
                throw std::invalid_argument("Недопустимые сим   волы во множестве стоп-слов"s);
        }
}


template <typename ExecutionPolicy, typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
    const ExecutionPolicy& policy, std::string_view raw_query,
                                  DocumentPredicate document_predicate) const {
    const auto query = ParseQuery(raw_query);
    auto matched_documents = FindAllDocuments(policy ,query, document_predicate);
        
    sort(policy, matched_documents.begin(), matched_documents.end(),
        [](const Document& lhs, const Document& rhs) {
        double EPSILON = 1e-6;
        if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
            return lhs.rating > rhs.rating;
        } else {
            return lhs.relevance > rhs.relevance;
        }
    });
    if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
        matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
    }
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(
    const ExecutionPolicy& policy, std::string_view raw_query, DocumentStatus status) const {
    const auto matched_documents = FindTopDocuments(
        policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
    });
    return matched_documents;
}

template <typename ExecutionPolicy>
std::vector<Document> SearchServer::FindTopDocuments(
    const ExecutionPolicy& policy, std::string_view raw_query) const {
    const auto matched_documents = FindTopDocuments(
        policy, raw_query, DocumentStatus::ACTUAL); 
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindTopDocuments(
    std::string_view raw_query, DocumentPredicate document_predicate) const {
    const auto matched_documents = FindTopDocuments(
        std::execution::seq, raw_query, document_predicate);
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
        const std::execution::sequenced_policy&, const Query& query, DocumentPredicate document_predicate) const {
    std::map<int, double> document_to_relevance;
    for (std::string_view word : query.plus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
        for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
            const auto& document_data = documents_.at(document_id);
            if (document_predicate(document_id, document_data.status, document_data.rating)) {
                document_to_relevance[document_id] += term_freq * inverse_document_freq;
            }
        }
    }

    for (std::string_view word : query.minus_words) {
        if (word_to_document_freqs_.count(word) == 0) {
            continue;
        }
        for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
            document_to_relevance.erase(document_id);
        }
    }

    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : document_to_relevance) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
        const std::execution::parallel_policy&, const Query& query, DocumentPredicate document_predicate) const {
   
    ConcurrentMap<int, double> document_to_relevance(100);

    std::for_each(std::execution::par, query.plus_words.begin(), query.plus_words.end(), 
        [&, document_predicate](const auto& word){
            if (word_to_document_freqs_.count(word) != 0) {
        
                const double inverse_document_freq = ComputeWordInverseDocumentFreq(word);
                for (const auto [document_id, term_freq] : word_to_document_freqs_.at(word)) {
                        const auto& document_data = documents_.at(document_id);
                        if (document_predicate(document_id, document_data.status, document_data.rating)) {
                            document_to_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                        }
                    }
            }
        });

    auto ordinary_map = document_to_relevance.BuildOrdinaryMap();
    for_each(std::execution::par, query.minus_words.begin(), query.minus_words.end(),
        [&](const auto& word){
            if (word_to_document_freqs_.count(word) != 0) {

                for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                    ordinary_map.erase(document_id);
                }
            }
        });


    std::vector<Document> matched_documents;
    for (const auto [document_id, relevance] : ordinary_map) {
        matched_documents.push_back(
            {document_id, relevance, documents_.at(document_id).rating});
    }
    return matched_documents;
}

template <typename DocumentPredicate>
std::vector<Document> SearchServer::FindAllDocuments(
        const Query& query, DocumentPredicate document_predicate) const {
            auto matched_documents = FindAllDocuments(std::execution::seq, query, document_predicate);
    return matched_documents;
}
