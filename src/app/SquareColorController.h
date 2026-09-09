#pragma once

#include <memory>

#include <QObject>
#include <QStringList>
#include <QTimer>

#include "Clock.h"
#include "SquareColorRound.h"

namespace store {
class RunRepository;
}

namespace app {

class ModuleListModel;

/// Drives a core::SquareColorRound from QML and persists the result.
class SquareColorController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ running NOTIFY stateChanged)
    Q_PROPERTY(QString promptText READ promptText NOTIFY promptChanged)
    Q_PROPERTY(int remainingMs READ remainingMs NOTIFY remainingChanged)
    Q_PROPERTY(int roundLengthMs READ roundLengthMs CONSTANT)
    Q_PROPERTY(int correct READ correct NOTIFY scoreChanged)
    Q_PROPERTY(int wrong READ wrong NOTIFY scoreChanged)

    Q_PROPERTY(int summaryCorrect MEMBER m_summaryCorrect NOTIFY summaryChanged)
    Q_PROPERTY(int summaryWrong MEMBER m_summaryWrong NOTIFY summaryChanged)
    Q_PROPERTY(int summaryAccuracyPercent MEMBER m_summaryAccuracyPercent
                   NOTIFY summaryChanged)
    Q_PROPERTY(int summaryMeanResponseMs MEMBER m_summaryMeanResponseMs
                   NOTIFY summaryChanged)
    Q_PROPERTY(QStringList summarySlowestSquares MEMBER m_summarySlowestSquares
                   NOTIFY summaryChanged)
    Q_PROPERTY(int bestScore MEMBER m_bestScore NOTIFY summaryChanged)

public:
    /// repository and moduleList may be null when storage failed to open.
    SquareColorController(store::RunRepository *repository,
                          ModuleListModel *moduleList,
                          QObject *parent = nullptr);

    bool running() const;
    QString promptText() const;
    int remainingMs() const;
    int roundLengthMs() const;
    int correct() const;
    int wrong() const;

    Q_INVOKABLE void start();

    /// Records an answer. Returns true when it was correct. Returns false in
    /// two distinct cases: not running (nothing emitted), or the clock
    /// expired between the last tick and this call (routes through
    /// finishRound(), which emits stateChanged, summaryChanged and
    /// roundFinished).
    Q_INVOKABLE bool answer(bool dark);

    Q_INVOKABLE void abort();

Q_SIGNALS:
    void stateChanged();
    void promptChanged();
    void remainingChanged();
    void scoreChanged();
    void summaryChanged();
    void answered(bool correct);
    void roundFinished();

private:
    void onTick();
    void finishRound(bool completed);

    core::MonotonicClock m_clock;
    std::unique_ptr<core::SquareColorRound> m_round;
    QTimer m_timer;

    store::RunRepository *m_repository;
    ModuleListModel *m_moduleList;

    int m_summaryCorrect = 0;
    int m_summaryWrong = 0;
    int m_summaryAccuracyPercent = 0;
    int m_summaryMeanResponseMs = 0;
    QStringList m_summarySlowestSquares;
    int m_bestScore = -1;
};

} // namespace app
