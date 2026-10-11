#pragma once

#include "HttpClient.hpp"
#include "Types.hpp"

#include <nlohmann/json.hpp>
#include <string>

class ElexonClient {
public:
    explicit ElexonClient(HttpClient& http);

    GridState fetchLatestGridState();

    void fetchActualDemand(GridState& grid);
    void fetchDemandForecast(GridState& grid);
    void fetchMarketIndexPrice(GridState& grid);

private:
    HttpClient& http_;

    static constexpr const char* BASE_URL =
        "https://data.elexon.co.uk/bmrs/api/v1";

    nlohmann::json fetchStream(
        const std::string& dataset,
        const std::string& fromTime,
        const std::string& toTime
    );

    static nlohmann::json extractRecords(
        const nlohmann::json& payload
    );

    static std::string utcNow();
    static std::string utcHoursAgo(int hours);
};
