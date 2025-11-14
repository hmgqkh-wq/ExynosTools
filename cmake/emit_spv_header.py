#!/usr/bin/env python3
# cmake/emit_spv_header.py
# SPDX-License-Identifier: MIT
# Usage: emit_spv_header.py <input.spv> <output.h> <symbol_name>
# Emits:
#   static const uint32_t <symbol_name>[] = { ... };
#   static const size_t <symbol_name>_len = sizeof(<symbol_name>);
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
    # derive symbol from output filename if caller omitted it
    base = os.path.splitext(os.path.basename(out_h))[0]
    sym = f"{base}_shader_spv"
else:
    # ensure symbol follows expected pattern if user passed base only
    if not sym.endswith("_shader_spv"):
        sym = f"{sym}"

if not os.path.isfile(in_spv):
    die(f"ERROR: Input SPV not found: {in_spv}", 3)

data = open(in_spv, "rb").read()
if len(data) == 0:
    die(f"ERROR: Input SPV empty: {in_spv}", 4)

# pad to 4 bytes
pad = (4 - (len(data) % 4)) % 4
if pad:
    data += b'\x00' * pad

words = struct.unpack("<" + "I" * (len(data) // 4), data)
guard = os.path.basename(out_h).upper().replace('.', '_').replace('-', '_') + "_"

# Create size symbol name with _len suffix
size_sym = f"{sym}_len"

with open(out_h, "w", newline="\n") as fh:
    fh.write("// SPDX-License-Identifier: MIT\n")
    fh.write("/* Generated from %s. Do not edit. */\n\n" % os.path.basename(in_spv))
    fh.write("#ifndef %s\n#define %s\n\n" % (guard, guard))
    fh.write("#include <stddef.h>\n#include <stdint.h>\n\n")
    fh.write("#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n")
    # Export as non-static so other translation units can link to it if expected.
    fh.write("const uint32_t %s[] = {\n" % sym)
    for i in range(0, len(words), 8):
        chunk = words[i:i+8]
        fh.write("    " + ", ".join("0x%08xu" % w for w in chunk))
        fh.write(",\n" if i + 8 < len(words) else "\n")
    fh.write("};\n\n")
    fh.write("const size_t %s = sizeof(%s);\n\n" % (size_sym, sym))
    fh.write("#ifdef __cplusplus\n}\n#endif\n\n")
    fh.write("#endif /* %s */\n" % guard)

sys.exit(0)
