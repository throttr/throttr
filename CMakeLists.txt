cmake_minimum_required(VERSION 3.25)
project(throttr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_TESTS "Build tests cases" ON)
option(ENABLE_FEATURE_METRICS "Enable metrics feature" ON)
option(ENABLE_STATIC_LINKING "Enable static linking feature" OFF)
option(ENABLE_NATIVE_OPTIMIZATION "Enable native CPU optimization feature" OFF)

if (ENABLE_STATIC_LINKING)
    set(Boost_USE_STATIC_LIBS ON)
    set(Boost_USE_STATIC_RUNTIME ON)
    set(BOOST_ALL_NO_LIB ON)
    set(BUILD_SHARED_LIBS OFF)
endif ()


add_compile_options(-Wno-unused-function)

if (ENABLE_FEATURE_METRICS)
    add_definitions(
            -DENABLED_FEATURE_METRICS
    )
endif ()

set(RUNTIME_VALUE_SIZE "UINT16" CACHE STRING "Set value type for Throttr (UINT8, UINT16, UINT32, UINT64)")
set_property(CACHE RUNTIME_VALUE_SIZE PROPERTY STRINGS UINT8 UINT16 UINT32 UINT64)

message(STATUS "Using RUNTIME_VALUE_SIZE = ${RUNTIME_VALUE_SIZE}")
message(STATUS "Using ENABLE_FEATURE_METRICS = ${ENABLE_FEATURE_METRICS}")
message(STATUS "Using ENABLE_STATIC_LINKING = ${ENABLE_STATIC_LINKING}")
message(STATUS "Using ENABLE_NATIVE_OPTIMIZATION = ${ENABLE_NATIVE_OPTIMIZATION}")

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
if (ENABLE_STATIC_LINKING)
    if (ENABLE_NATIVE_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto -march=native -mtune=native -g0 -static -static-libgcc -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-Ofast -flto -march=native -mtune=native -g0 -static -static-libgcc -DNDEBUG")
    else ()
        set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto -g0 -static -static-libgcc -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-Ofast -flto -g0 -static -static-libgcc -DNDEBUG")
    endif ()
else ()
    if (ENABLE_NATIVE_OPTIMIZATION)
        set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -flto -march=native -mtune=native -g0 -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-Ofast -flto -march=native -mtune=native -g0 -DNDEBUG")
    else ()
        set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -DNDEBUG")
        set(CMAKE_C_FLAGS_RELEASE "-Ofast -DNDEBUG")
    endif ()
endif ()

find_package(
        Boost 1.87.0
        REQUIRED
        COMPONENTS
        program_options
)

set(USED_LIBRARIES)

if (ENABLE_STATIC_LINKING)
    list(APPEND USED_LIBRARIES
            ${Boost_LIBRARIES}
            throttr::protocol
            fmt::fmt
    )
else ()
    list(APPEND USED_LIBRARIES
            ${Boost_LIBRARIES}
            throttr::protocol
            fmt::fmt
    )
endif ()

include_directories(src/include)

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(throttr_core ${SOURCES})

target_link_libraries(throttr_core ${USED_LIBRARIES})

include(FetchContent)

FetchContent_Declare(
        throttr-protocol
        GIT_REPOSITORY https://github.com/throttr/protocol.git
        GIT_TAG 7.3.2
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

    add_executable(tests ${TESTS})
    if(ENABLE_STATIC_LINKING)
        target_compile_options(tests PRIVATE -static -static-libgcc -static-libstdc++)
        target_link_options(tests PRIVATE -static -static-libgcc -static-libstdc++)
    endif ()

    target_link_libraries(tests GTest::gtest_main throttr_core)
    target_compile_definitions(tests PRIVATE THROTTR_VALUE_SIZE_${RUNTIME_VALUE_SIZE})

    include(GoogleTest)
    gtest_discover_tests(tests)
endif ()

add_executable(throttr main.cpp)
target_link_libraries(throttr throttr_core)
if (ENABLE_STATIC_LINKING)
    target_compile_options(throttr PRIVATE -static -static-libgcc -static-libstdc++)
    target_link_options(throttr PRIVATE -static -static-libgcc -static-libstdc++)
endif ()
target_compile_definitions(throttr PUBLIC THROTTR_VALUE_SIZE_${RUNTIME_VALUE_SIZE})
target_compile_definitions(throttr_core PUBLIC THROTTR_VALUE_SIZE_${RUNTIME_VALUE_SIZE})

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS 1 CACHE INTERNAL "No dev warnings")
