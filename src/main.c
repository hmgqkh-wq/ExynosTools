// src/main.c
// Minimal entrypoint to satisfy linker in CI builds.

#include <stdio.h>

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    puts("ExynosTools (CI stub) - no runtime executed.");
    return 0;
}
