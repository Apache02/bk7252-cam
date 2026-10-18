# Script mode (cmake -P): turns a file into a C source with a byte array and its size.
#
#   cmake -DINPUT=<file> -DOUTPUT=<file.c> -DSYMBOL=<name> -P embed_file.cmake
#
# The source defines `const unsigned char <name>[]` and `const unsigned <name>_size`.

file(READ "${INPUT}" hex HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," bytes "${hex}")

file(WRITE "${OUTPUT}"
    "const unsigned char ${SYMBOL}[] = {${bytes}};\n"
    "const unsigned ${SYMBOL}_size = sizeof(${SYMBOL});\n")
