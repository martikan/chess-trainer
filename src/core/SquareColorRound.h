#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "Clock.h"
#include "PromptGenerator.h"
#include "Square.h"

namespace core {

enum class RoundState {
    Idle,
    Running,
    Finished,
    Aborted,
};

enum class AnswerOutcome {
    Correct,
    Wrong,
    Ignored,
};

struct Answer {
    int ordinal = 0; ///< 1-based position within the round
    Square square{0, 0};
    bool expectedDark = false;
    bool answeredDark = false;
    bool correct = false;
    std::chrono::milliseconds responseTime{0};
};

/// A timed Square Color sprint: show a square name, take a light/dark answer,
/// repeat until the clock runs out.
class SquareColorRound
{
public:
    static constexpr std::chrono::milliseconds kDefaultRoundLength{30'000};

    SquareColorRound(const IClock &clock,
                     PromptGenerator generator,
                     std::chrono::milliseconds roundLength = kDefaultRoundLength);

    void start();

    /// Ends the round early. Answers already given are kept.
    void abort();

    RoundState state() const { return m_state; }

    /// Precondition: state() == RoundState::Running.
    Square currentPrompt() const;

    /// Clamped at zero. Reports the full round length before start() and zero
    /// once the round is finished or aborted.
    std::chrono::milliseconds remaining() const;

    /// Advances the state machine. Returns true only on the transition into
    /// Finished, so a repeating timer cannot fire the end handler twice.
    ///
    /// A round can also reach Finished through answer() alone, when an answer
    /// arrives after the clock has expired but before the next tick(); that
    /// transition is not reported here, since tick() only reports the one it
    /// performs itself. A caller driving the round from a timer must also
    /// check state() after each answer() call to catch that case.
    bool tick();

    /// Records an answer to the current prompt and advances to the next one.
    /// Returns Ignored, recording nothing, unless the round is running and the
    /// clock has not expired.
    AnswerOutcome answer(bool answeredDark);

    int correct() const { return m_correct; }
    int wrong() const { return m_wrong; }
    std::chrono::milliseconds roundLength() const { return m_roundLength; }
    const std::vector<Answer> &answers() const { return m_answers; }

private:
    bool expired() const;
    void showNextPrompt();

    const IClock &m_clock;
    PromptGenerator m_generator;
    std::chrono::milliseconds m_roundLength;

    RoundState m_state = RoundState::Idle;
    std::chrono::steady_clock::time_point m_roundStart{};
    std::chrono::steady_clock::time_point m_promptShownAt{};
    std::optional<Square> m_currentPrompt;

    std::vector<Answer> m_answers;
    int m_correct = 0;
    int m_wrong = 0;
};

} // namespace core
