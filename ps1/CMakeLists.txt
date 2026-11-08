# BOREagotchi LIFE! in HELLvile PARK... - PlayStation build (owner: ps1)
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
	DESCRIPTION  "BOREagotchi LIFE! in HELLvile PARK... (PS1)"
)

option(DOOMO_ENGINE_TEST "Boot into the 3D engine test / benchmark" OFF)
option(DOOMO_PAL         "Use PAL video timing"                     OFF)
option(DOOMO_CORE        "Link the portable core (OFF = platform bring-up with fallback assets)" ON)
option(DOOMO_PACK_ART    "Link the core's textures and skies LZ-compressed (tools/packart.py)" ON)
option(DOOMO_CUTSCENE_VIEWER "Debug mode: hold SELECT at boot for a cutscene viewer (src/cutview.c)" ON)
option(DOOMO_OVERLAYS    "Game disc: load scene code from OVL.BIN into one RAM slot (tools/ovl_plan.py)" ON)
option(DOOMO_INTRO_FMV "Boot-up FMV: the Dippinn Productions ident (src/fmv.c, INTRO.STR on the disc)" ON)
option(DOOMO_INTRO_REQUIRED "Fail the configure when the boot FMV cannot be built (CI): never ship a disc without the ident" OFF)
set(DOOMO_INTRO_STR "" CACHE FILEPATH "A ready-made INTRO.STR (mixed-mode .STR); empty = encode trailer/assets/dippinn_logo.mp4 with psxavenc")
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
# L1: the full UI tables of the languages (lang_tab_<id>.c, ~110 KB each)
# stay out of the PS1 EXE: its RAM is tight. It links each language's
# resident table (lang_res_<id>.c) instead (src/lang_ps1.c, doomo/lang.h).
list(FILTER CORE_SRCS EXCLUDE REGEX "/core/gen/lang_tab_[a-z0-9_]+\\.c$")
# --- CR7: the creator's part icons. The PS1 links the 4-bit packed copy (part_icons_rle.c, ~57 KB,
# in the creator overlay group with part_icons.c: ovl_plan.py) and unpacks an icon into the
# creator's buffer before plat_image uploads it to the VRAM image cache; the raw 140 KB copy
# (part_icons_data.c, GBA and native) never links here. ---
list(FILTER CORE_SRCS EXCLUDE REGEX "/core/gen/part_icons_data\\.c$")
file(GLOB_RECURSE DOOMO_DSC CONFIGURE_DEPENDS ${CORE_DIR}/data/story/*.dsc ${CORE_DIR}/data/story/*.txt)
add_custom_command(
	OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN
	COMMAND ${Python3_EXECUTABLE} ${CORE_DIR}/tools/textpack.py
	        --out-root ${CMAKE_CURRENT_BINARY_DIR}/textpack --bin ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN
	DEPENDS ${DOOMO_DSC} ${CORE_DIR}/tools/textpack.py
	COMMENT "Packing the story text (core/tools/textpack.py -> TEXT.BIN)"
	VERBATIM)
add_custom_target(textbin DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN)
# SFX.BIN: the sound effect PCM (core/gen/sfx_data.c) for src/sfxdisc.c
set(DOOMO_ISO_SFX "")
if(DOOMO_CORE)
	add_custom_command(
		OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/SFX.BIN
		COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/sfxbin.py
		        ${CORE_DIR}/gen/sfx_data.c ${CMAKE_CURRENT_BINARY_DIR}/SFX.BIN
		DEPENDS ${CORE_DIR}/gen/sfx_data.c ${CMAKE_CURRENT_SOURCE_DIR}/tools/sfxbin.py
		COMMENT "Packing the sound effects (tools/sfxbin.py -> SFX.BIN)"
		VERBATIM)
	set(DOOMO_ISO_SFX "<file name=\"SFX.BIN\" type=\"data\" source=\"SFX.BIN\" />")
	set(DOOMO_SFX_DEP ${CMAKE_CURRENT_BINARY_DIR}/SFX.BIN)
endif()
# --- end S2 ---

# E9: the mature content pack on the disc (src/packdisc.c). Off unless a pack file is given:
#   cmake -DDOOMO_PACK_FILE=/path/to/pack.dmpk ...   (built with: mpack.py build, port ps1)
# The stock disc has no PACK.DMP, links no reader and pays none of its 10 KB of RAM.
set(DOOMO_PACK_FILE "" CACHE FILEPATH "E9: a DMPK pack (port ps1) to put on the disc as PACK.DMP")
set(DOOMO_ISO_PACK "")
set(DOOMO_PACK_ON_DISC OFF)
if(DOOMO_CORE AND DOOMO_PACK_FILE)
	if(NOT EXISTS "${DOOMO_PACK_FILE}")
		message(FATAL_ERROR "DOOMO_PACK_FILE ${DOOMO_PACK_FILE} does not exist")
	endif()
	set(DOOMO_PACK_ON_DISC ON)
	set(DOOMO_ISO_PACK "<file name=\"PACK.DMP\" type=\"data\" source=\"${DOOMO_PACK_FILE}\" />")
	message(STATUS "doomogachi: content pack ${DOOMO_PACK_FILE} -> PACK.DMP")
endif()

# Boot FMV: INTRO.STR = the ident video as MDEC .STR + XA audio (psxavenc, 320x176,
# 15 fps, 2x). Without psxavenc and without DOOMO_INTRO_STR the intro is left out.
# The size is the 16:9 source fit to 320 wide, rounded to the MDEC's 16-pixel macroblocks
# (320x180 -> 176), and -I is REQUIRED: without it psxavenc (82f3871) shrinks the frames
# it scales to the source's aspect (16:9 at "-s 320x240" -> 320x192) while its MDEC encoder
# still reads them as 320x240, so every frame came out as garbage (luma, chroma and the
# next frame mixed: picture squashed into the top 96 rows over green bands). With -I the
# scaler and the encoder use the same size.
set(DOOMO_ISO_INTRO "")
set(DOOMO_INTRO_ON OFF)
set(DOOMO_INTRO_DEP "")
if(DOOMO_INTRO_FMV AND DOOMO_CORE AND NOT DOOMO_ENGINE_TEST)
	find_program(PSXAVENC psxavenc NO_CMAKE_FIND_ROOT_PATH)
	set(_intro_src ${DOOMO_ROOT}/trailer/assets/dippinn_logo.mp4)
	if(DOOMO_INTRO_STR AND EXISTS "${DOOMO_INTRO_STR}")
		set(DOOMO_INTRO_ON ON)
		set(DOOMO_INTRO_DEP ${DOOMO_INTRO_STR})
		set(DOOMO_INTRO_FILE ${DOOMO_INTRO_STR})
	elseif(PSXAVENC AND EXISTS ${_intro_src})
		set(DOOMO_INTRO_ON ON)
		set(DOOMO_INTRO_FILE ${CMAKE_CURRENT_BINARY_DIR}/INTRO.STR)
		add_custom_command(
			OUTPUT  ${DOOMO_INTRO_FILE}
			COMMAND ${PSXAVENC} -t str -f 37800 -c 2 -b 4 -s 320x176 -I -r 15 -x 2 -q
			        ${_intro_src} ${DOOMO_INTRO_FILE}
			DEPENDS ${_intro_src}
			COMMENT "Encoding the boot FMV (psxavenc -> INTRO.STR)"
			VERBATIM)
		set(DOOMO_INTRO_DEP ${DOOMO_INTRO_FILE})
	else()
		if(DOOMO_INTRO_REQUIRED)
			message(FATAL_ERROR "doomogachi: boot FMV required but cannot be built (no psxavenc on PATH, no -DDOOMO_INTRO_STR=, or trailer/assets/dippinn_logo.mp4 missing)")
		endif()
		message(WARNING "doomogachi: boot FMV LEFT OUT of this disc (no psxavenc on PATH and no -DDOOMO_INTRO_STR=): the game will start without the Dippinn ident")
	endif()
	if(DOOMO_INTRO_ON)
		set(DOOMO_ISO_INTRO "<file name=\"INTRO.STR\" type=\"mixed\" source=\"${DOOMO_INTRO_FILE}\" />")
	endif()
endif()

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
		# OVL_WHY=1 also prints the 40 biggest group objects and 60 biggest core objects that stay resident,
		# so a disc that ends too high names its own culprits in the CI log
		COMMAND ${CMAKE_COMMAND} -E env OVL_WHY=1 ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/ovl_plan.py
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
	# P1.M1: the part scripts link as 2 KB LZ blocks behind an 8 KB LRU (core/src/part_lru.c), not raw
	target_compile_definitions(${_t} PRIVATE DOOMO_PARTS_LZ=1)
	# CR2: the part-script VM (slot hooks, macro ops, LZ blocks + per-render arena) is ~22 KB resident
	# once painters call it, more than the headroom above the stack reserve: off on the PS1 until CR10
	# (SPEC P3 reclaims, SPU-RAM part store) lands. Scripted parts are drawn as their slot's default.
	target_compile_definitions(${_t} PRIVATE DOOMO_PARTS_VM=0)
	target_compile_definitions(${_t} PRIVATE DOOMO_ICONS_RLE=1)   # CR7: part_icons.h
	if(DOOMO_ENGINE_TEST)
		target_compile_definitions(${_t} PRIVATE DOOMO_ENGINE_TEST=1)
	endif()
	if(DOOMO_CORE)
		target_compile_definitions(${_t} PRIVATE DOOMO_HAVE_CORE=1)
	endif()
	if(DOOMO_CORE)
		# the core's SFX PCM is read from SFX.BIN, not linked (src/sfxdisc.c)
		target_compile_definitions(${_t} PRIVATE DOOMO_SFX_ON_DISC=1)
	endif()
	if(DOOMO_INTRO_ON)
		target_compile_definitions(${_t} PRIVATE DOOMO_INTRO_FMV=1)
	endif()
	if(DOOMO_PACK_ON_DISC)
		target_compile_definitions(${_t} PRIVATE DOOMO_PACK_ON_DISC=1)
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
	        ${CMAKE_CURRENT_BINARY_DIR}/TEXT.BIN ${DOOMO_SFX_DEP} ${DOOMO_PACK_FILE} ${DOOMO_INTRO_DEP}
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
# The core press journeys (core/tests/journeys, UX budgets) that start from a scene, not from
# the boot splash: run.py checks their press budgets and plays them on the disc.
# Left out: j1, j3, j5, j9 start at BOOT (the disc boots through the card splash and the launcher).
set(_journeys "")
foreach(_j j2_continue j4_combat_music j6_ow_save j7_island_save j8_ow_title j10_family j11_family_care
           life_new_island life_news life_pet life_rent life_feed life_help_next life_introduce life_missing_item)
	list(APPEND _journeys
		COMMAND ${_run} --exe ${_exe} --cue ${_cue} --script ${CORE_DIR}/tests/journeys/${_j}.txt
		        --out ${_shots}/journey_${_j})
endforeach()
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
		# CL4: the Field Guide (SCENE_LIB_HELP, the library overlay group: its pages and the lobby's eighth book)
		COMMAND ${_run} --exe ${_exe} --cue ${_cue} --script ${CORE_DIR}/tests/scripts/library_help.txt
		        --out ${_shots}/core_library_help
		${_journeys}
		DEPENDS disc
		COMMENT "PCSX-Redux harness: game"
		VERBATIM USES_TERMINAL)
endif()
