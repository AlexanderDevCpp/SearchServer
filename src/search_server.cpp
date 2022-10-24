#include "search_server.h"
using namespace std;

SearchServer::SearchServer(const string& stop_words_text)
    : SearchServer(SplitIntoWords(stop_words_text))

{

}

SearchServer::SearchServer(const string_view& stop_words_text)
    : SearchServer(SplitIntoWords(string{ stop_words_text.substr() }))
{

}

void SearchServer::AddDocument(int document_id, const string_view& document, DocumentStatus status, const vector<int>& ratings) {
    if ((document_id < 0) || (documents_.count(document_id) > 0)) {
        throw invalid_argument("Invalid document id"s);
    }
    const auto words = SplitIntoWordsNoStopView(document);

    const double inv_word_count = 1.0 / words.size();
    for (const string_view word : words) {
        string_words_.emplace(word);
        word_to_document_freqs_[*string_words_.find(word)][document_id] += inv_word_count;
        document_to_word_freqs_[document_id][*string_words_.find(word)] += inv_word_count;
    }
    SearchServer::documents_.emplace(document_id, DocumentData{ ComputeAverageRating(ratings), status });
    document_ids_.emplace(document_id);
}

int SearchServer::GetDocumentCount() const{
    return documents_.size();
}


std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query) const {
    return FindTopDocuments(raw_query, DocumentStatus::ACTUAL);
}
std::vector<Document> SearchServer::FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const {
    return FindTopDocuments(raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
        return document_status == status;
        });
}

tuple<vector<string_view>, DocumentStatus> SearchServer::MatchDocument(const string_view& raw_query, int document_id)const {
    return MatchDocument(std::execution::seq,raw_query, document_id);
}

void SearchServer::RemoveDocument(int document_id) {
    return RemoveDocument(std::execution::seq, document_id);
}
const map<string_view, double>& SearchServer::GetWordFrequencies(int document_id) const
{
    static map<string_view, double> empty;
    return document_to_word_freqs_.count(document_id) ? document_to_word_freqs_.at(document_id) : empty;
}

std::set<int>::const_iterator SearchServer::begin() const
{
    return document_ids_.begin();
}

std::set<int>::const_iterator SearchServer::end() const
{
    return document_ids_.end();
}

//private
bool SearchServer::IsStopWord(const string& word)const{
    return stop_words_.count(word);
}

bool SearchServer::IsStopWordView(const string_view word)const {
    return stop_words_.count(string(word));
}

bool SearchServer::IsValidWord(const string_view word){
    return none_of(word.begin(), word.end(), [](char c) {
        return c >= '\0' && c < ' ';
        });
}

vector<string> SearchServer::SplitIntoWordsNoStop(const string& text) const {
    vector<string> words;
    for (const string& word : SplitIntoWords(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word "s + word + " is invalid"s);
        }
        if (!IsStopWord(word)) {
            words.push_back(word);
        }
    }
    return words;
}

vector<string_view> SearchServer::SplitIntoWordsNoStopView(const string_view text) const {
    vector<string_view> words;
    for (const string_view word : SplitIntoWordsView(text)) {
        if (!IsValidWord(word)) {
            throw invalid_argument("Word is invalid"s);
        }
        if (!IsStopWordView(word)) {
            words.push_back(word);
        }
    }
    return words;
}


int SearchServer::ComputeAverageRating(const vector<int>& ratings) {
    if (ratings.empty()) {
        return 0;
    }
    return accumulate(ratings.begin(),ratings.end(),0) / static_cast<int>(ratings.size());
}

SearchServer::QueryWord SearchServer::ParseQueryWord(const string& text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    string word = text;
    bool is_minus = false;
    if (word[0] == '-') {
        is_minus = true;
        word = word.substr(1);
    }
    if (word.empty() || word[0] == '-' || !IsValidWord(word)) {
        throw invalid_argument("Query word "s + text + " is invalid");
    }
    return { word, is_minus, IsStopWord(word) };
}

SearchServer::QueryWord SearchServer::ParseQueryWordView(string_view text) const {
    if (text.empty()) {
        throw invalid_argument("Query word is empty"s);
    }
    bool is_minus = false;
    if (text[0] == '-') {
        text = text.substr(1);
        is_minus = true;
    }
    if ((text.empty() || text[0] == '-' || !IsValidWord(text))) {
        throw invalid_argument("Query word "s);
    }
    return { text, is_minus, IsStopWordView(text) };
}

SearchServer::Query SearchServer::ParseQuery(std::string_view text) const {

    auto words = SplitIntoWordsView(text);

    std::set<std::string_view> plus_words;
    std::set<std::string_view> minus_words;

    for (auto word : words) {
        const auto query_word = ParseQueryWordView(word);
        if (!query_word.is_stop) {
            if (query_word.is_minus) {
                minus_words.insert(move(query_word.data));
            }
            else {
                plus_words.insert(move(query_word.data));
            }
        }
    }
    return { std::vector<std::string_view>(plus_words.begin(), plus_words.end()),
        std::vector<std::string_view>(minus_words.begin(), minus_words.end()) };
}

double SearchServer::ComputeWordInverseDocumentFreq(const string& word) const {
    return log(GetDocumentCount() * 1.0 / word_to_document_freqs_.at(word).size());
}

void AddDocument(SearchServer& search_server, int document_id, const string& document, DocumentStatus status,
    const vector<int>& ratings) {
    try {
        search_server.AddDocument(document_id, document, status, ratings);
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка добавления документа "s << document_id << ": "s << e.what() << endl;
    }
}

void FindTopDocuments(const SearchServer& search_server, const string& raw_query) {
    cout << "Результаты поиска по запросу: "s << raw_query << endl;
    try {
        for (const Document& document : search_server.FindTopDocuments(raw_query)) {
            PrintDocument(document);
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка поиска: "s << e.what() << endl;
    }
}

void MatchDocuments(const SearchServer& search_server, const string& query) {
    try {
        cout << "Матчинг документов по запросу: "s << query << endl;
        for (const int id : search_server) {
            const int document_id = id;
            const auto [words, status] = search_server.MatchDocument(query, document_id);
            PrintMatchDocumentResult(document_id, words, status);
        }
    }
    catch (const invalid_argument& e) {
        cout << "Ошибка матчинга документов на запрос "s << query << ": "s << e.what() << endl;
    }
}