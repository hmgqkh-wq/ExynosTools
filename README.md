# ExynosTools (Xclipse 940 focused)

This repository contains a focused, non-fallback implementation targeting Samsung Xclipse 940 GPU for BC1..BC7 decode pipelines and related tooling.

Structure:
- src/ - C sources
- include/ - headers
- scripts/ - helper scripts (spv -> header)
- assets/shaders/src/ - GLSL compute shader sources (bc1..bc7)
- .github/workflows/build.yml - CI workflow that compiles shaders, converts to headers, downloads NDK, and builds for Android arm64.

CI expectations:
- The workflow requires glslangValidator to compile GLSL to SPIR-V.
- The workflow downloads Android NDK r25b in CI and sets ANDROID_NDK_HOME.
- The build is strict: missing shader source files cause failure.

For local development on Android (Termux) or Linux, see scripts/termux_setup.sh for a helper.
