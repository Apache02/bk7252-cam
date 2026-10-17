# Script mode (cmake -P): turns a text file into a C source with one NUL-terminated array.
#
#   cmake -DINPUT=<file> -DOUTPUT=<file.c> -DSYMBOL=<name> -P embed_text.cmake

file(READ "${INPUT}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")

file(WRITE "${OUTPUT}" "const char ${SYMBOL}[] = {${bytes}0x00};\n")
