# sources_fallback.cmake - the show.exe closure for the hosts' test build,
# used only while show/engine/core_sources.cmake and ps1_sources.cmake are
# not in the tree (the same lists: spec 8.7, the v1.1 link probe). The
# portrait files are added by CMakeLists.txt.
set(SHOW_CORE_NAMES
	portrait_body portrait_fx portrait_gfx portrait_gun portrait_hair portrait_lut
	beat camfx tween tween_tables
	ui ui_draw ui_raster ui_fmt font palette fixmath fixmath_table txt
	resident catalog rng town options
	stubs textures sprites plat_weak_fx plat_weak_music
	home home_draw home_edit map maps
)
set(SHOW_CORE_GEN_NAMES txt_dict furn_data furn_iso furn_grant)
set(SHOW_CORE_HOT ui_raster ui ui_draw font fixmath map rng)
set(SHOW_CORE_SRCS "")
foreach(_n ${SHOW_CORE_NAMES})
	list(APPEND SHOW_CORE_SRCS ${CORE_DIR}/src/${_n}.c)
endforeach()
foreach(_n ${SHOW_CORE_GEN_NAMES})
	list(APPEND SHOW_CORE_SRCS ${CORE_DIR}/gen/${_n}.c)
endforeach()
file(GLOB SHOW_CORE_MAPS CONFIGURE_DEPENDS ${CORE_DIR}/gen/map_*.c)
list(SORT SHOW_CORE_MAPS)

set(SHOW_PS1_NAMES
	gfx gfx2d vram view3d r3d overworld room room_test postfx xform prims model world
	packart timer pad hcmd main coreref libgcc_r3000 adpcm spu memcard
	cutview overlay txtload
)
set(SHOW_PS1_SRCS "")
foreach(_n ${SHOW_PS1_NAMES})
	list(APPEND SHOW_PS1_SRCS ${PS1_DIR}/src/${_n}.c)
endforeach()
