/*
obs-whitebalance
Copyright (C) 2026 obs-whitebalance contributors

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
*/

#include "wb-filter.h"
#include "wb-math.h"

#include <graphics/vec3.h>
#include <util/bmem.h>
#include <util/threading.h>
#include <plugin-support.h>

#include <string.h>

struct wb_filter {
	obs_source_t *context;

	gs_effect_t *effect;
	gs_eparam_t *p_image;
	gs_eparam_t *p_gain;

	/* written on the UI thread in update(), read on the render thread;
	 * three independent floats, a torn read is harmless for one frame */
	struct vec3 gain;

	gs_texrender_t *input_tr;
	gs_texrender_t *snap_tr;
	gs_stagesurf_t *stage;
	bool stage_pending;
	bool snapshot_logged;

	volatile bool snapshot_requested;

	pthread_mutex_t snap_mutex;
	uint8_t *snap_data;
	uint32_t snap_w, snap_h;
	uint64_t snap_seq;
};

static const enum gs_color_space supported_spaces[] = {
	GS_CS_SRGB,
	GS_CS_SRGB_16F,
	GS_CS_709_EXTENDED,
};

/* ------------------------------------------------------------------------- */

static const char *wb_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("WhiteBalance.Name");
}

static void wb_update(void *data, obs_data_t *settings)
{
	struct wb_filter *f = data;
	struct wb_rgb g = wb_gains_from_params(obs_data_get_double(settings, S_TEMPERATURE),
					       obs_data_get_double(settings, S_TINT));
	vec3_set(&f->gain, (float)g.r, (float)g.g, (float)g.b);
}

static void wb_destroy(void *data)
{
	struct wb_filter *f = data;

	obs_enter_graphics();
	gs_effect_destroy(f->effect);
	gs_texrender_destroy(f->input_tr);
	gs_texrender_destroy(f->snap_tr);
	gs_stagesurface_destroy(f->stage);
	obs_leave_graphics();

	pthread_mutex_destroy(&f->snap_mutex);
	bfree(f->snap_data);
	bfree(f);
}

static void *wb_create(obs_data_t *settings, obs_source_t *context)
{
	struct wb_filter *f = bzalloc(sizeof(*f));
	f->context = context;
	vec3_set(&f->gain, 1.0f, 1.0f, 1.0f);

	if (pthread_mutex_init(&f->snap_mutex, NULL) != 0) {
		bfree(f);
		return NULL;
	}

	char *path = obs_module_file("white_balance.effect");
	obs_enter_graphics();
	char *errors = NULL;
	f->effect = path ? gs_effect_create_from_file(path, &errors) : NULL;
	if (f->effect) {
		f->p_image = gs_effect_get_param_by_name(f->effect, "image");
		f->p_gain = gs_effect_get_param_by_name(f->effect, "gain");
	}
	obs_leave_graphics();

	if (!f->effect) {
		obs_log(LOG_ERROR, "failed to load effect '%s': %s", path ? path : "(null)",
			errors ? errors : "unknown error");
	}
	bfree(errors);
	bfree(path);

	if (!f->effect) {
		wb_destroy(f);
		return NULL;
	}

	wb_update(f, settings);
	return f;
}

static void wb_get_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, S_TEMPERATURE, 0.0);
	obs_data_set_default_double(settings, S_TINT, 0.0);
	obs_data_set_default_int(settings, S_SAMPLE_SIZE, 5);
}

/* ------------------------------------------------------------------------- */
/* properties                                                                */

static bool wb_pick_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	struct wb_filter *f = data;
	return wb_picker_run(f->context, f);
}

static bool wb_reset_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
	UNUSED_PARAMETER(props);
	UNUSED_PARAMETER(p);
	struct wb_filter *f = data;

	obs_data_t *s = obs_data_create();
	obs_data_set_double(s, S_TEMPERATURE, 0.0);
	obs_data_set_double(s, S_TINT, 0.0);
	obs_source_update(f->context, s);
	obs_data_release(s);
	return true;
}

static obs_properties_t *wb_get_properties(void *data)
{
	obs_properties_t *props = obs_properties_create();

	obs_properties_add_button2(props, "pick_button", obs_module_text("WhiteBalance.Pick"), wb_pick_clicked,
				   data);

	obs_property_t *list = obs_properties_add_list(props, S_SAMPLE_SIZE, obs_module_text("WhiteBalance.SampleSize"),
						       OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
	static const int sizes[] = {1, 3, 5, 9, 15, 31};
	for (size_t i = 0; i < OBS_COUNTOF(sizes); i++) {
		char label[32];
		snprintf(label, sizeof(label), "%d x %d px", sizes[i], sizes[i]);
		obs_property_list_add_int(list, label, sizes[i]);
	}
	obs_property_set_long_description(list, obs_module_text("WhiteBalance.SampleSize.Desc"));

	obs_properties_t *wb = obs_properties_create();
	obs_property_t *t = obs_properties_add_float_slider(wb, S_TEMPERATURE,
							    obs_module_text("WhiteBalance.Temperature"),
							    WB_PARAM_MIN, WB_PARAM_MAX, 0.1);
	obs_property_set_long_description(t, obs_module_text("WhiteBalance.Temperature.Desc"));
	obs_property_t *m = obs_properties_add_float_slider(wb, S_TINT, obs_module_text("WhiteBalance.Tint"),
							    WB_PARAM_MIN, WB_PARAM_MAX, 0.1);
	obs_property_set_long_description(m, obs_module_text("WhiteBalance.Tint.Desc"));
	obs_properties_add_button2(wb, "reset_button", obs_module_text("WhiteBalance.Reset"), wb_reset_clicked,
				   data);
	obs_properties_add_group(props, "wb_group", obs_module_text("WhiteBalance.Group"), OBS_GROUP_NORMAL, wb);

	return props;
}

/* ------------------------------------------------------------------------- */
/* rendering                                                                 */

static enum gs_color_space wb_get_color_space(void *data, size_t count, const enum gs_color_space *preferred)
{
	UNUSED_PARAMETER(count);
	UNUSED_PARAMETER(preferred);
	struct wb_filter *f = data;
	return obs_source_get_color_space(obs_filter_get_target(f->context), OBS_COUNTOF(supported_spaces),
					  supported_spaces);
}

/* Copy last frame's staged snapshot to CPU memory (map one frame late so the
 * GPU copy has completed and we don't stall the pipeline). */
static void wb_read_stage(struct wb_filter *f)
{
	if (!f->stage_pending)
		return;
	f->stage_pending = false;

	uint8_t *ptr = NULL;
	uint32_t linesize = 0;
	if (!gs_stagesurface_map(f->stage, &ptr, &linesize))
		return;

	const uint32_t w = gs_stagesurface_get_width(f->stage);
	const uint32_t h = gs_stagesurface_get_height(f->stage);
	const size_t row = (size_t)w * 4;

	pthread_mutex_lock(&f->snap_mutex);
	if (f->snap_w != w || f->snap_h != h || !f->snap_data) {
		bfree(f->snap_data);
		f->snap_data = bmalloc(row * h);
		f->snap_w = w;
		f->snap_h = h;
	}
	if (linesize == row) {
		memcpy(f->snap_data, ptr, row * h);
	} else {
		for (uint32_t y = 0; y < h; y++)
			memcpy(f->snap_data + row * y, ptr + (size_t)linesize * y, row);
	}
	f->snap_seq++;
	pthread_mutex_unlock(&f->snap_mutex);

	gs_stagesurface_unmap(f->stage);
}

static void wb_take_snapshot(struct wb_filter *f, gs_texture_t *input, uint32_t cx, uint32_t cy,
			     bool input_is_linear)
{
	if (!f->snap_tr)
		f->snap_tr = gs_texrender_create(GS_RGBA, GS_ZS_NONE);
	gs_texrender_reset(f->snap_tr);

	if (!gs_texrender_begin(f->snap_tr, cx, cy))
		return;

	const bool prev_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(false);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);

	gs_effect_set_texture(f->p_image, input);
	const char *tech = input_is_linear ? "SnapshotLinear" : "Snapshot";
	while (gs_effect_loop(f->effect, tech))
		gs_draw_sprite(input, 0, cx, cy);

	gs_blend_state_pop();
	gs_enable_framebuffer_srgb(prev_srgb);
	gs_texrender_end(f->snap_tr);

	if (!f->stage || gs_stagesurface_get_width(f->stage) != cx || gs_stagesurface_get_height(f->stage) != cy) {
		gs_stagesurface_destroy(f->stage);
		f->stage = gs_stagesurface_create(cx, cy, GS_RGBA);
	}
	if (!f->stage)
		return;

	gs_stage_texture(f->stage, gs_texrender_get_texture(f->snap_tr));
	f->stage_pending = true;

	if (!f->snapshot_logged) {
		f->snapshot_logged = true;
		obs_source_t *parent = obs_filter_get_parent(f->context);
		obs_log(LOG_INFO, "eyedropper capturing '%s' (%s): %ux%u, %s input", obs_source_get_name(parent),
			obs_source_get_id(parent), cx, cy, input_is_linear ? "linear" : "sRGB");
	}
	os_atomic_set_bool(&f->snapshot_requested, false);
}

static void wb_video_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(unused);
	struct wb_filter *f = data;

	obs_source_t *target = obs_filter_get_target(f->context);
	obs_source_t *parent = obs_filter_get_parent(f->context);
	if (!target || !parent) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	const uint32_t cx = obs_source_get_base_width(target);
	const uint32_t cy = obs_source_get_base_height(target);
	if (!cx || !cy) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	wb_read_stage(f);

	const enum gs_color_space space =
		obs_source_get_color_space(target, OBS_COUNTOF(supported_spaces), supported_spaces);
	const enum gs_color_format format = gs_get_format_from_space(space);

	/* 1. render the filter input (upstream chain) into our own texture,
	 *    exactly as obs_source_process_filter_begin would, so that we can
	 *    both correct it and snapshot it */
	if (f->input_tr && gs_texrender_get_format(f->input_tr) != format) {
		gs_texrender_destroy(f->input_tr);
		f->input_tr = NULL;
	}
	if (!f->input_tr)
		f->input_tr = gs_texrender_create(format, GS_ZS_NONE);
	gs_texrender_reset(f->input_tr);

	if (!gs_texrender_begin_with_color_space(f->input_tr, cx, cy, space)) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	const uint32_t parent_flags = obs_source_get_output_flags(parent);
	const bool custom_draw = (parent_flags & OBS_SOURCE_CUSTOM_DRAW) != 0;
	const bool async = (parent_flags & OBS_SOURCE_ASYNC) != 0;
	struct vec4 clear_color;
	vec4_zero(&clear_color);

	gs_blend_state_push();
	gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	gs_clear(GS_CLEAR_COLOR, &clear_color, 0.0f, 0);
	gs_ortho(0.0f, (float)cx, 0.0f, (float)cy, -100.0f, 100.0f);
	if (target == parent && !custom_draw && !async)
		obs_source_default_render(target);
	else
		obs_source_video_render(target);
	gs_blend_state_pop();
	gs_texrender_end(f->input_tr);

	gs_texture_t *tex = gs_texrender_get_texture(f->input_tr);
	if (!tex) {
		obs_source_skip_video_filter(f->context);
		return;
	}

	/* 2. eyedropper snapshot of the uncorrected input */
	if (os_atomic_load_bool(&f->snapshot_requested) && !f->stage_pending)
		wb_take_snapshot(f, tex, cx, cy, space != GS_CS_SRGB);

	/* 3. draw corrected output (mirrors libobs render_filter_tex) */
	const bool linear_srgb = gs_get_linear_srgb();
	const bool linear = linear_srgb || space != GS_CS_SRGB;

	const bool prev_srgb = gs_framebuffer_srgb_enabled();
	gs_enable_framebuffer_srgb(linear_srgb);
	if (linear_srgb)
		gs_effect_set_texture_srgb(f->p_image, tex);
	else
		gs_effect_set_texture(f->p_image, tex);
	gs_effect_set_vec3(f->p_gain, &f->gain);

	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(f->effect, linear ? "DrawLinear" : "Draw"))
		gs_draw_sprite(tex, 0, cx, cy);
	gs_blend_state_pop();

	gs_enable_framebuffer_srgb(prev_srgb);
}

/* ------------------------------------------------------------------------- */
/* snapshot API for the picker dialog                                        */

void wb_filter_request_snapshot(struct wb_filter *f)
{
	os_atomic_set_bool(&f->snapshot_requested, true);
}

bool wb_filter_copy_snapshot(struct wb_filter *f, uint64_t *seq, wb_snapshot_cb cb, void *param)
{
	bool copied = false;
	pthread_mutex_lock(&f->snap_mutex);
	if (f->snap_data && f->snap_seq != *seq) {
		cb(param, f->snap_data, f->snap_w, f->snap_h, f->snap_w * 4);
		*seq = f->snap_seq;
		copied = true;
	}
	pthread_mutex_unlock(&f->snap_mutex);
	return copied;
}

/* ------------------------------------------------------------------------- */

struct obs_source_info wb_filter_info = {
	.id = WB_FILTER_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_SRGB,
	.get_name = wb_get_name,
	.create = wb_create,
	.destroy = wb_destroy,
	.update = wb_update,
	.get_defaults = wb_get_defaults,
	.get_properties = wb_get_properties,
	.video_render = wb_video_render,
	.video_get_color_space = wb_get_color_space,
};
