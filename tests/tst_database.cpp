#include <QtTest>

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

QTEST_GUILESS_MAIN(TestDatabase)
#include "tst_database.moc"
