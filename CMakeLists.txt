cmake_minimum_required(VERSION 3.28)

project(chess-trainer
    VERSION 0.1.0
    DESCRIPTION "Chess board vision trainer"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(ECM 6.5 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(FeatureSummary)

# The About page shows the version, so it must not be able to drift from the
# one CMake already knows. Generate a header instead of repeating the literal
# in main.cpp.
ecm_setup_version(${PROJECT_VERSION}
    VARIABLE_PREFIX CHESSTRAINER
    VERSION_HEADER "${CMAKE_BINARY_DIR}/chess-trainer-version.h")

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Sql Test)
find_package(KF6 6.5 REQUIRED COMPONENTS Kirigami CoreAddons I18n)

# Set as early as possible, per Qt's CMake policy documentation, so it is in
# effect for every qt_add_qml_module() call in subdirectories added below.
# QTP0004 makes Qt generate qmldir files for extra directories under a QML
# module (needed starting with Task 12's src/app/qml/components/).
qt_policy(SET QTP0004 NEW)

qt_standard_project_setup(REQUIRES 6.6)

include(CTest)
enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/store)
add_subdirectory(src/app)
add_subdirectory(data)
add_subdirectory(tests)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
