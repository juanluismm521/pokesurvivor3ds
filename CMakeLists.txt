# Compilación en PC (Linux) del núcleo del juego y de sus pruebas.
# La 3DS se compila con el Makefile; esto es sólo src/core + tests/.
cmake_minimum_required(VERSION 3.16)
project(pokesurvivor_3ds_pc C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

option(PS_SANITIZE "AddressSanitizer + UBSan" ON)

add_compile_options(-Wall -Wextra "$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions;-fno-rtti>")
if(PS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer
                      -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=address,undefined)
endif()

# Tablas del juego generadas desde la web (tools/gen_data.js). El código de la
# web (reference/web) no va en el repositorio público: sin él se usan las tablas
# ya generadas que sí van (src/gen/gamedata.cpp y tests/web-generado/).
find_program(NODE node REQUIRED)
set(PS_HAY_WEB OFF)
if(EXISTS ${CMAKE_SOURCE_DIR}/reference/web/js/data)
  set(PS_HAY_WEB ON)
endif()
set(WEB_GEN ${CMAKE_SOURCE_DIR}/tests/web-generado)
if(PS_HAY_WEB)
  file(GLOB WEB_DATA CONFIGURE_DEPENDS reference/web/js/data/*.js reference/web/js/world/tiles.js
       reference/web/js/systems/*.js reference/web/js/entities/vfx.js)
  set(GEN_DATA ${CMAKE_BINARY_DIR}/gen/gamedata.cpp)
  set(GEN_PACK ${CMAKE_BINARY_DIR}/gen/paquete.json)   # paquete de datos del juego
  add_custom_command(OUTPUT ${GEN_DATA} ${GEN_PACK}
    COMMAND ${CMAKE_COMMAND} -E env PS_PAQUETE=${GEN_PACK} ${NODE} ${CMAKE_SOURCE_DIR}/tools/gen_data.js ${GEN_DATA}
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEN_PACK} ${WEB_GEN}/paquete.json
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/gen_data.js ${CMAKE_SOURCE_DIR}/tools/lib/websandbox.js ${WEB_DATA}
            ${CMAKE_SOURCE_DIR}/assets-src/megas/megas.json ${CMAKE_SOURCE_DIR}/assets-src/skins/skins.json
    COMMENT "Generando las tablas del juego")
else()
  message(STATUS "Sin reference/web: se usan las tablas ya generadas (src/gen, tests/web-generado)")
  set(GEN_DATA ${CMAKE_SOURCE_DIR}/src/gen/gamedata.cpp)
  set(GEN_PACK ${WEB_GEN}/paquete.json)
endif()

file(GLOB CORE_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/core/*.c)
add_library(ps_core STATIC ${CORE_SOURCES} ${GEN_DATA})
target_include_directories(ps_core PUBLIC src)

find_path(DOCTEST_INCLUDE doctest/doctest.h REQUIRED)

file(GLOB TEST_SOURCES CONFIGURE_DEPENDS tests/test_*.cpp)
add_executable(ps_tests ${TEST_SOURCES})
# doctest necesita excepciones; las pruebas sí las usan, el núcleo no.
target_compile_options(ps_tests PRIVATE -fexceptions)
target_include_directories(ps_tests PRIVATE ${DOCTEST_INCLUDE} tests)
target_link_libraries(ps_tests PRIVATE ps_core)
# Casos dorados: el JS original ejecutado con node (tools/golden/golden.js).
# Se copian a tests/web-generado/golden para quien compile sin reference/web.
if(PS_HAY_WEB)
  set(GOLDEN_DIR ${CMAKE_BINARY_DIR}/golden)
  add_custom_command(OUTPUT ${GOLDEN_DIR}/golden.json
    COMMAND ${NODE} ${CMAKE_SOURCE_DIR}/tools/golden/golden.js ${GOLDEN_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${GOLDEN_DIR} ${WEB_GEN}/golden
    DEPENDS ${CMAKE_SOURCE_DIR}/tools/golden/golden.js ${CMAKE_SOURCE_DIR}/tools/lib/websandbox.js ${WEB_DATA}
            ${CMAKE_SOURCE_DIR}/reference/web/js/world/world.js
    COMMENT "Sacando los casos dorados del JS de la web")
  add_custom_target(golden DEPENDS ${GOLDEN_DIR}/golden.json)
  add_dependencies(ps_tests golden)
else()
  set(GOLDEN_DIR ${WEB_GEN}/golden)
endif()
# El servidor de relevo (jugar online) vive en su propio repositorio, al lado.
# El servidor de relevo (repo pokesurvivor3ds-server): en el VPS, al lado de este repo (servidor/).
if(EXISTS "${CMAKE_SOURCE_DIR}/../servidor/relay.py")
  set(PS_RELAY_DEFAULT "${CMAKE_SOURCE_DIR}/../servidor/relay.py")
else()
  set(PS_RELAY_DEFAULT "${CMAKE_SOURCE_DIR}/../docker/pokesurvivor-relay/relay.py")
endif()
set(PS_RELAY_PY "${PS_RELAY_DEFAULT}" CACHE FILEPATH "relay.py del repositorio pokesurvivor3ds-server")
target_compile_definitions(ps_tests PRIVATE
  PS_SOURCE_DIR="${CMAKE_SOURCE_DIR}" PS_GOLDEN_DIR="${GOLDEN_DIR}" PS_RELAY_PY="${PS_RELAY_PY}" PS_PAQUETE="${GEN_PACK}")

# Herramienta: el bot juega una partida y escribe un resumen
add_executable(ps_sim tests/sim_main.cpp)
target_include_directories(ps_sim PRIVATE tests)
target_link_libraries(ps_sim PRIVATE ps_core)

# Herramienta: balanceo de la megapiedra frente a los objetos (ps_balance)
add_executable(ps_i18n tests/i18n_main.cpp)
target_link_libraries(ps_i18n PRIVATE ps_core)
add_executable(ps_balance tests/balance_main.cpp)
target_include_directories(ps_balance PRIVATE tests)
target_link_libraries(ps_balance PRIVATE ps_core)
# Herramienta: ¿cuesta llegar al tesoro de la Luminosfera? (ps_tesoro)
add_executable(ps_tesoro tests/treasure_main.cpp)
target_include_directories(ps_tesoro PRIVATE tests)
target_link_libraries(ps_tesoro PRIVATE ps_core)
# Herramienta: el juego en grupo por internet en casas de verdad (ps_red; mejor en un
# build sin sanitizers: cmake -DPS_SANITIZE=OFF -DCMAKE_BUILD_TYPE=Release)
add_executable(ps_red tests/red_main.cpp)
target_include_directories(ps_red PRIVATE tests)
target_link_libraries(ps_red PRIVATE ps_core)
target_compile_definitions(ps_red PRIVATE PS_RELAY_PY="${PS_RELAY_PY}")
# Herramienta: llegar al servidor si el DNS de casa no lo encuentra (ps_dns; necesita libcurl)
find_package(CURL)
if(CURL_FOUND)
  add_executable(ps_dns tests/dns_main.cpp)
  target_link_libraries(ps_dns PRIVATE ps_core CURL::libcurl)
endif()

enable_testing()
add_test(NAME ps_tests COMMAND ps_tests)
# Todo texto del port, entero en inglés (tools/i18n): extraer.py y ps_i18n estricto.
add_test(NAME i18n COMMAND sh -c "python3 ${CMAKE_SOURCE_DIR}/tools/i18n/extraer.py >/dev/null && $<TARGET_FILE:ps_i18n> ${CMAKE_SOURCE_DIR}/build/i18n/textos.tsv estricto ${CMAKE_SOURCE_DIR}/tools/i18n/ignorar.txt && $<TARGET_FILE:ps_i18n> datos estricto")

# ps_cloud: la sincronización con pokesurvivor.com desde el PC (con curl), para
# probarla contra el emulador de Firebase y la web (tools/cloud_e2e.js).
find_package(CURL QUIET)
if(CURL_FOUND)
  add_executable(ps_cloud tests/cloud_main.cpp)
  target_link_libraries(ps_cloud PRIVATE ps_core CURL::libcurl)
endif()
