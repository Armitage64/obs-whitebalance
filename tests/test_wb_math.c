/* Unit tests for wb-math (no OBS dependency).
 * Build: cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests
 */

#include "wb-math.h"

#include <math.h>
#include <stdio.h>

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                     \
		if (!(cond)) {                                                   \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			failures++;                                              \
		}                                                                \
	} while (0)

#define NEAR(a, b, eps) (fabs((a) - (b)) <= (eps))

static void test_srgb_roundtrip(void)
{
	for (int i = 0; i <= 1000; i++) {
		double v = i / 1000.0;
		CHECK(NEAR(wb_linear_to_srgb(wb_srgb_to_linear(v)), v, 1e-9));
	}
	/* 18% gray is ~ sRGB 0.46 */
	CHECK(NEAR(wb_linear_to_srgb(0.18), 0.4614, 1e-3));
}

static void test_zero_is_identity(void)
{
	struct wb_rgb g = wb_gains_from_params(0, 0);
	CHECK(NEAR(g.r, 1, 1e-12) && NEAR(g.g, 1, 1e-12) && NEAR(g.b, 1, 1e-12));
}

static void test_signs(void)
{
	struct wb_rgb warm = wb_gains_from_params(50, 0);
	CHECK(warm.r > warm.g && warm.g > warm.b);
	struct wb_rgb cool = wb_gains_from_params(-50, 0);
	CHECK(cool.b > cool.g && cool.g > cool.r);
	struct wb_rgb magenta = wb_gains_from_params(0, 50);
	CHECK(magenta.g < magenta.r && magenta.g < magenta.b);
	struct wb_rgb green = wb_gains_from_params(0, -50);
	CHECK(green.g > green.r && green.g > green.b);
}

static void test_luma_preserved(void)
{
	for (int t = -100; t <= 100; t += 25)
		for (int m = -100; m <= 100; m += 25)
			CHECK(NEAR(wb_luma709(wb_gains_from_params(t, m)), 1.0, 1e-12));
}

static void test_roundtrip_params(void)
{
	for (int t = -90; t <= 90; t += 15) {
		for (int m = -90; m <= 90; m += 15) {
			/* a neutral gray seen through the inverse of (t, m) */
			struct wb_rgb g = wb_gains_from_params(t, m);
			/* 0.1 base keeps every corner case below the clip threshold */
			struct wb_rgb s = {0.1 / g.r, 0.1 / g.g, 0.1 / g.b};
			double T = 0, M = 0;
			CHECK(wb_params_from_sample(s, &T, &M) == WB_SOLVE_OK);
			CHECK(NEAR(T, t, 1e-9));
			CHECK(NEAR(M, m, 1e-9));
		}
	}
}

static void test_known_cast_neutralized(void)
{
	/* gray card shot with an arbitrary cast */
	const struct wb_rgb casts[] = {
		{1.4, 1.0, 0.7}, {0.7, 1.0, 1.4}, {1.0, 1.3, 1.0}, {1.1, 0.85, 1.2}, {1.25, 1.1, 0.8},
	};
	for (size_t i = 0; i < sizeof(casts) / sizeof(casts[0]); i++) {
		struct wb_rgb s = {0.18 * casts[i].r, 0.18 * casts[i].g, 0.18 * casts[i].b};
		double T = 0, M = 0;
		enum wb_solve_status st = wb_params_from_sample(s, &T, &M);
		CHECK(st == WB_SOLVE_OK);
		struct wb_rgb g = wb_gains_from_params(T, M);
		double r = s.r * g.r, gg = s.g * g.g, b = s.b * g.b;
		CHECK(NEAR(r, gg, 1e-4 * gg));
		CHECK(NEAR(b, gg, 1e-4 * gg));
	}
}

static void test_rejections(void)
{
	double T = 7, M = 8;
	struct wb_rgb dark = {0.001, 0.2, 0.2};
	CHECK(wb_params_from_sample(dark, &T, &M) == WB_SOLVE_TOO_DARK);
	CHECK(T == 7 && M == 8);
	struct wb_rgb clipped = {0.99, 0.5, 0.5};
	CHECK(wb_params_from_sample(clipped, &T, &M) == WB_SOLVE_CLIPPED);
	CHECK(T == 7 && M == 8);
	struct wb_rgb nan = {NAN, 0.2, 0.2};
	CHECK(wb_params_from_sample(nan, &T, &M) == WB_SOLVE_TOO_DARK);
}

static void test_clamp(void)
{
	/* R/B ratio of 1/32 => needs 5 stops => T = 250 -> clamped */
	struct wb_rgb s = {0.01, 0.1, 0.32};
	double T = 0, M = 0;
	CHECK(wb_params_from_sample(s, &T, &M) == WB_SOLVE_CLAMPED);
	CHECK(T == WB_PARAM_MAX);
	CHECK(M >= WB_PARAM_MIN && M <= WB_PARAM_MAX);
}

int main(void)
{
	test_srgb_roundtrip();
	test_zero_is_identity();
	test_signs();
	test_luma_preserved();
	test_roundtrip_params();
	test_known_cast_neutralized();
	test_rejections();
	test_clamp();
	if (failures) {
		fprintf(stderr, "%d failure(s)\n", failures);
		return 1;
	}
	printf("all wb-math tests passed\n");
	return 0;
}
