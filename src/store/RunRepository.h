#pragma once

#include <optional>
#include <vector>

#include <QDateTime>
#include <QString>

#include "SquareColorRound.h"

namespace store {

class Database;

/// One completed or aborted round, ready to persist.
struct RunRecord {
    QString moduleId;
    QDateTime startedAtUtc;
    int roundLengthMs = 0;
    int correct = 0;
    int wrong = 0;
    bool completed = false;
    std::vector<core::Answer> answers;
};

/// Aggregates over a module's completed runs. Both fields are nullopt until
/// at least one run has been completed.
struct ModuleSummary {
    std::optional<int> bestScore;
    std::optional<int> meanResponseMs;
};

class RunRepository
{
public:
    explicit RunRepository(Database &database);

    /// Writes the run and all its answers in a single transaction. Returns the
    /// new run id, or nullopt with errorOut set.
    std::optional<qint64> writeRun(const RunRecord &record, QString *errorOut);

    /// Best score and mean response time across the module's completed runs.
    std::optional<ModuleSummary> moduleSummary(const QString &moduleId,
                                               QString *errorOut);

private:
    Database &m_database;
};

} // namespace store
