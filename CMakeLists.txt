cmake_minimum_required(VERSION 3.25)
project(throttr)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED True)

option(BUILD_TESTS "Build tests cases" ON)
option(ENABLE_FEATURE_METRICS "Enable metrics feature" ON)
option(ENABLE_STATIC_LINKING "Enable static linking feature" OFF)

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

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g --coverage -fprofile-arcs -ftest-coverage")
if (ENABLE_STATIC_LINKING)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -static -static-libgcc -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -static -static-libgcc -DNDEBUG")
    find_path(SENTRY_INCLUDE_DIR sentry.h PATHS /usr/local/include)
    find_library(SENTRY_LIBRARY sentry PATHS /usr/local/lib /usr/local/lib64)

    if (NOT SENTRY_INCLUDE_DIR OR NOT SENTRY_LIBRARY)
        message(FATAL_ERROR "Sentry not found. Install sentry-native.")
    endif()
else ()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
    find_package(sentry REQUIRED)
endif ()

if (ENABLE_STATIC_LINKING)
    set(ZLIB_USE_STATIC_LIBS ON)
    set(CURL_STATICLIB TRUE)
    set(CURL_USE_STATIC_LIBS TRUE)
    set(CURL_INCLUDE_DIR "/usr/include")
    set(CURL_LIBRARY "/usr/lib/libcurl.a")

    if(NOT EXISTS ${CURL_LIBRARY})
        message(FATAL_ERROR "libcurl.a not found at ${CURL_LIBRARY}")
    endif()

    add_library(CURL_STATIC_LIB STATIC IMPORTED)
    set_target_properties(CURL_STATIC_LIB PROPERTIES
            IMPORTED_LOCATION "${CURL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    )
    set(OPENSSL_USE_STATIC_LIBS TRUE)
else ()
    find_package(CURL REQUIRED)
endif ()

find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)


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
            ${SENTRY_LIBRARY}
            CURL_STATIC_LIB
            /usr/lib/libnghttp2.a
            OpenSSL::SSL
            OpenSSL::Crypto
    )
else ()
    list(APPEND USED_LIBRARIES
            ${Boost_LIBRARIES}
            throttr::protocol
            fmt::fmt
            sentry::sentry
            CURL::libcurl
            OpenSSL::SSL
            OpenSSL::Crypto
    )
endif ()

include_directories(src/include ${SENTRY_INCLUDE_DIR})

file(GLOB_RECURSE SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library(throttr_core ${SOURCES})

target_link_libraries(throttr_core ${USED_LIBRARIES})

include(FetchContent)

FetchContent_Declare(
        throttr-protocol
        GIT_REPOSITORY https://github.com/throttr/protocol.git
        GIT_TAG 6.0.0
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
