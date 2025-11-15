Build notes (drop-in):

1) Ensure include/ contains headers (example: include/xeno_log.h, include/rt_path.h, include/xeno_wrapper.h).
2) If using the provided logging implementation, src/logging.c must exist. CMake is configured to compile src/logging.c
   as the single non-inline implementation automatically; no additional steps required.
3) Clean build between attempts:
   rm -rf build-android && ./build_clean.sh build-android
4) For Android NDK cross-build, use build_android_example.sh:
   export ANDROID_NDK_PATH=/path/to/ndk
   ./build_android_example.sh build-android
5) If CI injects extra linker flags, ensure CMake preserves guards for AArch64-only flags in CMakeLists.txt.
