#!/usr/bin/env python3
# cmake/emit_spv_header.py
# SPDX-License-Identifier: MIT
# Usage: emit_spv_header.py <input.spv> <output.h> <symbol_name>
# Emits:
#   const unsigned char <symbol_name>[] = { ... bytes ... };
#   const size_t <symbol_name>_len = <size in bytes>;
import sys, os, struct

def die(msg, code=1):
    print(msg, file=sys.stderr)
    sys.exit(code)

if len(sys.argv) < 3:
    die("Usage: emit_spv_header.py <input.spv> <output.h> <symbol_name>", 2)

in_spv = sys.argv[1]
out_h = sys.argv[2]
sym = sys.argv[3] if len(sys.argv) >= 4 else None

if sym is None:
    base = os.path.splitext(os.path.basename(out_h))[0]
    sym = f"{base}_shader_spv"

if not os.path.isfile(in_spv):
    die(f"ERROR: Input SPV not found: {in_spv}", 3)

data = open(in_spv, "rb").read()
if len(data) == 0:
    die(f"ERROR: Input SPV empty: {in_spv}", 4)

# pad to 4 bytes for SPIR-V compliance, but emit full byte list (length reflects real bytes)
pad = (4 - (len(data) % 4)) % 4
if pad:
    data += b'\x00' * pad

guard = os.path.basename(out_h).upper().replace('.', '_').replace('-', '_') + "_"
size_sym = f"{sym}_len"

with open(out_h, "w", newline="\n") as fh:
    fh.write("// SPDX-License-Identifier: MIT\n")
    fh.write("/* Generated from %s. Do not edit. */\n\n" % os.path.basename(in_spv))
    fh.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
    fh.write("#include <stddef.h>\n#include <stdint.h>\n\n")
    fh.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
    # Emit bytes as unsigned char array (callers expecting const unsigned char* will match)
    fh.write("const unsigned char %s[] = {\n" % sym)
    # write bytes, 16 per line
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        fh.write("    " + ", ".join("0x%02x" % b for b in chunk))
        fh.write(",\n" if i + 16 < len(data) else "\n")
    fh.write("};\n\n")
    fh.write("const size_t %s = sizeof(%s);\n\n" % (size_sym, sym))
    fh.write("#ifdef __cplusplus\n}\n#endif\n\n")
    fh.write("#endif /* %s */\n" % guard)

sys.exit(0)
