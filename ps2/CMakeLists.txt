cmake_minimum_required(VERSION 3.16)
# Configure with ps2dev's official EE toolchain:
# cmake -S ps2 -B ps2/build -DCMAKE_TOOLCHAIN_FILE="$PS2DEV/share/ps2dev.cmake"
project(DoomogachiPS2 C)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT DEFINED ENV{PS2DEV} OR NOT DEFINED ENV{PS2SDK})
  message(FATAL_ERROR "Set PS2DEV and PS2SDK; install ps2dev PS2SDK + gsKit")
endif()
if(NOT DEFINED ENV{GSKIT})
  message(FATAL_ERROR "Set GSKIT to the installed gsKit directory")
endif()
if(NOT CMAKE_C_COMPILER MATCHES "ps2-elf")
  message(FATAL_ERROR "A PS2 EE cross compiler is required; use ps2dev.cmake")
endif()

add_executable(doomogachi_ps2 src/main.c src/padmap.c)
set_target_properties(doomogachi_ps2 PROPERTIES SUFFIX ".elf")
target_compile_options(doomogachi_ps2 PRIVATE -O2 -Wall -Wextra -ffunction-sections -fdata-sections)
target_include_directories(doomogachi_ps2 PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/../core/include"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
  "$ENV{GSKIT}/include"
  "$ENV{PS2SDK}/ee/include")
target_link_directories(doomogachi_ps2 PRIVATE "$ENV{GSKIT}/lib" "$ENV{PS2SDK}/ee/lib")
target_link_libraries(doomogachi_ps2 PRIVATE gskit_toolkit gskit dmakit pad)
