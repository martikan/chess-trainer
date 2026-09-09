#include <QtTest>

#include "Square.h"

using core::Square;

class TestSquare : public QObject
{
    Q_OBJECT

private slots:
    void allSixtyFourSquaresHaveCorrectColour();
    void cornersMatchARealBoard();
    void algebraicRoundTrips();
    void fromAlgebraicAcceptsUppercase();
    void fromAlgebraicRejectsMalformedInput_data();
    void fromAlgebraicRejectsMalformedInput();
};

void TestSquare::allSixtyFourSquaresHaveCorrectColour()
{
    // A square is dark when file + rank is even, with both indices 0-based.
    // Verified independently here by parity of the algebraic characters.
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const Square square(file, rank);
            const bool expectedDark = ((file + rank) % 2) == 0;
            QCOMPARE(square.isDark(), expectedDark);
        }
    }
}

void TestSquare::cornersMatchARealBoard()
{
    QVERIFY(Square::fromAlgebraic("a1")->isDark());
    QVERIFY(!Square::fromAlgebraic("h1")->isDark());
    QVERIFY(!Square::fromAlgebraic("a8")->isDark());
    QVERIFY(Square::fromAlgebraic("h8")->isDark());
}

void TestSquare::algebraicRoundTrips()
{
    for (int file = 0; file < 8; ++file) {
        for (int rank = 0; rank < 8; ++rank) {
            const Square original(file, rank);
            const auto parsed = Square::fromAlgebraic(original.algebraic());
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, original);
        }
    }
    QCOMPARE(Square(4, 3).algebraic(), std::string("e4"));
}

void TestSquare::fromAlgebraicAcceptsUppercase()
{
    const auto parsed = Square::fromAlgebraic("E4");
    QVERIFY(parsed.has_value());
    QCOMPARE(parsed->algebraic(), std::string("e4"));
}

void TestSquare::fromAlgebraicRejectsMalformedInput_data()
{
    // QString, not std::string: QTest columns require a registered metatype,
    // and std::string is not one by default.
    QTest::addColumn<QString>("text");

    QTest::newRow("empty")          << QString();
    QTest::newRow("file too high")  << QStringLiteral("i4");
    QTest::newRow("rank zero")      << QStringLiteral("e0");
    QTest::newRow("rank too high")  << QStringLiteral("e9");
    QTest::newRow("too long")       << QStringLiteral("e44");
    QTest::newRow("one character")  << QStringLiteral("e");
    QTest::newRow("reversed")       << QStringLiteral("4e");
    QTest::newRow("whitespace")     << QStringLiteral(" e4");
}

void TestSquare::fromAlgebraicRejectsMalformedInput()
{
    QFETCH(QString, text);
    QVERIFY(!Square::fromAlgebraic(text.toStdString()).has_value());
}

QTEST_APPLESS_MAIN(TestSquare)
#include "tst_square.moc"
