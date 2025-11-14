#extension GL_GOOGLE_include_directive : enable

// Shared helpers. No #version here (only in .comp files).
float unpackAlpha(uint packed) {
    return float(packed) / 255.0;
}
