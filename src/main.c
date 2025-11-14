// src/main.c
// Minimal entrypoint to satisfy linker in CI builds.
// Replace with your real main when integrating into the full runtime.

#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("ExynosTools (CI stub) - no runtime executed.");
    return 0;
}
