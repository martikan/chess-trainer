#include "SquareColorController.h"

#include <cstdint>
#include <random>
#include <string>

#include <QDateTime>
#include <QDebug>

#include "ModuleListModel.h"
#include "ModuleRegistry.h"
#include "RoundSummary.h"
#include "RunRepository.h"

namespace app {

namespace {

/// A fresh nondeterministic seed per round, so consecutive rounds do not
/// replay the same prompt sequence.
std::uint64_t makeSeed()
{
    std::random_device device;
    return (static_cast<std::uint64_t>(device()) << 32)
        ^ static_cast<std::uint64_t>(device());
}

} // namespace

SquareColorController::SquareColorController(store::RunRepository *repository,
                                              ModuleListModel *moduleList,
                                              QObject *parent)
    : QObject(parent)
    , m_repository(repository)
    , m_moduleList(moduleList)
{
    // 16 ms keeps the countdown ring smooth at 60 Hz. The authoritative clock
    // is in the core; this only samples it.
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &SquareColorController::onTick);
}

bool SquareColorController::running() const
{
    return m_round && m_round->state() == core::RoundState::Running;
}

QString SquareColorController::promptText() const
{
    if (!running()) {
        return {};
    }
    return QString::fromStdString(m_round->currentPrompt().algebraic());
}

int SquareColorController::remainingMs() const
{
    if (!m_round) {
        return static_cast<int>(core::SquareColorRound::kDefaultRoundLength.count());
    }
    return static_cast<int>(m_round->remaining().count());
}

int SquareColorController::roundLengthMs() const
{
    return static_cast<int>(core::SquareColorRound::kDefaultRoundLength.count());
}

int SquareColorController::correct() const
{
    return m_round ? m_round->correct() : 0;
}

int SquareColorController::wrong() const
{
    return m_round ? m_round->wrong() : 0;
}

void SquareColorController::start()
{
    // A re-entrant start() must not silently discard an in-flight round:
    // end and record it first, exactly as a player-initiated abort() would.
    // abort() is a no-op when nothing is running, so this is safe to call
    // unconditionally.
    abort();

    m_round = std::make_unique<core::SquareColorRound>(
        m_clock,
        core::PromptGenerator(makeSeed()),
        core::SquareColorRound::kDefaultRoundLength);
    m_round->start();
    m_timer.start();

    Q_EMIT stateChanged();
    Q_EMIT promptChanged();
    Q_EMIT remainingChanged();
    Q_EMIT scoreChanged();
}

bool SquareColorController::answer(bool dark)
{
    if (!running()) {
        return false;
    }

    const core::AnswerOutcome outcome = m_round->answer(dark);
    if (outcome == core::AnswerOutcome::Ignored) {
        // The clock expired between the last tick and this keypress.
        finishRound(true);
        return false;
    }

    const bool wasCorrect = outcome == core::AnswerOutcome::Correct;

    Q_EMIT scoreChanged();
    Q_EMIT promptChanged();
    Q_EMIT answered(wasCorrect);
    return wasCorrect;
}

void SquareColorController::abort()
{
    if (!running()) {
        return;
    }
    m_round->abort();
    finishRound(false);
}

void SquareColorController::onTick()
{
    if (!m_round) {
        return;
    }

    Q_EMIT remainingChanged();

    // tick() reports true only on the transition, so this cannot fire twice.
    if (m_round->tick()) {
        finishRound(true);
    }
}

void SquareColorController::finishRound(bool completed)
{
    m_timer.stop();

    const auto summary = core::summarise(m_round->answers());
    m_summaryCorrect = summary.correct;
    m_summaryWrong = summary.wrong;
    m_summaryAccuracyPercent = summary.accuracyPercent;
    m_summaryMeanResponseMs = summary.meanResponseMs;

    m_summarySlowestSquares.clear();
    for (const core::Square &square : summary.slowestSquares) {
        m_summarySlowestSquares << QString::fromStdString(square.algebraic());
    }

    if (m_repository) {
        store::RunRecord record;
        record.moduleId = QString::fromStdString(
            std::string(core::kSquareColorModuleId));
        record.startedAtUtc = QDateTime::currentDateTimeUtc();
        record.roundLengthMs = static_cast<int>(m_round->roundLength().count());
        record.correct = summary.correct;
        record.wrong = summary.wrong;
        record.completed = completed;
        record.answers = m_round->answers();

        QString error;
        // A write failure must not lose the round on screen: the summary is
        // already computed from memory and is shown either way. It is still
        // logged, since a silently discarded run is the worst failure mode.
        if (!m_repository->writeRun(record, &error)) {
            qWarning() << "SquareColorController: writeRun failed:" << error;
        }

        if (const auto stats = m_repository->moduleSummary(record.moduleId, &error)) {
            m_bestScore = stats->bestScore.value_or(-1);
        } else {
            qWarning() << "SquareColorController: moduleSummary query failed:" << error;
        }
    }

    if (m_moduleList) {
        m_moduleList->refresh();
    }

    // promptText() and remainingMs() both change value the instant the round
    // stops being Running (to "" and to whatever remaining() reports once
    // finished), on all three paths that reach here: a tick-driven finish, an
    // abort(), and an Ignored answer. Emitting once here, rather than in each
    // caller, covers all three.
    Q_EMIT stateChanged();
    Q_EMIT promptChanged();
    Q_EMIT remainingChanged();
    Q_EMIT summaryChanged();
    Q_EMIT roundFinished();
}

} // namespace app
