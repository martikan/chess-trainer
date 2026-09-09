#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include "Database.h"
#include "RunRepository.h"

int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: qqc2-desktop-style links QtWidgets
    // and fails to load under a QGuiApplication.
    QApplication application(argc, argv);

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("chess-trainer"));

    KAboutData about(QStringLiteral("io.github.martikan.ChessTrainer"),
                     i18n("Chess Trainer"),
                     QStringLiteral("0.1.0"),
                     i18n("Train your chess board vision"),
                     KAboutLicense::MIT,
                     i18n("© 2026 Richard Martikan"));
    about.addAuthor(i18n("Richard Martikan"),
                    i18n("Author"),
                    QStringLiteral("ric.martikan@gmail.com"));
    about.setHomepage(QStringLiteral("https://github.com/martikan/chess-trainer"));
    about.setBugAddress(
        QByteArrayLiteral("https://github.com/martikan/chess-trainer/issues"));
    KAboutData::setApplicationData(about);

    QApplication::setWindowIcon(
        QIcon::fromTheme(QStringLiteral("io.github.martikan.ChessTrainer")));

    // Storage failure must never block training, so an error here is carried
    // into the UI as a message rather than aborting startup. Task 13 renders
    // it; for now it only affects whether persistence works.
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    store::Database database;
    QString storageError;
    const bool storageReady =
        database.open(dataDirectory + QStringLiteral("/trainer.db"), &storageError)
        && database.migrate(&storageError);

    store::RunRepository repository(database);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("storageReady"),
                                             storageReady);
    engine.rootContext()->setContextProperty(QStringLiteral("storageError"),
                                             storageError);

    // qt_add_qml_module only auto-registers a QML file as a type when its
    // basename starts with an uppercase letter, so main.qml (lowercase, by
    // convention for an application's entry point) is never registered as a
    // type named "main" and loadFromModule() cannot find it. Load it by its
    // module resource URL instead.
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/ChessTrainer/qml/main.qml")));
    if (engine.rootObjects().isEmpty()) {
        return 1;
    }

    return QApplication::exec();
}
