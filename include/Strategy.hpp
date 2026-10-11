#pragma once

#include "Types.hpp"

MarketFeatures calculateFeatures(const GridState& grid);

MarketState classifyMarketState(const MarketFeatures& features);

PaperSignal generatePaperSignal(
    const GridState& grid,
    const MarketFeatures& features,
    MarketState state
);

const char* toString(MarketState state);
const char* toString(PaperSignal signal);