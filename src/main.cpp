#include "ElexonClient.hpp"
#include "HttpClient.hpp"
#include "Strategy.hpp"
#include "Types.hpp"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>

namespace {
    void printNumber( const char* label,double value, const char* unit) {
        std::cout
            << std::left << std::setw(34) << label
            << std::fixed << std::setprecision(2)
            << value << ' ' << unit << '\n';
    }

    void printOptional(const char* label,const std::optional<double>& value,const char* unit){
        std::cout << std::left << std::setw(34) << label;

        if (value.has_value()) {
            std::cout
                << std::fixed << std::setprecision(2)
                << *value << ' ' << unit;
        } else {
            std::cout << "unavailable";
        }

        std::cout << '\n';
    }

    std::string csvEscape(const std::string& value) {
        std::string result = "\"";

        for (const char ch : value) {
            if (ch == '"') {
                result += "\"\"";
            } else {
                result += ch;
            }
        }

        result += '"';

        return result;
    }

    void writeOptional(std::ofstream& file, const std::optional<double>& value) {
        if (value.has_value()) {
            file << std::setprecision(12) << *value;
        }
    }

    void appendObservation(const GridState& grid, const MarketFeatures& features, MarketState state, PaperSignal signal) {
        const std::string filename = "electricity_algo_observations.csv";

        std::ifstream existingFile(filename);
        const bool needsHeader = !existingFile.good();
        existingFile.close();

        std::ofstream file(filename, std::ios::app);

        if (!file) {
            std::cerr << "Warning: could not open CSV observation file.\n";
            return;
        }

        if (needsHeader){
            file
                << "snapshot_publish_time,"
                << "snapshot_settlement_date,"
                << "snapshot_settlement_period,"
                << "snapshot_interval_start,"
                << "actual_demand_mw,"
                << "actual_demand_date,"
                << "actual_demand_period,"
                << "actual_demand_start,"
                << "actual_demand_publish_time,"
                << "forecast_mw,"
                << "forecast_date,"
                << "forecast_period,"
                << "forecast_start,"
                << "forecast_publish_time,"
                << "forecast_error_mw,"
                << "forecast_error_percent,"
                << "wind_mw,"
                << "solar_mw,"
                << "nuclear_mw,"
                << "gas_mw,"
                << "imports_mw,"
                << "exports_mw,"
                << "market_index_price,"
                << "market_index_volume_mwh,"
                << "market_index_providers,"
                << "market_state,"
                << "paper_signal\n";
        }

        file
            << csvEscape(grid.snapshotPublishTime) << ','
            << csvEscape(grid.settlementDate) << ','
            << grid.settlementPeriod << ','
            << csvEscape(grid.intervalStartTime) << ',';

        writeOptional(file, grid.actualDemand);

        file
            << ','
            << csvEscape(grid.actualDemandSettlementDate) << ','
            << grid.actualDemandSettlementPeriod << ','
            << csvEscape(grid.actualDemandStartTime) << ','
            << csvEscape(grid.actualDemandPublishTime) << ',';

        writeOptional(file, grid.demandForecast);

        file
            << ','
            << csvEscape(grid.forecastSettlementDate) << ','
            << grid.forecastSettlementPeriod << ','
            << csvEscape(grid.forecastStartTime) << ','
            << csvEscape(grid.forecastPublishTime) << ',';

        writeOptional(file, features.demandForecastErrorMW);

        file << ',';

        writeOptional(file, features.demandErrorPercent);

        file
            << ','
            << grid.windGen << ','
            << grid.solarGen << ','
            << grid.nuclearGen << ','
            << grid.gasGen << ','
            << grid.imports << ','
            << grid.exports << ',';

        writeOptional(file, grid.marketIndexPrice);

        file << ',';

        writeOptional(file, grid.marketIndexVolume);

        file
            << ','
            << csvEscape(grid.marketIndexProviders) << ','
            << toString(state) << ','
            << csvEscape(toString(signal))
            << '\n';
    }
}

int main() {
    std::cout
        << "====================================\n"
        << "          ELECTRICITY ALGO\n"
        << "====================================\n\n";

    try {
        HttpClient http;
        ElexonClient elexon(http);

        GridState grid = elexon.fetchLatestGridState();

        std::cout << "FUELINST ingestion successful.\n";

        try {
            elexon.fetchActualDemand(grid);
        } catch (const std::exception& error) {
            std::cerr
                << "ATL ingestion error: "
                << error.what() << '\n';
        }

        try {
            elexon.fetchDemandForecast(grid);
        } catch (const std::exception& error) {
            std::cerr
                << "DATL ingestion error: "
                << error.what() << '\n';
        }

        try {
            elexon.fetchMarketIndexPrice(grid);
        } catch (const std::exception& error) {
            std::cerr << "MID ingestion error: " << error.what() << '\n';
        }

        const MarketFeatures features = calculateFeatures(grid);

        const MarketState state = classifyMarketState(features);

        const PaperSignal signal = generatePaperSignal(grid, features, state);

        std::cout
            << "\n====================================\n"
            << "           CURRENT GRID STATE\n"
            << "====================================\n";

        std::cout << "Snapshot publication: " << grid.snapshotPublishTime << '\n';
        std::cout << "Snapshot settlement date: " << grid.settlementDate << '\n';
        std::cout << "Snapshot settlement period: " << grid.settlementPeriod << '\n';
        std::cout << "Snapshot interval start: " << grid.intervalStartTime << '\n';

        printNumber("Wind generation:", grid.windGen, "MW");
        printNumber("Solar generation:", grid.solarGen, "MW");
        printNumber("Nuclear generation:", grid.nuclearGen, "MW");
        printNumber("Gas generation:", grid.gasGen, "MW");
        printNumber("Tracked generation:", features.trackedGenerationMW, "MW");
        printNumber("Imports:", grid.imports, "MW");
        printNumber("Exports:", grid.exports, "MW");
        printNumber("Net imports:", features.netImportsMW, "MW");

        std::cout << "\n--- Actual demand ---\n";

        printOptional("Actual demand:", grid.actualDemand, "MW");

        std::cout<< "Actual-demand date: " << (grid.actualDemandSettlementDate.empty() ? "unavailable" : grid.actualDemandSettlementDate)<< '\n';
        std::cout << "Actual-demand period: " << (grid.actualDemandSettlementPeriod == 0 ? "unavailable" : std::to_string(grid.actualDemandSettlementPeriod))<< '\n';
        std::cout << "Actual-demand interval: " << (grid.actualDemandStartTime.empty() ? "unavailable" : grid.actualDemandStartTime) << '\n';
        std::cout << "Actual-demand publication: " << (grid.actualDemandPublishTime.empty() ? "unavailable" : grid.actualDemandPublishTime) << '\n';
        std::cout << "\n--- Demand forecast ---\n";

        printOptional("Demand forecast:", grid.demandForecast, "MW");

        std::cout << "Forecast date: " << (grid.forecastSettlementDate.empty() ? "unavailable" : grid.forecastSettlementDate) << '\n';
        std::cout << "Forecast period: " << (grid.forecastSettlementPeriod == 0 ? "unavailable" : std::to_string(grid.forecastSettlementPeriod)) << '\n';
        std::cout << "Forecast interval: " << (grid.forecastStartTime.empty() ? "unavailable" : grid.forecastStartTime) << '\n';
        std::cout << "Forecast publication: " << (grid.forecastPublishTime.empty() ? "unavailable" : grid.forecastPublishTime) << '\n';

        printOptional("Demand forecast error:", features.demandForecastErrorMW, "MW");
        printOptional("Demand forecast error:", features.demandErrorPercent, "%");

        std::cout << "\n--- Market index ---\n";

        printOptional("Market index price:", grid.marketIndexPrice, "GBP/MWh");
        printOptional("Market index volume:", grid.marketIndexVolume, "MWh");

        std::cout << "MID providers: " << (grid.marketIndexProviders.empty() ? "unavailable" : grid.marketIndexProviders) << '\n';
        std::cout
            << "\n====================================\n"
            << "             SIGNAL ENGINE\n"
            << "====================================\n";

        std::cout << "Demand state: " << toString(state) << '\n';
        std::cout << "Paper signal: " << toString(signal) << '\n';
        std::cout << "Order execution: DISABLED\n";
        std::cout << "====================================\n";

        appendObservation(grid, features, state, signal);

        std::cout << "\nObservation file: " << "electricity_algo_observations.csv\n";
    } catch (const std::exception& error) {
        std::cerr << "\nFATAL ERROR: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
