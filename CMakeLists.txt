cmake_minimum_required(VERSION 3.20)
project(antares CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(STRICT_FLAGS
        /W4
        /WX
        /permissive-
    )
else()
    set(STRICT_FLAGS
        -Wall
        -Werror
        -Wpedantic
        -Wextra
        -Wshadow
        -Wformat=2
        -Wswitch-enum
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        list(APPEND STRICT_FLAGS
            -Wnull-dereference
            -Wno-zero-length-array
            -Wno-c99-extensions
            -Wno-strict-prototypes
        )
    endif()
endif()

find_package(unofficial-libplist CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(OpenSSL REQUIRED)

if(MSVC)
    set(ENV{PKG_CONFIG_PATH} "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib/pkgconfig;${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/debug/lib/pkgconfig")
    
    set(OPENSSL_USE_STATIC_LIBS TRUE)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)

    string(APPEND CMAKE_EXE_LINKER_FLAGS_RELEASE " /LTCG /INCREMENTAL:NO /NODEFAULTLIB:msvcrtd")
    string(APPEND CMAKE_CXX_FLAGS_RELEASE " /GL")
    string(APPEND CMAKE_C_FLAGS_RELEASE " /GL")
endif()

pkg_check_modules(LIBUSB REQUIRED IMPORTED_TARGET libusb-1.0)

add_library(antares-core STATIC
    core/lockdownd/Crypto.cpp
    core/lockdownd/LockdownDaemonClient.cpp
    core/device/Device.cpp
    core/usb/mux.c
)

target_compile_options(antares-core PRIVATE ${STRICT_FLAGS})

target_include_directories(antares-core PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(antares-core PUBLIC
    unofficial::libplist::libplist
    unofficial::libplist::libplist++
    PkgConfig::LIBUSB
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(antares-core PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(antares-core PRIVATE ws2_32)
endif()

# CLI

add_executable(antares cli/main.cpp)
target_compile_options(antares PRIVATE ${STRICT_FLAGS})
target_link_libraries(antares PRIVATE antares-core)

# GUI

find_package(wxWidgets COMPONENTS core base QUIET)
if(wxWidgets_FOUND)
    include(${wxWidgets_USE_FILE})

    add_executable(antares-gui WIN32
        gui/main.cpp
        gui/MainFrame.cpp
    )
    target_compile_options(antares-gui PRIVATE ${STRICT_FLAGS})
    target_link_libraries(antares-gui PRIVATE
        antares-core
        ${wxWidgets_LIBRARIES}
    )

    if(APPLE)
        target_link_options(antares-gui PRIVATE -Wl,-no_warn_duplicate_libraries)

        set_target_properties(antares-gui PROPERTIES
            OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
            OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"

            MACOSX_BUNDLE TRUE
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.nightwind.antares"
            MACOSX_BUNDLE_BUNDLE_NAME "Antares"
        )
    endif()
else()
    message(STATUS "wxWidgets not found, skipping GUI target")
endif()

# Platform-specific

if(APPLE)
    target_link_options(antares PRIVATE
        -Wl,-no_warn_duplicate_libraries
    )

    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
    elseif(CMAKE_OSX_ARCHITECTURES STREQUAL "x86_64")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "10.14" CACHE STRING "" FORCE)
    endif()

    set_target_properties(antares PROPERTIES
        OSX_ARCHITECTURES "${CMAKE_OSX_ARCHITECTURES}"
        OSX_DEPLOYMENT_TARGET "${CMAKE_OSX_DEPLOYMENT_TARGET}"
    )
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Release" AND NOT WIN32)
    add_custom_command(TARGET antares POST_BUILD
        COMMAND strip -x $<TARGET_FILE:antares>
    )
endif()
