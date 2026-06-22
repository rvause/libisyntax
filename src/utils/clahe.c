#include "clahe.h"
#include <string.h>
#include <math.h>

void clahe_scratch_sizes(int grid_x, int grid_y, int nr_bins,
                         size_t* out_cdfs_size, size_t* out_hist_size) {
    if (out_cdfs_size) *out_cdfs_size = (size_t)grid_x * grid_y * nr_bins * sizeof(float);
    if (out_hist_size) *out_hist_size = (size_t)nr_bins * sizeof(int);
}

void clahe_apply(float* luma, int width, int height,
                 float clip_limit, int nr_bins, int context_dimension,
                 clahe_scratch_t scratch) {
    if (width <= 0 || height <= 0 || nr_bins <= 0 || context_dimension <= 0) return;

    int grid_x = width / context_dimension;
    int grid_y = height / context_dimension;
    if (grid_x < 2) grid_x = 2;
    if (grid_y < 2) grid_y = 2;

    int tile_w = width / grid_x;
    int tile_h = height / grid_y;
    if (tile_w < 1) tile_w = 1;
    if (tile_h < 1) tile_h = 1;

    int n_tiles = grid_x * grid_y;
    float* cdfs = scratch.cdfs;
    int* hist = scratch.hist;

    float clip_threshold = clip_limit * (float)(tile_w * tile_h) / nr_bins;

    for (int ty = 0; ty < grid_y; ty++) {
        for (int tx = 0; tx < grid_x; tx++) {
            int tile_idx = ty * grid_x + tx;
            float* cdf = cdfs + (size_t)tile_idx * nr_bins;

            memset(hist, 0, nr_bins * sizeof(int));
            int x_start = tx * tile_w;
            int y_start = ty * tile_h;
            int x_end = (tx == grid_x - 1) ? width : x_start + tile_w;
            int y_end = (ty == grid_y - 1) ? height : y_start + tile_h;

            int actual_pixels = 0;
            for (int y = y_start; y < y_end; y++) {
                for (int x = x_start; x < x_end; x++) {
                    float v = luma[y * width + x];
                    int bin = (int)(v * (nr_bins - 1) + 0.5f);
                    if (bin < 0) bin = 0;
                    if (bin >= nr_bins) bin = nr_bins - 1;
                    hist[bin]++;
                    actual_pixels++;
                }
            }

            // Clip and redistribute excess
            int total_excess = 0;
            int clip_int = (int)(clip_threshold + 0.5f);
            for (int b = 0; b < nr_bins; b++) {
                if (hist[b] > clip_int) {
                    total_excess += hist[b] - clip_int;
                    hist[b] = clip_int;
                }
            }
            int redist_per_bin = total_excess / nr_bins;
            int leftover = total_excess - redist_per_bin * nr_bins;
            for (int b = 0; b < nr_bins; b++) {
                hist[b] += redist_per_bin;
                if (b < leftover) hist[b]++;
            }

            // CDF, normalized to [0, 1]
            int cumsum = 0;
            for (int b = 0; b < nr_bins; b++) {
                cumsum += hist[b];
                cdf[b] = (actual_pixels > 0) ? (float)cumsum / actual_pixels : 0.0f;
            }
        }
    }

    // Bilinear-interpolated mapping
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float v = luma[y * width + x];
            int bin = (int)(v * (nr_bins - 1) + 0.5f);
            if (bin < 0) bin = 0;
            if (bin >= nr_bins) bin = nr_bins - 1;

            float fx = ((float)x + 0.5f) / tile_w - 0.5f;
            float fy = ((float)y + 0.5f) / tile_h - 0.5f;

            int tx0 = (int)floorf(fx);
            int ty0 = (int)floorf(fy);
            int tx1 = tx0 + 1;
            int ty1 = ty0 + 1;

            if (tx0 < 0) { tx0 = 0; }
            if (ty0 < 0) { ty0 = 0; }
            if (tx1 < 0) { tx1 = 0; }
            if (ty1 < 0) { ty1 = 0; }
            if (tx0 >= grid_x) tx0 = grid_x - 1;
            if (ty0 >= grid_y) ty0 = grid_y - 1;
            if (tx1 >= grid_x) tx1 = grid_x - 1;
            if (ty1 >= grid_y) ty1 = grid_y - 1;

            float ax = fx - floorf(fx);
            float ay = fy - floorf(fy);
            if (ax < 0) ax = 0; if (ax > 1) ax = 1;
            if (ay < 0) ay = 0; if (ay > 1) ay = 1;

            const float* cdf00 = cdfs + (size_t)(ty0 * grid_x + tx0) * nr_bins;
            const float* cdf10 = cdfs + (size_t)(ty0 * grid_x + tx1) * nr_bins;
            const float* cdf01 = cdfs + (size_t)(ty1 * grid_x + tx0) * nr_bins;
            const float* cdf11 = cdfs + (size_t)(ty1 * grid_x + tx1) * nr_bins;

            float v0 = cdf00[bin] * (1 - ax) + cdf10[bin] * ax;
            float v1 = cdf01[bin] * (1 - ax) + cdf11[bin] * ax;
            luma[y * width + x] = v0 * (1 - ay) + v1 * ay;
        }
    }
}
