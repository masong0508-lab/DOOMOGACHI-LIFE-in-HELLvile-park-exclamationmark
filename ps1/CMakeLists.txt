# DOOMOGACHI LIFE in HELLvile Park! - PlayStation build (owner: ps1)
#
#   cmake --preset ps1            (configure, see CMakePresets.json)
#   cmake --build --preset ps1    -> build/doomogachi.exe, .bin/.cue disc
#
# Builds ps1/src/*.c (the platform layer, -Werror) together with EVERY
# portable core source, mirroring core/sources.mk:
#     CORE_SRCS := core/src/*.c core/gen/*.c   (sorted)
# Object files are linked directly (no .a archives) so that the weak
# placeholders in core/src/stubs.c are overridden by strong definitions.
#
# Options:
#   -DDOOMO_ENGINE_TEST=ON   boot straight into the 3D engine showcase
#   -DDOOMO_PAL=ON           PAL video (default NTSC)
cmake_minimum_required(VERSION 3.21)

project(
	doomogachi
	LANGUAGES    C ASM
	VERSION      1.0.0
	DESCRIPTION  "DOOMOGACHI LIFE in HELLvile Park! (PS1)"
)

option(DOOMO_ENGINE_TEST "Boot into the 3D engine test / benchmark" OFF)
option(DOOMO_PAL         "Use PAL video timing"                     OFF)
set(DOOMO_EXE_NAME "doomogachi" CACHE STRING "Output executable base name")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DOOMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CORE_DIR   ${DOOMO_ROOT}/core)

# Mirror core/sources.mk (wildcards!). CONFIGURE_DEPENDS re-globs on every
# build so new core files from other modules are picked up automatically.
file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CORE_DIR}/src/*.c ${CORE_DIR}/gen/*.c)
list(SORT CORE_SRCS)
file(GLOB PS1_SRCS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c
                                     ${CMAKE_CURRENT_SOURCE_DIR}/src/*.s)
list(SORT PS1_SRCS)

list(LENGTH CORE_SRCS _ncore)
message(STATUS "doomogachi: ${_ncore} core sources")

add_executable(${DOOMO_EXE_NAME} ${PS1_SRCS} ${CORE_SRCS})
set_target_properties(${DOOMO_EXE_NAME} PROPERTIES
	PSN00BSDK_TARGET_TYPE EXECUTABLE_GPREL)
target_link_libraries(${DOOMO_EXE_NAME} PRIVATE ${PSN00BSDK_EXECUTABLE_LINK_LIBRARIES})
target_link_options(${DOOMO_EXE_NAME} PRIVATE
	-T${PSN00BSDK_LDSCRIPTS}/exe.ld
	-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.ldmap
	-Wl,--no-warn-rwx-segments)
target_include_directories(${DOOMO_EXE_NAME} PRIVATE
	${CORE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(${DOOMO_EXE_NAME} PRIVATE -O2 -Wall -Wextra)

if(DOOMO_ENGINE_TEST)
	target_compile_definitions(${DOOMO_EXE_NAME} PRIVATE DOOMO_ENGINE_TEST=1)
endif()
if(DOOMO_PAL)
	target_compile_definitions(${DOOMO_EXE_NAME} PRIVATE DOOMO_PAL=1)
endif()

# Platform code: strict. Core code: C99, warnings on (owners run -Werror
# in their own xcheck; a stray warning from another module must not break
# the disc build).
set_source_files_properties(${PS1_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-Werror;-Wno-error=unused-function")
set_source_files_properties(${CORE_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=c99")

# ELF -> PS-X EXE (see tools/elf2exe.py for why not the SDK's elf2x).
add_custom_command(
	TARGET ${DOOMO_EXE_NAME} POST_BUILD
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/elf2exe.py
	        $<TARGET_FILE:${DOOMO_EXE_NAME}>
	        ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.exe
	COMMAND ${TOOLCHAIN_NM} -f posix -l -n $<TARGET_FILE:${DOOMO_EXE_NAME}>
	        > ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.map
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.exe
	           ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.map
	VERBATIM
)

# Bootable, license-free disc image (works on emulators, softmodded and
# modchipped consoles; see README.md).
set(DOOMO_ISO_XML ${CMAKE_CURRENT_BINARY_DIR}/iso.xml)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/iso.xml ${DOOMO_ISO_XML})
add_custom_command(
	OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.bin
	        ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.cue
	COMMAND ${MKPSXISO} -y -q
	        -o ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.bin
	        -c ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.cue
	        ${DOOMO_ISO_XML}
	DEPENDS ${DOOMO_EXE_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/system.cnf
	        ${CMAKE_CURRENT_SOURCE_DIR}/iso.xml
	        ${CMAKE_CURRENT_SOURCE_DIR}/README.md
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "Building disc image ${DOOMO_EXE_NAME}.bin/.cue"
	VERBATIM
)
add_custom_target(disc ALL DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.bin
	${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.cue)
