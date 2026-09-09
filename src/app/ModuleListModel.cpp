#include "ModuleListModel.h"

#include <KLocalizedString>

#include "ModuleRegistry.h"
#include "RunRepository.h"

namespace app {

ModuleListModel::ModuleListModel(store::RunRepository *repository,
                                  QObject *parent)
    : QAbstractListModel(parent)
    , m_repository(repository)
{
    for (const core::ModuleDescriptor &descriptor : core::moduleRegistry()) {
        m_rows.push_back(Row{descriptor, -1, -1});
    }
    loadStatistics();
}

int ModuleListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(m_rows.size());
}

QVariant ModuleListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }

    const Row &row = m_rows[static_cast<std::size_t>(index.row())];

    switch (role) {
    case ModuleIdRole:
        return QString::fromStdString(row.descriptor.id);
    case DisplayNameRole:
        // The registry holds English source strings; they become translatable
        // at this boundary.
        return i18n(row.descriptor.displayName.c_str());
    case DescriptionRole:
        return i18n(row.descriptor.description.c_str());
    case IconNameRole:
        return QString::fromStdString(row.descriptor.iconName);
    case QmlPageRole:
        return QString::fromStdString(row.descriptor.qmlPage);
    case EnabledRole:
        return row.descriptor.enabled;
    case BestScoreRole:
        return row.bestScore;
    case MeanResponseMsRole:
        return row.meanResponseMs;
    default:
        return {};
    }
}

QHash<int, QByteArray> ModuleListModel::roleNames() const
{
    return {
        {ModuleIdRole, QByteArrayLiteral("moduleId")},
        {DisplayNameRole, QByteArrayLiteral("displayName")},
        {DescriptionRole, QByteArrayLiteral("description")},
        {IconNameRole, QByteArrayLiteral("iconName")},
        {QmlPageRole, QByteArrayLiteral("qmlPage")},
        {EnabledRole, QByteArrayLiteral("enabled")},
        {BestScoreRole, QByteArrayLiteral("bestScore")},
        {MeanResponseMsRole, QByteArrayLiteral("meanResponseMs")},
    };
}

void ModuleListModel::loadStatistics()
{
    if (!m_repository) {
        return;
    }

    for (Row &row : m_rows) {
        QString error;
        const auto summary = m_repository->moduleSummary(
            QString::fromStdString(row.descriptor.id), &error);
        if (!summary) {
            continue;
        }
        row.bestScore = summary->bestScore.value_or(-1);
        row.meanResponseMs = summary->meanResponseMs.value_or(-1);
    }
}

void ModuleListModel::refresh()
{
    loadStatistics();
    if (!m_rows.empty()) {
        Q_EMIT dataChanged(index(0), index(static_cast<int>(m_rows.size()) - 1),
                           {BestScoreRole, MeanResponseMsRole});
    }
}

} // namespace app
