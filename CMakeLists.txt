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
    set(CURL_USE_STATIC_LIBS ON)
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
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fno-strict-aliasing -flto -g -static -static-libgcc -DNDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -fno-strict-aliasing -flto -g -static -static-libgcc -DNDEBUG")

    # This overwrites CURL as Sentry could found ".so" version.
    set(CURL_INCLUDE_DIR "/usr/include")
    set(CURL_LIBRARY "/usr/lib/libcurl.a")
    if(NOT EXISTS ${CURL_LIBRARY})
        message(FATAL_ERROR "Static libcurl.a not found at ${CURL_LIBRARY}")
    endif()
    add_library(CURL::libcurl STATIC IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES
            IMPORTED_LOCATION "${CURL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CURL_INCLUDE_DIR}"
    )

    # This overwrites LIBZ as Sentry could found ".so" version.
    set(ZLIB_INCLUDE_DIR "/usr/include")
    set(ZLIB_LIBRARY "/usr/lib/libz.a")
    if(NOT EXISTS ${ZLIB_LIBRARY})
        message(FATAL_ERROR "Static libz.a not found at ${ZLIB_LIBRARY}")
    endif()
    add_library(ZLIB::ZLIB STATIC IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
            IMPORTED_LOCATION "${ZLIB_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    )

    # This overwrites NGHTTP2 as Sentry could found ".so" version
    set(NGHTTP2_INCLUDE_DIR "/usr/include")
    set(NGHTTP2_LIBRARY "/usr/lib/libnghttp2.a")

    if(NOT EXISTS ${NGHTTP2_LIBRARY})
        message(FATAL_ERROR "Static libnghttp2.a not found at ${NGHTTP2_LIBRARY}")
    endif()

    add_library(NGHTTP2::nghttp2 STATIC IMPORTED)
    set_target_properties(NGHTTP2::nghttp2 PROPERTIES
            IMPORTED_LOCATION "${NGHTTP2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${NGHTTP2_INCLUDE_DIR}"
    )

    # idn2
    set(IDN2_LIBRARY "/usr/lib/libidn2.a")
    add_library(IDN2::idn2 STATIC IMPORTED)
    set_target_properties(IDN2::idn2 PROPERTIES IMPORTED_LOCATION "${IDN2_LIBRARY}")

    # brotli
    set(BROTLI_LIBRARY "/usr/lib/libbrotlidec.a")
    add_library(BROTLI::brotli STATIC IMPORTED)
    set_target_properties(BROTLI::brotli PROPERTIES IMPORTED_LOCATION "${BROTLI_LIBRARY}")

    # zstd
    set(ZSTD_LIBRARY "/usr/lib/libzstd.a")
    add_library(ZSTD::zstd STATIC IMPORTED)
    set_target_properties(ZSTD::zstd PROPERTIES IMPORTED_LOCATION "${ZSTD_LIBRARY}")

    # psl
    set(PSL_LIBRARY "/usr/lib/libpsl.a")
    add_library(PSL::psl STATIC IMPORTED)
    set_target_properties(PSL::psl PROPERTIES IMPORTED_LOCATION "${PSL_LIBRARY}")

    # c-ares
    set(CARES_LIBRARY "/usr/lib/libcares.a")
    add_library(CARES::cares STATIC IMPORTED)
    set_target_properties(CARES::cares PROPERTIES IMPORTED_LOCATION "${CARES_LIBRARY}")

    # This loads Sentry library.
    find_package(sentry REQUIRED)
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
            sentry::sentry
            OpenSSL::SSL
            OpenSSL::Crypto
            CURL::libcurl
            ZLIB::ZLIB
            NGHTTP2::nghttp2
            IDN2::idn2
            BROTLI::brotli
            ZSTD::zstd
            PSL::psl
            CARES::cares
            brotlidec
            brotlicommon
            idn2
            unistring
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
