#!/usr/bin/env python3
# cmake/emit_spv_header.py
# SPDX-License-Identifier: MIT
import sys, os, struct

def die(msg, code=1):
    print(msg, file=sys.stderr)
    sys.exit(code)

if len(sys.argv) < 3:
    die("Usage: emit_spv_header.py <input.spv> <output.h> <symbol_prefix>", 2)

in_spv = sys.argv[1]
out_h = sys.argv[2]
sym = sys.argv[3] if len(sys.argv) >= 4 else None

if not os.path.isfile(in_spv):
    die(f"ERROR: Input SPV not found: {in_spv}", 3)

data = open(in_spv, "rb").read()
if len(data) == 0:
    die(f"ERROR: Input SPV empty: {in_spv}", 4)

pad = (4 - (len(data) % 4)) % 4
if pad: data += b'\x00' * pad

if sym is None:
    base = os.path.splitext(os.path.basename(out_h))[0]
    sym = f"{base}_shader_spv"

size_sym = f"{sym}_len"
guard = os.path.basename(out_h).upper().replace('.', '_').replace('-', '_') + "_"

with open(out_h, "w", newline="\n") as fh:
    fh.write("// SPDX-License-Identifier: MIT\n")
    fh.write("/* Generated from %s. Do not edit. */\n\n" % os.path.basename(in_spv))
    fh.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
    fh.write("#include <stddef.h>\n#include <stdint.h>\n\n")
    fh.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
    fh.write("extern const unsigned char %s[];\n" % sym)
    fh.write("extern const size_t %s;\n\n" % size_sym)
    fh.write("#ifdef __cplusplus\n}\n#endif\n\n")
    fh.write("#endif /* %s */\n" % guard)

with open(out_h, "a", newline="\n") as fh:
    fh.write("\n/* Implementation: byte array and size */\n")
    fh.write("const unsigned char %s[] = {\n" % sym)
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        fh.write("    " + ", ".join("0x%02x" % b for b in chunk))
        fh.write(",\n" if i + 16 < len(data) else "\n")
    fh.write("};\n\n")
    fh.write("const size_t %s = sizeof(%s);\n" % (size_sym, sym))
