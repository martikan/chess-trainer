#pragma once

#include <QAbstractListModel>

#include "ModuleDescriptor.h"

namespace store {
class RunRepository;
}

namespace app {

/// Presents core::moduleRegistry() to QML, joined with each module's
/// best-score and mean-response-time aggregates.
class ModuleListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        ModuleIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        DescriptionRole,
        IconNameRole,
        QmlPageRole,
        EnabledRole,
        BestScoreRole,
        MeanResponseMsRole,
    };

    /// repository may be null when storage failed to open; the model then
    /// reports no statistics and every module still lists normally.
    explicit ModuleListModel(store::RunRepository *repository,
                             QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    /// Re-reads the aggregates. Called after a round is written.
    Q_INVOKABLE void refresh();

private:
    struct Row {
        core::ModuleDescriptor descriptor;
        int bestScore = -1;
        int meanResponseMs = -1;
    };

    void loadStatistics();

    store::RunRepository *m_repository;
    std::vector<Row> m_rows;
};

} // namespace app
