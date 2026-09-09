#include <QtTest>

#include "Clock.h"
#include "PromptGenerator.h"
#include "SquareColorRound.h"

using namespace std::chrono_literals;

using core::AnswerOutcome;
using core::FakeClock;
using core::PromptGenerator;
using core::RoundState;
using core::SquareColorRound;

namespace {

/// A round wired to a clock the test controls. Seed 42 is arbitrary but fixed,
/// so prompts are reproducible. Relies on the constructor's default round
/// length rather than repeating it, so every test using this fixture also
/// exercises the default-parameter binding.
struct Fixture {
    FakeClock clock;
    SquareColorRound round{clock, PromptGenerator(42)};
};

/// Answers the current prompt correctly and returns the outcome.
AnswerOutcome answerCorrectly(SquareColorRound &round)
{
    return round.answer(round.currentPrompt().isDark());
}

} // namespace

class TestRound : public QObject
{
    Q_OBJECT

private slots:
    void startsIdleAndBecomesRunning();
    void constructedWithoutRoundLengthUsesTheDefault();
    void remainingCountsDownAndClampsAtZero();
    void tickFinishesTheRoundExactlyOnce();
    void correctAnswerScoresAndAdvancesThePrompt();
    void wrongAnswerScoresAsWrong();
    void answerRecordsResponseTime();
    void answerAfterExpiryIsIgnoredAndRecordsNothing();
    void answerBeforeStartIsIgnored();
    void abortStopsTheRoundAndKeepsAnswers();
    void abortBeforeStartIsANoOp();
    void abortAfterFinishIsANoOp();
    void answersCarrySequentialOrdinals();
};

void TestRound::startsIdleAndBecomesRunning()
{
    Fixture fixture;
    QCOMPARE(fixture.round.state(), RoundState::Idle);
    QCOMPARE(fixture.round.correct(), 0);
    QCOMPARE(fixture.round.wrong(), 0);

    fixture.round.start();
    QCOMPARE(fixture.round.state(), RoundState::Running);
}

void TestRound::constructedWithoutRoundLengthUsesTheDefault()
{
    FakeClock clock;
    SquareColorRound round{clock, PromptGenerator(42)};
    QCOMPARE(round.roundLength(), SquareColorRound::kDefaultRoundLength);
}

void TestRound::remainingCountsDownAndClampsAtZero()
{
    Fixture fixture;
    QCOMPARE(fixture.round.remaining(), SquareColorRound::kDefaultRoundLength);

    fixture.round.start();
    QCOMPARE(fixture.round.remaining(), SquareColorRound::kDefaultRoundLength);

    fixture.clock.advance(10'000ms);
    QCOMPARE(fixture.round.remaining(), SquareColorRound::kDefaultRoundLength - 10'000ms);

    fixture.clock.advance(25'000ms);
    QCOMPARE(fixture.round.remaining(), 0ms);
}

void TestRound::tickFinishesTheRoundExactlyOnce()
{
    Fixture fixture;
    fixture.round.start();

    fixture.clock.advance(SquareColorRound::kDefaultRoundLength - 1ms);
    QVERIFY(!fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Running);

    fixture.clock.advance(1ms);
    QVERIFY(fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Finished);

    // Only the transition reports true, so a 16 ms timer cannot fire the
    // end-of-round handler repeatedly.
    QVERIFY(!fixture.round.tick());
}

void TestRound::correctAnswerScoresAndAdvancesThePrompt()
{
    Fixture fixture;
    fixture.round.start();

    const auto firstPrompt = fixture.round.currentPrompt();
    QCOMPARE(answerCorrectly(fixture.round), AnswerOutcome::Correct);
    QCOMPARE(fixture.round.correct(), 1);
    QCOMPARE(fixture.round.wrong(), 0);
    QVERIFY(!(fixture.round.currentPrompt() == firstPrompt));
}

void TestRound::wrongAnswerScoresAsWrong()
{
    Fixture fixture;
    fixture.round.start();

    const bool wrongAnswer = !fixture.round.currentPrompt().isDark();
    QCOMPARE(fixture.round.answer(wrongAnswer), AnswerOutcome::Wrong);
    QCOMPARE(fixture.round.correct(), 0);
    QCOMPARE(fixture.round.wrong(), 1);
}

void TestRound::answerRecordsResponseTime()
{
    Fixture fixture;
    fixture.round.start();

    const auto prompt = fixture.round.currentPrompt();
    fixture.clock.advance(480ms);
    answerCorrectly(fixture.round);

    fixture.clock.advance(1'320ms);
    answerCorrectly(fixture.round);

    const auto &answers = fixture.round.answers();
    QCOMPARE(static_cast<int>(answers.size()), 2);
    QCOMPARE(answers[0].square, prompt);
    QCOMPARE(answers[0].responseTime, 480ms);
    QCOMPARE(answers[0].expectedDark, prompt.isDark());
    QCOMPARE(answers[0].answeredDark, prompt.isDark());
    QVERIFY(answers[0].correct);
    // Measured from prompt shown, not from round start.
    QCOMPARE(answers[1].responseTime, 1'320ms);
}

void TestRound::answerAfterExpiryIsIgnoredAndRecordsNothing()
{
    Fixture fixture;
    fixture.round.start();
    answerCorrectly(fixture.round);

    fixture.clock.advance(SquareColorRound::kDefaultRoundLength);
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);

    QCOMPARE(static_cast<int>(fixture.round.answers().size()), 1);
    QCOMPARE(fixture.round.correct(), 1);
    QCOMPARE(fixture.round.wrong(), 0);
    QCOMPARE(fixture.round.state(), RoundState::Finished);
}

void TestRound::answerBeforeStartIsIgnored()
{
    Fixture fixture;
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);
    QVERIFY(fixture.round.answers().empty());
    QCOMPARE(fixture.round.state(), RoundState::Idle);
}

void TestRound::abortStopsTheRoundAndKeepsAnswers()
{
    Fixture fixture;
    fixture.round.start();
    answerCorrectly(fixture.round);

    fixture.round.abort();
    QCOMPARE(fixture.round.state(), RoundState::Aborted);
    QCOMPARE(static_cast<int>(fixture.round.answers().size()), 1);
    QCOMPARE(fixture.round.remaining(), 0ms);

    // Aborted is terminal: no further answers, and tick() reports no transition.
    QCOMPARE(fixture.round.answer(true), AnswerOutcome::Ignored);
    QVERIFY(!fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Aborted);
}

void TestRound::abortBeforeStartIsANoOp()
{
    // abort() only interrupts a round in progress; there is nothing running
    // yet to abort, so the round stays Idle rather than becoming Aborted.
    Fixture fixture;
    fixture.round.abort();
    QCOMPARE(fixture.round.state(), RoundState::Idle);
}

void TestRound::abortAfterFinishIsANoOp()
{
    // Likewise, a round that has already finished on its own has nothing left
    // to abort; Finished stays Finished rather than being overwritten.
    Fixture fixture;
    fixture.round.start();
    fixture.clock.advance(SquareColorRound::kDefaultRoundLength);
    QVERIFY(fixture.round.tick());
    QCOMPARE(fixture.round.state(), RoundState::Finished);

    fixture.round.abort();
    QCOMPARE(fixture.round.state(), RoundState::Finished);
}

void TestRound::answersCarrySequentialOrdinals()
{
    Fixture fixture;
    fixture.round.start();

    for (int i = 0; i < 5; ++i) {
        fixture.clock.advance(100ms);
        answerCorrectly(fixture.round);
    }

    const auto &answers = fixture.round.answers();
    QCOMPARE(static_cast<int>(answers.size()), 5);
    for (int i = 0; i < 5; ++i) {
        QCOMPARE(answers[i].ordinal, i + 1);
    }
}

QTEST_APPLESS_MAIN(TestRound)
#include "tst_round.moc"
