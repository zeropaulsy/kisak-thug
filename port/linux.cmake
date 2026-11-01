# Linux build of KisakThug, included from CMakeLists.txt. Windows never reaches this file.
# Platform model (from hit-and-run-linux): Linux is a PC platform, so __PLAT_WN32__ stays defined and the
# existing PC code paths are shared; OS differences are marked with #ifdef __linux__.
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(PORT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/port")
set(DXVK_DIR "${PORT_DIR}/third_party/dxvk-native/usr")
if(NOT EXISTS "${DXVK_DIR}/lib32/libdxvk_d3d9.so")
	execute_process(COMMAND "${PORT_DIR}/fetch_dxvk.sh" COMMAND_ERROR_IS_FATAL ANY)
endif()

target_compile_definitions(KisakThug PUBLIC
	_LINUX __PLAT_WN32__ _DEBUG _CONSOLE GSI_NO_THREADS KISAK_ORIGINAL_ALLOCATOR DXVK_WSI_SDL2)

# Windows-only sources with a Linux replacement or no Linux use.
get_target_property(KISAK_SOURCES KisakThug SOURCES)
list(FILTER KISAK_SOURCES EXCLUDE REGEX "/common/win32/")
set_property(TARGET KisakThug PROPERTY SOURCES ${KISAK_SOURCES})
target_sources(KisakThug PRIVATE "${PORT_DIR}/compat/d3dx_compat.cpp"
	"${PORT_DIR}/compat/win32_compat.cpp"
	"${PORT_DIR}/compat/linux_debug.cpp")

target_include_directories(KisakThug BEFORE PUBLIC
	"${PORT_DIR}/compat"
	"${CMAKE_CURRENT_SOURCE_DIR}/Code"
	"${CMAKE_CURRENT_SOURCE_DIR}/Code/Sk"
	"${DXVK_DIR}/include/dxvk"
	"/usr/include/SDL2")

target_compile_options(KisakThug PRIVATE
	-g -fpermissive -fms-extensions -fno-strict-aliasing -fno-delete-null-pointer-checks -msse2 -mfpmath=sse -w
	"SHELL:-include ${PORT_DIR}/compat/msvc_compat.h")

target_link_directories(KisakThug PUBLIC "${DXVK_DIR}/lib32")
target_link_libraries(KisakThug PUBLIC dxvk_d3d9 SDL2 pthread dl m)
set_target_properties(KisakThug PROPERTIES
	RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
	BUILD_RPATH "\$ORIGIN;${DXVK_DIR}/lib32")

# Allocator isolation: see port/compat/kisak.map. Check: nm -D --defined-only KisakThug | grep _Zn   must be empty.
target_link_options(KisakThug PRIVATE -static-libstdc++ -static-libgcc -Wl,--exclude-libs,ALL
	"-Wl,--version-script=${PORT_DIR}/compat/kisak.map")
set_property(TARGET KisakThug APPEND PROPERTY LINK_DEPENDS "${PORT_DIR}/compat/kisak.map")
