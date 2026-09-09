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
/// so prompts are reproducible.
struct Fixture {
    FakeClock clock;
    SquareColorRound round{clock, PromptGenerator(42), 30'000ms};
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
    void remainingCountsDownAndClampsAtZero();
    void tickFinishesTheRoundExactlyOnce();
    void correctAnswerScoresAndAdvancesThePrompt();
    void wrongAnswerScoresAsWrong();
    void answerRecordsResponseTime();
    void answerAfterExpiryIsIgnoredAndRecordsNothing();
    void answerBeforeStartIsIgnored();
    void abortStopsTheRoundAndKeepsAnswers();
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

void TestRound::remainingCountsDownAndClampsAtZero()
{
    Fixture fixture;
    QCOMPARE(fixture.round.remaining(), 30'000ms);

    fixture.round.start();
    QCOMPARE(fixture.round.remaining(), 30'000ms);

    fixture.clock.advance(10'000ms);
    QCOMPARE(fixture.round.remaining(), 20'000ms);

    fixture.clock.advance(25'000ms);
    QCOMPARE(fixture.round.remaining(), 0ms);
}

void TestRound::tickFinishesTheRoundExactlyOnce()
{
    Fixture fixture;
    fixture.round.start();

    fixture.clock.advance(29'999ms);
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

    fixture.clock.advance(30'000ms);
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
