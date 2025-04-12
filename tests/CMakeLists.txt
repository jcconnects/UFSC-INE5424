cmake_minimum_required(VERSION 3.10)

# Find all test source files in each test directory
file(GLOB UNIT_TEST_SOURCES "unit_tests/*.cpp")
file(GLOB INTEGRATION_TEST_SOURCES "integration_tests/*.cpp")
file(GLOB SYSTEM_TEST_SOURCES "system_tests/*.cpp")

# Create unit test executables
foreach(unit_test_source ${UNIT_TEST_SOURCES})
    get_filename_component(test_name ${unit_test_source} NAME_WE)
    add_executable(${test_name} ${unit_test_source})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_name} ${CMAKE_PROJECT_NAME})
    add_test(NAME UnitTest_${test_name} COMMAND ${test_name})
endforeach()

# Create integration test executables
foreach(integration_test_source ${INTEGRATION_TEST_SOURCES})
    get_filename_component(test_name ${integration_test_source} NAME_WE)
    add_executable(${test_name} ${integration_test_source})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_name} ${CMAKE_PROJECT_NAME})
    add_test(NAME IntegrationTest_${test_name} COMMAND ${test_name})
endforeach()

# Create system test executables
foreach(system_test_source ${SYSTEM_TEST_SOURCES})
    get_filename_component(test_name ${system_test_source} NAME_WE)
    add_executable(${test_name} ${system_test_source})
    target_include_directories(${test_name} PRIVATE ${CMAKE_SOURCE_DIR}/include)
    target_link_libraries(${test_name} ${CMAKE_PROJECT_NAME})
    add_test(NAME SystemTest_${test_name} COMMAND ${test_name})
endforeach()

# Make unit tests run first, then integration tests, then system tests
set_property(TEST UnitTest_* PROPERTY FIXTURES_REQUIRED unit_tests)
set_property(TEST IntegrationTest_* PROPERTY FIXTURES_REQUIRED integration_tests)
set_property(TEST SystemTest_* PROPERTY FIXTURES_REQUIRED system_tests)
set_property(TEST IntegrationTest_* PROPERTY DEPENDS UnitTest_*)
set_property(TEST SystemTest_* PROPERTY DEPENDS IntegrationTest_*) 