# ExynosTools - Custom Vulkan Driver for Xclipse 940

This driver enhances texture decoding (BC1-BC7), ray tracing, VRS for FPS, async compute for emulators on S24 FE.

**Installation**:
- Build with CMake.
- Copy libxeno_wrapper.so to Winlator's lib folder.
- Set EXYNOSTOOLS_HUD=1 for FPS display.

**Features**:
- BCn emulation with RDNA3 optimizations.
- Ray tracing for lighting.
- VRS for performance.

**Testing**:
- Run bc_test.c for validation.
