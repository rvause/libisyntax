#include "libisyntax.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "third_party/stb_image_write.h"  // for png export

#define CHECK_LIBISYNTAX_OK(_libisyntax_call) do { \
    isyntax_error_t result = _libisyntax_call;     \
    assert(result == LIBISYNTAX_OK);               \
} while(0)

#define LOG_VAR(fmt, var) printf("%s: %s=" fmt "\n", __FUNCTION__, #var, var)

// This program reads an arbitrary rectangular region from an iSyntax file at a
// given pyramid level and writes it to a PNG. Coordinates and dimensions are
// expressed in pixels at the requested level.
//
// Usage:
//   isyntax_region <isyntax_file> <level> <x> <y> <width> <height> <output.png> [--postprocess]
//
// --postprocess applies viewer-equivalent image enhancement (CLAHE, sharpening,
// and scanner->sRGB color correction) using parameters embedded in the file.

int main(int argc, char** argv) {

    if (argc <= 1) {
        printf("Usage: %s <isyntax_file> <level> <x> <y> <width> <height> <output.png> [--postprocess]\n"
               "  Reads an arbitrary region at the given pyramid level into output.png.\n"
               "  x, y, width and height are in pixels at the requested level.\n"
               "  --postprocess  apply viewer-equivalent image enhancement\n",
               argv[0]);
        return 0;
    }

    // The --postprocess flag may appear anywhere among the arguments.
    bool enable_postprocess = false;
    int positional_index = 0;
    char* positional[7];
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--postprocess") == 0) {
            enable_postprocess = true;
        } else if (positional_index < 7) {
            positional[positional_index++] = argv[i];
        }
    }

    if (positional_index < 7) {
        printf("Usage: %s <isyntax_file> <level> <x> <y> <width> <height> <output.png> [--postprocess]\n", argv[0]);
        return 1;
    }

    char* filename = positional[0];
    int level = atoi(positional[1]);
    int64_t x = (int64_t)atoll(positional[2]);
    int64_t y = (int64_t)atoll(positional[3]);
    int64_t width = (int64_t)atoll(positional[4]);
    int64_t height = (int64_t)atoll(positional[5]);
    const char* output_png = positional[6];

    libisyntax_init();

    isyntax_t* isyntax;
    if (libisyntax_open(filename, /*flags=*/0, &isyntax) != LIBISYNTAX_OK) {
        printf("Failed to open %s\n", filename);
        return -1;
    }
    printf("Successfully opened %s\n", filename);

    if (enable_postprocess) {
        const isyntax_image_t* wsi_image = libisyntax_get_wsi_image(isyntax);
        CHECK_LIBISYNTAX_OK(libisyntax_image_set_postprocessing(isyntax, (isyntax_image_t*)wsi_image,
                                                                 LIBISYNTAX_POSTPROCESS_ALL));
    }

    LOG_VAR("%d", level);
    LOG_VAR("%lld", (long long)x);
    LOG_VAR("%lld", (long long)y);
    LOG_VAR("%lld", (long long)width);
    LOG_VAR("%lld", (long long)height);
    LOG_VAR("%s", output_png);

    const isyntax_image_t* wsi_image = libisyntax_get_wsi_image(isyntax);
    int32_t level_count = libisyntax_image_get_level_count(wsi_image);
    if (level < 0 || level >= level_count) {
        printf("Level %d is out of range (0..%d)\n", level, level_count - 1);
        libisyntax_close(isyntax);
        return 1;
    }

    const isyntax_level_t* requested_level = libisyntax_image_get_level(wsi_image, level);
    int32_t level_width = libisyntax_level_get_width(requested_level);
    int32_t level_height = libisyntax_level_get_height(requested_level);
    LOG_VAR("%d", level_width);
    LOG_VAR("%d", level_height);

    isyntax_cache_t* isyntax_cache = NULL;
    CHECK_LIBISYNTAX_OK(libisyntax_cache_create("region cache", 2000, &isyntax_cache));
    CHECK_LIBISYNTAX_OK(libisyntax_cache_inject(isyntax_cache, isyntax));

    // RGBA is what stbi expects.
    uint32_t* pixels_rgba = (uint32_t*)malloc((size_t)width * (size_t)height * 4);
    CHECK_LIBISYNTAX_OK(libisyntax_read_region(isyntax, isyntax_cache, level, x, y, width, height,
                                               pixels_rgba, LIBISYNTAX_PIXEL_FORMAT_RGBA));

    printf("Writing %s...\n", output_png);
    stbi_write_png(output_png, (int)width, (int)height, 4, pixels_rgba, (int)width * 4);
    printf("Done writing %s.\n", output_png);

    free(pixels_rgba);
    libisyntax_cache_destroy(isyntax_cache);
    libisyntax_close(isyntax);
    return 0;
}
