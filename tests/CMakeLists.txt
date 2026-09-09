# One executable per unit under test. Each is registered with CTest.
set(CORE_TESTS
    square
    promptgenerator
    round
    moduleregistry
)

foreach(test_name IN LISTS CORE_TESTS)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_link_libraries(tst_${test_name} PRIVATE
        Qt6::Test
        chesstrainer-core)
    add_test(NAME tst_${test_name} COMMAND tst_${test_name})
endforeach()

set(STORE_TESTS
    database
    repository
)

foreach(test_name IN LISTS STORE_TESTS)
    add_executable(tst_${test_name} tst_${test_name}.cpp)
    target_link_libraries(tst_${test_name} PRIVATE
        Qt6::Test
        chesstrainer-core
        chesstrainer-store)
    add_test(NAME tst_${test_name} COMMAND tst_${test_name})
endforeach()
