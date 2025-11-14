#version 450

// Specialization constant ID 0 (matches CMake -DWORKGROUP_CONST_ID=0)
layout(constant_id = 0) const uint WG_SIZE = 64;

// Shared definitions for all BC shaders
layout(binding = 0) buffer SrcBC {
    uint bc_words[];
};

layout(binding = 1, rgba8) uniform writeonly image2D dst_rgba;

uvec2 dispatch_size(uvec2 size_px) {
    // Compute number of groups (X,Y) based on WG_SIZE
    uint groupsX = (size_px.x + WG_SIZE - 1u) / WG_SIZE;
    uint groupsY = (size_px.y + WG_SIZE - 1u) / WG_SIZE;
    return uvec2(groupsX, groupsY);
}

// Push constants: width, height, groupsX, reserved, wg_size, scale_100
layout(push_constant) uniform PushData {
    uint width;
    uint height;
    uint groupsX;
    uint reserved0;
    uint wg_size;
    uint scale100;
} PC;

vec4 debug_color(uvec2 px) {
    // Simple, deterministic pattern for validation
    float fx = float(px.x % 256u) / 255.0;
    float fy = float(px.y % 256u) / 255.0;
    float s  = float(PC.scale100) / 100.0;
    return vec4(fx * s, fy * s, 0.5 * s, 1.0);
}

// Write one pixel if within bounds
void write_pixel(ivec2 p) {
    if (uint(p.x) < PC.width && uint(p.y) < PC.height) {
        imageStore(dst_rgba, p, debug_color(uvec2(p)));
    }
}
