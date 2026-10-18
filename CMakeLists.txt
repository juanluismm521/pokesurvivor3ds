# Compilación en PC (Linux) del núcleo del juego y de sus pruebas.
# La 3DS se compila con el Makefile; esto es sólo src/core + tests/.
cmake_minimum_required(VERSION 3.16)
project(pokesurvivor_3ds_pc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(PS_SANITIZE "AddressSanitizer + UBSan" ON)

add_compile_options(-Wall -Wextra -fno-exceptions -fno-rtti)
if(PS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer
                      -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp)
add_library(ps_core STATIC ${CORE_SOURCES})
target_include_directories(ps_core PUBLIC src)

find_path(DOCTEST_INCLUDE doctest/doctest.h REQUIRED)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)
add_executable(ps_tests ${TEST_SOURCES})
# doctest necesita excepciones; las pruebas sí las usan, el núcleo no.
target_compile_options(ps_tests PRIVATE -fexceptions)
target_include_directories(ps_tests PRIVATE ${DOCTEST_INCLUDE} tests)
target_link_libraries(ps_tests PRIVATE ps_core)
target_compile_definitions(ps_tests PRIVATE
  PS_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

enable_testing()
add_test(NAME ps_tests COMMAND ps_tests)
