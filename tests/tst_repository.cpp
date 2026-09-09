#include <QtTest>

#include <QSqlError>
#include <QSqlQuery>

#include "Database.h"
#include "RunRepository.h"

using namespace std::chrono_literals;

namespace {

core::Answer makeAnswer(int ordinal,
                        const char *square,
                        bool correct,
                        std::chrono::milliseconds responseTime)
{
    const auto parsed = core::Square::fromAlgebraic(square);
    Q_ASSERT(parsed.has_value());

    core::Answer answer;
    answer.ordinal = ordinal;
    answer.square = *parsed;
    answer.expectedDark = parsed->isDark();
    answer.answeredDark = correct ? parsed->isDark() : !parsed->isDark();
    answer.correct = correct;
    answer.responseTime = responseTime;
    return answer;
}

store::RunRecord makeRun(int correct, int wrong, bool completed,
                         std::vector<core::Answer> answers)
{
    store::RunRecord record;
    record.moduleId = QStringLiteral("square-color");
    record.startedAtUtc = QDateTime(QDate(2026, 9, 9), QTime(8, 0), QTimeZone::UTC);
    record.roundLengthMs = 30'000;
    record.correct = correct;
    record.wrong = wrong;
    record.completed = completed;
    record.answers = std::move(answers);
    return record;
}

} // namespace

class TestRepository : public QObject
{
    Q_OBJECT

private slots:
    void init();

    void writeRunReturnsTheNewRunId();
    void writeRunPreservesEveryAnswerField();
    void writeRunPersistsStartedAtAsUtcIso8601();
    void writeRunRollsBackWhenAnAnswerInsertFails();
    void moduleSummaryIsEmptyOnAFreshDatabase();
    void moduleSummaryReportsBestScoreAndMeanResponseTime();
    void moduleSummaryIgnoresAbortedRuns();
    void moduleSummaryIsScopedToOneModule();
    void deletingARunCascadesToItsAnswers();

private:
    std::unique_ptr<store::Database> m_database;
    std::unique_ptr<store::RunRepository> m_repository;
};

void TestRepository::init()
{
    m_database = std::make_unique<store::Database>();
    QString error;
    QVERIFY2(m_database->open(QStringLiteral(":memory:"), &error), qPrintable(error));
    QVERIFY2(m_database->migrate(&error), qPrintable(error));
    m_repository = std::make_unique<store::RunRepository>(*m_database);
}

void TestRepository::writeRunReturnsTheNewRunId()
{
    QString error;
    const auto first = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error);
    QVERIFY2(first.has_value(), qPrintable(error));

    const auto second = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "d5", true, 600ms)}), &error);
    QVERIFY2(second.has_value(), qPrintable(error));

    QVERIFY(*second > *first);
}

void TestRepository::writeRunPreservesEveryAnswerField()
{
    QString error;
    const auto runId = m_repository->writeRun(
        makeRun(1, 1, true,
                {makeAnswer(1, "e4", true, 480ms),
                 makeAnswer(2, "h6", false, 1'320ms)}),
        &error);
    QVERIFY2(runId.has_value(), qPrintable(error));

    QSqlQuery query(m_database->handle());
    query.prepare(QStringLiteral(
        "SELECT ordinal, square, expected_dark, answered_dark, correct, "
        "response_ms FROM answer WHERE run_id = ? ORDER BY ordinal"));
    query.addBindValue(*runId);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

    QVERIFY(query.next());
    QCOMPARE(query.value(0).toInt(), 1);
    QCOMPARE(query.value(1).toString(), QStringLiteral("e4"));
    QCOMPARE(query.value(2).toBool(), false); // e4 is light
    QCOMPARE(query.value(3).toBool(), false);
    QCOMPARE(query.value(4).toBool(), true);
    QCOMPARE(query.value(5).toInt(), 480);

    QVERIFY(query.next());
    QCOMPARE(query.value(1).toString(), QStringLiteral("h6"));
    QCOMPARE(query.value(4).toBool(), false);
    QCOMPARE(query.value(5).toInt(), 1'320);

    QVERIFY(!query.next());
}

void TestRepository::writeRunPersistsStartedAtAsUtcIso8601()
{
    auto record = makeRun(1, 0, true, {makeAnswer(1, "e4", true, 500ms)});
    const QDateTime startedAt(QDate(2026, 3, 4), QTime(8, 30, 15, 250),
                              QTimeZone::UTC);
    record.startedAtUtc = startedAt;

    QString error;
    const auto runId = m_repository->writeRun(record, &error);
    QVERIFY2(runId.has_value(), qPrintable(error));

    QSqlQuery query(m_database->handle());
    query.prepare(
        QStringLiteral("SELECT started_at FROM run WHERE id = ?"));
    query.addBindValue(*runId);
    QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
    QVERIFY(query.next());

    const QString stored = query.value(0).toString();

    // Pins the exact on-disk format, not just the round-tripped value: 'T'
    // and 'Z' are literal separators, not format specifiers.
    QCOMPARE(stored, QStringLiteral("2026-03-04T08:30:15.250Z"));

    // fromString() with no 't' specifier in the format returns the parsed
    // fields tagged as local time; the trailing literal 'Z' in the stored
    // string is the only marker that they are actually UTC, so the reader
    // must re-tag them explicitly before comparing.
    QDateTime readBack = QDateTime::fromString(
        stored, QStringLiteral("yyyy-MM-dd'T'HH:mm:ss.zzz'Z'"));
    readBack.setTimeZone(QTimeZone::UTC);
    QCOMPARE(readBack, startedAt);
}

void TestRepository::writeRunRollsBackWhenAnAnswerInsertFails()
{
    // Force the second answer insert to fail so the rollback path is actually
    // exercised by a real SQLite error, not just inspected in the source: a
    // UNIQUE index on (run_id, ordinal) turns a repeated ordinal into a
    // genuine constraint violation partway through the answer loop.
    QSqlQuery makeUnique(m_database->handle());
    QVERIFY2(makeUnique.exec(QStringLiteral(
                 "CREATE UNIQUE INDEX ux_answer_run_ordinal "
                 "ON answer(run_id, ordinal)")),
             qPrintable(makeUnique.lastError().text()));

    QString error;
    const auto runId = m_repository->writeRun(
        makeRun(1, 1, true,
                {makeAnswer(1, "e4", true, 500ms),
                 makeAnswer(1, "d5", true, 600ms)}),
        &error);

    QVERIFY(!runId.has_value());
    QVERIFY(!error.isEmpty());

    QSqlQuery runCount(m_database->handle());
    QVERIFY(runCount.exec(QStringLiteral("SELECT COUNT(*) FROM run")));
    QVERIFY(runCount.next());
    QCOMPARE(runCount.value(0).toInt(), 0);

    QSqlQuery answerCount(m_database->handle());
    QVERIFY(answerCount.exec(QStringLiteral("SELECT COUNT(*) FROM answer")));
    QVERIFY(answerCount.next());
    QCOMPARE(answerCount.value(0).toInt(), 0);
}

void TestRepository::moduleSummaryIsEmptyOnAFreshDatabase()
{
    QString error;
    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);

    QVERIFY2(summary.has_value(), qPrintable(error));
    QVERIFY(!summary->bestScore.has_value());
    QVERIFY(!summary->meanResponseMs.has_value());
}

void TestRepository::moduleSummaryReportsBestScoreAndMeanResponseTime()
{
    // Deliberately uneven answer counts per run (1 vs. 3): a flat mean over
    // every answer and a per-run average-of-averages diverge only when the
    // runs are not the same size, so this is what would catch a regression
    // to the wrong (per-run) semantics.
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(12, 1, true, {makeAnswer(1, "e4", true, 400ms)}), &error));
    QVERIFY(m_repository->writeRun(
        makeRun(26, 2, true,
                {makeAnswer(1, "a1", true, 800ms),
                 makeAnswer(2, "h8", false, 1'200ms),
                 makeAnswer(3, "b3", true, 1'600ms)}),
        &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY2(summary.has_value(), qPrintable(error));

    QCOMPARE(*summary->bestScore, 26);
    // Flat mean over every answer in completed runs, correct and wrong
    // alike: (400 + 800 + 1200 + 1600) / 4 = 1000. The wrong, per-run
    // average-of-averages would instead report (400 + 1200) / 2 = 800.
    QCOMPARE(*summary->meanResponseMs, 1'000);
}

void TestRepository::moduleSummaryIgnoresAbortedRuns()
{
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(10, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error));
    // An aborted run with a higher score must not become the record.
    QVERIFY(m_repository->writeRun(
        makeRun(99, 0, false, {makeAnswer(1, "d5", true, 100ms)}), &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY(summary.has_value());
    QCOMPARE(*summary->bestScore, 10);
    QCOMPARE(*summary->meanResponseMs, 500);
}

void TestRepository::moduleSummaryIsScopedToOneModule()
{
    QString error;
    QVERIFY(m_repository->writeRun(
        makeRun(10, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error));

    auto other = makeRun(50, 0, true, {makeAnswer(1, "d5", true, 100ms)});
    other.moduleId = QStringLiteral("coordinates");
    QVERIFY(m_repository->writeRun(other, &error));

    const auto summary =
        m_repository->moduleSummary(QStringLiteral("square-color"), &error);
    QVERIFY(summary.has_value());
    QCOMPARE(*summary->bestScore, 10);
}

void TestRepository::deletingARunCascadesToItsAnswers()
{
    QString error;
    const auto runId = m_repository->writeRun(
        makeRun(1, 0, true, {makeAnswer(1, "e4", true, 500ms)}), &error);
    QVERIFY(runId.has_value());

    QSqlQuery remove(m_database->handle());
    remove.prepare(QStringLiteral("DELETE FROM run WHERE id = ?"));
    remove.addBindValue(*runId);
    QVERIFY2(remove.exec(), qPrintable(remove.lastError().text()));

    QSqlQuery count(m_database->handle());
    QVERIFY(count.exec(QStringLiteral("SELECT COUNT(*) FROM answer")));
    QVERIFY(count.next());
    QCOMPARE(count.value(0).toInt(), 0);
}

QTEST_GUILESS_MAIN(TestRepository)
#include "tst_repository.moc"
