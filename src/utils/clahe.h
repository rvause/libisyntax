#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Scratch buffers needed by clahe_apply, sized for a given grid and bin count.
// Use clahe_scratch_sizes() to determine the sizes, then allocate the buffers from
// whatever allocator is appropriate (e.g. the tile-load arena).
typedef struct {
    float* cdfs;      // size: n_tiles * nr_bins * sizeof(float)
    int*   hist;      // size: nr_bins * sizeof(int)
} clahe_scratch_t;

void clahe_scratch_sizes(int grid_x, int grid_y, int nr_bins,
                         size_t* out_cdfs_size, size_t* out_hist_size);

// Apply Contrast-Limited Adaptive Histogram Equalization (CLAHE) to a float luma
// buffer in-place. Values should be in [0, 1]. width/height are tile dimensions.
// clip_limit, nr_bins, context_dimension come from the iSyntax DPImagePostProcessing
// metadata. scratch must be pre-filled by the caller (see clahe_scratch_sizes).
void clahe_apply(float* luma, int width, int height,
                 float clip_limit, int nr_bins, int context_dimension,
                 clahe_scratch_t scratch);

#ifdef __cplusplus
}
#endif
