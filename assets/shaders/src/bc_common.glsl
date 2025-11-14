#extension GL_GOOGLE_include_directive : enable

// No #version here — only in .comp files.
// Shared helpers tuned for mobile GPUs: branch-free inner loops, simple math.

// Normalize an 8-bit channel to [0,1].
float norm8(uint v) { return float(v) * (1.0 / 255.0); }

// Decode 5-bit to [0,1] with 31 as max
float norm5(uint v) { return float(v) * (1.0 / 31.0); }

// Decode 6-bit to [0,1] with 63 as max
float norm6(uint v) { return float(v) * (1.0 / 63.0); }

// Compute 4-color palette for BC1 (RGB565 endpoints, 2-bit indices).
// bc1ModeOpaque: true -> 4-color; false -> 3-color + transparent.
void bc1Palette(uvec2 endpoints, out vec3 pal[4], out bool opaque)
{
    // endpoints packed as: [lo=endpoints.x, hi=endpoints.y], each 16-bit RGB565
    uint c0 = endpoints.x & 0xFFFFu;
    uint c1 = (endpoints.x >> 16) & 0xFFFFu; // if stored as two 16-bit in endpoints.x; else adapt
    // Many bitstreams store as 32-bit: low 16 C0, high 16 C1. If yours uses endpoints.xy, adapt.

    // Extract RGB565
    uint r0 = (c0 >> 11) & 0x1Fu;
    uint g0 = (c0 >> 5)  & 0x3Fu;
    uint b0 =  c0        & 0x1Fu;
    uint r1 = (c1 >> 11) & 0x1Fu;
    uint g1 = (c1 >> 5)  & 0x3Fu;
    uint b1 =  c1        & 0x1Fu;

    vec3 C0 = vec3(norm5(r0), norm6(g0), norm5(b0));
    vec3 C1 = vec3(norm5(r1), norm6(g1), norm5(b1));

    opaque = (c0 > c1); // standard BC1 rule: c0 > c1 => 4-color; else 3-color+alpha
    pal[0] = C0;
    pal[1] = C1;
    pal[2] = (2.0 * C0 + 1.0 * C1) * (1.0 / 3.0);
    pal[3] = (1.0 * C0 + 2.0 * C1) * (1.0 / 3.0);
}

// Compute 8 alpha values from endpoints (BC2/BC3/BC4/BC5 style interpolation).
void alpha8(float a0, float a1, out float alphas[8])
{
    alphas[0] = a0;
    alphas[1] = a1;
    alphas[2] = (6.0 * a0 + 1.0 * a1) * (1.0 / 7.0);
    alphas[3] = (5.0 * a0 + 2.0 * a1) * (1.0 / 7.0);
    alphas[4] = (4.0 * a0 + 3.0 * a1) * (1.0 / 7.0);
    alphas[5] = (3.0 * a0 + 4.0 * a1) * (1.0 / 7.0);
    alphas[6] = (2.0 * a0 + 5.0 * a1) * (1.0 / 7.0);
    alphas[7] = (1.0 * a0 + 6.0 * a1) * (1.0 / 7.0);
}

// Extract a 2-bit/3-bit index from a 32/48-bit index field (packed little-endian per texel).
uint idx2(uint indices, uint texel) { return (indices >> (texel * 2u)) & 0x3u; }
uint idx3(uint indices, uint texel) { return (indices >> (texel * 3u)) & 0x7u; }
