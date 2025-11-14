// Shared helpers, push constants, and storage types.
// NOTE: Do NOT compile this file directly. It is included by .comp shaders.

layout(set = 0, binding = 0) readonly buffer BCWords {
    uint bc_words[];
};

layout(push_constant) uniform PushConst {
    // Scale hints or flags (e.g., 200 == signed channels for BC4/5)
    uint scale100;
    // Workgroup specialization default; not used directly here
    uint wg_hint;
} PC;

// Specialization constant for workgroup size (set by pipeline specialization).
layout(constant_id = 0) const uint WG_SIZE = 64u;

// Utility: index helpers for a 4x4 texel block per BC block
uint block_index() {
    return gl_GlobalInvocationID.x;
}

uvec2 block_origin(uint bi) {
    // Each block maps to 4x4 pixels; caller defines tiling outside
    // Here we assume linear blocks laid out along X
    uint bx = bi * 4u;
    return uvec2(bx, 0u);
}

// Store function: write a rgba32f pixel to an image
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
    // Interpret v as signed with 'bits' width
    int smax = (1 << (bits - 1)) - 1;
    int sval = int(v);
    if ((sval & (1 << (bits - 1))) != 0) { // negative
        sval = sval - (1 << bits);
    }
    // Map to [-1, 1]
    float f = float(sval) / float(smax);
    return clamp(f, -1.0, 1.0);
}
