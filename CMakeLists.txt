cmake_minimum_required(VERSION 3.25)
project(throttr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_TESTS "Build tests cases" ON)

set(CMAKE_CXX_FLAGS_DEBUG "-g --coverage -fdump-ipa-inline")

find_package(
        Boost 1.87.0
        COMPONENTS
        json
        thread
        program_options
        random
        coroutine
        charconv
        system
        iostreams
        REQUIRED
)


include_directories(src/include)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

include(FetchContent)

if (BUILD_TESTS)
    FetchContent_Declare(
            googletest
            URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
    )

    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    file(GLOB_RECURSE TESTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc")

    add_executable(tests
            ${SOURCES}
            ${TESTS}
    )

    target_link_libraries(tests
            GTest::gtest_main
            ${Boost_LIBRARIES}
    )

    include(GoogleTest)
    gtest_discover_tests(tests)
endif ()

add_executable(throttr main.cpp)
