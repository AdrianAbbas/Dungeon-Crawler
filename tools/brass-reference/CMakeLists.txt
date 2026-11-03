# Session 12: LIGHT SEEKER's brass rendered and measured the same way as DUNGEON CRAWLER's
# (Tests/BrassMetrics.h), for the horn comparison. Builds LIGHT SEEKER's own sources out of
# tree from a read-only checkout; nothing in that checkout is written or changed.
#   cmake -S tools/brass-reference -B build-ref -G Ninja -DCMAKE_BUILD_TYPE=Release \
#         -DLS_SOURCE=/path/to/light-seeker -DJUCE_PATH=/path/to/JUCE
#   cmake --build build-ref --target BrassReference
#   build-ref/BrassReference_artefacts/Release/BrassReference out-dir
cmake_minimum_required(VERSION 3.22)
project(BRASS_REFERENCE C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(LS_SOURCE "" CACHE PATH "A LIGHT SEEKER checkout (read only)")
set(LS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory("${LS_SOURCE}" "${CMAKE_BINARY_DIR}/light-seeker")
juce_add_console_app(BrassReference PRODUCT_NAME "BrassReference")
get_target_property(ref_definitions BrassReference COMPILE_DEFINITIONS)
list(REMOVE_ITEM ref_definitions JUCE_STANDALONE_APPLICATION=1)
set_target_properties(BrassReference PROPERTIES COMPILE_DEFINITIONS "${ref_definitions}")
target_sources(BrassReference PRIVATE main.cpp)
target_include_directories(BrassReference PRIVATE "${LS_SOURCE}/Tests" "${LS_SOURCE}/Source" "${CMAKE_CURRENT_SOURCE_DIR}/../../Tests")
target_link_libraries(BrassReference PRIVATE LightSeeker juce::juce_audio_utils)
target_compile_definitions(BrassReference PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
