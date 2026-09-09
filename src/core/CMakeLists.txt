add_library(chesstrainer-core STATIC
    Square.cpp
    PromptGenerator.cpp
    SquareColorRound.cpp
)

target_include_directories(chesstrainer-core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_features(chesstrainer-core PUBLIC cxx_std_20)

# This target must never link Qt. qt_standard_project_setup() turns AUTOMOC on
# globally; switch it off here so a stray Q_OBJECT fails loudly instead of
# quietly pulling Qt into the domain layer.
set_target_properties(chesstrainer-core PROPERTIES
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
    POSITION_INDEPENDENT_CODE ON)
