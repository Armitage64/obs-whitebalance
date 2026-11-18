/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "wb-math.h"

#include <math.h>

double wb_srgb_to_linear(double v)
{
	if (v <= 0.04045)
		return v / 12.92;
	return pow((v + 0.055) / 1.055, 2.4);
}

double wb_linear_to_srgb(double v)
{
	if (v <= 0.0031308)
		return v * 12.92;
	return 1.055 * pow(v, 1.0 / 2.4) - 0.055;
}

double wb_luma709(struct wb_rgb c)
{
	return 0.2126 * c.r + 0.7152 * c.g + 0.0722 * c.b;
}

static double clampd(double v, double lo, double hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

struct wb_rgb wb_gains_from_params(double temperature, double tint)
{
	const double t = WB_TEMP_SCALE * temperature;
	const double m = WB_TINT_SCALE * tint;

	struct wb_rgb g;
	g.r = exp2(t + m);
	g.b = exp2(-t + m);
	g.g = exp2(-2.0 * m);

	const double y = wb_luma709(g);
	g.r /= y;
	g.g /= y;
	g.b /= y;
	return g;
}

enum wb_solve_status wb_params_from_sample(struct wb_rgb s, double *temperature, double *tint)
{
	if (isnan(s.r) || isnan(s.g) || isnan(s.b))
		return WB_SOLVE_TOO_DARK;

	const double mn = fmin(s.r, fmin(s.g, s.b));
	const double mx = fmax(s.r, fmax(s.g, s.b));

	if (mn < WB_SAMPLE_MIN_LINEAR)
		return WB_SOLVE_TOO_DARK;
	if (mx >= WB_SAMPLE_MAX_LINEAR)
		return WB_SOLVE_CLIPPED;

	/* ideal neutralizing gains are 1/c (up to a common scale) */
	const double lr = -log2(s.r);
	const double lg = -log2(s.g);
	const double lb = -log2(s.b);

	const double t = (lr - lb) / (2.0 * WB_TEMP_SCALE);
	const double m = (lr + lb - 2.0 * lg) / (6.0 * WB_TINT_SCALE);

	const double tc = clampd(t, WB_PARAM_MIN, WB_PARAM_MAX);
	const double mc = clampd(m, WB_PARAM_MIN, WB_PARAM_MAX);

	*temperature = tc;
	*tint = mc;
	return (tc != t || mc != m) ? WB_SOLVE_CLAMPED : WB_SOLVE_OK;
}
