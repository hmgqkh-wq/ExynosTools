#!/usr/bin/env python3
# scripts/spv_to_header.py
# Usage: python3 scripts/spv_to_header.py input.spv out_header.h symbol_prefix
# Example: python3 scripts/spv_to_header.py build-android/generated_spv/bc1.spv build-android/generated_spv/bc1_shader.h bc1_shader

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
        h.write("/* Auto-generated from {} */\n".format(os.path.basename(spv_path)))
        h.write("#ifndef {0}_H_\n#define {0}_H_\n\n".format(array_name.upper()))
        h.write("#include <stdint.h>\n#include <stddef.h>\n\n")
        h.write(f"const uint32_t {array_name}[] = {{\n")
        for i in range(0, word_count, 8):
            chunk = words[i:i+8]
            line = ", ".join(f"0x{w:08x}" for w in chunk)
            if i + 8 < word_count:
                h.write(f"    {line},\n")
            else:
                h.write(f"    {line}\n")
        h.write("};\n\n")
        h.write(f"const size_t {len_name} = sizeof({array_name});\n\n")
        h.write("#endif\n")

def main():
    if len(sys.argv) != 4:
        print("usage: spv_to_header.py input.spv out.h symbol_prefix", file=sys.stderr)
        sys.exit(2)
    emit_header(sys.argv[1], sys.argv[2], sys.argv[3])

if __name__ == "__main__":
    main()
