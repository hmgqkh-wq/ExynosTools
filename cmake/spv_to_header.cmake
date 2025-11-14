# Usage: cmake -DINPUT=path/to.spv -DOUTPUT=path/to.h -DSYMBOL=name -P this_script.cmake

file(READ "${INPUT}" BINARY_DATA HEX)
string(LENGTH "${HEX}" HEXLEN)
math(EXPR WORDS "${HEXLEN} / 8")

set(C_ARRAY "")
set(I 0)
while(I LESS WORDS)
    math(EXPR START "${I} * 8")
    string(SUBSTRING "${HEX}" ${START} 8 WORD_HEX)
    set(C_ARRAY "${C_ARRAY}0x${WORD_HEX},")
    math(EXPR I "${I} + 1")
endwhile()

file(WRITE "${OUTPUT}" "/* Auto-generated from ${INPUT} */\n")
file(APPEND "${OUTPUT}" "#pragma once\n#include <stddef.h>\n#include <stdint.h>\n\n")
file(APPEND "${OUTPUT}" "static const uint32_t ${SYMBOL}[] = {\n${C_ARRAY}\n};\n")
file(APPEND "${OUTPUT}" "static const size_t ${SYMBOL}_size = sizeof(${SYMBOL});\n")
