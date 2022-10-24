#pragma once
#include <numeric>
#include <cmath>
#include <map>
#include <algorithm>
#include <tuple>
#include "document.h"
#include "read_input_functions.h"
#include <execution>
#include "concurrent_map.h"
#include <type_traits>
#include <future>
#include "log_duration.h"
#include <iterator>
#include <type_traits>
#include <utility>
const int MAX_RESULT_DOCUMENT_COUNT = 5;
const double EPSILON = 1e-6;
//using namespace std;
class SearchServer {
public:
    explicit SearchServer(const std::string& stop_words_text);
    explicit SearchServer(const std::string_view& stop_words_text);
    template <typename StringContainer>
    explicit SearchServer(const StringContainer& stop_words)
        : stop_words_(MakeUniqueNonEmptyStrings(stop_words))
    {
        if (!all_of(stop_words_.begin(), stop_words_.end(), IsValidWord)) {
            throw std::invalid_argument("Some of stop words are invalid");
        }
    }


    void AddDocument(int document_id, const std::string_view& document, DocumentStatus status, const std::vector<int>& ratings);


    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const Policy& policy, std::string_view raw_query, DocumentPredicate document_predicate) const {
        const auto query = ParseQuery(raw_query);

        auto matched_documents = FindAllDocuments(policy, query, document_predicate);

        sort(policy, matched_documents.begin(), matched_documents.end(), [&](const Document& lhs, const Document& rhs) {
            if (std::abs(lhs.relevance - rhs.relevance) < EPSILON) {
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


    template <typename DocumentPredicate>
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentPredicate document_predicate) const {
        return FindTopDocuments(std::execution::seq, raw_query, document_predicate);
    }
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view& raw_query, DocumentStatus status) const {
        return FindTopDocuments(policy, raw_query, [status](int document_id, DocumentStatus document_status, int rating) {
            return document_status == status;
            });
    }
    template <typename Policy>
    std::vector<Document> FindTopDocuments(const Policy& policy, const std::string_view& raw_query) const {
        return FindTopDocuments(policy, raw_query, DocumentStatus::ACTUAL);
    }
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query, DocumentStatus status) const;
    std::vector<Document> FindTopDocuments(const std::string_view& raw_query) const;

    template <typename Policy>
    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(Policy policy, const std::string_view& raw_query, int document_id)const {

        if (!document_ids_.count(document_id)) {
            throw std::out_of_range("invalid document id");
        }

        const auto query = ParseQuery(raw_query);
        std::vector<std::string_view> matched_words(query.plus_words.size());
        int count = 0;

        std::copy_if(policy, query.plus_words.begin(), query.plus_words.end(), matched_words.begin(), [&](auto& word) {
            if (word_to_document_freqs_.at(word).count(document_id))
            {
                count++;
                return true;
            }
            return false;
            });

        matched_words.resize(count);

        for_each(policy, query.minus_words.begin(), query.minus_words.end(), [&](auto word) {
            if (word_to_document_freqs_.count(word)) {
                if (word_to_document_freqs_.at(word).count(document_id)) {
                    matched_words.clear();
                }
            }
            });

        return { matched_words, documents_.at(document_id).status };

    }

        

    std::tuple<std::vector<std::string_view>, DocumentStatus> MatchDocument(const std::string_view& raw_query, int document_id) const;
    int GetDocumentCount() const;
    const std::map<std::string_view, double>& GetWordFrequencies(int document_id) const;

    template <typename Policy>
    void RemoveDocument(Policy policy, int document_id) {
        if (documents_.count(document_id))
        {
            auto word_frequencies(GetWordFrequencies(document_id));
            for_each(policy, word_frequencies.begin(), word_frequencies.end(),
                [&document_id, this](std::pair<std::string_view, double> word) {
                    word_to_document_freqs_.at(word.first).erase(document_id);
                });
            documents_.erase(document_id);
            document_ids_.erase(document_id);
            document_to_word_freqs_.erase(document_id);
        }
    }

    void RemoveDocument(int document_id);
    std::set<int>::const_iterator begin() const;
    std::set<int>::const_iterator end() const;

private:


    struct DocumentData {
        int rating;
        DocumentStatus status;
    };

    const std::set<std::string> stop_words_;
    std::set<std::string, std::less<>> string_words_;
    std::map<std::string_view, std::map<int, double>> word_to_document_freqs_;
    std::map<int, std::map<std::string_view, double>> document_to_word_freqs_;
    std::map<int, DocumentData> documents_;
    std::set<int> document_ids_;
    bool IsStopWord(const std::string& word) const;
    bool IsStopWordView(const std::string_view word) const;
    static bool IsValidWord(const std::string_view word);
    std::vector<std::string> SplitIntoWordsNoStop(const std::string& text) const;
    std::vector<std::string_view> SplitIntoWordsNoStopView(const std::string_view text) const;
    static int ComputeAverageRating(const std::vector<int>& ratings);
    struct QueryWord
    {
        std::string_view data;
        bool is_minus;
        bool is_stop;
    };
    QueryWord ParseQueryWord(const std::string& text) const;
    QueryWord ParseQueryWordView(const std::string_view text) const;
    struct Query {
        std::vector<std::string_view> plus_words;
        std::vector<std::string_view> minus_words;
    };
    Query ParseQuery(std::string_view text) const;
    double ComputeWordInverseDocumentFreq(const std::string& word) const;

    template <typename Policy, typename DocumentPredicate>
    std::vector<Document> FindAllDocuments(const Policy& policy, const Query& query, DocumentPredicate document_predicate) const {
        ConcurrentMap<int, double> document_relevance(documents_.size() > 100 ? documents_.size() / 100 : 4);
        auto plus_words_func = [&](const std::string_view word) {
            auto it_word = word_to_document_freqs_.find(word);
            if (it_word == word_to_document_freqs_.end()) {
                return;
            }
            auto& [_, docs_freqs] = *it_word;
            const double inverse_document_freq = std::log(static_cast<double>(GetDocumentCount()) / docs_freqs.size());
            for (const auto& [document_id, term_freq] : docs_freqs) {
                const SearchServer::DocumentData& document_data = documents_.at(document_id);
                if (document_predicate(document_id, document_data.status, document_data.rating)) {
                    document_relevance[document_id].ref_to_value += term_freq * inverse_document_freq;
                }

            }
        };

        std::for_each(policy, query.plus_words.begin(), query.plus_words.end(), plus_words_func);

        std::for_each(policy, query.minus_words.begin(), query.minus_words.end(),
            [this, &document_relevance](const auto minus_word) {
                auto it_word = word_to_document_freqs_.find(minus_word);
                if (it_word == word_to_document_freqs_.end()) {
                    return;
                }
                auto& [_, docs_freqs] = *it_word;
                for (const auto [id, _] : docs_freqs) {
                    document_relevance.erase(id);
                }
            });

        std::vector<Document> matched_documents;
        auto doc_rel = document_relevance.BuildOrdinaryMap();
        matched_documents.reserve(doc_rel.size());

        for (auto& [document_id, relevance] : doc_rel) {
            matched_documents.push_back({ document_id, relevance, documents_.at(document_id).rating });
        }

        return matched_documents;
    }

};

void AddDocument(SearchServer& search_server, int document_id, const std::string& document, DocumentStatus status,
    const std::vector<int>& ratings);
void FindTopDocuments(const SearchServer& search_server, const std::string& raw_query);
void MatchDocuments(const SearchServer& search_server, const std::string& query);
