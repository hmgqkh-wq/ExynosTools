#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# Usage: emit_spv_header.py <input.spv> <output.h> <symbol_prefix>
import sys, os, struct

if len(sys.argv) < 3:
    print("Usage: emit_spv_header.py <input.spv> <output.h> <symbol_prefix>", file=sys.stderr)
    sys.exit(2)

in_spv, out_h = sys.argv[1], sys.argv[2]
sym = sys.argv[3] if len(sys.argv) >= 4 else "shader_spv"

if not os.path.isfile(in_spv):
    print(f"Input SPV not found: {in_spv}", file=sys.stderr)
    sys.exit(3)

data = open(in_spv, "rb").read()
pad = (4 - (len(data) % 4)) % 4
if pad: data += b'\x00' * pad
words = struct.unpack("<" + "I" * (len(data) // 4), data)
guard = os.path.basename(out_h).upper().replace('.', '_').replace('-', '_') + "_"

with open(out_h, "w", newline="\n") as fh:
    fh.write("// SPDX-License-Identifier: MIT\n")
    fh.write("/* Generated from %s. */\n\n" % os.path.basename(in_spv))
    fh.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
    fh.write("#include <stddef.h>\n#include <stdint.h>\n\n")
    fh.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
    fh.write("static const uint32_t %s[] = {\n" % sym)
    for i in range(0, len(words), 8):
        fh.write("    " + ", ".join(f"0x{w:08x}u" for w in words[i:i+8]))
        fh.write(",\n" if i + 8 < len(words) else "\n")
    fh.write("};\n\n")
    fh.write(f"static const size_t {sym}_size = sizeof({sym});\n\n")
    fh.write("#ifdef __cplusplus\n}\n#endif\n\n")
    fh.write(f"#endif /* {guard} */\n")
