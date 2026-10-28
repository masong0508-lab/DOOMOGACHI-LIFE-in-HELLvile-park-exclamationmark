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
option(DOOMO_PACK_ART    "Link the core's textures and skies LZ-compressed (tools/packart.py)" ON)
option(DOOMO_CUTSCENE_VIEWER "Debug mode: hold SELECT at boot for a cutscene viewer (src/cutview.c)" ON)
option(DOOMO_OVERLAYS    "Game disc: load scene code from OVL.BIN into one RAM slot (tools/ovl_plan.py)" ON)
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

# --- S2: the story text packs (SPEC C3) --- core/gen/text_*.c are the
# GBA's and the native runner's ROM copies; the PS1 reads the same bytes
# from TEXT.BIN on the disc into a resident window (src/txtload.c), so they
# are never linked here. TEXT.BIN is a build product, never committed.
list(FILTER CORE_SRCS EXCLUDE REGEX "/core/gen/text_[a-z0-9_]+\\.c$")
# the long Spanish strings (option help...) stay out of the PS1 EXE: its RAM is tight
list(FILTER CORE_SRCS EXCLUDE REGEX "/core/gen/lang_help_es\\.c$")
file(GLOB_RECURSE DOOMO_DSC CONFIGURE_DEPENDS ${CORE_DIR}/data/story/*.dsc ${CORE_DIR}/data/story/*.txt)
add_custom_command(
	OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN
	COMMAND ${Python3_EXECUTABLE} ${CORE_DIR}/tools/textpack.py
	        --out-root ${CMAKE_CURRENT_BINARY_DIR}/textpack --bin ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN
	DEPENDS ${DOOMO_DSC} ${CORE_DIR}/tools/textpack.py
	COMMENT "Packing the story text (core/tools/textpack.py -> TEXT.BIN)"
	VERBATIM)
add_custom_target(textbin DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN)
# --- end S2 ---

list(LENGTH CORE_SRCS _ncore)
message(STATUS "doomogachi: ${_ncore} core sources")

# Packed art (tools/packart.py, src/packart.c): the core's world textures
# and skies (360 KB of pixels only this platform reads) are linked
# LZ-compressed (~80 KB). art_data.c is compiled without those two arrays,
# whose symbols become virtual addresses (--defsym below, outside RAM).
set(DOOMO_PACKED_ART OFF)
if(DOOMO_PACK_ART AND DOOMO_CORE AND EXISTS ${CORE_DIR}/gen/art_data.c)
	set(DOOMO_PACKED_ART ON)
	list(FILTER CORE_SRCS EXCLUDE REGEX "/gen/art_data\\.c$")
	set(_pack_out ${CMAKE_CURRENT_BINARY_DIR}/packed)
	add_custom_command(
		OUTPUT  ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/packart.py
		        ${CORE_DIR}/gen/art_data.c ${_pack_out}
		DEPENDS ${CORE_DIR}/gen/art_data.c ${CMAKE_CURRENT_SOURCE_DIR}/tools/packart.py
		COMMENT "Packing textures and skies (tools/packart.py)"
		VERBATIM)
	list(APPEND CORE_SRCS ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c)
	# the maps' cell grids too (tools/packmaps.py, map.h map_def_cells)
	set(_maps ${CORE_SRCS})
	list(FILTER _maps INCLUDE REGEX "/core/gen/map_[a-z0-9_]+\\.c$")
	list(FILTER CORE_SRCS EXCLUDE REGEX "/core/gen/map_[a-z0-9_]+\\.c$")
	set(_packed_maps "")
	foreach(_m ${_maps})
		get_filename_component(_mn ${_m} NAME)
		list(APPEND _packed_maps ${_pack_out}/maps/${_mn})
	endforeach()
	add_custom_command(
		OUTPUT  ${_packed_maps}
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/packmaps.py
		        ${_pack_out}/maps ${_maps}
		DEPENDS ${_maps} ${CMAKE_CURRENT_SOURCE_DIR}/tools/packmaps.py
		        ${CMAKE_CURRENT_SOURCE_DIR}/tools/packart.py
		COMMENT "Packing the maps' cells (tools/packmaps.py)"
		VERBATIM)
	list(APPEND CORE_SRCS ${_packed_maps})
endif()

# Every source is compiled once into an object library; the executable
# links those objects (directly, never through an archive: the core's weak
# stubs must be overridden).
add_library(doomo_objs OBJECT ${PS1_SRCS} ${CORE_SRCS})
set_target_properties(doomo_objs PROPERTIES PSN00BSDK_TARGET_TYPE EXECUTABLE_GPREL)
# (the SDK libraries' interface carries the compile flags for the target type)
target_link_libraries(doomo_objs PRIVATE ${PSN00BSDK_EXECUTABLE_LINK_LIBRARIES})

# Scene overlays (game disc only; docs/PS1.md section 3): ovl_plan.py reads
# the relocations of all objects, decides which scene code and const data
# can live in a slot loaded from the disc, and writes the linker script and
# the scene -> group table. The engine test keeps everything resident.
set(DOOMO_USE_OVERLAYS OFF)
if(DOOMO_OVERLAYS AND DOOMO_CORE AND NOT DOOMO_ENGINE_TEST)
	set(DOOMO_USE_OVERLAYS ON)
endif()
string(REGEX REPLACE "nm$" "readelf" TOOLCHAIN_READELF "${TOOLCHAIN_NM}")
string(REGEX REPLACE "nm\\.exe$" "readelf.exe" TOOLCHAIN_READELF "${TOOLCHAIN_READELF}")
if(DOOMO_USE_OVERLAYS)
	set(_ovl_ld ${CMAKE_CURRENT_BINARY_DIR}/ovl.ld)
	set(_ovl_c  ${CMAKE_CURRENT_BINARY_DIR}/ovl_table.c)
	add_custom_command(
		OUTPUT  ${_ovl_ld} ${_ovl_c}
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/ovl_plan.py
		        --readelf ${TOOLCHAIN_READELF} --out-ld ${_ovl_ld} --out-c ${_ovl_c} --report
		        $<TARGET_OBJECTS:doomo_objs>
		DEPENDS doomo_objs $<TARGET_OBJECTS:doomo_objs> ${CMAKE_CURRENT_SOURCE_DIR}/tools/ovl_plan.py
		COMMENT "Planning the scene overlays (tools/ovl_plan.py)"
		COMMAND_EXPAND_LISTS
		VERBATIM)
	add_executable(${DOOMO_EXE_NAME} ${_ovl_c} $<TARGET_OBJECTS:doomo_objs>)
	set(_ldscript ${_ovl_ld})
	set_target_properties(${DOOMO_EXE_NAME} PROPERTIES LINK_DEPENDS ${_ovl_ld})
else()
	add_executable(${DOOMO_EXE_NAME} $<TARGET_OBJECTS:doomo_objs>)
	set(_ldscript ${PSN00BSDK_LDSCRIPTS}/exe.ld)
endif()
if(DOOMO_PACKED_ART)
	target_compile_definitions(doomo_objs PRIVATE DOOMO_PACKED_ART=1 DOOMO_PACKED_MAPS=1)
	target_link_options(${DOOMO_EXE_NAME} PRIVATE
		-Wl,--defsym=doomo_art_tex_pix=0x01000000
		-Wl,--defsym=doomo_art_sky_pix=0x01800000
		-Wl,--defsym=doomo_art_spr_pix_v=0x02000000)
endif()
set_target_properties(${DOOMO_EXE_NAME} PROPERTIES
	PSN00BSDK_TARGET_TYPE EXECUTABLE_GPREL)
target_link_libraries(${DOOMO_EXE_NAME} PRIVATE ${PSN00BSDK_EXECUTABLE_LINK_LIBRARIES})
target_link_options(${DOOMO_EXE_NAME} PRIVATE
	-T${_ldscript}
	-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${DOOMO_EXE_NAME}.ldmap
	-Wl,--no-warn-rwx-segments)
foreach(_t doomo_objs ${DOOMO_EXE_NAME})
	target_include_directories(${_t} PRIVATE
		${CORE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR}/src)
	target_compile_options(${_t} PRIVATE -O2 -Wall -Wextra)
	target_compile_definitions(${_t} PRIVATE DOOMO_PLATFORM_PS1=1)
	if(DOOMO_ENGINE_TEST)
		target_compile_definitions(${_t} PRIVATE DOOMO_ENGINE_TEST=1)
	endif()
	if(DOOMO_CORE)
		target_compile_definitions(${_t} PRIVATE DOOMO_HAVE_CORE=1)
	endif()
	if(DOOMO_PAL)
		target_compile_definitions(${_t} PRIVATE DOOMO_PAL=1)
	endif()
	if(DOOMO_USE_OVERLAYS)
		target_compile_definitions(${_t} PRIVATE DOOMO_OVERLAYS=1)
	endif()
	if(DOOMO_CUTSCENE_VIEWER)
		target_compile_definitions(${_t} PRIVATE DOOMO_CUTSCENE_VIEWER=1)
	endif()
endforeach()

# Platform code: strict. Core code: C99, warnings on (owners run -Werror
# in their own xcheck; a stray warning from another module must not break
# the disc build).
set_source_files_properties(${PS1_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-Werror;-fno-strict-aliasing")
# The whole EXE (code, art, roster, BSS) must fit in 2 MB of RAM, so the
# core is compiled for size (-Os: ~120 KB smaller than -O2) except the
# files that run every frame, which keep -O2 (DOOMO_CORE_HOT; measured
# with the harness, docs/PS1.md section 3). Files that link into an overlay
# group (fp_island_gfx, concert_gfx, exorcism_gfx) cost no resident RAM:
# they only grow their group, which stays below the slot size.
set(DOOMO_CORE_HOT ui_raster ui ui_draw font fixmath map actor combat rng pet_gfx pet_scene
	fp_island_gfx concert_gfx exorcism_gfx
	CACHE STRING "core/src files (no extension) compiled -O2 instead of -Os")
set_source_files_properties(${CORE_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=c99;-Os")
foreach(_hot ${DOOMO_CORE_HOT})
	set_source_files_properties(${CORE_DIR}/src/${_hot}.c PROPERTIES
		COMPILE_OPTIONS "-std=c99;-O2")
endforeach()

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
# the scene overlays' bytes -> OVL.BIN on the disc (tools/ovl_pack.py)
set(DOOMO_ISO_OVL "")
if(DOOMO_USE_OVERLAYS)
	add_custom_command(
		TARGET ${DOOMO_EXE_NAME} POST_BUILD
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/ovl_pack.py
		        $<TARGET_FILE:${DOOMO_EXE_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/OVL.BIN
		BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/OVL.BIN
		VERBATIM)
	set(DOOMO_ISO_OVL "<file name=\"OVL.BIN\" type=\"data\" source=\"OVL.BIN\" />")
endif()

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
	        ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN
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
		COMMAND ${_run} --exe ${_exe} --cue ${_cue} --script ${CORE_DIR}/tests/scripts/boot.txt
		        --out ${_shots}/core_boot
		COMMAND ${_run} --exe ${_exe} --cue ${_cue} --script ${CORE_DIR}/tests/scripts/ui_gallery.txt
		        --out ${_shots}/core_ui_gallery
		COMMAND ${_run} --exe ${_exe} --cue ${_cue} --script ${CORE_DIR}/tests/scripts/saves.txt
		        --out ${_shots}/core_saves
		DEPENDS disc
		COMMENT "PCSX-Redux harness: game"
		VERBATIM USES_TERMINAL)
endif()
