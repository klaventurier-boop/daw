# Studio Voice Pro — moteur audio v0.1
#
# Bibliothèque `svp_core` : moteur C++20 SANS JUCE et sans aucune IHM.
# Outils : `svp-render` (traitement WAV hors ligne), `svp-gen` (WAV de test).
# Tests : `svp_tests`, inscrits dans ctest par suite de tests.
#
# IMPORTANT : compiler HORS de /home (le système de fichiers /home ne dispose
# que de ~290 Mo). Utiliser par exemple :
#   cmake -S . -B /tmp/svp-build -DCMAKE_BUILD_TYPE=Release
#   cmake --build /tmp/svp-build -j2
#   ctest --test-dir /tmp/svp-build --output-on-failure

cmake_minimum_required(VERSION 3.20)

project(StudioVoicePro
    VERSION 0.1.0
    DESCRIPTION "Studio Voice Pro - moteur audio hors ligne (v0.1)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Sorties de compilation par défaut sous /tmp si l'appelant n'a rien précisé,
# afin de ne jamais écrire d'objets dans /home.
if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Type de build" FORCE)
endif()

add_library(svp_core STATIC
    src/audio/Buffer.cpp
    src/audio/SampleRate.cpp
    src/dsp/Denormals.cpp
    src/dsp/GainPan.cpp
    src/dsp/Mixer.cpp
    src/dsp/Sanitize.cpp
    src/engine/Engine.cpp
    src/io/WavFile.cpp
    src/metering/Meter.cpp
    src/transport/Transport.cpp
)

target_include_directories(svp_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")

if(MSVC)
    target_compile_options(svp_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(svp_core PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
endif()

# --- Outils en ligne de commande -------------------------------------------
add_executable(svp-render tools/svp_render.cpp)
target_link_libraries(svp-render PRIVATE svp_core)

add_executable(svp-gen tools/svp_gen.cpp)
target_link_libraries(svp-gen PRIVATE svp_core)

# --- Tests -----------------------------------------------------------------
option(SVP_BUILD_TESTS "Construire les tests unitaires" ON)

if(SVP_BUILD_TESTS)
    enable_testing()

    add_executable(svp_tests
        tests/main.cpp
        tests/test_audio.cpp
        tests/test_convert.cpp
        tests/test_dsp.cpp
        tests/test_engine.cpp
        tests/test_metering.cpp
        tests/test_transport.cpp
        tests/test_wav.cpp
    )
    target_link_libraries(svp_tests PRIVATE svp_core)
    target_include_directories(svp_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")

    # Répertoire de travail des tests (fichiers WAV temporaires) : /tmp, jamais /home.
    set(SVP_TEST_TMPDIR "${CMAKE_BINARY_DIR}/test-tmp" CACHE PATH "Répertoire des WAV de test")
    file(MAKE_DIRECTORY "${SVP_TEST_TMPDIR}")

    # Une entrée ctest par suite : le binaire accepte un filtre en argument.
    set(SVP_TEST_SUITES audio convert dsp engine metering transport io)
    foreach(suite IN LISTS SVP_TEST_SUITES)
        add_test(NAME svp.${suite} COMMAND svp_tests ${suite})
        set_tests_properties(svp.${suite} PROPERTIES
            ENVIRONMENT "SVP_TEST_TMPDIR=${SVP_TEST_TMPDIR}"
            WORKING_DIRECTORY "${SVP_TEST_TMPDIR}")
    endforeach()

    message(STATUS "Studio Voice Pro : tests activés (ctest --test-dir ${CMAKE_BINARY_DIR})")
endif()

message(STATUS "Studio Voice Pro v${PROJECT_VERSION} : build dans ${CMAKE_BINARY_DIR}")
message(STATUS "  moteur sans IHM : svp_core (audio, io, dsp, metering, transport)")

# ---------------------------------------------------------------------------
# v0.2 : format de projet (module `project`) et lecteur/écrivain JSON maison.
# Ajouté en fin de fichier : l'ordre de définition des cibles n'a pas
# d'importance en CMake, et les listes d'origine restent lisibles.
target_sources(svp_core PRIVATE
    src/json/Json.cpp
    src/project/Project.cpp
    src/project/ProjectIO.cpp
)
add_executable(svp-project tools/svp_project.cpp)
target_link_libraries(svp-project PRIVATE svp_core)
if(SVP_BUILD_TESTS)
    target_sources(svp_tests PRIVATE tests/test_project.cpp)
    add_test(NAME svp.project COMMAND svp_tests project)
    set_tests_properties(svp.project PROPERTIES
        ENVIRONMENT "SVP_TEST_TMPDIR=${SVP_TEST_TMPDIR}"
        WORKING_DIRECTORY "${SVP_TEST_TMPDIR}")
endif()
