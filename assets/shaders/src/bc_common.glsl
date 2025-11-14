#version 450

#ifndef WORKGROUP_CONST_ID
#define WORKGROUP_CONST_ID 0
#endif

#ifndef WG_SIZE_DEFAULT
#define WG_SIZE_DEFAULT 64
#endif

layout(constant_id = WORKGROUP_CONST_ID) const uint WG_SIZE = WG_SIZE_DEFAULT;

layout(std430, binding = 0) readonly buffer SrcBC {
    uint bc_words[];
};

layout(binding = 1, rgba8) uniform writeonly image2D dst_rgba;

layout(push_constant) uniform PushData {
    uint width;
    uint height;
    uint groupsX;
    uint reserved0;
    uint wg_size;
    uint scale100;
} PC;

uint block_index() {
    return gl_GlobalInvocationID.x + gl_GlobalInvocationID.y * PC.groupsX;
}

uvec2 block_origin(uint bi) {
    uint rowBlocks = (PC.width + 3u) / 4u;
    uint by = bi / rowBlocks;
    uint bx = bi % rowBlocks;
    return uvec2(bx * 4u, by * 4u);
}

uint u5(uint v, uint sh) { return (v >> sh) & 31u; }
uint u6(uint v, uint sh) { return (v >> sh) & 63u; }
uint u8(uint v, uint sh) { return (v >> sh) & 255u; }

uint getBits2x(uint lo, uint hi, uint start, uint count) {
    if (start + count <= 32u) return (lo >> start) & ((1u << count) - 1u);
    uint loPart = lo >> start;
    uint hiPart = hi & ((1u << ((start + count) - 32u)) - 1u);
    return loPart | (hiPart << (32u - start));
}

float unormN(uint v, uint n) { return float(v) / float((1u << n) - 1u); }

float snormN(uint v, uint n) {
    int ival = int(v);
    if ((ival & (1 << (n - 1))) != 0) ival -= (1 << n);
    int maxv = (1 << (n - 1)) - 1;
    return clamp(float(ival) / float(maxv), -1.0, 1.0);
}

void store_px(ivec2 p, vec4 rgba) {
    if (uint(p.x) < PC.width && uint(p.y) < PC.height)
        imageStore(dst_rgba, p, clamp(rgba, vec4(0.0), vec4(1.0)));
}
