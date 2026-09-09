#include <QtTest>

#include <algorithm>
#include <set>

#include "ModuleRegistry.h"

class TestModuleRegistry : public QObject
{
    Q_OBJECT

private slots:
    void containsTheSquareColorModuleAsEnabled();
    void identifiersAreUnique();
    void everyEnabledModuleHasAPage();
    void everyModuleHasNameAndDescription();
};

void TestModuleRegistry::containsTheSquareColorModuleAsEnabled()
{
    const auto &modules = core::moduleRegistry();

    const auto found = std::find_if(
        modules.begin(), modules.end(), [](const core::ModuleDescriptor &module) {
            return module.id == core::kSquareColorModuleId;
        });

    QVERIFY(found != modules.end());
    QVERIFY(found->enabled);
}

void TestModuleRegistry::identifiersAreUnique()
{
    std::set<std::string> seen;
    for (const auto &module : core::moduleRegistry()) {
        QVERIFY2(seen.insert(module.id).second,
                 qPrintable(QStringLiteral("duplicate module id: %1")
                                .arg(QString::fromStdString(module.id))));
    }
}

void TestModuleRegistry::everyEnabledModuleHasAPage()
{
    for (const auto &module : core::moduleRegistry()) {
        if (module.enabled) {
            QVERIFY2(!module.qmlPage.empty(),
                     qPrintable(QStringLiteral("enabled module %1 has no page")
                                    .arg(QString::fromStdString(module.id))));
        }
    }
}

void TestModuleRegistry::everyModuleHasNameAndDescription()
{
    for (const auto &module : core::moduleRegistry()) {
        QVERIFY(!module.displayName.empty());
        QVERIFY(!module.description.empty());
        QVERIFY(!module.iconName.empty());
    }
}

QTEST_APPLESS_MAIN(TestModuleRegistry)
#include "tst_moduleregistry.moc"
