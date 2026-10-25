# ps1_sources.cmake - the PS1 platform files show.exe links (spec 4.1).
#
# All of ps1/src except the engine test (etest*.c): the 2D layer, VRAM
# cache, GTE 3D, overworld, room, post-FX, timer, pad, the harness mailbox
# (hcmd.c), main.c's frame protocol, and the SPU / memory card drivers
# main.c initialises. cutview.c, overlay.c and txtload.c are compiled
# WITHOUT -DDOOMO_CUTSCENE_VIEWER / -DDOOMO_OVERLAYS: main.c calls them
# unconditionally and their #ifdef bodies compile to stubs.
# Read-only; a name the game deletes stops the show's configure step.

set(SHOW_PS1_NAMES
	gfx gfx2d vram view3d r3d overworld room room_test postfx xform prims model world
	packart timer pad hcmd main coreref libgcc_r3000 adpcm spu memcard
	cutview overlay txtload
)

set(SHOW_PS1_SRCS "")
foreach(_n ${SHOW_PS1_NAMES})
	if(NOT EXISTS ${PS1_DIR}/src/${_n}.c)
		message(FATAL_ERROR "show: ps1/src/${_n}.c is gone (show/engine/ps1_sources.cmake)")
	endif()
	list(APPEND SHOW_PS1_SRCS ${PS1_DIR}/src/${_n}.c)
endforeach()
