#include <QtTest>

#include <set>

#include "PromptGenerator.h"

using core::PromptGenerator;
using core::Square;

class TestPromptGenerator : public QObject
{
    Q_OBJECT

private slots:
    void neverRepeatsConsecutively();
    void sameSeedProducesSameSequence();
    void differentSeedsDiverge();
    void coversEverySquare();
};

void TestPromptGenerator::neverRepeatsConsecutively()
{
    PromptGenerator generator(1234);

    Square previous = generator.next();
    for (int i = 0; i < 5000; ++i) {
        const Square current = generator.next();
        QVERIFY2(!(current == previous),
                 qPrintable(QStringLiteral("repeated %1 at draw %2")
                                .arg(QString::fromStdString(current.algebraic()))
                                .arg(i)));
        previous = current;
    }
}

void TestPromptGenerator::sameSeedProducesSameSequence()
{
    PromptGenerator first(42);
    PromptGenerator second(42);

    for (int i = 0; i < 200; ++i) {
        QCOMPARE(first.next(), second.next());
    }
}

void TestPromptGenerator::differentSeedsDiverge()
{
    PromptGenerator first(1);
    PromptGenerator second(2);

    bool sawDifference = false;
    for (int i = 0; i < 200 && !sawDifference; ++i) {
        if (!(first.next() == second.next())) {
            sawDifference = true;
        }
    }
    QVERIFY(sawDifference);
}

void TestPromptGenerator::coversEverySquare()
{
    PromptGenerator generator(7);

    std::set<int> seen;
    for (int i = 0; i < 20000; ++i) {
        seen.insert(generator.next().index());
    }
    QCOMPARE(static_cast<int>(seen.size()), 64);
}

QTEST_APPLESS_MAIN(TestPromptGenerator)
#include "tst_promptgenerator.moc"
