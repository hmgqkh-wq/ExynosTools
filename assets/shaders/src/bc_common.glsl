// Include-only shared helpers.
// Do NOT compile this file directly.

layout(set = 0, binding = 0) readonly buffer BCWords {
    uint bc_words[];
};

layout(push_constant) uniform PushConst {
    uint scale100; // 200 => signed channels
    uint wg_hint;
} PC;

// Map invocation to block index (1 thread per 4x4 block)
uint block_index() {
    return gl_GlobalInvocationID.x;
}

// Each block starts at (block*4, 0) for simplicity
uvec2 block_origin(uint bi) {
    return uvec2(bi * 4u, 0u);
}

// Output image
layout(set = 0, binding = 1, rgba32f) uniform writeonly image2D out_img;

void store_px(ivec2 pos, vec4 rgba) {
    imageStore(out_img, pos, rgba);
}

// Normalization helpers
float unormN(uint v, uint bits) {
    uint maxv = (bits >= 32u) ? 0xFFFFFFFFu : ((1u << bits) - 1u);
    return float(v) / float(maxv);
}

float snormN(uint v, uint bits) {
    int smax = (1 << (bits - 1)) - 1;
    int sval = int(v);
    if ((sval & (1 << (bits - 1))) != 0) {
        sval = sval - (1 << bits);
    }
    float f = float(sval) / float(smax);
    return clamp(f, -1.0, 1.0);
}

// Read 3-bit index starting at bit position 'pos' from a 48-bit stream in two uints:
// lo holds bits [0..31], hi holds bits [32..47] (use low 16 bits).
uint read3_48(uint lo, uint hi16, uint pos) {
    if (pos <= 29u) {
        return (lo >> pos) & 7u;
    } else {
        // Crosses into hi
        uint lbits = 32u - pos;            // 1..3 bits from lo
        uint l = (lo >> pos) & ((1u << lbits) - 1u);
        uint r = hi16 & ((1u << (3u - lbits)) - 1u);
        return (r << lbits) | l;
    }
}

// Read 4-bit index in a 64-bit stream packed into two uints (lo, hi)
uint read4_64(uint lo, uint hi, uint pos) {
    if (pos <= 28u) {
        return (lo >> pos) & 0xFu;
    } else {
        uint lbits = 32u - pos; // 1..4
        uint l = (lo >> pos) & ((1u << lbits) - 1u);
        uint r = hi & ((1u << (4u - lbits)) - 1u);
        return (r << lbits) | l;
    }
}

// Read 2-bit index in a 32-bit word
uint read2_32(uint w, uint pos) {
    return (w >> pos) & 3u;
}
