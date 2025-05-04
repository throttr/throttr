cmake_minimum_required(VERSION 3.25)
project(throttr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)
set(BOOST_ALL_NO_LIB ON)
set(BUILD_SHARED_LIBS OFF)

option(BUILD_TESTS "Build tests cases" ON)

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

set(ZLIB_USE_STATIC_LIBS ON)

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
        REQUIRED
)


include_directories(src/include)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

include(FetchContent)

FetchContent_Declare(
        throttr-protocol
        GIT_REPOSITORY https://github.com/throttr/protocol.git
        GIT_TAG 4.0.0
)

FetchContent_MakeAvailable(throttr-protocol)

FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG 11.2.0
)

FetchContent_MakeAvailable(fmt)

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
            throttr::protocol
            fmt::fmt
    )

    include(GoogleTest)
    gtest_discover_tests(tests)
endif ()

add_executable(throttr main.cpp ${SOURCES})
target_link_libraries(throttr
        ${Boost_LIBRARIES}
        throttr::protocol
        fmt::fmt
)
target_compile_options(throttr PRIVATE -static -static-libgcc -static-libstdc++)
target_link_options(throttr PRIVATE -static -static-libgcc -static-libstdc++)
