// just a design choice, was facing problems with repeated header file processing, pragma once ensures it's processed only once
#pragma once 

#include <optional>
#include <string>

struct GridState {
    // datatypes for latest generation snapshots (will update more variables soon)
    std::string snapshotPublishTime;
    std::string settlementDate;
    std::string intervalStartTime;
    int settlementPeriod = 0;
    double windGen = 0.0;
    double solarGen = 0.0;
    double nuclearGen = 0.0;
    double gasGen = 0.0;
    double imports = 0.0;
    double exports = 0.0;
    // These are the actual demand obersvation container types
    std::optional<double> actualDemand;
    std::string actualDemandPublishTime;
    std::string actualDemandStartTime;
    std::string actualDemandSettlementDate;
    int actualDemandSettlementPeriod = 0;
    // these are the demand forecast container types
    std::optional<double> demandForecast;
    std::string forecastPublishTime;
    std::string forecastStartTime;
    std::string forecastSettlementDate;
    int forecastSettlementPeriod = 0;
    // just market index data
    std::optional<double> marketIndexPrice;
    std::optional<double> marketIndexVolume;
    std::string marketIndexProviders;
};

struct MarketFeatures {
    std::optional<double> demandForecastErrorMW;
    double trackedGenerationMW = 0.0;
    double netImportsMW = 0.0;
    std::optional<double> demandErrorPercent;
};

// I'm using enums to replace traditional integers (A, B, C is complied as 1,2,3 by the enumerator function)
enum class MarketState { InsufficientData, DemandAboveForecast, DemandBelowForecast, DemandNearForecast };

enum class PaperSignal { NoSignal, Hold, Buy, Sell};
