/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#pragma once

#include <obs-module.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WB_FILTER_ID "white_balance_eyedropper_filter"

#define S_TEMPERATURE "temperature"
#define S_TINT "tint"
#define S_SAMPLE_SIZE "sample_size"

struct wb_filter;

extern struct obs_source_info wb_filter_info;

/* Ask the render thread to capture the filter's (uncorrected) input. The
 * pixels become available one or two frames later via wb_filter_copy_snapshot. */
void wb_filter_request_snapshot(struct wb_filter *f);

typedef void (*wb_snapshot_cb)(void *param, const uint8_t *rgba, uint32_t width, uint32_t height, uint32_t linesize);

/* If a snapshot newer than *seq exists, invoke cb with it (under lock),
 * update *seq and return true. Pixels are non-linear sRGB RGBA8, opaque,
 * un-premultiplied. */
bool wb_filter_copy_snapshot(struct wb_filter *f, uint64_t *seq, wb_snapshot_cb cb, void *param);

/* Implemented in picker-dialog.cpp. Runs the modal eyedropper dialog.
 * Returns true if the properties view should be refreshed. */
bool wb_picker_run(obs_source_t *filter_source, struct wb_filter *f);

#ifdef __cplusplus
}
#endif
