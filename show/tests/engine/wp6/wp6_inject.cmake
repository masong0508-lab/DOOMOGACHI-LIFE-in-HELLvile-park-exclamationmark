# wp6_inject.cmake - adds the WP6 test program to a TEST build of show.exe
# (show/build/test-wp6/), without touching show/engine/CMakeLists.txt:
#
#   cmake --preset ps1-show -B show/build/test-wp6 \
#         -DCMAKE_PROJECT_doomo_show_INCLUDE=<abs>/show/tests/engine/wp6/wp6_inject.cmake \
#         -DWP6_DATA=<abs>/show/build/test-wp6/gen/wp6_data.c
#
# CMake includes this file right after the project() call of doomo_show (the
# PSn00bSDK toolchain owns CMAKE_PROJECT_INCLUDE itself); the
# deferred call runs at the end of the CMakeLists, when the `show` target
# exists. The driver takes the frame through show_game.c's self-test hook
# (SHOW_SELFTEST, with show_selftest.c itself left out: it is only compiled
# when the SHOW_SELFTEST option is ON). Never used for a master.
set(WP6_DIR ${CMAKE_CURRENT_LIST_DIR})
function(_wp6_inject)
	target_sources(show PRIVATE ${WP6_DIR}/wp6_driver.c ${WP6_DATA})
	target_include_directories(show PRIVATE ${WP6_DIR})
	target_compile_definitions(show PRIVATE SHOW_SELFTEST=1 SHOW_WP6_TEST=1)
	set_source_files_properties(${WP6_DIR}/wp6_driver.c ${WP6_DATA} PROPERTIES
		COMPILE_OPTIONS "-std=gnu99;-Werror;-fno-strict-aliasing")
endfunction()
cmake_language(DEFER CALL _wp6_inject)
