#include "SquareColorRound.h"

#include <algorithm>
#include <cassert>

namespace core {

using std::chrono::duration_cast;
using std::chrono::milliseconds;

SquareColorRound::SquareColorRound(const IClock &clock,
                                   PromptGenerator generator,
                                   milliseconds roundLength)
    : m_clock(clock)
    , m_generator(std::move(generator))
    , m_roundLength(roundLength)
{
}

void SquareColorRound::start()
{
    m_roundStart = m_clock.now();
    m_state = RoundState::Running;
    m_answers.clear();
    m_correct = 0;
    m_wrong = 0;
    showNextPrompt();
}

void SquareColorRound::abort()
{
    if (m_state == RoundState::Running) {
        m_state = RoundState::Aborted;
    }
}

Square SquareColorRound::currentPrompt() const
{
    assert(m_state == RoundState::Running);
    assert(m_currentPrompt.has_value());
    return *m_currentPrompt;
}

milliseconds SquareColorRound::remaining() const
{
    switch (m_state) {
    case RoundState::Idle:
        return m_roundLength;
    case RoundState::Finished:
    case RoundState::Aborted:
        return milliseconds{0};
    case RoundState::Running:
        break;
    }

    const auto elapsed = duration_cast<milliseconds>(m_clock.now() - m_roundStart);
    return std::max(milliseconds{0}, m_roundLength - elapsed);
}

bool SquareColorRound::expired() const
{
    const auto elapsed = duration_cast<milliseconds>(m_clock.now() - m_roundStart);
    return elapsed >= m_roundLength;
}

bool SquareColorRound::tick()
{
    if (m_state != RoundState::Running || !expired()) {
        return false;
    }

    m_state = RoundState::Finished;
    return true;
}

void SquareColorRound::showNextPrompt()
{
    m_currentPrompt = m_generator.next();
    m_promptShownAt = m_clock.now();
}

AnswerOutcome SquareColorRound::answer(bool answeredDark)
{
    if (m_state != RoundState::Running) {
        return AnswerOutcome::Ignored;
    }

    // The clock may have run out since the last tick. An answer arriving after
    // expiry belongs to a round that is already over.
    if (expired()) {
        m_state = RoundState::Finished;
        return AnswerOutcome::Ignored;
    }

    const Square square = *m_currentPrompt;
    const bool expectedDark = square.isDark();
    const bool correct = answeredDark == expectedDark;

    m_answers.push_back(Answer{
        .ordinal = static_cast<int>(m_answers.size()) + 1,
        .square = square,
        .expectedDark = expectedDark,
        .answeredDark = answeredDark,
        .correct = correct,
        .responseTime = duration_cast<milliseconds>(m_clock.now() - m_promptShownAt),
    });

    if (correct) {
        ++m_correct;
    } else {
        ++m_wrong;
    }

    showNextPrompt();
    return correct ? AnswerOutcome::Correct : AnswerOutcome::Wrong;
}

} // namespace core
