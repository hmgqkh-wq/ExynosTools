#!/usr/bin/env python3
# Non-executable required: run via python3 scripts/spv_to_header.py input.spv out.h symbol_prefix
import sys, os, struct

def emit_header(spv_path, hdr_path, sym_prefix):
    if not os.path.isfile(spv_path):
        raise FileNotFoundError(spv_path)
    with open(spv_path, "rb") as f:
        data = f.read()
    if len(data) % 4 != 0:
        raise ValueError("SPIR-V length not multiple of 4: " + spv_path)
    word_count = len(data) // 4
    words = struct.unpack("<{}I".format(word_count), data)
    array_name = f"{sym_prefix}_spv"
    len_name = f"{sym_prefix}_spv_len"
    os.makedirs(os.path.dirname(hdr_path), exist_ok=True)
    with open(hdr_path, "w", newline="\n") as h:
        h.write("/* Auto-generated from {} - do not edit manually */\n".format(os.path.basename(spv_path)))
        guard = (array_name + "_H_").upper()
        h.write("#ifndef {0}\n#define {0}\n\n".format(guard))
        h.write("#include <stdint.h>\n#include <stddef.h>\n\n")
        h.write("static const uint32_t {0}[] = {{\n".format(array_name))
        for i in range(0, word_count, 8):
            chunk = words[i:i+8]
            line = ", ".join("0x{0:08x}".format(w) for w in chunk)
            if i + 8 < word_count:
                h.write("    " + line + ",\n")
            else:
                h.write("    " + line + "\n")
        h.write("};\n\n")
        h.write("static const size_t {0} = sizeof({1});\n\n".format(len_name, array_name))
        h.write("#endif /* {0} */\n".format(guard))

def main():
    if len(sys.argv) != 4:
        print("usage: spv_to_header.py input.spv out.h symbol_prefix", file=sys.stderr)
        sys.exit(2)
    try:
        emit_header(sys.argv[1], sys.argv[2], sys.argv[3])
    except Exception as e:
        print("Error:", e, file=sys.stderr)
        sys.exit(1)

if __name__ == "__main__":
    main()
