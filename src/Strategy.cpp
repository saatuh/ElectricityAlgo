#include "Strategy.hpp"

MarketFeatures calculateFeatures(const GridState& grid)
{
    MarketFeatures features;

    features.trackedGenerationMW =
        grid.windGen +
        grid.solarGen +
        grid.nuclearGen +
        grid.gasGen;

    features.netImportsMW =
        grid.imports - grid.exports;

    const bool sameInterval =
        grid.actualDemand.has_value() &&
        grid.demandForecast.has_value() &&
        grid.actualDemandSettlementDate ==
            grid.forecastSettlementDate &&
        grid.actualDemandSettlementPeriod ==
            grid.forecastSettlementPeriod &&
        !grid.actualDemandStartTime.empty() &&
        grid.actualDemandStartTime ==
            grid.forecastStartTime;

    if (sameInterval && *grid.demandForecast > 0.0)
    {
        const double error =
            *grid.actualDemand - *grid.demandForecast;

        features.demandForecastErrorMW = error;

        features.demandErrorPercent =
            100.0 * error / *grid.demandForecast;
    }

    return features;
}

MarketState classifyMarketState(const MarketFeatures& features)
{
    if (!features.demandErrorPercent.has_value())
    {
        return MarketState::InsufficientData;
    }

    if (*features.demandErrorPercent > 2.0)
    {
        return MarketState::DemandAboveForecast;
    }

    if (*features.demandErrorPercent < -2.0)
    {
        return MarketState::DemandBelowForecast;
    }

    return MarketState::DemandNearForecast;
}

PaperSignal generatePaperSignal(
    const GridState& grid,
    const MarketFeatures& features,
    MarketState state)
{
    if (!features.demandForecastErrorMW.has_value() ||
        !grid.marketIndexPrice.has_value() ||
        !grid.marketIndexVolume.has_value() ||
        *grid.marketIndexVolume <= 0.0)
    {
        return PaperSignal::NoSignal;
    }

    switch (state)
    {
        case MarketState::DemandAboveForecast:
            return PaperSignal::Buy;

        case MarketState::DemandBelowForecast:
            return PaperSignal::Sell;

        case MarketState::DemandNearForecast:
            return PaperSignal::Hold;

        case MarketState::InsufficientData:
            return PaperSignal::NoSignal;
    }

    return PaperSignal::NoSignal;
}

const char* toString(MarketState state)
{
    switch (state)
    {
        case MarketState::InsufficientData:
            return "INSUFFICIENT_DATA";

        case MarketState::DemandAboveForecast:
            return "DEMAND_ABOVE_FORECAST";

        case MarketState::DemandBelowForecast:
            return "DEMAND_BELOW_FORECAST";

        case MarketState::DemandNearForecast:
            return "DEMAND_NEAR_FORECAST";
    }

    return "UNKNOWN";
}

const char* toString(PaperSignal signal)
{
    switch (signal)
    {
        case PaperSignal::NoSignal:
            return "NO SIGNAL";

        case PaperSignal::Hold:
            return "HOLD";

        case PaperSignal::Buy:
            return "BUY (PAPER ONLY)";

        case PaperSignal::Sell:
            return "SELL (PAPER ONLY)";
    }

    return "UNKNOWN";
}