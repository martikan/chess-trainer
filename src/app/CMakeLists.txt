qt_add_executable(chess-trainer
    main.cpp
    ModuleListModel.h
    ModuleListModel.cpp
    SquareColorController.h
    SquareColorController.cpp
)

qt_add_qml_module(chess-trainer
    URI ChessTrainer
    VERSION 1.0
    QML_FILES
        qml/main.qml
        qml/HomePage.qml
        qml/DrillPage.qml
        qml/SummarySheet.qml
        qml/components/CountdownRing.qml
        qml/components/AnswerButton.qml
)

# The application icon is embedded as well as installed: an uninstalled build
# has no icon theme entry to look up, and the About page and window icon would
# fall back to a "missing icon" placeholder. One size is enough as a fallback,
# since the installed theme entry wins whenever it is present.
qt_add_resources(chess-trainer "appicon"
    PREFIX "/"
    BASE ${CMAKE_SOURCE_DIR}/data
    FILES ${CMAKE_SOURCE_DIR}/data/icons/hicolor/128x128/apps/io.github.martikan.ChessTrainer.png)

# chess-trainer-version.h is generated into the top-level build directory.
target_include_directories(chess-trainer PRIVATE ${CMAKE_BINARY_DIR})

target_link_libraries(chess-trainer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Quick
    Qt6::QuickControls2
    KF6::Kirigami
    KF6::CoreAddons
    KF6::I18n
    chesstrainer-core
    chesstrainer-store)

install(TARGETS chess-trainer ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
