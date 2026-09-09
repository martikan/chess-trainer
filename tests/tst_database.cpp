#include <QtTest>

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "Database.h"

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    void migratesAnEmptyDatabaseToVersionOne();
    void migrationIsIdempotent();
    void createsBothTables();
    void enforcesForeignKeys();
    void reportsAnErrorForAnUnwritablePath();
    void independentInstancesDoNotShareAConnection();
    void cascadesAnswerDeletionWhenARunIsDeleted();
};

void TestDatabase::migratesAnEmptyDatabaseToVersionOne()
{
    store::Database database;
    QString error;

    QVERIFY2(database.open(QStringLiteral(":memory:"), &error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 0);

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), store::Database::kCurrentSchemaVersion);
}

void TestDatabase::migrationIsIdempotent()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));

    QVERIFY2(database.migrate(&error), qPrintable(error));
    QVERIFY2(database.migrate(&error), qPrintable(error));
    QCOMPARE(database.schemaVersion(), 1);
}

void TestDatabase::createsBothTables()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));
    QVERIFY(database.migrate(&error));

    QSqlQuery query(database.handle());
    QVERIFY(query.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name")));

    QStringList tables;
    while (query.next()) {
        tables << query.value(0).toString();
    }

    QVERIFY(tables.contains(QStringLiteral("run")));
    QVERIFY(tables.contains(QStringLiteral("answer")));
}

void TestDatabase::enforcesForeignKeys()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));
    QVERIFY(database.migrate(&error));

    // Without PRAGMA foreign_keys = ON, SQLite silently accepts this row.
    QSqlQuery query(database.handle());
    query.prepare(QStringLiteral(
        "INSERT INTO answer (run_id, ordinal, square, expected_dark, "
        "answered_dark, correct, response_ms) "
        "VALUES (9999, 1, 'e4', 1, 1, 1, 500)"));
    QVERIFY2(!query.exec(), "insert with a dangling run_id should be rejected");
}

void TestDatabase::reportsAnErrorForAnUnwritablePath()
{
    store::Database database;
    QString error;

    QVERIFY(!database.open(QStringLiteral("/proc/definitely/not/writable.db"),
                           &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!database.isOpen());
}

void TestDatabase::independentInstancesDoNotShareAConnection()
{
    store::Database first;
    store::Database second;
    QString error;

    QVERIFY(first.open(QStringLiteral(":memory:"), &error));
    QVERIFY(second.open(QStringLiteral(":memory:"), &error));

    QVERIFY(first.migrate(&error));
    // second is a distinct in-memory database, so it is still unmigrated.
    QCOMPARE(second.schemaVersion(), 0);
}

void TestDatabase::cascadesAnswerDeletionWhenARunIsDeleted()
{
    store::Database database;
    QString error;
    QVERIFY(database.open(QStringLiteral(":memory:"), &error));
    QVERIFY(database.migrate(&error));

    QSqlQuery insertRun(database.handle());
    insertRun.prepare(QStringLiteral(
        "INSERT INTO run (module_id, started_at, round_length_ms, correct, "
        "wrong, completed) VALUES ('square-color', '2026-09-09T00:00:00Z', "
        "30000, 0, 0, 0)"));
    QVERIFY2(insertRun.exec(), qPrintable(insertRun.lastError().text()));
    const QVariant runId = insertRun.lastInsertId();
    QVERIFY(runId.isValid());

    QSqlQuery insertAnswer(database.handle());
    insertAnswer.prepare(QStringLiteral(
        "INSERT INTO answer (run_id, ordinal, square, expected_dark, "
        "answered_dark, correct, response_ms) "
        "VALUES (:run_id, :ordinal, 'e4', 1, 1, 1, 500)"));
    for (int ordinal = 1; ordinal <= 2; ++ordinal) {
        insertAnswer.bindValue(QStringLiteral(":run_id"), runId);
        insertAnswer.bindValue(QStringLiteral(":ordinal"), ordinal);
        QVERIFY2(insertAnswer.exec(), qPrintable(insertAnswer.lastError().text()));
    }

    QSqlQuery countBefore(database.handle());
    countBefore.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM answer WHERE run_id = :run_id"));
    countBefore.bindValue(QStringLiteral(":run_id"), runId);
    QVERIFY(countBefore.exec());
    QVERIFY(countBefore.next());
    // Confirms the inserts actually landed, so a later zero-rows result means
    // the cascade fired rather than the inserts having silently failed.
    QCOMPARE(countBefore.value(0).toInt(), 2);

    QSqlQuery deleteRun(database.handle());
    deleteRun.prepare(QStringLiteral("DELETE FROM run WHERE id = :run_id"));
    deleteRun.bindValue(QStringLiteral(":run_id"), runId);
    QVERIFY2(deleteRun.exec(), qPrintable(deleteRun.lastError().text()));

    // ON DELETE CASCADE only fires when PRAGMA foreign_keys = ON is in effect
    // on the connection performing the delete, so this also re-proves that
    // Database::open() applies the pragma on this connection.
    QSqlQuery countAfter(database.handle());
    countAfter.prepare(QStringLiteral(
        "SELECT COUNT(*) FROM answer WHERE run_id = :run_id"));
    countAfter.bindValue(QStringLiteral(":run_id"), runId);
    QVERIFY(countAfter.exec());
    QVERIFY(countAfter.next());
    QCOMPARE(countAfter.value(0).toInt(), 0);
}

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
