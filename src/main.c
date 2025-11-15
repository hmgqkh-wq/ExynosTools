#include "logging.h"
#include "xeno_bc.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    logging_info("ExynosTools starting (Xclipse 940 non-fallback)");
    // In production, initialize Vulkan and run decode tests.
    return 0;
}
