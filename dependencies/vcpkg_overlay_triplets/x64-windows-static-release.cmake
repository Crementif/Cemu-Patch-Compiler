set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE static)
set(VCPKG_LIBRARY_LINKAGE static)

# release only and without debug info, the debug and /Z7 builds of llvm take up tens of gigabytes
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/windows-nodebuginfo.cmake")
set(VCPKG_LOAD_VCVARS_ENV ON)
