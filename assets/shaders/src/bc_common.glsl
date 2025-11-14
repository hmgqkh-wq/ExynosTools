#version 450
#extension GL_GOOGLE_include_directive : enable

float unpackAlpha(uint packed) {
    return float(packed) / 255.0;
}
