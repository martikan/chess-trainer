#include <QApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QUrl>
#include <QVariant>

#include <KAboutData>
#include <KLocalizedContext>
#include <KLocalizedString>

#include "chess-trainer-version.h"

#include "Database.h"
#include "ModuleListModel.h"
#include "RunRepository.h"
#include "SquareColorController.h"

int main(int argc, char *argv[])
{
    // QApplication, not QGuiApplication: qqc2-desktop-style links QtWidgets
    // and fails to load under a QGuiApplication.
    QApplication application(argc, argv);

    QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));

    KLocalizedString::setApplicationDomain(QByteArrayLiteral("chess-trainer"));

    KAboutData about(QStringLiteral("io.github.martikan.ChessTrainer"),
                     i18n("Chess Trainer"),
                     QStringLiteral(CHESSTRAINER_VERSION_STRING),
                     i18n("Train your chess board vision"),
                     KAboutLicense::MIT,
                     i18n("© 2026 Richard Martikan"));
    about.addAuthor(i18n("Richard Martikan"),
                    i18n("Author"),
                    QStringLiteral("ric.martikan@gmail.com"));
    about.setHomepage(QStringLiteral("https://github.com/martikan/chess-trainer"));
    about.setBugAddress(
        QByteArrayLiteral("https://github.com/martikan/chess-trainer/issues"));
    // KAboutData otherwise prefixes the component name with "org.kde.", which
    // no longer names the installed desktop file and makes Kirigami offer KDE
    // donation and Get Involved links from a non-KDE application.
    about.setDesktopFileName(
        QStringLiteral("io.github.martikan.ChessTrainer"));

    // The theme entry only exists once the icons in data/ are installed, so an
    // uninstalled build has nothing to look up. Fall back to the copy embedded
    // in the binary rather than showing a missing-icon placeholder.
    const QIcon applicationIcon = QIcon::fromTheme(
        QStringLiteral("io.github.martikan.ChessTrainer"),
        QIcon(QStringLiteral(
            ":/icons/hicolor/128x128/apps/io.github.martikan.ChessTrainer.png")));
    about.setProgramLogo(applicationIcon);
    KAboutData::setApplicationData(about);

    QApplication::setWindowIcon(applicationIcon);

    // Storage failure must never block training, so an error here is carried
    // into the UI as a message rather than aborting startup. A broken
    // database file is moved aside and retried once before giving up.
    const QString dataDirectory =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    store::Database database;
    QString storageError;
    QString recoveredFrom;
    const bool storageReady = database.openOrRecover(
        dataDirectory + QStringLiteral("/trainer.db"),
        &storageError,
        &recoveredFrom);

    store::RunRepository repository(database);
    app::ModuleListModel moduleList(storageReady ? &repository : nullptr);
    app::SquareColorController squareColor(storageReady ? &repository : nullptr,
                                           &moduleList);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    // KF6 exposes no AboutData QML singleton, so the KAboutData gadget has to
    // be handed to QML by hand. Named applicationAboutData rather than
    // aboutData: the latter would resolve to AboutPage's own property and
    // self-bind.
    engine.rootContext()->setContextProperty(
        QStringLiteral("applicationAboutData"),
        QVariant::fromValue(about));
    engine.rootContext()->setContextProperty(QStringLiteral("storageReady"),
                                             storageReady);
    engine.rootContext()->setContextProperty(
        QStringLiteral("storageMessage"),
        storageReady
            ? (recoveredFrom.isEmpty()
                   ? QString()
                   : i18n("Your statistics database could not be read and was "
                          "moved to %1. A new one has been started.",
                          recoveredFrom))
            : i18n("Statistics cannot be saved: %1. Training still works.",
                   storageError));
    engine.rootContext()->setContextProperty(QStringLiteral("moduleList"),
                                             &moduleList);
    engine.rootContext()->setContextProperty(QStringLiteral("squareColor"),
                                             &squareColor);

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
