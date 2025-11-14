#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# cmake/emit_spv_header.py
# Usage: emit_spv_header.py <input.spv> <output.h> <symbol_prefix>
# Converts a binary .spv into a C header declaring and defining
# `static const uint32_t <symbol_prefix>[]` and `static const size_t <symbol_prefix>_size`.

import sys, os, struct

if len(sys.argv) != 4:
    print("Usage: emit_spv_header.py <input.spv> <output.h> <symbol_prefix>", file=sys.stderr)
    sys.exit(2)

in_spv = sys.argv[1]
out_h = sys.argv[2]
sym = sys.argv[3]

if not os.path.isfile(in_spv):
    print(f"Input SPV not found: {in_spv}", file=sys.stderr)
    sys.exit(3)

with open(in_spv, "rb") as f:
    data = f.read()

# Ensure length is multiple of 4 for uint32_t words
pad = (4 - (len(data) % 4)) % 4
if pad:
    data += b'\x00' * pad

words = struct.unpack("<" + "I" * (len(data) // 4), data)
word_count = len(words)
header_guard = os.path.basename(out_h).upper().replace('.', '_').replace('-', '_') + "_"

with open(out_h, "w", newline="\n") as fh:
    fh.write("// SPDX-License-Identifier: MIT\n")
    fh.write("/* Generated from %s. Do not edit by hand. */\n\n" % os.path.basename(in_spv))
    fh.write("#ifndef %s\n" % header_guard)
    fh.write("#define %s\n\n" % header_guard)
    fh.write("#include <stddef.h>\n")
    fh.write("#include <stdint.h>\n\n")
    fh.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
    fh.write("/* SPIR-V binary data as uint32_t array */\n")
    fh.write("static const uint32_t %s[] = {\n" % sym)
    for i in range(0, word_count, 8):
        line = words[i:i+8]
        fh.write("    " + ", ".join("0x%08xu" % w for w in line))
        fh.write(",\n" if i + 8 < word_count else "\n")
    fh.write("};\n\n")
    fh.write("static const size_t %s_size = sizeof(%s);\n\n" % (sym, sym))
    fh.write("#ifdef __cplusplus\n}\n#endif\n\n")
    fh.write("#endif /* %s */\n" % header_guard)

sys.exit(0)
