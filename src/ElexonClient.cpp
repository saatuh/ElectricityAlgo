// I import functions from the Elexon API client here, namely fetchActualDemand();, fetchDemandForecast();, fetchMatketIndexPrice(); and the GridState srtuct (public by default, hence chosen over an object)
#include "ElexonClient.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
using Clock = std::chrono::system_clock;

namespace{
    std::string getString(const json& record,const std::string& key) {
        if (!record.is_object() || !record.contains(key) || !record[key].is_string()) {
            return "";
        }
        return record[key].get<std::string>();
    }

    int getInt(const json& record, const std::string& key){
        if (!record.is_object() || !record.contains(key) || !record[key].is_number()) {
            return 0;
        }

        try{
            return record[key].get<int>();
        } catch (const json::exception&) {
            return 0;
        }
    }

    double getNumber(const json& record, const std::string& key)
    {
        if (!record.is_object() || !record.contains(key) || !record[key].is_number()) {
            return 0.0;
        }

        return record[key].get<double>();
    }

    bool hasNumber(const json& record, const std::string& key) {
        return record.is_object() && record.contains(key) && record[key].is_number();
    }

    std::string formatUtc(Clock::time_point time) {
        const std::time_t value = Clock::to_time_t(time);
        std::tm utcTime{};

        if (gmtime_s(&utcTime, &value) != 0) {
            throw std::runtime_error(
                "Failed to convert time to UTC."
            );
        }

        std::ostringstream output;
        output << std::put_time(&utcTime,"%Y-%m-%dT%H:%M:%SZ");

        return output.str();
    }

    std::string queryEncode(const std::string& value) {
        std::string result;

        for (const char ch : value) {
            if (ch == ':') {
                result += "%3A";
            } else if (ch == ' ') {
                result += "%20";
            } else {
                result += ch;
            }
        }

        return result;
    }

    bool isInterconnector(const std::string& fuelType) {
        return fuelType == "INTELEC" || fuelType == "INTFR" || fuelType == "INTGRNL" || fuelType == "INTIFA2" || fuelType == "INTIRL" || fuelType == "INTNED" || fuelType == "INTNEM" || fuelType == "INTNSL" || fuelType == "INTVKL";
    }
}

ElexonClient::ElexonClient(HttpClient& http)
    : http_(http)
{
}

std::string ElexonClient::utcNow() {
    return formatUtc(Clock::now());
}

std::string ElexonClient::utcHoursAgo(int hours) {
    return formatUtc(Clock::now() - std::chrono::hours{hours});
}

json ElexonClient::extractRecords(const json& payload) {
    if (payload.is_array()) {
        return payload;
    }

    if (payload.is_object() && payload.contains("data") && payload["data"].is_array()) {
        return payload["data"];
    }

    throw std::runtime_error(
        "Unexpected Elexon response: expected an array "
        "or an object containing a data array."
    );
}

json ElexonClient::fetchStream(const std::string& dataset,const std::string& fromTime,const std::string& toTime) {
    const std::string url = std::string(BASE_URL) + "/datasets/" + dataset + "/stream?publishDateTimeFrom=" + queryEncode(fromTime) + "&publishDateTimeTo=" + queryEncode(toTime);

    return extractRecords(http_.getJson(url));
}

GridState ElexonClient::fetchLatestGridState() {
    const std::string url = std::string(BASE_URL) + "/datasets/FUELINST";
    const json records = extractRecords(http_.getJson(url));
    if (records.empty()) {
        throw std::runtime_error("FUELINST returned no records.");
    }

    std::string latestPublishTime;

    for (const auto& record : records){
        const std::string publishTime = getString(record, "publishTime");

        if (publishTime > latestPublishTime) {
            latestPublishTime = publishTime;
        }
    }

    if (latestPublishTime.empty()){
        throw std::runtime_error("FUELINST contains no publication timestamps.");
    }

    GridState grid;
    grid.snapshotPublishTime = latestPublishTime;

    bool intervalIdentified = false;

    for (const auto& record : records){
        if (getString(record, "publishTime") != latestPublishTime) {
            continue;
        }

        if (!hasNumber(record, "generation")) {
            continue;
        }

        const std::string settlementDate = getString(record, "settlementDate");

        const std::string startTime = getString(record, "startTime");

        const int period = getInt(record, "settlementPeriod");

        if (settlementDate.empty() || startTime.empty() || period <= 0) {
            continue;
        }

        if (!intervalIdentified) {
            grid.settlementDate = settlementDate;
            grid.intervalStartTime = startTime;
            grid.settlementPeriod = period;
            intervalIdentified = true;
        } else if ( grid.settlementDate != settlementDate || grid.intervalStartTime != startTime || grid.settlementPeriod != period) {
            throw std::runtime_error(
                "Latest FUELINST records contain inconsistent "
                "settlement interval metadata."
            );
        }

        const std::string fuelType = getString(record, "fuelType");

        const double generation = getNumber(record, "generation");

        if (fuelType == "WIND") {
            grid.windGen += generation;
        } else if (fuelType == "SOLAR"){
            grid.solarGen += generation;
        } else if (fuelType == "NUCLEAR") {
            grid.nuclearGen += generation;
        } else if (fuelType == "CCGT") {
            grid.gasGen += generation;
        } else if (isInterconnector(fuelType)) {
            if (generation >= 0.0) {
                grid.imports += generation;
            } else {
                grid.exports += -generation;
            }
        }
    }

    if (!intervalIdentified) {
        throw std::runtime_error("Could not identify the latest FUELINST settlement interval.");
    }

    return grid;
}

void ElexonClient::fetchActualDemand(GridState& grid) {
    grid.actualDemand.reset();
    grid.actualDemandPublishTime.clear();
    grid.actualDemandStartTime.clear();
    grid.actualDemandSettlementDate.clear();
    grid.actualDemandSettlementPeriod = 0;

    json records;

    const std::string dateUrl = std::string(BASE_URL) + "/demand/actual/total?from=" + grid.settlementDate + "&to=" + grid.settlementDate + "&settlementPeriodFrom=" + std::to_string(grid.settlementPeriod) + "&settlementPeriodTo=" + std::to_string(grid.settlementPeriod) + "&format=json";

    try {
        records = extractRecords(http_.getJson(dateUrl));
        std::cout
            << "ATL date endpoint returned "
            << records.size() << " record(s).\n";
    } catch (const std::exception& error) {
        std::cerr << "ATL date endpoint failed: " << error.what() << '\n';
    }

    if (!records.is_array() || records.empty()) {
        try {
            records = fetchStream("ATL", utcHoursAgo(168), utcNow());

            std::cout
                << "ATL stream returned "
                << records.size()
                << " record(s) in the publication window.\n";
        } catch (const std::exception& error) {
            std::cerr
                << "ATL stream failed: "
                << error.what() << '\n';

            return;
        }
    }

    const json* best = nullptr;
    int bestPeriod = 0;
    std::string bestPublishTime;

    for (const auto& record : records) {
        if (getString(record, "settlementDate") != grid.settlementDate || !hasNumber(record, "quantity")) {
            continue;
        }

        const int period = getInt(record, "settlementPeriod");

        if (period <= 0 || period > grid.settlementPeriod) {
            continue;
        }

        const std::string publishTime = getString(record, "publishTime");

        if (best == nullptr || period > bestPeriod || (period == bestPeriod && publishTime > bestPublishTime)) {
            best = &record;
            bestPeriod = period;
            bestPublishTime = publishTime;
        }
    }

    if (best == nullptr){
        std::cout
            << "ATL: no available actual-demand record on "
            << grid.settlementDate
            << " up to period "
            << grid.settlementPeriod << ".\n";

        if (records.is_array()) {
            int shown = 0;

            for (const auto& record : records){
                if (shown >= 3){
                    break;
                }

                std::cout
                    << "ATL diagnostic record: "
                    << record.dump() << '\n';

                ++shown;
            }
        }

        return;
    }

    grid.actualDemand = getNumber(*best, "quantity");

    grid.actualDemandPublishTime = getString(*best, "publishTime");

    grid.actualDemandStartTime = getString(*best, "startTime");

    grid.actualDemandSettlementDate = getString(*best, "settlementDate");

    grid.actualDemandSettlementPeriod = getInt(*best, "settlementPeriod");

    std::cout
        << "ATL loaded: " << *grid.actualDemand << " MW"
        << "; actual-demand period "
        << grid.actualDemandSettlementPeriod
        << "; interval " << grid.actualDemandStartTime
        << "; published "
        << grid.actualDemandPublishTime << ".\n";

    if (grid.actualDemandSettlementPeriod < grid.settlementPeriod) {
        std::cout
            << "ATL latency: actual demand is "
            << grid.settlementPeriod -
               grid.actualDemandSettlementPeriod
            << " settlement period(s) behind the latest grid snapshot.\n";
    }
}

void ElexonClient::fetchDemandForecast(GridState& grid) {
    grid.demandForecast.reset();
    grid.forecastPublishTime.clear();
    grid.forecastStartTime.clear();
    grid.forecastSettlementDate.clear();
    grid.forecastSettlementPeriod = 0;

    const bool hasActual = grid.actualDemand.has_value();

    const std::string targetDate = hasActual ? grid.actualDemandSettlementDate : grid.settlementDate;

    const int targetPeriod = hasActual ? grid.actualDemandSettlementPeriod : grid.settlementPeriod;

    const json records = fetchStream("DATL",utcHoursAgo(168),utcNow());

    const json* best = nullptr;
    std::string bestPublishTime;

    for (const auto& record : records) {
        if (getString(record, "settlementDate") != targetDate || getInt(record, "settlementPeriod") != targetPeriod || !hasNumber(record, "quantity")) {
            continue;
        }

        const std::string publishTime = getString(record, "publishTime");

        if (best == nullptr || publishTime > bestPublishTime) {
            best = &record;
            bestPublishTime = publishTime;
        }
    }

    if (best == nullptr){
        std::cout
            << "DATL: no forecast for "
            << targetDate << ", period "
            << targetPeriod << ".\n";

        return;
    }

    grid.demandForecast = getNumber(*best, "quantity");

    grid.forecastPublishTime = getString(*best, "publishTime");

    grid.forecastStartTime = getString(*best, "startTime");

    grid.forecastSettlementDate = getString(*best, "settlementDate");

    grid.forecastSettlementPeriod = getInt(*best, "settlementPeriod");
}

void ElexonClient::fetchMarketIndexPrice(GridState& grid) {
    grid.marketIndexPrice.reset();
    grid.marketIndexVolume.reset();
    grid.marketIndexProviders.clear();

    const bool hasActual = grid.actualDemand.has_value();

    const std::string targetDate = hasActual ? grid.actualDemandSettlementDate : grid.settlementDate;

    const int targetPeriod = hasActual ? grid.actualDemandSettlementPeriod : grid.settlementPeriod;

    const std::string url = std::string(BASE_URL) + "/balancing/pricing/market-index?from=" + targetDate + "&to=" + targetDate + "&settlementPeriodFrom=" + std::to_string(targetPeriod) + "&settlementPeriodTo=" + std::to_string(targetPeriod) + "&format=json";

    json records;

    try {
        records = extractRecords(http_.getJson(url));
    } catch (const std::exception& error) {
        std::cerr
            << "MID request failed: "
            << error.what() << '\n';

        return;
    }

    double weightedPriceTotal = 0.0;
    double volumeTotal = 0.0;
    std::string providers;

    for (const auto& record : records) {
        if (getString(record, "settlementDate") != targetDate || getInt(record, "settlementPeriod") != targetPeriod) {
            continue;
        }

        if (!hasNumber(record, "price") || !hasNumber(record, "volume")) {
            continue;
        }

        const double price = getNumber(record, "price");

        const double volume = getNumber(record, "volume");

        if (volume <= 0.0) {
            continue;
        }

        weightedPriceTotal += price * volume;
        volumeTotal += volume;

        const std::string provider = getString(record, "dataProvider");

        if (!provider.empty()) {
            if (!providers.empty()) {
                providers += ", ";
            }

            providers += provider;
        }
    }

    if (volumeTotal <= 0.0) {
        std::cout
            << "MID: no usable price/volume records for "
            << targetDate << ", period "
            << targetPeriod << ".\n";

        if (records.empty()) {
            std::cout << "MID returned an empty record array.\n";
        } else {
            std::cout
                << "MID sample response record: "
                << records.front().dump() << '\n';
        }

        return;
    }

    grid.marketIndexPrice = weightedPriceTotal / volumeTotal;
    grid.marketIndexVolume = volumeTotal;
    grid.marketIndexProviders = providers;
}
