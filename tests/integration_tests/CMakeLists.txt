# Add the new memory management test to the integration tests
add_executable(memory_management_test memory_management_test.cpp)
target_link_libraries(memory_management_test PRIVATE project_lib pthread)
add_test(NAME memory_management_test COMMAND memory_management_test)

# Add this test to the list of all integration tests
add_custom_target(integration_tests 
    DEPENDS 
        # Add your existing integration tests here
        memory_management_test # Add the new test
) 