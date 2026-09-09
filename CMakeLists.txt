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
include(ECMInstallIcons)
include(FeatureSummary)

find_package(Qt6 6.6 REQUIRED COMPONENTS
    Core Gui Widgets Quick QuickControls2 Sql Test)
find_package(KF6 6.5 REQUIRED COMPONENTS Kirigami CoreAddons I18n)

qt_standard_project_setup(REQUIRES 6.6)

include(CTest)
enable_testing()

add_subdirectory(src/core)
add_subdirectory(src/store)
add_subdirectory(tests)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
