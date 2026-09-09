#include <QtTest>

#include "RoundSummary.h"

using namespace std::chrono_literals;

using core::Answer;
using core::Square;

namespace {

Answer answerFor(const char *square, bool correct,
                 std::chrono::milliseconds responseTime)
{
    const auto parsed = Square::fromAlgebraic(square);
    Q_ASSERT(parsed.has_value());

    Answer answer;
    answer.square = *parsed;
    answer.expectedDark = parsed->isDark();
    answer.answeredDark = correct ? parsed->isDark() : !parsed->isDark();
    answer.correct = correct;
    answer.responseTime = responseTime;
    return answer;
}

std::vector<std::string> names(const std::vector<Square> &squares)
{
    std::vector<std::string> result;
    for (const Square &square : squares) {
        result.push_back(square.algebraic());
    }
    return result;
}

} // namespace

class TestRoundSummary : public QObject
{
    Q_OBJECT

private slots:
    void emptyRoundSummarisesToZeroes();
    void countsCorrectAndWrong();
    void accuracyRoundsToNearestPercent();
    void meanResponseTimeCoversEveryAnswer();
    void meanResponseTimeRoundsToNearestMillisecond();
    void reportsTheThreeSlowestSquares();
    void collapsesRepeatedSquaresToTheirWorstTime();
    void reportsFewerThanThreeWhenTheRoundWasShort();
    void reportsNoSlowestSquaresWhenNoneRequested();
    void tiedWorstTimesBreakOnSquareIndex();
};

void TestRoundSummary::emptyRoundSummarisesToZeroes()
{
    const auto summary = core::summarise({});

    QCOMPARE(summary.correct, 0);
    QCOMPARE(summary.wrong, 0);
    QCOMPARE(summary.total, 0);
    QCOMPARE(summary.accuracyPercent, 0);
    QCOMPARE(summary.meanResponseMs, 0);
    QVERIFY(summary.slowestSquares.empty());
}

void TestRoundSummary::countsCorrectAndWrong()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", false, 500ms),
        answerFor("a1", true, 600ms),
    });

    QCOMPARE(summary.correct, 2);
    QCOMPARE(summary.wrong, 1);
    QCOMPARE(summary.total, 3);
}

void TestRoundSummary::accuracyRoundsToNearestPercent()
{
    // 2 of 3 is 66.67%, which must round to 67 rather than truncate to 66.
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", true, 400ms),
        answerFor("a1", false, 400ms),
    });

    QCOMPARE(summary.accuracyPercent, 67);
}

void TestRoundSummary::meanResponseTimeCoversEveryAnswer()
{
    // Wrong answers count too: the figure reports how fast the user responds,
    // not how fast they are when they happen to be right.
    const auto summary = core::summarise({
        answerFor("e4", true, 400ms),
        answerFor("d5", false, 1'200ms),
    });

    QCOMPARE(summary.meanResponseMs, 800);
}

void TestRoundSummary::meanResponseTimeRoundsToNearestMillisecond()
{
    // 100 + 100 + 102 = 302, divided by 3 is 100.67ms, which must round to
    // 101 rather than truncate to 100.
    const auto summary = core::summarise({
        answerFor("e4", true, 100ms),
        answerFor("d5", true, 100ms),
        answerFor("a1", true, 102ms),
    });

    QCOMPARE(summary.meanResponseMs, 101);
}

void TestRoundSummary::reportsTheThreeSlowestSquares()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 300ms),
        answerFor("h6", true, 1'400ms),
        answerFor("a7", true, 1'100ms),
        answerFor("d2", true, 900ms),
        answerFor("b5", true, 400ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"h6", "a7", "d2"}));
}

void TestRoundSummary::collapsesRepeatedSquaresToTheirWorstTime()
{
    // e4 appears twice: a fast first answer (100ms) and a slow second one
    // (1'500ms). Its worst time is 1'500ms, its average is 800ms, and its
    // first-occurrence time is 100ms - three different values that rank it
    // differently against h6's single 900ms answer. Only "worst" puts e4
    // ahead of h6; "average" and "first occurrence" both put h6 ahead
    // instead, and "first occurrence" drops e4 out of the top three entirely.
    const auto summary = core::summarise({
        answerFor("e4", true, 100ms),
        answerFor("e4", false, 1'500ms),
        answerFor("h6", true, 900ms),
        answerFor("a7", true, 700ms),
        answerFor("b5", true, 600ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"e4", "h6", "a7"}));
}

void TestRoundSummary::reportsFewerThanThreeWhenTheRoundWasShort()
{
    const auto summary = core::summarise({
        answerFor("e4", true, 300ms),
        answerFor("h6", true, 900ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"h6", "e4"}));
}

void TestRoundSummary::reportsNoSlowestSquaresWhenNoneRequested()
{
    // slowestCount = 0 must return no squares, but the scalar fields still
    // need to be computed normally: a caller asking for zero slowest squares
    // still wants the accuracy and the mean.
    const auto summary = core::summarise({
        answerFor("e4", true, 300ms),
        answerFor("h6", false, 900ms),
    }, 0);

    QVERIFY(summary.slowestSquares.empty());
    QCOMPARE(summary.correct, 1);
    QCOMPARE(summary.wrong, 1);
    QCOMPARE(summary.total, 2);
    QCOMPARE(summary.accuracyPercent, 50);
    QCOMPARE(summary.meanResponseMs, 600);
}

void TestRoundSummary::tiedWorstTimesBreakOnSquareIndex()
{
    // a1 (index 0) and h8 (index 63) share the same worst time (1'000ms).
    // The ranking must not depend on answer order or sort stability - ties
    // break on square index, so a1 always sorts ahead of h8.
    const auto summary = core::summarise({
        answerFor("h6", true, 900ms),
        answerFor("h8", true, 1'000ms),
        answerFor("a1", true, 1'000ms),
    });

    QCOMPARE(names(summary.slowestSquares),
             (std::vector<std::string>{"a1", "h8", "h6"}));
}

QTEST_APPLESS_MAIN(TestRoundSummary)
#include "tst_roundsummary.moc"
