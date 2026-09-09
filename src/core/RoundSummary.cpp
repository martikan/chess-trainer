#include "RoundSummary.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace core {

RoundSummary summarise(const std::vector<Answer> &answers,
                       std::size_t slowestCount)
{
    RoundSummary summary;
    if (answers.empty()) {
        return summary;
    }

    summary.total = static_cast<int>(answers.size());

    // Worst response time per square, indexed by square. -1 means unanswered.
    std::array<long long, 64> worstBySquare{};
    worstBySquare.fill(-1);

    long long totalResponseMs = 0;

    for (const Answer &answer : answers) {
        if (answer.correct) {
            ++summary.correct;
        } else {
            ++summary.wrong;
        }

        const long long responseMs = answer.responseTime.count();
        totalResponseMs += responseMs;

        const std::size_t index = static_cast<std::size_t>(answer.square.index());
        worstBySquare[index] = std::max(worstBySquare[index], responseMs);
    }

    summary.accuracyPercent = static_cast<int>(
        std::lround(100.0 * summary.correct / summary.total));
    summary.meanResponseMs =
        static_cast<int>(std::lround(static_cast<double>(totalResponseMs)
                                     / summary.total));

    struct Ranked {
        int index;
        long long worstMs;
    };

    std::vector<Ranked> ranked;
    ranked.reserve(answers.size());
    for (int index = 0; index < 64; ++index) {
        const long long worstMs = worstBySquare[static_cast<std::size_t>(index)];
        if (worstMs >= 0) {
            ranked.push_back(Ranked{index, worstMs});
        }
    }

    // Slowest first. Ties break on square index so the order is deterministic
    // rather than dependent on answer sequence.
    std::sort(ranked.begin(), ranked.end(), [](const Ranked &a, const Ranked &b) {
        if (a.worstMs != b.worstMs) {
            return a.worstMs > b.worstMs;
        }
        return a.index < b.index;
    });

    const std::size_t take = std::min(slowestCount, ranked.size());
    summary.slowestSquares.reserve(take);
    for (std::size_t i = 0; i < take; ++i) {
        summary.slowestSquares.push_back(Square::fromIndex(ranked[i].index));
    }

    return summary;
}

} // namespace core
