#pragma once
#include <deque>
#include "search_server.h"

class RequestQueue {
public:

    explicit RequestQueue(const SearchServer& search_server);

    template <typename DocumentPredicate>
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentPredicate document_predicate) {
        QueryResult temp;
        temp.time = 0;
        std::vector<Document> documents = search_server_.FindTopDocuments(raw_query, document_predicate);
        if (!documents.empty())
        {
            for (auto& request : requests_)
            {
                request.time++;
            }
            for (auto& request : requests_)
            {
                if (request.time > 1440)
                {
                    requests_.pop_front();
                }
            }
            return documents;
        }
        else
        {
            if (requests_.empty())
            {
                requests_.push_back(QueryResult(temp));
            }
            else
            {
                requests_.push_back(QueryResult(temp));
                for (auto& request : requests_)
                {
                    request.time++;
                }
                for (auto& request : requests_)
                {
                    if (request.time > 1440)
                    {
                        requests_.pop_front();
                    }
                }
            }
            return documents;
        }
    }
    std::vector<Document> AddFindRequest(const std::string& raw_query, DocumentStatus status);
    std::vector<Document> AddFindRequest(const std::string& raw_query);
    int GetNoResultRequests() const;
private:
    struct QueryResult 
    {
        int time = 0;
    };
    std::deque<QueryResult> requests_;
    const SearchServer& search_server_;
    const static int sec_in_day_ = 1440;
};