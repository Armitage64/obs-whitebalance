/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

/* Pure white-balance math. No OBS dependency so it can be unit tested.
 *
 * Model (all gains applied in linear light):
 *   log2 gR =  A*T + B*M
 *   log2 gB = -A*T + B*M
 *   log2 gG = -2*B*M
 * then normalized so that Rec.709 luminance of a neutral is preserved.
 *   T > 0 : warmer (more red, less blue)
 *   M > 0 : magenta (less green)
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#define WB_PARAM_MIN -100.0
#define WB_PARAM_MAX 100.0

/* +-100 temperature => +-2 stops R/B ratio; +-100 tint => +-2 stops G vs R+B */
#define WB_TEMP_SCALE (1.0 / 100.0)
#define WB_TINT_SCALE (1.0 / 150.0)

/* Rejection thresholds for the averaged (linear) sample */
#define WB_SAMPLE_MIN_LINEAR 0.003
#define WB_SAMPLE_MAX_LINEAR 0.95

struct wb_rgb {
	double r, g, b;
};

enum wb_solve_status {
	WB_SOLVE_OK = 0,
	WB_SOLVE_CLAMPED,  /* solved, but result exceeded +-100 and was clamped */
	WB_SOLVE_TOO_DARK, /* a channel is too close to black to be reliable */
	WB_SOLVE_CLIPPED,  /* a channel is at/near clipping */
};

double wb_srgb_to_linear(double v);
double wb_linear_to_srgb(double v);
double wb_luma709(struct wb_rgb c);

/* Temperature/tint -> luminance-preserving linear RGB gains. */
struct wb_rgb wb_gains_from_params(double temperature, double tint);

/* Averaged linear sample -> temperature/tint that make it neutral.
 * On TOO_DARK / CLIPPED the outputs are left untouched. */
enum wb_solve_status wb_params_from_sample(struct wb_rgb sample_linear, double *temperature, double *tint);

#ifdef __cplusplus
}
#endif
