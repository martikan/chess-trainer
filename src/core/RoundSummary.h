#pragma once

#include <cstddef>
#include <vector>

#include "SquareColorRound.h"

namespace core {

struct RoundSummary {
    int correct = 0;
    int wrong = 0;
    int total = 0;
    int accuracyPercent = 0;
    int meanResponseMs = 0;
    /// Up to slowestCount squares, worst first. A square answered more than
    /// once appears once, ranked by its worst response time.
    std::vector<Square> slowestSquares;
};

RoundSummary summarise(const std::vector<Answer> &answers,
                       std::size_t slowestCount = 3);

} // namespace core
