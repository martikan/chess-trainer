#include "RunRepository.h"

#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>

#include "Database.h"

namespace store {

namespace {

const QString kIsoFormat = QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzZ");

} // namespace

RunRepository::RunRepository(Database &database)
    : m_database(database)
{
}

std::optional<qint64> RunRepository::writeRun(const RunRecord &record,
                                              QString *errorOut)
{
    QSqlDatabase database = m_database.handle();

    if (!database.transaction()) {
        if (errorOut) {
            *errorOut = database.lastError().text();
        }
        return std::nullopt;
    }

    const auto rollback = [&](const QString &message) -> std::optional<qint64> {
        database.rollback();
        if (errorOut) {
            *errorOut = message;
        }
        return std::nullopt;
    };

    QSqlQuery insertRun(database);
    insertRun.prepare(QStringLiteral(
        "INSERT INTO run (module_id, started_at, round_length_ms, correct, "
        "wrong, completed) VALUES (?, ?, ?, ?, ?, ?)"));
    insertRun.addBindValue(record.moduleId);
    insertRun.addBindValue(record.startedAtUtc.toUTC().toString(kIsoFormat));
    insertRun.addBindValue(record.roundLengthMs);
    insertRun.addBindValue(record.correct);
    insertRun.addBindValue(record.wrong);
    insertRun.addBindValue(record.completed ? 1 : 0);

    if (!insertRun.exec()) {
        return rollback(insertRun.lastError().text());
    }

    const qint64 runId = insertRun.lastInsertId().toLongLong();

    QSqlQuery insertAnswer(database);
    insertAnswer.prepare(QStringLiteral(
        "INSERT INTO answer (run_id, ordinal, square, expected_dark, "
        "answered_dark, correct, response_ms) VALUES (?, ?, ?, ?, ?, ?, ?)"));

    for (const core::Answer &answer : record.answers) {
        insertAnswer.bindValue(0, runId);
        insertAnswer.bindValue(1, answer.ordinal);
        insertAnswer.bindValue(
            2, QString::fromStdString(answer.square.algebraic()));
        insertAnswer.bindValue(3, answer.expectedDark ? 1 : 0);
        insertAnswer.bindValue(4, answer.answeredDark ? 1 : 0);
        insertAnswer.bindValue(5, answer.correct ? 1 : 0);
        insertAnswer.bindValue(
            6, static_cast<int>(answer.responseTime.count()));

        if (!insertAnswer.exec()) {
            return rollback(insertAnswer.lastError().text());
        }
    }

    if (!database.commit()) {
        return rollback(database.lastError().text());
    }

    return runId;
}

std::optional<ModuleSummary> RunRepository::moduleSummary(const QString &moduleId,
                                                          QString *errorOut)
{
    QSqlDatabase database = m_database.handle();
    ModuleSummary summary;

    QSqlQuery best(database);
    best.prepare(QStringLiteral(
        "SELECT MAX(correct) FROM run WHERE module_id = ? AND completed = 1"));
    best.addBindValue(moduleId);

    if (!best.exec() || !best.next()) {
        if (errorOut) {
            *errorOut = best.lastError().text();
        }
        return std::nullopt;
    }
    // MAX over no rows yields SQL NULL, which is a null QVariant.
    if (!best.value(0).isNull()) {
        summary.bestScore = best.value(0).toInt();
    }

    QSqlQuery mean(database);
    mean.prepare(QStringLiteral(
        "SELECT AVG(a.response_ms) FROM answer a "
        "JOIN run r ON a.run_id = r.id "
        "WHERE r.module_id = ? AND r.completed = 1"));
    mean.addBindValue(moduleId);

    if (!mean.exec() || !mean.next()) {
        if (errorOut) {
            *errorOut = mean.lastError().text();
        }
        return std::nullopt;
    }
    if (!mean.value(0).isNull()) {
        summary.meanResponseMs = static_cast<int>(qRound(mean.value(0).toDouble()));
    }

    return summary;
}

} // namespace store
