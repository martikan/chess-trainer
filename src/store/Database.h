#pragma once

#include <QSqlDatabase>
#include <QString>

namespace store {

/// Owns one SQLite connection and applies the schema migration ladder.
///
/// Each instance takes a unique Qt connection name, so several databases can
/// be open at once - which is what lets tests use independent ":memory:"
/// databases without colliding.
class Database
{
public:
    static constexpr int kCurrentSchemaVersion = 1;

    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    /// Opens the database, creating parent directories as needed, and sets
    /// PRAGMA foreign_keys = ON. Pass ":memory:" for a transient database.
    bool open(const QString &path, QString *errorOut);

    bool isOpen() const;

    /// Applies every pending migration. Safe to call repeatedly.
    bool migrate(QString *errorOut);

    /// Reads PRAGMA user_version. Zero means an empty, unmigrated database.
    int schemaVersion() const;

    QSqlDatabase handle() const;

private:
    bool applyVersion1(QString *errorOut);
    bool setSchemaVersion(int version, QString *errorOut);

    QString m_connectionName;
};

} // namespace store
