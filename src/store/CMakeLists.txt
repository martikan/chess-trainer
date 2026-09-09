add_library(chesstrainer-store STATIC
    Database.cpp
)

target_include_directories(chesstrainer-store
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(chesstrainer-store
    PUBLIC
        Qt6::Core
        Qt6::Sql
        chesstrainer-core)

set_target_properties(chesstrainer-store PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
