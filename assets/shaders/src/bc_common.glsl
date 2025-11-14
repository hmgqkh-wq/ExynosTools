#version 450

vec4 unpackColor(uint packed) {
    float r = float((packed >> 0) & 0xFF) / 255.0;
    float g = float((packed >> 8) & 0xFF) / 255.0;
    float b = float((packed >> 16) & 0xFF) / 255.0;
    float a = float((packed >> 24) & 0xFF) / 255.0;
    return vec4(r, g, b, a);
}

float unpackAlpha(uint packed) {
    return float(packed) / 255.0;
}

// Add wave intrinsic for subgroup min (RDNA3 optimization)
float waveMin(float val) {
    return subgroupMin(val);  // Reduces sync cost
}

// Add half-precision helper
half4 unpackColorHalf(uint packed) {
    half r = half((packed >> 0) & 0xFF) / 255.0h;
    half g = half((packed >> 8) & 0xFF) / 255.0h;
    half b = half((packed >> 16) & 0xFF) / 255.0h;
    half a = half((packed >> 24) & 0xFF) / 255.0h;
    return half4(r, g, b, a);
}
