#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <iostream>

//#include "search_server.h"

using namespace std;

/* Подставьте вашу реализацию класса SearchServer сюда */
const int MAX_RESULT_DOCUMENT_COUNT = 5;

string ReadLine() {
    string s;
    getline(cin, s);
    return s;
}

int ReadLineWithNumber() {
    int result;
    cin >> result;
    ReadLine();
    return result;
}

vector<string> SplitIntoWords(const string& text) {
    vector<string> words;
    string word;
    for (const char c : text) {
        if (c == ' ') {
            if (!word.empty()) {
                words.push_back(word);
                word.clear();
            }
        }
        else {
            word += c;
        }
    }
    if (!word.empty()) {
        words.push_back(word);
    }

    return words;
}

struct Document {
    int id;
    double relevance;
    int rating;
};

enum class DocumentStatus {
    ACTUAL,
    IRRELEVANT,
    BANNED,
    REMOVED,
};

class SearchServer {
public:
    void SetStopWords(const string& text) {
        for (const string& word : SplitIntoWords(text)) {
            stop_words_.insert(word);
        }
    }

    void AddDocument(int document_id, const string& document, DocumentStatus status,
        const vector<int>& ratings) {
        const vector<string> words = SplitIntoWordsNoStop(document);
        const double inv_word_count = 1.0 / words.size();
        for (const string& word : words) {
            word_to_document_freqs_[word][document_id] += inv_word_count;
        }
        documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    }

    template <typename DocumentPredicate>
    vector<Document> FindTopDocuments(const string& raw_query,
        DocumentPredicate document_predicate) const {
        const Query query = ParseQuery(raw_query);
        auto matched_documents = FindAllDocuments(query, document_predicate);

        sort(matched_documents.begin(), matched_documents.end(),
            [](const Document& lhs, const Document& rhs) {
                if (abs(lhs.relevance - rhs.relevance) < 1e-6) {
                    return lhs.rating > rhs.rating;
                }
                else {
                    return lhs.relevance > rhs.relevance;
                }
            });
        if (matched_documents.size() > MAX_RESULT_DOCUMENT_COUNT) {
            matched_documents.resize(MAX_RESULT_DOCUMENT_COUNT);
        }
        return matched_documents;
    }

    vector<Document> FindTopDocuments(const string& raw_query, DocumentStatus user_status = DocumentStatus::ACTUAL) const {
            return FindTopDocuments(raw_query, [user_status]([[maybe_unused]] int document_id, DocumentStatus status, int rating) {return status == user_status; });
    }

    int GetDocumentCount() const {
        return static_cast<int>(documents_.size());
    }

    tuple<vector<string>, DocumentStatus> MatchDocument(const string& raw_query,
        int document_id) const {
        const Query query = ParseQuery(raw_query);
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
        return { matched_words, documents_.at(document_id).status };
    }

private:
    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    set<string> stop_words_;
    map<string, map<int, double>> word_to_document_freqs_;
    map<int, DocumentData> documents_;

    bool IsStopWord(const string& word) const {
        return stop_words_.count(word) > 0;
    }

    vector<string> SplitIntoWordsNoStop(const string& text) const {
        vector<string> words;
        for (const string& word : SplitIntoWords(text)) {
            if (!IsStopWord(word)) {
                words.push_back(word);
            }
        }
        return words;
    }

    static int ComputeAverageRating(const vector<int>& ratings) {
        if (ratings.empty()) {
            return 0;
        }
        int rating_sum = 0;
        for (const int rating : ratings) {
            rating_sum += rating;
        }
        return rating_sum / static_cast<int>(ratings.size());
    }

    struct QueryWord {
        string data;
        bool is_minus;
        bool is_stop;
    };

    QueryWord ParseQueryWord(string text) const {
        bool is_minus = false;
        // Word shouldn't be empty
        if (text[0] == '-') {
            is_minus = true;
            text = text.substr(1);
        }
        return { text, is_minus, IsStopWord(text) };
    }

    struct Query {
        set<string> plus_words;
        set<string> minus_words;
    };

    Query ParseQuery(const string& text) const {
        Query query;
        for (const string& word : SplitIntoWords(text)) {
            const QueryWord query_word = ParseQueryWord(word);
            if (!query_word.is_stop) {
                if (query_word.is_minus) {
                    query.minus_words.insert(query_word.data);
                }
                else {
                    query.plus_words.insert(query_word.data);
                }
            }
        }
        return query;
    }

    // Existence required
    double ComputeWordInverseDocumentFreq(const string& word) const {
        return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
    }

    template <typename DocumentPredicate>
    vector<Document> FindAllDocuments(const Query& query, DocumentPredicate document_predicate) const {
        map<int, double> document_to_relevance;
        for (const string& word : query.plus_words) {
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

        for (const string& word : query.minus_words) {
            if (word_to_document_freqs_.count(word) == 0) {
                continue;
            }
            for (const auto [document_id, _] : word_to_document_freqs_.at(word)) {
                document_to_relevance.erase(document_id);
            }
        }

        vector<Document> matched_documents;
        for (const auto [document_id, relevance] : document_to_relevance) {
            matched_documents.push_back(
                { document_id, relevance, documents_.at(document_id).rating });
        }
        return matched_documents;
    }
};
/*
   Подставьте сюда вашу реализацию макросов
   ASSERT, ASSERT_EQUAL, ASSERT_EQUAL_HINT, ASSERT_HINT и RUN_TEST
   
*/
template <typename T, typename U>
void AssertEqualImpl(const T& t, const U& u, const string& t_str, const string& u_str, const string& file,
const string& func, unsigned line, const string& hint) {
    if (t != u) {
        cerr << boolalpha;
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT_EQUAL("s << t_str << ", "s << u_str << ") failed: "s;
        cerr << t << " != "s << u << "."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT_EQUAL(a, b) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, ""s)
#define ASSERT_EQUAL_HINT(a, b, hint) AssertEqualImpl((a), (b), #a, #b, __FILE__, __FUNCTION__, __LINE__, (hint))

void AssertImpl(bool value, const string& expr_str, const string& file, const string& func, unsigned line,
const string& hint) {
    if (!value) {
        cerr << file << "("s << line << "): "s << func << ": "s;
        cerr << "ASSERT("s << expr_str << ") failed."s;
        if (!hint.empty()) {
            cerr << " Hint: "s << hint;
        }
        cerr << endl;
        abort();
    }
}

#define ASSERT(expr) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, ""s)

#define ASSERT_HINT(expr, hint) AssertImpl(!!(expr), #expr, __FILE__, __FUNCTION__, __LINE__, (hint))

template <typename Func>
void RunTestImpl(const Func& func, const string& func_name) {
func();
    cerr << func_name << " OK"s << endl;
}

#define RUN_TEST(func) RunTestImpl((func), #func)
// -------- Начало модульных тестов поисковой системы ----------

// Тест проверяет, что поисковая система исключает стоп-слова при добавлении документов
void TestExcludeStopWordsFromAddedDocumentContent() {
    const int doc_id = 42;
    const string content = "cat in the city"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("in"s);
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }

    {
        SearchServer server;
        server.SetStopWords("in the"s);
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        ASSERT_HINT(server.FindTopDocuments("in"s).empty(),
                    "Stop words must be excluded from documents"s);
    }
}

/*
Разместите код остальных тестов здесь
*/
void TestAddDocument() {
    const int doc_id = 7;
    const string content = "today is the day"s;
    const vector<int> ratings = {1, 2, 3};
    
    {
        SearchServer server;
        server.AddDocument(doc_id, content,  DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("today");
        ASSERT_EQUAL(found_docs.size(), 1u);
        const Document& doc0 = found_docs[0];
        ASSERT_EQUAL(doc0.id, doc_id);
    }
}

void TestExcludeStopWords() {
    const int doc_id = 7;
    const string content = "today is the day"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        server.SetStopWords("the"s);
        server.AddDocument(doc_id, content,  DocumentStatus::ACTUAL, ratings);
        ASSERT(server.FindTopDocuments("the"s).empty());
    }
}

void TestExcludeDocsWithMinusWords() {
    const int doc_id_1 = 7;
    const string content_1 = "today is the day"s;
    const vector<int> ratings_1 = {1, 2, 3};
    
    const int doc_id_2 = 10;
    const string content_2 = "It is a wonderful world"s;
    const vector<int> ratings_2 = {2, 4, 3};
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1,  DocumentStatus::ACTUAL, ratings_1);
        
        ASSERT(server.FindTopDocuments("-today is the day"s).empty());
        ASSERT(server.FindTopDocuments("-today -is -the -day"s).empty());
        ASSERT(server.FindTopDocuments("-today"s).empty());
    }
    
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1,  DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2,  DocumentStatus::ACTUAL, ratings_2);
        
        const auto found_docs_1 = server.FindTopDocuments("-is");
        ASSERT_EQUAL(found_docs_1.size(), 0);
        
        const auto found_docs_2 = server.FindTopDocuments("-today is");
        ASSERT_EQUAL(found_docs_2.size(), 1u);
    }
}

void TestMatchDocuments() {
    const int doc_id = 7;
    const string content = "today is the day"s;
    const vector<int> ratings = {1, 2, 3};
    {
        SearchServer server;
        const string query = "the day one"s;
        
        server.AddDocument(doc_id, content,  DocumentStatus::ACTUAL, ratings);
        const auto [matched_words,_] = server.MatchDocument(query, doc_id);
        ASSERT_EQUAL(matched_words.size(), 2);
    }

    {
        SearchServer server;
        const string query = "the -day one"s;
        
        server.AddDocument(doc_id, content,  DocumentStatus::ACTUAL, ratings);
        const auto [matched_words,_] = server.MatchDocument(query, doc_id);
        ASSERT(matched_words.empty());
    }
}

void TestSortRelevance () {
    const int doc_id_1 = 7;
    const string content_1 = "today is the day"s;
    const vector<int> ratings_1 = {1, 2, 3};
    
    const int doc_id_2 = 10;
    const string content_2 = "It is a wonderful world"s;
    const vector<int> ratings_2 = {2, 4, 3};
    
    const int doc_id_3 = 3;
    const string content_3 = "the best days"s;
    const vector<int> ratings_3 = {0, -2, 3};
    
    const int doc_id_4 = 40;
    const string content_4 = "It is a wonderful day"s;
    const vector<int> ratings_4 = {4, 1, 5};
    
    {
        SearchServer server;
        server.AddDocument(doc_id_1, content_1,  DocumentStatus::ACTUAL, ratings_1);
        server.AddDocument(doc_id_2, content_2,  DocumentStatus::ACTUAL, ratings_2);
        server.AddDocument(doc_id_3, content_3,  DocumentStatus::ACTUAL, ratings_3);
        server.AddDocument(doc_id_4, content_4,  DocumentStatus::ACTUAL, ratings_4);
        
        const auto found_docs = server.FindTopDocuments("the day one"s);
        ASSERT(found_docs[0].relevance >= found_docs[1].relevance &&
               found_docs[1].relevance >= found_docs[2].relevance &&
               found_docs[2].relevance >= found_docs[3].relevance);
    }
}

void TestComputeAverageRating() {
    const int doc_id = 7;
    const string content = "today is the day"s;
    {
        const vector<int> ratings = {1, 2, 3};
        SearchServer server;
        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("today");
        
        ASSERT_EQUAL(found_docs[0].rating, 2);
    }
    
    {
        const vector<int> ratings = {0, 0, 0, 0};
        SearchServer server;
        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("today");
        
        ASSERT_EQUAL(found_docs[0].rating, 0);
    }
    
    {
        const vector<int> ratings = {-4, 1, 6};
        SearchServer server;
        
        server.AddDocument(doc_id, content, DocumentStatus::ACTUAL, ratings);
        const auto found_docs = server.FindTopDocuments("today");
        
        ASSERT_EQUAL(found_docs[0].rating, 1);
    }
}

void TestDocumentPredicate() {
    SearchServer server;
    
    server.AddDocument(0, "today is the day"s,        DocumentStatus::ACTUAL, {8, -3});
    server.AddDocument(1, "It is a wonderful world"s,       DocumentStatus::ACTUAL, {7, 2, 7});
    server.AddDocument(2, "the best days"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
    server.AddDocument(3, "It is a wonderful day"s,         DocumentStatus::BANNED, {9});
    
    auto found_docs = server.FindTopDocuments("today is best day"s);
    ASSERT_EQUAL(found_docs.size(), 3u);
    
    found_docs = server.FindTopDocuments("today is best day"s, [](int document_id, DocumentStatus status, int rating) {return status == DocumentStatus::ACTUAL;});
    ASSERT_EQUAL(found_docs.size(), 3u);
    
    
    found_docs = server.FindTopDocuments("today is best day"s, [](int document_id, DocumentStatus status, int rating) {return document_id % 2 == 0;});
    ASSERT_EQUAL(found_docs.size(), 2u);
}

void TestDocumentStatus() {
    SearchServer server;
    
    server.AddDocument(0, "today is the day"s, DocumentStatus::ACTUAL, {8, -3});
 //   server.AddDocument(1, "It is a wonderful world"s, DocumentStatus::IRRELEVANT, {7, 2, 7});
    server.AddDocument(2, "the best days"s, DocumentStatus::ACTUAL, {5, -12, 2, 1});
//    server.AddDocument(3, "It is a wonderful day"s, DocumentStatus::BANNED, {9});
    server.AddDocument(4, "It is a not a wonderful day"s, DocumentStatus::REMOVED, {-100});

    auto found_docs = server.FindTopDocuments("today is best day"s,DocumentStatus::ACTUAL);
    ASSERT_EQUAL(found_docs.size(), 2u);
    
    found_docs = server.FindTopDocuments("today is best day"s,DocumentStatus::REMOVED);
    ASSERT_EQUAL(found_docs.size(), 1u);
}

void TestRelevance() {
    {
        SearchServer server;
        server.AddDocument(0, "today is the day"s, DocumentStatus::ACTUAL, {8, -3});
        auto found_docs = server.FindTopDocuments("today"s);
        ASSERT_EQUAL(found_docs[0].relevance, 0);
    }
    
    {
        SearchServer server;
        server.AddDocument(1, "today today today"s, DocumentStatus::ACTUAL, {8, -3});
        auto found_docs = server.FindTopDocuments("today"s);
        ASSERT_EQUAL(found_docs[0].relevance, 0);
    }
    
    {
        SearchServer server;
        server.AddDocument(0, "today today today"s, DocumentStatus::ACTUAL, {8});
        server.AddDocument(1, "It is a wonderful day"s, DocumentStatus::ACTUAL, {9});
        auto found_docs = server.FindTopDocuments("today"s);
        ASSERT((found_docs[0].relevance - 0.693147) < 1e-6 );
    }
}

// Функция TestSearchServer является точкой входа для запуска тестов
void TestSearchServer() {
    RUN_TEST(TestExcludeStopWordsFromAddedDocumentContent);
    // Не забудьте вызывать остальные тесты здесь
    RUN_TEST(TestAddDocument);
    RUN_TEST(TestExcludeStopWords);
    RUN_TEST(TestExcludeDocsWithMinusWords);
    RUN_TEST(TestMatchDocuments);
    RUN_TEST(TestSortRelevance);
    RUN_TEST(TestComputeAverageRating);
    RUN_TEST(TestDocumentPredicate);
    RUN_TEST(TestDocumentStatus);
    RUN_TEST(TestRelevance);
}

// --------- Окончание модульных тестов поисковой системы -----------

int main() {
    TestSearchServer();
    // Если вы видите эту строку, значит все тесты прошли успешно
    cout << "Search server testing finished"s << endl;
}
