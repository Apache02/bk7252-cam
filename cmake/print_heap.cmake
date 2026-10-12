# Prints the heap size (_empty_ram_begin.._empty_ram_end) of a linked ELF.
# Usage: cmake -DNM=<nm> -DELF=<elf> -P print_heap.cmake
execute_process(
        COMMAND ${NM} ${ELF}
        OUTPUT_VARIABLE NM_OUT
        RESULT_VARIABLE NM_RESULT
)
if(NOT NM_RESULT EQUAL 0)
    return()
endif()

string(REGEX MATCH "([0-9a-fA-F]+) [A-Za-z] _empty_ram_begin" _ "${NM_OUT}")
set(HEAP_BEGIN "0x${CMAKE_MATCH_1}")
string(REGEX MATCH "([0-9a-fA-F]+) [A-Za-z] _empty_ram_end" _ "${NM_OUT}")
set(HEAP_END "0x${CMAKE_MATCH_1}")

if(HEAP_BEGIN STREQUAL "0x" OR HEAP_END STREQUAL "0x")
    return()
endif()

math(EXPR HEAP_SIZE "${HEAP_END} - ${HEAP_BEGIN}")
math(EXPR HEAP_KB "${HEAP_SIZE} / 1024")
string(LENGTH "${HEAP_SIZE}" LEN)
math(EXPR PAD "11 - ${LEN}")
string(REPEAT " " ${PAD} SPACES)
message("            HEAP: ${SPACES}${HEAP_SIZE} B  (${HEAP_KB} KB, whole free RAM)")
