# vcpkg's windows toolchain with the debug info flags removed from release builds
include("${CMAKE_CURRENT_LIST_DIR}/../vcpkg/scripts/toolchains/windows.cmake")

foreach(lang C CXX)
	string(REPLACE "/Z7" "" flags "${CMAKE_${lang}_FLAGS_RELEASE}")
	set(CMAKE_${lang}_FLAGS_RELEASE "${flags}" CACHE STRING "" FORCE)
endforeach()
foreach(kind EXE SHARED MODULE)
	string(REPLACE "/DEBUG" "" flags "${CMAKE_${kind}_LINKER_FLAGS_RELEASE}")
	set(CMAKE_${kind}_LINKER_FLAGS_RELEASE "${flags}" CACHE STRING "" FORCE)
endforeach()
