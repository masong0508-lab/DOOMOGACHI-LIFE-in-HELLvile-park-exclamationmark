# DOOMOGACHI LIFE in HELLvile Park! - the show player, show.exe (owner: show)
#
#   cmake --preset ps1-show            (show/engine/CMakePresets.json)
#   cmake --build --preset ps1-show    -> show/build/ps1-show/show.exe + show.bin/.cue
#   (or: show/tools/show build [--preset ps1-show|ps1-show-8mb])
#
# A STAND-ALONE project (spec 4.1, 4.3). It reads core/ and ps1/src/ by
# path and writes only under its binary dir, show/build/<preset>/. It
# never includes, configures or edits a game file: the two portrait files
# it changes are compiled from patched COPIES in <binary dir>/gen/, made by
# apply_patch.py from show/engine/patches/. A patch whose context no longer
# matches stops this build with the file name; the game build is never
# involved (AT1.4).
#
# The compile flags are the game's (ps1/CMakeLists.txt): -O2 -Wall -Wextra,
# core -std=c99 -Os (its hot list -O2), platform -std=gnu99 -Werror
# -fno-strict-aliasing, DOOMO_PLATFORM_PS1 DOOMO_HAVE_CORE DOOMO_PACKED_ART
# DOOMO_PACKED_MAPS. Not DOOMO_OVERLAYS (everything is resident) and not
# DOOMO_CUTSCENE_VIEWER. Plus DOOMO_SHOW=1 (the patch hunks).
#
# Options:
#   -DSHOW_RAM_KB=8192          the ps1-show-8mb hatch (EMU-ONLY, spec 8.11)
#   -DSHOW_SELFTEST=ON          link show_selftest.c + the roster: the AT1.3
#                               portrait CRC self-test (tests/engine/)
#   -DSHOW_PORTRAIT_ORIGINAL=ON compile the game's UNPATCHED portrait files
#                               (the reference side of AT1.3; never a master)
cmake_minimum_required(VERSION 3.21)

project(
	doomo_show
	LANGUAGES    C ASM
	VERSION      0.1.0
	DESCRIPTION  "DOOMOGACHI LIFE in HELLvile Park! - the show player (PS1)"
)

set(SHOW_RAM_KB 2048 CACHE STRING "RAM the player may assume (2048 = retail, 8192 = the 8 MB hatch)")
option(SHOW_SELFTEST "Link the AT1.3 portrait self-test (show_selftest.c + the roster)" OFF)
option(SHOW_PORTRAIT_ORIGINAL "Compile the game's unpatched portrait files (AT1.3 reference)" OFF)
set(SHOW_EXE_NAME "show" CACHE STRING "Output executable base name")
set(SHOW_PC_BLOCKS 240 CACHE STRING "Portrait pool blocks (spec 4.3 (a); 255 is the hard maximum)")

find_package(Python3 COMPONENTS Interpreter REQUIRED)

set(SHOW_ENGINE ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(SHOW_ROOT  ${SHOW_ENGINE}/.. ABSOLUTE)
get_filename_component(DOOMO_ROOT ${SHOW_ROOT}/.. ABSOLUTE)
set(CORE_DIR ${DOOMO_ROOT}/core)
set(PS1_DIR  ${DOOMO_ROOT}/ps1)
set(SHOW_GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${SHOW_ENGINE}/core_sources.cmake)
include(${SHOW_ENGINE}/ps1_sources.cmake)

# --- the show's own sources: every show/engine/show_*.c (other packages add
# files here without touching this list); the self-test only on request ---
file(GLOB SHOW_SRCS CONFIGURE_DEPENDS ${SHOW_ENGINE}/show_*.c)
list(SORT SHOW_SRCS)
if(NOT SHOW_SELFTEST)
	list(FILTER SHOW_SRCS EXCLUDE REGEX "/show_selftest\\.c$")
endif()
list(FILTER SHOW_SRCS EXCLUDE REGEX "/show_wrap\\.c$")      # P2 trace: deferred
if(SHOW_SELFTEST)
	list(APPEND SHOW_CORE_SRCS ${CORE_DIR}/src/roster.c ${CORE_DIR}/gen/roster_data.c)
endif()

# --- the portrait hunks (spec 4.3): patched copies in gen/ ---
# (SHOW_PATCH_SRC_DIR only exists so tests/engine can feed a stale copy, AT1.4)
set(SHOW_PATCH_SRC_DIR ${CORE_DIR}/src CACHE PATH "Where the originals of the patched files are read")
set(SHOW_PORTRAIT_SRCS "")
foreach(_p ${SHOW_PATCHED})
	if(SHOW_PORTRAIT_ORIGINAL)
		list(APPEND SHOW_PORTRAIT_SRCS ${CORE_DIR}/src/${_p}.c)
	else()
		add_custom_command(
			OUTPUT  ${SHOW_GEN}/${_p}.c
			COMMAND ${Python3_EXECUTABLE} ${SHOW_ENGINE}/apply_patch.py
			        ${SHOW_PATCH_SRC_DIR}/${_p}.c ${SHOW_ENGINE}/patches/${_p}.c.patch ${SHOW_GEN}/${_p}.c
			DEPENDS ${SHOW_PATCH_SRC_DIR}/${_p}.c ${SHOW_ENGINE}/patches/${_p}.c.patch
			        ${SHOW_ENGINE}/apply_patch.py
			COMMENT "show: patching a copy of core/src/${_p}.c (patches/${_p}.c.patch)"
			VERBATIM)
		list(APPEND SHOW_PORTRAIT_SRCS ${SHOW_GEN}/${_p}.c)
	endif()
endforeach()

# --- packed art and maps, made by the game's own tools (read-only) ---
set(_pack_out ${SHOW_GEN}/packed)
add_custom_command(
	OUTPUT  ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c
	COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/packart.py ${CORE_DIR}/gen/art_data.c ${_pack_out}
	DEPENDS ${CORE_DIR}/gen/art_data.c ${PS1_DIR}/tools/packart.py
	COMMENT "show: packing textures and skies (ps1/tools/packart.py)"
	VERBATIM)
set(SHOW_PACKED_SRCS ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c)
set(_packed_maps "")
foreach(_m ${SHOW_CORE_MAPS})
	get_filename_component(_mn ${_m} NAME)
	list(APPEND _packed_maps ${_pack_out}/maps/${_mn})
endforeach()
if(_packed_maps)
	add_custom_command(
		OUTPUT  ${_packed_maps}
		COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/packmaps.py ${_pack_out}/maps ${SHOW_CORE_MAPS}
		DEPENDS ${SHOW_CORE_MAPS} ${PS1_DIR}/tools/packmaps.py ${PS1_DIR}/tools/packart.py
		COMMENT "show: packing the maps' cells (ps1/tools/packmaps.py)"
		VERBATIM)
	list(APPEND SHOW_PACKED_SRCS ${_packed_maps})
endif()

set(SHOW_ALL_CORE ${SHOW_CORE_SRCS} ${SHOW_PORTRAIT_SRCS} ${SHOW_PACKED_SRCS})

# Objects are linked directly (no archive), so the core's weak stubs are
# overridden by strong definitions exactly as in the game.
add_executable(${SHOW_EXE_NAME} ${SHOW_SRCS} ${SHOW_PS1_SRCS} ${SHOW_ALL_CORE})
set_target_properties(${SHOW_EXE_NAME} PROPERTIES PSN00BSDK_TARGET_TYPE EXECUTABLE_GPREL)
target_link_libraries(${SHOW_EXE_NAME} PRIVATE ${PSN00BSDK_EXECUTABLE_LINK_LIBRARIES})
target_include_directories(${SHOW_EXE_NAME} PRIVATE
	${SHOW_ROOT}/include ${SHOW_ENGINE}/include ${SHOW_ENGINE}
	${CORE_DIR}/include ${PS1_DIR}/src)
target_compile_options(${SHOW_EXE_NAME} PRIVATE -O2 -Wall -Wextra)
target_compile_definitions(${SHOW_EXE_NAME} PRIVATE
	DOOMO_PLATFORM_PS1=1 DOOMO_HAVE_CORE=1 DOOMO_PACKED_ART=1 DOOMO_PACKED_MAPS=1
	DOOMO_SHOW=1 SHOW_RAM_KB=${SHOW_RAM_KB})
if(SHOW_SELFTEST)
	target_compile_definitions(${SHOW_EXE_NAME} PRIVATE SHOW_SELFTEST=1)
endif()
if(SHOW_PORTRAIT_ORIGINAL)
	target_compile_definitions(${SHOW_EXE_NAME} PRIVATE SHOW_PORTRAIT_ORIGINAL=1)
endif()
target_link_options(${SHOW_EXE_NAME} PRIVATE
	-T${PSN00BSDK_LDSCRIPTS}/exe.ld
	-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.ldmap
	-Wl,--no-warn-rwx-segments
	-Wl,--wrap=plat_post_offset
	-Wl,--wrap=gfx_upload_strided   # lane 3: synchronous uploads at host prep (show_host_wrap.c)
	-Wl,--defsym=doomo_art_tex_pix=0x01000000
	-Wl,--defsym=doomo_art_sky_pix=0x01800000
	-Wl,--defsym=doomo_art_spr_pix_v=0x02000000)

# the game's per-file flags
set_source_files_properties(${SHOW_PS1_SRCS} ${SHOW_SRCS} PROPERTIES
	COMPILE_OPTIONS "-std=gnu99;-Werror;-fno-strict-aliasing")
set_source_files_properties(${SHOW_ALL_CORE} PROPERTIES COMPILE_OPTIONS "-std=c99;-Os")
foreach(_hot ${SHOW_CORE_HOT})
	set_source_files_properties(${CORE_DIR}/src/${_hot}.c PROPERTIES COMPILE_OPTIONS "-std=c99;-O2")
endforeach()
# the copies include "portrait_int.h" from next to the originals
if(NOT SHOW_PORTRAIT_ORIGINAL)
	set_source_files_properties(${SHOW_PORTRAIT_SRCS} PROPERTIES
		INCLUDE_DIRECTORIES ${CORE_DIR}/src)
	set_source_files_properties(${SHOW_GEN}/portrait.c PROPERTIES
		COMPILE_DEFINITIONS PC_BLOCKS=${SHOW_PC_BLOCKS})
endif()

# --- ELF -> PS-X EXE with the game's converter (its 2 MB gate), the
# symbol map the harness reads, the R3000 ISA check and the RAM report ---
string(REGEX REPLACE "nm$" "objdump" TOOLCHAIN_OBJDUMP "${TOOLCHAIN_NM}")
add_custom_command(
	TARGET ${SHOW_EXE_NAME} POST_BUILD
	COMMAND ${Python3_EXECUTABLE} ${SHOW_ENGINE}/ram_report.py
	        --elf2exe ${PS1_DIR}/tools/elf2exe.py --ram-kb ${SHOW_RAM_KB}
	        $<TARGET_FILE:${SHOW_EXE_NAME}> ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.exe
	        ${CMAKE_CURRENT_BINARY_DIR}/ram.txt
	COMMAND ${TOOLCHAIN_NM} -f posix -l -n $<TARGET_FILE:${SHOW_EXE_NAME}>
	        > ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.map
	COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/check_isa.py
	        ${TOOLCHAIN_OBJDUMP} $<TARGET_FILE:${SHOW_EXE_NAME}>
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.exe
	           ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.map
	           ${CMAKE_CURRENT_BINARY_DIR}/ram.txt
	VERBATIM)

# --- the show disc: SYSTEM.CNF, SHOW.EXE, SHOW/EP###.SHW (iso_show.xml).
# Episodes join it through SHOW_ISO_FILES (a list of "NAME=path" pairs,
# e.g. -DSHOW_ISO_FILES=EP101.SHW=/abs/build/ep101/EP101.SHW) ---
set(SHOW_ISO_FILES "" CACHE STRING "Episode files on the disc: NAME=path;...")
set(SHOW_ISO_EPISODES "")
set(_iso_deps "")
foreach(_f ${SHOW_ISO_FILES})
	string(REPLACE "=" ";" _kv "${_f}")
	list(GET _kv 0 _name)
	list(GET _kv 1 _path)
	string(APPEND SHOW_ISO_EPISODES "\t\t\t\t<file name=\"${_name}\" type=\"data\" source=\"${_path}\" />\n")
	list(APPEND _iso_deps ${_path})
endforeach()
if(SHOW_ISO_EPISODES)
	set(SHOW_ISO_EPISODES "\t\t\t<dir name=\"SHOW\">\n${SHOW_ISO_EPISODES}\t\t\t</dir>\n")
endif()
set(SHOW_ISO_XML ${CMAKE_CURRENT_BINARY_DIR}/iso_show.xml)
configure_file(${SHOW_ENGINE}/iso_show.xml ${SHOW_ISO_XML})
add_custom_command(
	OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.bin ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.cue
	COMMAND ${MKPSXISO} -y -q
	        -o ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.bin
	        -c ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.cue
	        ${SHOW_ISO_XML}
	DEPENDS ${SHOW_EXE_NAME} ${SHOW_ENGINE}/system.cnf ${SHOW_ENGINE}/iso_show.xml ${_iso_deps}
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	COMMENT "show: building the disc image ${SHOW_EXE_NAME}.bin/.cue"
	VERBATIM)
add_custom_target(disc ALL DEPENDS
	${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.bin ${CMAKE_CURRENT_BINARY_DIR}/${SHOW_EXE_NAME}.cue)
