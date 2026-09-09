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
    void reportsTheThreeSlowestSquares();
    void collapsesRepeatedSquaresToTheirWorstTime();
    void reportsFewerThanThreeWhenTheRoundWasShort();
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
    // e4 appears twice. It must be listed once, ranked by its worst time.
    const auto summary = core::summarise({
        answerFor("e4", true, 200ms),
        answerFor("e4", false, 1'500ms),
        answerFor("h6", true, 800ms),
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

QTEST_APPLESS_MAIN(TestRoundSummary)
#include "tst_roundsummary.moc"
