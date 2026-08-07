set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_DEPLOYMENT_TARGET "11.0")
set(VCPKG_C_FLAGS "-mmacosx-version-min=11.0 -target arm64-apple-macos11.0")
set(VCPKG_CXX_FLAGS "-mmacosx-version-min=11.0 -target arm64-apple-macos11.0")
set(VCPKG_CMAKE_CONFIGURE_OPTIONS 
    "-DCMAKE_OSX_ARCHITECTURES=arm64"
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.0"
)
