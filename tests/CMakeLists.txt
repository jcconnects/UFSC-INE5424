cmake_minimum_required(VERSION 3.10)
project(INE5424_Tests)

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
if(UNIT_TEST_SOURCES)
    foreach(unit_test_source ${UNIT_TEST_SOURCES})
        get_filename_component(test_name ${unit_test_source} NAME_WE)
        set_tests_properties(UnitTest_${test_name} PROPERTIES FIXTURES_REQUIRED unit_tests)
    endforeach()
endif()

if(INTEGRATION_TEST_SOURCES)
    foreach(integration_test_source ${INTEGRATION_TEST_SOURCES})
        get_filename_component(test_name ${integration_test_source} NAME_WE)
        set_tests_properties(IntegrationTest_${test_name} PROPERTIES FIXTURES_REQUIRED integration_tests)
        set_tests_properties(IntegrationTest_${test_name} PROPERTIES DEPENDS "UnitTest_*")
    endforeach()
endif()

if(SYSTEM_TEST_SOURCES)
    foreach(system_test_source ${SYSTEM_TEST_SOURCES})
        get_filename_component(test_name ${system_test_source} NAME_WE)
        set_tests_properties(SystemTest_${test_name} PROPERTIES FIXTURES_REQUIRED system_tests)
        set_tests_properties(SystemTest_${test_name} PROPERTIES DEPENDS "IntegrationTest_*")
    endforeach()
endif() 