#version 450

// Specialization constant ID 0 (matches pipeline and CMake)
layout(constant_id = 0) const uint WG_SIZE = 64;

// Descriptor layout
layout(std430, binding = 0) readonly buffer SrcBC {
    uint bc_words[];
};
layout(binding = 1, rgba8) uniform writeonly image2D dst_rgba;

// Push constants: width, height, groupsX, reserved, wg_size, scale_100
layout(push_constant) uniform PushData {
    uint width;
    uint height;
    uint groupsX;
    uint reserved0;
    uint wg_size;
    uint scale100;
} PC;

// Helpers
uint u8(uint w, uint byteIdx) {
    return (w >> (byteIdx * 8u)) & 0xffu;
}
uint u5(uint v, uint shift) { return (v >> shift) & 31u; }
uint u6(uint v, uint shift) { return (v >> shift) & 63u; }

float u8_to_float(uint v) { return float(v) / 255.0; }
float u5_to_float(uint v) { return float(v) / 31.0; }
float u6_to_float(uint v) { return float(v) / 63.0; }

vec4 clamp01(vec4 c) { return clamp(c, vec4(0.0), vec4(1.0)); }

// Write one pixel if in bounds
void store_px(ivec2 p, vec4 rgba) {
    if (uint(p.x) < PC.width && uint(p.y) < PC.height) {
        imageStore(dst_rgba, p, clamp01(rgba));
    }
}

// Get 4x4 block origin (in pixels) from linear block index
// Blocks are 4x4 pixels; rowBlocks = ceil(width / 4)
uvec2 block_origin(uint blockIndex) {
    uint rowBlocks = (PC.width + 3u) / 4u;
    uint by = blockIndex / rowBlocks;
    uint bx = blockIndex % rowBlocks;
    return uvec2(bx * 4u, by * 4u);
}
