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
option(DOOMO_CORE        "Link the portable core (OFF = platform bring-up with fallback assets)" ON)
set(DOOMO_EXE_NAME "doomogachi" CACHE STRING "Output executable base name")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(DOOMO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
set(CORE_DIR   ${DOOMO_ROOT}/core)

# Mirror core/sources.mk (wildcards!). CONFIGURE_DEPENDS re-globs on every
# build so new core files from other modules are picked up automatically.
if(DOOMO_CORE)
	file(GLOB CORE_SRCS CONFIGURE_DEPENDS ${CORE_DIR}/src/*.c ${CORE_DIR}/gen/*.c)
	list(SORT CORE_SRCS)
else()
	set(CORE_SRCS "")
endif()
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
if(DOOMO_CORE)
	target_compile_definitions(${DOOMO_EXE_NAME} PRIVATE DOOMO_HAVE_CORE=1)
endif()
if(DOOMO_PAL)
	target_compile_definitions(${DOOMO_EXE_NAME} PRIVATE DOOMO_PAL=1)
endif()

# Platform code: strict. Core code: C99, warnings on (owners run -Werror
# in their own xcheck; a stray warning from another module must not break
# the disc build).
set_source_files_properties(${PS1_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-Werror;-fno-strict-aliasing")
set_source_files_properties(${CORE_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=c99")

# objdump next to the SDK's nm (for the R3000 ISA check)
string(REGEX REPLACE "nm$" "objdump" TOOLCHAIN_OBJDUMP "${TOOLCHAIN_NM}")
string(REGEX REPLACE "nm\\.exe$" "objdump.exe" TOOLCHAIN_OBJDUMP "${TOOLCHAIN_OBJDUMP}")

# ELF -> PS-X EXE (see tools/elf2exe.py for why not the SDK's elf2x).
add_custom_command(
	TARGET ${DOOMO_EXE_NAME} POST_BUILD
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/elf2exe.py
	        $<TARGET_FILE:${DOOMO_EXE_NAME}>
	        ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.exe
	COMMAND ${TOOLCHAIN_NM} -f posix -l -n $<TARGET_FILE:${DOOMO_EXE_NAME}>
	        > ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.map
	COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_isa.py
	        ${TOOLCHAIN_OBJDUMP} $<TARGET_FILE:${DOOMO_EXE_NAME}>
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

# ---- headless tests in PCSX-Redux (ps1/harness, see README.md) ------------
#   cmake --build --preset ps1 --target harness              game: disc boot +
#                                       smoke, core boot / UI gallery / saves
#   cmake --build --preset ps1-enginetest --target harness   engine tour,
#                                       memory card self test, 3D citizens
# Screenshots, TTY logs and card images go to <build dir>/shots/<script>/.
set(DOOMO_EMU  "/opt/ps1emu/pcsx-redux"  CACHE FILEPATH "pcsx-redux binary for the harness")
set(DOOMO_BIOS "/opt/ps1emu/openbios.bin" CACHE FILEPATH "BIOS image for the harness")
find_program(DOOMO_HOST_CC NAMES cc gcc clang NO_CMAKE_FIND_ROOT_PATH)

set(_exe   ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.exe)
set(_cue   ${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.cue)
set(_shots ${CMAKE_CURRENT_BINARY_DIR}/shots)
set(_hs    ${CMAKE_CURRENT_SOURCE_DIR}/harness/scripts)
set(_run   ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/harness/run.py
           --emu ${DOOMO_EMU} --bios ${DOOMO_BIOS} --timeout 400)
if(DOOMO_HOST_CC)
	set(_adpcm
		COMMAND ${DOOMO_HOST_CC} -O2 -I${CMAKE_CURRENT_SOURCE_DIR}/src
		        ${CMAKE_CURRENT_SOURCE_DIR}/harness/adpcm_test.c
		        ${CMAKE_CURRENT_SOURCE_DIR}/src/adpcm.c -lm
		        -o ${CMAKE_CURRENT_BINARY_DIR}/adpcm_test
		COMMAND ${CMAKE_CURRENT_BINARY_DIR}/adpcm_test)
else()
	set(_adpcm "")
endif()
if(DOOMO_ENGINE_TEST)
	add_custom_target(harness
		${_adpcm}
		COMMAND ${_run} --exe ${_exe} --script ${_hs}/enginetest.txt --out ${_shots}/enginetest
		        --expect "audio selftest: PASS"
		COMMAND ${_run} --exe ${_exe} --script ${_hs}/memcard.txt --out ${_shots}/memcard
		        --expect "memcard selftest: PASS"
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/harness/memcard_check.py
		        ${_shots}/memcard/card1.mcd --icons ${_shots}/memcard/icons.png
		        --expect BASLUS-DOOMO0*
		COMMAND ${_run} --exe ${_exe} --script ${_hs}/model.txt --out ${_shots}/model
		DEPENDS ${DOOMO_EXE_NAME}
		COMMENT "PCSX-Redux harness: engine test"
		VERBATIM USES_TERMINAL)
else()
	add_custom_target(harness
		${_adpcm}
		COMMAND ${_run} --cue ${_cue} --script ${_hs}/game_smoke.txt --out ${_shots}/game_smoke
		        --expect "memcard: wrote"
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/harness/memcard_check.py
		        ${_shots}/game_smoke/card1.mcd --icons ${_shots}/game_smoke/icons.png
		        --expect BASLUS-DOOMO1*
		COMMAND ${_run} --exe ${_exe} --script ${CORE_DIR}/tests/scripts/boot.txt
		        --out ${_shots}/core_boot
		COMMAND ${_run} --exe ${_exe} --script ${CORE_DIR}/tests/scripts/ui_gallery.txt
		        --out ${_shots}/core_ui_gallery
		COMMAND ${_run} --exe ${_exe} --script ${CORE_DIR}/tests/scripts/saves.txt
		        --out ${_shots}/core_saves
		DEPENDS disc
		COMMENT "PCSX-Redux harness: game"
		VERBATIM USES_TERMINAL)
endif()
