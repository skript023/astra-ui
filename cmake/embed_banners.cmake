# Run with cmake -P: turns every image listed in ${ASSETS}/banners.txt into a byte array in ${OUTPUT}.
file(STRINGS "${ASSETS}/banners.txt" lines)
# CMake regex has no {n} repetition, so a 32-byte row is spelled out to wrap the arrays.
string(REPEAT "0x..," 32 row)
set(arrays "")
set(entries "")
set(index 0)
foreach(line IN LISTS lines)
    if(NOT line MATCHES "^([^ ]+) (.+)$")
        continue()
    endif()
    set(name "${CMAKE_MATCH_2}")
    file(READ "${ASSETS}/${CMAKE_MATCH_1}" hex HEX)
    string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")
    string(REGEX REPLACE "(${row})" "\\1\n" bytes "${bytes}")
    string(APPEND arrays "\t\tconstexpr unsigned char banner_${index}[] = {\n${bytes}};\n")
    string(APPEND entries "\t\t\t{\"${name}\", banner_${index}},\n")
    math(EXPR index "${index} + 1")
endforeach()

file(WRITE "${OUTPUT}.tmp" "// Generated from assets/banners by cmake/embed_banners.cmake.
#include <astra/banners.hpp>

namespace astra
{
\tnamespace
\t{
${arrays}
\t\tconstexpr banner list[] = {
${entries}\t\t};
\t}

\tstd::span<const banner> banners()
\t{
\t\treturn list;
\t}
}
")
file(COPY_FILE "${OUTPUT}.tmp" "${OUTPUT}" ONLY_IF_DIFFERENT)
file(REMOVE "${OUTPUT}.tmp")
