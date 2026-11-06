include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(basic_tests basic_tests.cpp)
target_link_libraries(basic_tests PRIVATE file_collector GTest::gtest_main)

add_executable(multithread_tests multithread_tests.cpp)
target_link_libraries(multithread_tests PRIVATE file_collector
                      GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(basic_tests)
gtest_discover_tests(multithread_tests)
