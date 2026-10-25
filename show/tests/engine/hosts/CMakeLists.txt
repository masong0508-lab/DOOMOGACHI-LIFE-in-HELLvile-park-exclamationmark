# show/tests/engine/hosts - the stage-kind hosts' test player (owner: show,
# ENGINE lane 3). Builds hosttest.exe: the game's renderers, the show's
# host files (show/engine/show_host*.c, show_rcopy_*.c, show_hud.c) and a
# small driver (host_driver.c) that plays the test shots of scenes/*.shw,
# packed by show/tools/showtk/hosts.py into a generated C table. Used
# before the player's shot loop exists, and kept as the hosts' own test.
#
#   python3 show/tests/engine/hosts/run_hosts.py      (configure, build, run, report)
#
# A stand-alone project like show/engine (spec 4.1): reads core/ and ps1/
# by path, writes only its binary dir (show/build/hosttest).
cmake_minimum_required(VERSION 3.21)
project(doomo_show_hosttest LANGUAGES C ASM)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

get_filename_component(HT_DIR    ${CMAKE_CURRENT_SOURCE_DIR} ABSOLUTE)
get_filename_component(SHOW_ROOT ${HT_DIR}/../../.. ABSOLUTE)
get_filename_component(DOOMO_ROOT ${SHOW_ROOT}/.. ABSOLUTE)
set(SHOW_ENGINE ${SHOW_ROOT}/engine)
set(CORE_DIR ${DOOMO_ROOT}/core)
set(PS1_DIR  ${DOOMO_ROOT}/ps1)
set(HT_GEN   ${CMAKE_CURRENT_BINARY_DIR}/gen)

# the curated lists: the player's own (show/engine) when they exist, so the
# test links exactly what show.exe links; else the same closure here
if(EXISTS ${SHOW_ENGINE}/core_sources.cmake AND EXISTS ${SHOW_ENGINE}/ps1_sources.cmake)
	include(${SHOW_ENGINE}/core_sources.cmake)
	include(${SHOW_ENGINE}/ps1_sources.cmake)
else()
	include(${HT_DIR}/sources_fallback.cmake)
endif()

# the hosts' extra core modules (the weapon art of the first-person HUD)
set(HT_CORE_EXTRA weld weld_gfx scene_scratch)
foreach(_n ${HT_CORE_EXTRA})
	list(APPEND SHOW_CORE_SRCS ${CORE_DIR}/src/${_n}.c)
endforeach()
list(REMOVE_DUPLICATES SHOW_CORE_SRCS)

# the show's host files (and nothing else of show/engine: the test has its
# own doomo_init / doomo_frame in host_driver.c)
file(GLOB HT_SHOW_SRCS CONFIGURE_DEPENDS
	${SHOW_ENGINE}/show_host*.c ${SHOW_ENGINE}/show_rcopy_*.c ${SHOW_ENGINE}/show_hud.c
	${SHOW_ENGINE}/show_actor.c)
list(FILTER HT_SHOW_SRCS EXCLUDE REGEX "/show_host_mod\\.c$")   # the player plug-in (show_api.h)
list(SORT HT_SHOW_SRCS)

# the test shots: scenes/*.shw -> one C table (hosts.py)
file(GLOB HT_SCENES CONFIGURE_DEPENDS ${HT_DIR}/scenes/*.shw)
list(SORT HT_SCENES)
add_custom_command(
	OUTPUT  ${HT_GEN}/host_scenes.c
	COMMAND ${Python3_EXECUTABLE} -m showtk.hosts cgen -o ${HT_GEN}/host_scenes.c ${HT_SCENES}
	WORKING_DIRECTORY ${SHOW_ROOT}/tools
	DEPENDS ${HT_SCENES} ${SHOW_ROOT}/tools/showtk/hosts.py
	COMMENT "hosttest: packing the test shots (showtk/hosts.py)"
	VERBATIM)

# packed art and maps, by the game's own tools (read-only)
set(_pack_out ${HT_GEN}/packed)
add_custom_command(
	OUTPUT  ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c
	COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/packart.py ${CORE_DIR}/gen/art_data.c ${_pack_out}
	DEPENDS ${CORE_DIR}/gen/art_data.c ${PS1_DIR}/tools/packart.py
	COMMENT "hosttest: packing textures and skies"
	VERBATIM)
set(HT_PACKED ${_pack_out}/art_data_ps1.c ${_pack_out}/packed_art.c)
set(_packed_maps "")
foreach(_m ${SHOW_CORE_MAPS})
	get_filename_component(_mn ${_m} NAME)
	list(APPEND _packed_maps ${_pack_out}/maps/${_mn})
endforeach()
add_custom_command(
	OUTPUT  ${_packed_maps}
	COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/packmaps.py ${_pack_out}/maps ${SHOW_CORE_MAPS}
	DEPENDS ${SHOW_CORE_MAPS} ${PS1_DIR}/tools/packmaps.py ${PS1_DIR}/tools/packart.py
	COMMENT "hosttest: packing the maps' cells"
	VERBATIM)
list(APPEND HT_PACKED ${_packed_maps})

# the portrait files: the same patched copies show.exe compiles (spec 4.3),
# with its 240-block pool: the game's 48 blocks thrash on a crowd, and every
# re-render is a new VRAM uid (vram.c's hash then fills with live entries)
set(HT_PORTRAIT "")
foreach(_p portrait portrait_head)
	add_custom_command(
		OUTPUT  ${HT_GEN}/${_p}.c
		COMMAND ${Python3_EXECUTABLE} ${SHOW_ENGINE}/apply_patch.py
		        ${CORE_DIR}/src/${_p}.c ${SHOW_ENGINE}/patches/${_p}.c.patch ${HT_GEN}/${_p}.c
		DEPENDS ${CORE_DIR}/src/${_p}.c ${SHOW_ENGINE}/patches/${_p}.c.patch ${SHOW_ENGINE}/apply_patch.py
		VERBATIM)
	list(APPEND HT_PORTRAIT ${HT_GEN}/${_p}.c)
endforeach()
set(HT_ALL_CORE ${SHOW_CORE_SRCS} ${HT_PORTRAIT} ${HT_PACKED})

add_executable(hosttest ${HT_DIR}/host_driver.c ${HT_DIR}/host_stubs.c ${HT_GEN}/host_scenes.c
	${HT_SHOW_SRCS} ${SHOW_PS1_SRCS} ${HT_ALL_CORE})
set_target_properties(hosttest PROPERTIES PSN00BSDK_TARGET_TYPE EXECUTABLE_GPREL)
target_link_libraries(hosttest PRIVATE ${PSN00BSDK_EXECUTABLE_LINK_LIBRARIES})
target_include_directories(hosttest PRIVATE
	${SHOW_ROOT}/include ${SHOW_ENGINE} ${CORE_DIR}/include ${PS1_DIR}/src)
target_compile_options(hosttest PRIVATE -O2 -Wall -Wextra)
target_compile_definitions(hosttest PRIVATE
	DOOMO_PLATFORM_PS1=1 DOOMO_HAVE_CORE=1 DOOMO_PACKED_ART=1 DOOMO_PACKED_MAPS=1
	DOOMO_SHOW=1 SHOW_HOSTTEST=1)
target_link_options(hosttest PRIVATE
	-T${PSN00BSDK_LDSCRIPTS}/exe.ld
	-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/hosttest.ldmap
	-Wl,--no-warn-rwx-segments
	-Wl,--wrap=gfx_upload_strided
	-Wl,--defsym=doomo_art_tex_pix=0x01000000
	-Wl,--defsym=doomo_art_sky_pix=0x01800000
	-Wl,--defsym=doomo_art_spr_pix_v=0x02000000)
set_source_files_properties(${SHOW_PS1_SRCS} ${HT_SHOW_SRCS} ${HT_DIR}/host_driver.c ${HT_DIR}/host_stubs.c
	PROPERTIES COMPILE_OPTIONS "-std=gnu99;-Werror;-fno-strict-aliasing")
set_source_files_properties(${HT_ALL_CORE} ${HT_GEN}/host_scenes.c PROPERTIES COMPILE_OPTIONS "-std=c99;-Os")
set_source_files_properties(${HT_PORTRAIT} PROPERTIES INCLUDE_DIRECTORIES ${CORE_DIR}/src)
set_source_files_properties(${HT_GEN}/portrait.c PROPERTIES COMPILE_DEFINITIONS PC_BLOCKS=240)
foreach(_hot ${SHOW_CORE_HOT})
	set_source_files_properties(${CORE_DIR}/src/${_hot}.c PROPERTIES COMPILE_OPTIONS "-std=c99;-O2")
endforeach()

string(REGEX REPLACE "nm$" "objdump" TOOLCHAIN_OBJDUMP "${TOOLCHAIN_NM}")
add_custom_command(
	TARGET hosttest POST_BUILD
	COMMAND ${Python3_EXECUTABLE} ${PS1_DIR}/tools/elf2exe.py $<TARGET_FILE:hosttest>
	        ${CMAKE_CURRENT_BINARY_DIR}/hosttest.exe
	COMMAND ${TOOLCHAIN_NM} -f posix -l -n $<TARGET_FILE:hosttest> > ${CMAKE_CURRENT_BINARY_DIR}/hosttest.map
	BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/hosttest.exe ${CMAKE_CURRENT_BINARY_DIR}/hosttest.map
	VERBATIM)
