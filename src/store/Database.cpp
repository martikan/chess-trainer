#include "Database.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace store {

namespace {

/// Assigns the error message if the caller asked for one, and always returns
/// false so call sites can `return fail(...)`.
bool fail(QString *errorOut, const QString &message)
{
    if (errorOut) {
        *errorOut = message;
    }
    return false;
}

} // namespace

Database::Database()
    : m_connectionName(QStringLiteral("chess-trainer-")
                       + QUuid::createUuid().toString(QUuid::WithoutBraces))
{
}

Database::~Database()
{
    if (QSqlDatabase::contains(m_connectionName)) {
        // The QSqlDatabase copy must be out of scope before removeDatabase,
        // otherwise Qt warns about a connection still in use.
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
            if (database.isOpen()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

bool Database::open(const QString &path, QString *errorOut)
{
    if (path != QStringLiteral(":memory:")) {
        const QDir parent = QFileInfo(path).dir();
        if (!parent.exists() && !parent.mkpath(QStringLiteral("."))) {
            return fail(errorOut,
                        QStringLiteral("Cannot create directory %1")
                            .arg(parent.absolutePath()));
        }
    }

    QSqlDatabase database =
        QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    database.setDatabaseName(path);

    if (!database.open()) {
        const QString message = database.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        return fail(errorOut, message);
    }

    // SQLite ignores REFERENCES clauses unless this is enabled, per connection.
    QSqlQuery pragma(database);
    if (!pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"))) {
        return fail(errorOut, pragma.lastError().text());
    }

    return true;
}

bool Database::isOpen() const
{
    return QSqlDatabase::contains(m_connectionName)
        && QSqlDatabase::database(m_connectionName, false).isOpen();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

int Database::schemaVersion() const
{
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        return 0;
    }
    return query.value(0).toInt();
}

bool Database::setSchemaVersion(int version, QString *errorOut)
{
    // PRAGMA does not accept bound parameters, so the value is interpolated.
    // It is an int from our own constant, never external input.
    QSqlQuery query(handle());
    if (!query.exec(QStringLiteral("PRAGMA user_version = %1").arg(version))) {
        return fail(errorOut, query.lastError().text());
    }
    return true;
}

bool Database::migrate(QString *errorOut)
{
    if (!isOpen()) {
        return fail(errorOut, QStringLiteral("Database is not open"));
    }

    if (schemaVersion() >= kCurrentSchemaVersion) {
        return true;
    }

    if (schemaVersion() < 1 && !applyVersion1(errorOut)) {
        return false;
    }

    return true;
}

bool Database::applyVersion1(QString *errorOut)
{
    QSqlDatabase database = handle();
    if (!database.transaction()) {
        return fail(errorOut, database.lastError().text());
    }

    const QStringList statements = {
        QStringLiteral("CREATE TABLE run ("
                       "  id              INTEGER PRIMARY KEY,"
                       "  module_id       TEXT    NOT NULL,"
                       "  started_at      TEXT    NOT NULL,"
                       "  round_length_ms INTEGER NOT NULL,"
                       "  correct         INTEGER NOT NULL,"
                       "  wrong           INTEGER NOT NULL,"
                       "  completed       INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE answer ("
                       "  id            INTEGER PRIMARY KEY,"
                       "  run_id        INTEGER NOT NULL"
                       "                REFERENCES run(id) ON DELETE CASCADE,"
                       "  ordinal       INTEGER NOT NULL,"
                       "  square        TEXT    NOT NULL,"
                       "  expected_dark INTEGER NOT NULL,"
                       "  answered_dark INTEGER NOT NULL,"
                       "  correct       INTEGER NOT NULL,"
                       "  response_ms   INTEGER NOT NULL)"),
        QStringLiteral("CREATE INDEX idx_answer_run ON answer(run_id)"),
        QStringLiteral("CREATE INDEX idx_answer_square ON answer(square)"),
    };

    for (const QString &statement : statements) {
        QSqlQuery query(database);
        if (!query.exec(statement)) {
            const QString message = query.lastError().text();
            database.rollback();
            return fail(errorOut, message);
        }
    }

    if (!database.commit()) {
        return fail(errorOut, database.lastError().text());
    }

    return setSchemaVersion(1, errorOut);
}

QString Database::moveAside(const QString &path, QString *errorOut)
{
    QString target = path + QStringLiteral(".bak");
    for (int suffix = 2; QFile::exists(target) && suffix < 100; ++suffix) {
        target = path + QStringLiteral(".bak.%1").arg(suffix);
    }

    if (!QFile::rename(path, target)) {
        if (errorOut) {
            *errorOut = QStringLiteral("Cannot move %1 aside").arg(path);
        }
        return {};
    }

    return target;
}

bool Database::openOrRecover(const QString &path,
                              QString *errorOut,
                              QString *recoveredFromOut)
{
    if (recoveredFromOut) {
        recoveredFromOut->clear();
    }

    QString firstError;
    if (open(path, &firstError) && migrate(&firstError)) {
        return true;
    }

    // An in-memory database has nothing to recover, and neither does a path
    // that does not exist - in both cases the first failure is the real one.
    if (path == QStringLiteral(":memory:") || !QFile::exists(path)) {
        return fail(errorOut, firstError);
    }

    // Release the failed connection before touching the file.
    if (QSqlDatabase::contains(m_connectionName)) {
        {
            QSqlDatabase database = QSqlDatabase::database(m_connectionName, false);
            if (database.isOpen()) {
                database.close();
            }
        }
        QSqlDatabase::removeDatabase(m_connectionName);
    }

    QString moveError;
    const QString backup = moveAside(path, &moveError);
    if (backup.isEmpty()) {
        return fail(errorOut,
                    QStringLiteral("%1 (and %2)").arg(firstError, moveError));
    }

    QString retryError;
    if (!open(path, &retryError) || !migrate(&retryError)) {
        return fail(errorOut, retryError);
    }

    if (recoveredFromOut) {
        *recoveredFromOut = backup;
    }
    return true;
}

} // namespace store
