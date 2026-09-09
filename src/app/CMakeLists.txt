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
