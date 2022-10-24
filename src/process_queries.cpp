#include "process_queries.h"
std::vector<std::vector<Document>> ProcessQueries(
    const SearchServer& search_server,
    const std::vector<std::string>& queries) {
    std::vector<std::vector<Document>> result(queries.size());
    std::transform(std::execution::par, queries.begin(), queries.end(),
        result.begin(),
        [&search_server](const std::string_view query) {
            return search_server.FindTopDocuments(std::execution::par_unseq,query);
        });
    return result;
}

std::list<Document> ProcessQueriesJoined(const SearchServer& search_server, const std::vector<std::string>& queries) {
    auto results_for_queries = ProcessQueries(search_server, queries);

    return std::transform_reduce(std::execution::par,
        results_for_queries.begin(), results_for_queries.end(),
        std::list<Document> { },
        [](auto lhs, auto rhs) {
            lhs.splice(lhs.end(), rhs);
            return lhs;
        },
        [](auto& container) {
            return std::list<Document>(std::make_move_iterator(container.begin()),
                std::make_move_iterator(container.end()));
        });
}