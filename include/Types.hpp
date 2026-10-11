#pragma once

#include <optional>
#include <string>

struct GridState
{
    // Latest generation snapshot
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

    // Actual demand observation
    std::optional<double> actualDemand;
    std::string actualDemandPublishTime;
    std::string actualDemandStartTime;
    std::string actualDemandSettlementDate;
    int actualDemandSettlementPeriod = 0;

    // Demand forecast
    std::optional<double> demandForecast;
    std::string forecastPublishTime;
    std::string forecastStartTime;
    std::string forecastSettlementDate;
    int forecastSettlementPeriod = 0;

    // Market index data
    std::optional<double> marketIndexPrice;
    std::optional<double> marketIndexVolume;
    std::string marketIndexProviders;
};

struct MarketFeatures
{
    std::optional<double> demandForecastErrorMW;
    double trackedGenerationMW = 0.0;
    double netImportsMW = 0.0;
    std::optional<double> demandErrorPercent;
};

enum class MarketState
{
    InsufficientData,
    DemandAboveForecast,
    DemandBelowForecast,
    DemandNearForecast
};

enum class PaperSignal
{
    NoSignal,
    Hold,
    Buy,
    Sell
};