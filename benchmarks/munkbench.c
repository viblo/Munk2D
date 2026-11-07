/* Copyright (c) 2026 Victor Blomqvist
 * Copyright (c) 2007-2024 Scott Lembcke and Howling Moon Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "chipmunk/chipmunk.h"

#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef MUNK2D_VERSION
#define MUNK2D_VERSION "unknown"
#endif

#define FPS 60.0

#ifndef CP_PI
#define CP_PI 3.14159265358979323846264338327950288
#endif

typedef struct Benchmark Benchmark;

typedef cpSpace *(*BenchmarkInitFunc)(int size, void **state);
typedef void (*BenchmarkUpdateFunc)(cpSpace *space, void *state, cpFloat dt);
typedef void (*BenchmarkDestroyStateFunc)(void *state);
typedef void (*BenchmarkPrintMetricsFunc)(void *state);

struct Benchmark {
	const char *name;
	int steps;
	int extended_size;
	int smoke_size;
	int reference_size;
	int size_end;
	int size_inc;
	int smoke_batch;
	int reference_batch;
	BenchmarkInitFunc init;
	BenchmarkUpdateFunc update;
	BenchmarkDestroyStateFunc destroy_state;
	BenchmarkPrintMetricsFunc print_metrics;
};

typedef struct TumblerState {
	int count;
	int target_count;
} TumblerState;

typedef struct CallbackState {
	uint64_t begin_count;
	uint64_t pre_solve_count;
	uint64_t post_solve_count;
	uint64_t separate_count;
	uint64_t wildcard_count;
} CallbackState;

typedef struct SleepWakeState {
	cpBody *wake_body;
	int step;
	int wake_step;
} SleepWakeState;

typedef enum ConstraintKind {
	CONSTRAINT_SLIDE,
	CONSTRAINT_PIVOT,
	CONSTRAINT_GROOVE,
	CONSTRAINT_GEAR,
	CONSTRAINT_RATCHET,
	CONSTRAINT_ROTARY_LIMIT,
	CONSTRAINT_DAMPED_SPRING,
	CONSTRAINT_DAMPED_ROTARY_SPRING,
	CONSTRAINT_PIN,
	CONSTRAINT_SIMPLE_MOTOR,
	CONSTRAINT_KIND_COUNT,
} ConstraintKind;

typedef struct ConstraintBenchState {
	ConstraintKind kind;
	int count;
	cpConstraint **constraints;
	cpFloat peak_error;
	cpFloat peak_impulse;
} ConstraintBenchState;

typedef struct ConstraintMixState {
	int count;
	int step;
	cpConstraint **constraints;
	ConstraintKind *kinds;
	cpBody **dynamic_bodies;
	cpBody *wake_body;
	cpBool wake_body_was_sleeping;
	cpBool wake_body_is_awake;
	cpFloat peak_error[CONSTRAINT_KIND_COUNT];
	cpFloat peak_impulse[CONSTRAINT_KIND_COUNT];
} ConstraintMixState;

typedef enum BenchmarkProfile {
	BENCHMARK_PROFILE_SMOKE,
	BENCHMARK_PROFILE_REFERENCE,
	BENCHMARK_PROFILE_EXTENDED,
} BenchmarkProfile;

typedef struct RunResult {
	const char *benchmark;
	int size;
	int sample;
	int batch;
	double init_time;
	double run_time;
} RunResult;

static cpContactSolverType benchmarkContactSolver = CP_CONTACT_SOLVER_BASELINE;

static double
now_seconds(void)
{
	return (double)clock()/(double)CLOCKS_PER_SEC;
}

static void
usage(const char *argv0)
{
	printf("MunkBench - Munk2D benchmark suite\n\n");
	printf("Usage: %s [-b benchmark ...] [--profile smoke|reference|extended] [-s size] [--warmup n --samples n --batch n] [--summary-json] [--svg file --step n]\n\n", argv0);
	printf("Options:\n");
	printf("  -b, --benchmarks  Run only the named benchmarks.\n");
	printf("  --profile NAME    Size profile: smoke, reference (default), or extended.\n");
	printf("  --contact-solver  baseline (default), avx2 or wasm-simd. Unavailable backend is an error.\n");
	printf("  -s, --size        Explicit size override; use -1 for a size sweep.\n");
	printf("  --warmup n        Untimed independent runs before sampling. Default: 0.\n");
	printf("  --samples n       Number of raw timing samples to emit. Default: 1.\n");
	printf("  --batch n|auto    Independent simulations measured per sample. Default: 1.\n");
	printf("  --summary-json    Emit validation checkpoint summaries as JSON instead of timing CSV.\n");
	printf("  --checkpoints     Comma-separated steps for --summary-json; accepts 'final'. Runs only to the last checkpoint.\n");
	printf("  --svg file        Write an SVG snapshot for a single benchmark. Use '-' for stdout.\n");
	printf("  --step n          Step to render for --svg. Default: benchmark final step.\n");
	printf("  -h, --help        Show this help.\n");
}

static cpSpace *
new_space(cpFloat gx, cpFloat gy)
{
	cpSpace *space = cpSpaceNew();
	if(benchmarkContactSolver != CP_CONTACT_SOLVER_BASELINE && !cpSpaceSetContactSolver(space, benchmarkContactSolver)){
		fprintf(stderr, "Unable to select requested contact solver.\n");
		cpSpaceFree(space);
		exit(1);
	}
	cpSpaceSetGravity(space, cpv(gx, gy));
	return space;
}

static cpBody *
add_dynamic_body(cpSpace *space, cpVect position)
{
	(void)space;
	cpBody *body = cpBodyNew(0.0, 0.0);
	cpBodySetPosition(body, position);
	return body;
}

static cpBody *
add_static_body(cpSpace *space, cpVect position)
{
	cpBody *body = cpSpaceAddBody(space, cpBodyNewStatic());
	cpBodySetPosition(body, position);
	return body;
}

static cpShape *
add_shape_with_density(cpSpace *space, cpShape *shape, cpFloat density)
{
	cpBody *body = cpShapeGetBody(shape);

	if(density > 0.0){
		cpFloat old_mass = cpBodyGetMass(body);
		cpFloat old_moment = cpBodyGetMoment(body);
		cpVect old_cog = cpBodyGetCenterOfGravity(body);

		cpShapeSetDensity(shape, density);
		cpFloat shape_mass = cpShapeGetMass(shape);
		cpFloat shape_moment = cpShapeGetMoment(shape);
		cpVect shape_cog = cpShapeGetCenterOfGravity(shape);

		cpShapeSetMass(shape, 0.0);

		if(shape_mass > 0.0){
			cpFloat new_mass = old_mass + shape_mass;
			cpVect new_cog = (old_mass > 0.0 ? cpvlerp(old_cog, shape_cog, shape_mass/new_mass) : shape_cog);
			cpFloat new_moment = shape_moment;
			if(old_mass > 0.0){
				new_moment += old_moment + cpvdistsq(old_cog, shape_cog)*(shape_mass*old_mass)/new_mass;
			}

			cpVect position = cpBodyGetPosition(body);
			cpBodySetCenterOfGravity(body, new_cog);
			cpBodySetMass(body, new_mass);
			cpBodySetMoment(body, new_moment);
			cpBodySetPosition(body, position);
		}
	}

	if(cpBodyGetSpace(body) == NULL) cpSpaceAddBody(space, body);
	return cpSpaceAddShape(space, shape);
}

static cpShape *
add_box_shape(cpSpace *space, cpBody *body, cpFloat width, cpFloat height, cpFloat density)
{
	return add_shape_with_density(space, cpBoxShapeNew(body, width, height, 0.0), density);
}

static cpShape *
add_box_shape_bb(cpSpace *space, cpBody *body, cpFloat l, cpFloat b, cpFloat r, cpFloat t, cpFloat density)
{
	cpFloat left = cpfmin(l, r);
	cpFloat right = cpfmax(l, r);
	cpFloat bottom = cpfmin(b, t);
	cpFloat top = cpfmax(b, t);
	return add_shape_with_density(space, cpBoxShapeNew2(body, cpBBNew(left, bottom, right, top), 0.0), density);
}

static cpShape *
add_circle_shape(cpSpace *space, cpBody *body, cpFloat radius, cpVect offset, cpFloat density)
{
	return add_shape_with_density(space, cpCircleShapeNew(body, radius, offset), density);
}

static cpShape *
add_poly_shape(cpSpace *space, cpBody *body, int count, const cpVect *verts, cpFloat density)
{
	return add_shape_with_density(space, cpPolyShapeNew(body, count, verts, cpTransformIdentity, 0.0), density);
}

static cpShape *
add_static_segment(cpSpace *space, cpVect a, cpVect b, cpFloat radius)
{
	return cpSpaceAddShape(space, cpSegmentShapeNew(cpSpaceGetStaticBody(space), a, b, radius));
}

/* Simple growable pointer array used to collect bodies/shapes/constraints
 * during space teardown. We cannot remove children while iterating the
 * space, and post-step callbacks would never fire (no step runs after
 * teardown), so we gather pointers first and free them afterwards. */
typedef struct PtrArray {
	void **items;
	size_t count;
	size_t capacity;
} PtrArray;

static void
ptr_array_init(PtrArray *array)
{
	array->items = NULL;
	array->count = 0;
	array->capacity = 0;
}

static void
ptr_array_push(PtrArray *array, void *item)
{
	if(array->count == array->capacity){
		size_t new_capacity = array->capacity ? array->capacity*2 : 64;
		void **resized = (void **)realloc(array->items, new_capacity*sizeof(void *));
		if(!resized){
			fprintf(stderr, "Out of memory.\n");
			exit(1);
		}
		array->items = resized;
		array->capacity = new_capacity;
	}
	array->items[array->count++] = item;
}

static void
ptr_array_dispose(PtrArray *array)
{
	free(array->items);
	array->items = NULL;
	array->count = 0;
	array->capacity = 0;
}

static void
collect_shape(cpShape *shape, void *data)
{
	ptr_array_push((PtrArray *)data, shape);
}

static void
collect_constraint(cpConstraint *constraint, void *data)
{
	ptr_array_push((PtrArray *)data, constraint);
}

static void
collect_body(cpBody *body, void *data)
{
	ptr_array_push((PtrArray *)data, body);
}

static void
free_space_children(cpSpace *space)
{
	PtrArray shapes, constraints, bodies;
	ptr_array_init(&shapes);
	ptr_array_init(&constraints);
	ptr_array_init(&bodies);

	cpSpaceEachBody(space, collect_body, &bodies);
	/* cpSpaceEachConstraint visits only active constraints. Wake every sleeping
	 * component before collecting them, including bodies without shapes.
	 * Snapshot bodies first: activation changes the space's body lists. */
	for(size_t i = 0; i < bodies.count; i++){
		cpBodyActivate((cpBody *)bodies.items[i]);
	}
	cpSpaceEachShape(space, collect_shape, &shapes);
	cpSpaceEachConstraint(space, collect_constraint, &constraints);

	for(size_t i = 0; i < shapes.count; i++){
		cpShape *shape = (cpShape *)shapes.items[i];
		cpSpaceRemoveShape(space, shape);
		cpShapeFree(shape);
	}
	for(size_t i = 0; i < constraints.count; i++){
		cpConstraint *constraint = (cpConstraint *)constraints.items[i];
		cpSpaceRemoveConstraint(space, constraint);
		cpConstraintFree(constraint);
	}
	for(size_t i = 0; i < bodies.count; i++){
		cpBody *body = (cpBody *)bodies.items[i];
		/* The space's embedded static body lives inside the cpSpace struct
		 * and must never be removed or freed. */
		if(body == cpSpaceGetStaticBody(space)) continue;
		cpSpaceRemoveBody(space, body);
		cpBodyFree(body);
	}

	ptr_array_dispose(&shapes);
	ptr_array_dispose(&constraints);
	ptr_array_dispose(&bodies);
}

static void
default_update(cpSpace *space, void *state, cpFloat dt)
{
	(void)state;
	cpSpaceStep(space, dt);
}

static void
add_pair_update(cpSpace *space, void *state, cpFloat dt)
{
	(void)state;
	for(int i = 0; i < 4; i++) cpSpaceStep(space, dt/4.0);
}

static cpSpace *
init_falling_squares(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpBodySetPosition(cpSpaceGetStaticBody(space), cpv(0.0, -10.0));
	cpSpaceAddShape(space, cpBoxShapeNew(cpSpaceGetStaticBody(space), 100.0*2.0, 3.0*2.0, 0.0));

	for(int i = 0; i < 15; i++){
		cpFloat a = 0.5 + (cpFloat)i/15.0*2.5;
		for(int j = 0; j < size; j++){
			cpBody *body = add_dynamic_body(space, cpv(i*7.0 - 30.0, 2.0*a*(size - j)));
			add_box_shape(space, body, a*2.0, a*2.0, 5.0);
		}
	}

	return space;
}

static cpSpace *
init_falling_circles(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpBodySetPosition(cpSpaceGetStaticBody(space), cpv(0.0, -10.0));
	cpSpaceAddShape(space, cpBoxShapeNew(cpSpaceGetStaticBody(space), 100.0*2.0, 3.0*2.0, 0.0));

	for(int i = 0; i < 15; i++){
		cpFloat a = 0.5 + (cpFloat)i/15.0*2.5;
		for(int j = 0; j < size; j++){
			cpBody *body = add_dynamic_body(space, cpv(i*7.0 + j*0.25 - 100.0, 2.0*a*(size - j)));
			add_circle_shape(space, body, a/2.0, cpvzero, 5.0);
		}
	}

	return space;
}

static void
tumbler_update(cpSpace *space, void *state, cpFloat dt)
{
	TumblerState *tumbler = (TumblerState *)state;
	cpSpaceStep(space, dt);

	if(tumbler->count < tumbler->target_count){
		cpBody *body = add_dynamic_body(space, cpv(0.0, 10.0));
		add_box_shape(space, body, 0.125*2.0, 0.125*2.0, 1.0);
		tumbler->count++;
	}
}

static void
tumbler_destroy_state(void *state)
{
	free(state);
}

static cpSpace *
init_tumbler(int size, void **state)
{
	cpSpace *space = new_space(0.0, -10.0);

	TumblerState *tumbler = (TumblerState *)calloc(1, sizeof(TumblerState));
	if(!tumbler){
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	tumbler->target_count = size;
	*state = tumbler;

	cpBody *body = add_dynamic_body(space, cpv(0.0, 10.0));
	add_shape_with_density(space, cpSegmentShapeNew(body, cpv(-10.0, 10.0), cpv(10.0, 10.0), 0.5), 5.0);
	add_shape_with_density(space, cpSegmentShapeNew(body, cpv(10.0, 10.0), cpv(10.0, -10.0), 0.5), 5.0);
	add_shape_with_density(space, cpSegmentShapeNew(body, cpv(10.0, -10.0), cpv(-10.0, -10.0), 0.5), 5.0);
	add_shape_with_density(space, cpSegmentShapeNew(body, cpv(-10.0, -10.0), cpv(-10.0, 10.0), 0.5), 5.0);

	cpSpaceAddConstraint(space, cpSimpleMotorNew(cpSpaceGetStaticBody(space), body, 0.05*CP_PI));
	cpSpaceAddConstraint(space, cpPinJointNew(cpSpaceGetStaticBody(space), body, cpv(0.0, 10.0), cpv(0.0, 0.0)));

	return space;
}

static cpSpace *
init_add_pair(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, 0.0);
	cpFloat min_x = -9.0/10.0;
	cpFloat max_x = 9.0/10.0;
	cpFloat min_y = 4.0/10.0;
	cpFloat max_y = 6.0/10.0;

	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv(
			min_x + (max_x - min_x)*(cpFloat)i/(cpFloat)size,
			min_y + (max_y - min_y)*(cpFloat)(i % 32)/32.0
		));
		add_circle_shape(space, body, 0.1/10.0, cpvzero, 0.01/10.0);
	}

	cpBody *body = add_dynamic_body(space, cpv(-40.0/10.0, 5.0/10.0));
	add_box_shape(space, body, 1.5*2.0/10.0, 1.5*2.0/10.0, 1.0/10.0);
	cpBodySetVelocity(body, cpv(175.0/10.0, 0.0));

	return space;
}

static cpSpace *
init_mild_n2(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	add_static_segment(space, cpv(-50.0, 0.0), cpv(50.0, 0.0), 1.0);

	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv(-5.0, -1.0));
		add_box_shape_bb(space, body, -3.0, 3.0, 3.0, 4.0, 2.0);
		add_box_shape_bb(space, body, -3.0, 4.0, -2.0, 0.0, 2.0);
		add_box_shape_bb(space, body, 2.0, 4.0, 3.0, 0.0, 0.0);
	}

	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv(15.0, 1.0));
		cpVect left[] = {cpv(-2.0, 0.0), cpv(1.0, 2.0), cpv(0.0, 4.0)};
		cpVect right[] = {cpv(2.0, 0.0), cpv(-1.0, 2.0), cpv(0.0, 4.0)};
		add_poly_shape(space, body, 3, left, 2.0);
		add_poly_shape(space, body, 3, right, 2.0);
	}

	return space;
}

static cpSpace *
init_n2(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv(i*0.01, -i*0.01));
		add_circle_shape(space, body, 1.0, cpvzero, 1.0);
	}
	return space;
}

static cpSpace *
init_multifixture(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	add_static_segment(space, cpv(-35.0, 0.0), cpv(35.0, 0.0), 1.0);
	add_static_segment(space, cpv(-36.0, 50.0), cpv(-36.0, 0.0), 1.0);
	add_static_segment(space, cpv(36.0, 50.0), cpv(36.0, 0.0), 1.0);

	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv(-20.0 + (i % 6)*7.0 + (cpFloat)i/10.0, 1.0 + ((cpFloat)i/6.0)*5.0));
		int c = 50 - i/2;
		for(int z = 0; z < c; z++){
			cpFloat bs = 2.0/(cpFloat)c;
			cpFloat ps = 2.0*z*bs + bs;
			add_box_shape_bb(space, body, -bs + ps - 2.0, 3.0, bs + ps - 2.0, 4.0, 2.0/(cpFloat)c);
			add_box_shape_bb(space, body, -2.0, bs + ps, -1.0, -bs + ps, 2.0/(cpFloat)c);
			add_box_shape_bb(space, body, 1.0, bs + ps, 2.0, -bs + ps, 0.0);
		}
	}

	return space;
}

static cpSpace *
init_mostly_static_single_body(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpFloat a = 0.5;
	cpBodySetPosition(cpSpaceGetStaticBody(space), cpv(0.0, -a));
	int n = size;
	int m = size;
	cpVect position = cpv(0.0, 0.0);

	for(int j = 0; j < m; j++){
		position = cpv(-n*a, position.y);
		for(int i = 0; i < n; i++){
			if(abs(j - i) > 3){
				cpSpaceAddShape(space, cpBoxShapeNew2(cpSpaceGetStaticBody(space), cpBBNew(position.x - a, position.y - a, position.x + a, position.y + a), 0.0));
			} else if(i == j){
				cpBody *body = add_dynamic_body(space, position);
				add_circle_shape(space, body, a*2.0, cpvzero, 1.0);
			}
			position = cpvadd(position, cpv(2.0*a, 0.0));
		}
		position = cpvsub(position, cpv(0.0, 2.0*a));
	}

	return space;
}

static cpSpace *
init_mostly_static_multi_body(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpFloat a = 0.5;
	int n = size;
	int m = size;
	cpVect position = cpv(0.0, 0.0);

	for(int j = 0; j < m; j++){
		position = cpv(-n*a, position.y);
		for(int i = 0; i < n; i++){
			if(abs(j - i) > 3){
				cpBody *body = add_static_body(space, position);
				cpSpaceAddShape(space, cpBoxShapeNew(body, a*2.0, a*2.0, 0.0));
			} else if(i == j){
				cpBody *body = add_dynamic_body(space, position);
				add_circle_shape(space, body, a*2.0, cpvzero, 1.0);
			}
			position = cpvadd(position, cpv(2.0*a, 0.0));
		}
		position = cpvsub(position, cpv(0.0, 2.0*a));
	}

	return space;
}

static cpSpace *
init_diagonal(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpFloat a = 0.5;
	int n = size;
	cpFloat m = (cpFloat)size/2.0;
	cpVect position = cpv(0.0, 0.0);

	for(int j = 0; j < (int)m; j++){
		position = cpv(-n*a, position.y);
		for(int i = 0; i < n; i++){
			cpBody *body = add_static_body(space, position);
			cpBodySetAngle(body, CP_PI/4.0);
			cpSpaceAddShape(space, cpBoxShapeNew(body, a, (3.0*j + 1.0)*a, 0.0));
			position = cpvadd(position, cpv(8.0*a, 0.0));
		}
		position = cpvsub(position, cpv(0.0, 8.0*a));
	}

	/* Pymunk's reference scene hardcodes 3000 dynamic circles regardless of
	 * size. Here we scale the dynamic count with size so the benchmark size
	 * actually controls both halves of the scene; 60*size preserves the
	 * original 3000-body count at the default size of 50. */
	int dynamic_count = 60*size;
	for(int i = 0; i < dynamic_count; i++){
		cpBody *body = add_dynamic_body(space, cpv(((cpFloat)i/15.0)*2.0 - 75.0, (cpFloat)(i % 15)*2.0 + 50.0));
		add_circle_shape(space, body, 0.5, cpvzero, 1.0);
	}

	return space;
}

static cpSpace *
init_mixed_static_dynamic(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	int n = 150;
	int m = 150;
	cpVect cntr = cpv((cpFloat)m/2.0, (cpFloat)n/2.0);
	cpFloat a = 0.5;

	for(int j = 0; j < m; j++){
		for(int i = 0; i < n; i++){
			cpVect pos = cpv((cpFloat)i, (cpFloat)j);
			cpVect delta = cpvsub(pos, cntr);
			if(cpvdot(delta, delta) > 67.0*67.0){
				cpSpaceAddShape(space, cpCircleShapeNew(cpSpaceGetStaticBody(space), a, pos));
			}
		}
	}

	for(int i = 0; i < size; i++){
		cpFloat s = (cpFloat)i/(cpFloat)size;
		cpVect pos = cpv(cos(s*30.0)*(s*50.0 + 10.0), sin(s*30.0)*(s*50.0 + 10.0));
		cpBody *body = add_dynamic_body(space, cpvadd(pos, cntr));
		cpBodySetVelocity(body, pos);
		add_circle_shape(space, body, a, cpvzero, 0.5);
	}

	return space;
}

static cpBody *
big_mobile_add_node(cpSpace *space, cpBody *parent, cpVect local_anchor, int depth, int max_depth, cpFloat offset, cpFloat a)
{
	cpFloat density = 20.0;
	cpVect h = cpv(0.0, a);
	cpVect p = cpvsub(cpvadd(cpBodyGetPosition(parent), local_anchor), h);
	cpBody *body = add_dynamic_body(space, p);
	add_box_shape(space, body, 0.25*a*2.0, a*2.0, density + p.x*0.02);

	if(depth == max_depth) return body;

	add_box_shape_bb(space, body, -offset, -0.25*a - a, offset, 0.25*a - a, density);

	cpVect a1 = cpv(offset, -a);
	cpVect a2 = cpv(-offset, -a);
	cpBody *body1 = big_mobile_add_node(space, body, a1, depth + 1, max_depth, 0.5*offset, a);
	cpBody *body2 = big_mobile_add_node(space, body, a2, depth + 1, max_depth, 0.5*offset, a);

	cpConstraint *j1 = cpSpaceAddConstraint(space, cpPinJointNew(body, body1, a1, h));
	cpConstraint *j2 = cpSpaceAddConstraint(space, cpPinJointNew(body, body2, a2, h));
	cpConstraintSetCollideBodies(j1, cpFalse);
	cpConstraintSetCollideBodies(j2, cpFalse);

	return body;
}

static cpSpace *
init_big_mobile(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -10.0);
	cpBodySetPosition(cpSpaceGetStaticBody(space), cpv(0.0, 20.0));
	cpFloat a = 0.25;
	cpVect h = cpv(0.0, a);
	cpBody *root = big_mobile_add_node(space, cpSpaceGetStaticBody(space), cpvzero, 0, size, 200.0, a);
	cpSpaceAddConstraint(space, cpPinJointNew(cpSpaceGetStaticBody(space), root, cpvzero, h));
	return space;
}

static cpSpace *
init_slow_explosion(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, 0.0);
	for(int i = 0; i < size; i++){
		cpFloat s = (cpFloat)i*30.0/(cpFloat)size;
		cpVect pos = cpv(cos(s*30.0)*(s*30.0 + 5.0), sin(s*30.0)*(s*30.0 + 5.0));
		cpBody *body = add_dynamic_body(space, pos);
		cpBodySetVelocity(body, cpvmult(cpBodyGetPosition(body), 0.2));
		add_circle_shape(space, body, 0.5, cpvzero, 0.5);
	}
	return space;
}

static cpSpace *
init_frictional_pyramid(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -100.0);
	cpShape *ground = add_static_segment(space, cpv(-50.0, 0.0), cpv(50.0, 0.0), 0.5);
	cpShapeSetFriction(ground, 0.8);

	for(int row = 0; row < size; row++){
		for(int col = 0; col < size - row; col++){
			cpBody *body = add_dynamic_body(space, cpv((cpFloat)col + 0.5*(cpFloat)row - 0.5*(cpFloat)size, 0.55 + 1.05*(cpFloat)row));
			cpShape *shape = add_box_shape(space, body, 0.9, 0.9, 1.0);
			cpShapeSetFriction(shape, 0.8);
		}
	}
	return space;
}

static void callback_begin(cpArbiter *arb, cpSpace *space, cpDataPointer data)
{
	(void)arb; (void)space;
	((CallbackState *)data)->begin_count++;
}

static void callback_pre_solve(cpArbiter *arb, cpSpace *space, cpDataPointer data)
{
	(void)space;
	CallbackState *callback = (CallbackState *)data;
	callback->pre_solve_count++;
	cpArbiterSetProcessCollision(arb, (callback->pre_solve_count % 11) != 0);
	if((callback->pre_solve_count % 3) == 0) cpArbiterSetFriction(arb, 0.35);
}

static void callback_post_solve(cpArbiter *arb, cpSpace *space, cpDataPointer data)
{
	(void)arb; (void)space;
	((CallbackState *)data)->post_solve_count++;
}

static void callback_separate(cpArbiter *arb, cpSpace *space, cpDataPointer data)
{
	(void)arb; (void)space;
	((CallbackState *)data)->separate_count++;
}

static void callback_wildcard(cpArbiter *arb, cpSpace *space, cpDataPointer data)
{
	(void)arb; (void)space;
	((CallbackState *)data)->wildcard_count++;
}

static void callback_destroy_state(void *state)
{
	free(state);
}

static void callback_print_metrics(void *state)
{
	CallbackState *callback = (CallbackState *)state;
	printf(",\"benchmark_metrics\":{\"begin_count\":%" PRIu64 ",\"pre_solve_count\":%" PRIu64
		",\"post_solve_count\":%" PRIu64 ",\"separate_count\":%" PRIu64
		",\"wildcard_count\":%" PRIu64 "}",
		callback->begin_count, callback->pre_solve_count, callback->post_solve_count,
		callback->separate_count, callback->wildcard_count);
}

static cpSpace *
init_collision_callbacks(int size, void **state)
{
	cpSpace *space = new_space(0.0, -30.0);
	CallbackState *callback = (CallbackState *)calloc(1, sizeof(CallbackState));
	if(!callback){fprintf(stderr, "Out of memory.\n"); exit(1);}
	*state = callback;

	cpShape *ground = add_static_segment(space, cpv(-50.0, 0.0), cpv(50.0, 0.0), 0.5);
	cpShapeSetFriction(ground, 0.6);
	cpShapeSetElasticity(ground, 0.6);
	cpShapeSetCollisionType(ground, 2);
	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv((cpFloat)(i % 12) - 6.0, 2.0 + 1.2*(cpFloat)(i/12)));
		cpShape *shape = (i % 2 == 0 ? add_circle_shape(space, body, 0.45, cpvzero, 1.0) : add_box_shape(space, body, 0.8, 0.8, 1.0));
		cpShapeSetFriction(shape, 0.6);
		cpShapeSetElasticity(shape, 0.6);
		cpShapeSetCollisionType(shape, (i % 3 == 0 ? 1 : 3));
	}

	cpCollisionHandler *specific = cpSpaceAddCollisionHandler(space, 1, 2);
	specific->beginFunc = callback_begin;
	specific->preSolveFunc = callback_pre_solve;
	specific->postSolveFunc = callback_post_solve;
	specific->separateFunc = callback_separate;
	specific->userData = callback;
	cpCollisionHandler *wildcard = cpSpaceAddWildcardHandler(space, 3);
	wildcard->postSolveFunc = callback_wildcard;
	wildcard->userData = callback;
	return space;
}

static cpSpace *
init_surface_velocity(int size, void **state)
{
	(void)state;
	cpSpace *space = new_space(0.0, -100.0);
	cpShape *ground = add_static_segment(space, cpv(-60.0, 0.0), cpv(60.0, 0.0), 0.5);
	cpShapeSetFriction(ground, 0.8);
	cpShapeSetSurfaceVelocity(ground, cpv(12.0, 0.0));

	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv((cpFloat)(i % 12) - 6.0, 0.75 + 1.1*(cpFloat)(i/12)));
		cpShape *shape = (i % 2 == 0 ? add_circle_shape(space, body, 0.45, cpvzero, 1.0) : add_box_shape(space, body, 0.8, 0.8, 1.0));
		cpShapeSetFriction(shape, 0.8);
	}
	return space;
}

static void sleep_wake_update(cpSpace *space, void *state, cpFloat dt)
{
	SleepWakeState *sleep = (SleepWakeState *)state;
	cpSpaceStep(space, dt);
	sleep->step++;
	if(sleep->step == sleep->wake_step){
		cpBodyApplyImpulseAtLocalPoint(sleep->wake_body, cpv(80.0, 40.0), cpvzero);
	}
}

static void sleep_wake_destroy_state(void *state)
{
	free(state);
}

static cpSpace *
init_sleep_wake(int size, void **state)
{
	cpSpace *space = new_space(0.0, -100.0);
	cpSpaceSetSleepTimeThreshold(space, 0.25);
	cpSpaceSetIdleSpeedThreshold(space, 0.5);
	cpShape *ground = add_static_segment(space, cpv(-50.0, 0.0), cpv(50.0, 0.0), 0.5);
	cpShapeSetFriction(ground, 0.8);

	SleepWakeState *sleep = (SleepWakeState *)calloc(1, sizeof(SleepWakeState));
	if(!sleep){fprintf(stderr, "Out of memory.\n"); exit(1);}
	sleep->wake_step = 180;
	*state = sleep;
	for(int i = 0; i < size; i++){
		cpBody *body = add_dynamic_body(space, cpv((cpFloat)(i % 10) - 5.0, 0.55 + 1.05*(cpFloat)(i/10)));
		cpShape *shape = add_box_shape(space, body, 0.9, 0.9, 1.0);
		cpShapeSetFriction(shape, 0.8);
		if(i == 0) sleep->wake_body = body;
	}
	return space;
}

static cpSpace *
init_constraint_benchmark(int size, void **state, ConstraintKind kind)
{
	cpSpace *space = new_space(0.0, -4.0);
	ConstraintBenchState *constraint_state = (ConstraintBenchState *)calloc(1, sizeof(ConstraintBenchState));
	if(!constraint_state){fprintf(stderr, "Out of memory.\n"); exit(1);}
	constraint_state->kind = kind;
	constraint_state->count = size;
	constraint_state->constraints = (cpConstraint **)calloc((size_t)size, sizeof(cpConstraint *));
	if(!constraint_state->constraints){fprintf(stderr, "Out of memory.\n"); exit(1);}
	*state = constraint_state;

	for(int i=0; i<size; i++){
		cpFloat x = (cpFloat)(i % 32)*4.0;
		cpFloat y = (cpFloat)(i/32)*4.0;
		cpBody *a = cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
		cpBody *b = cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
		cpBodySetPosition(a, cpv(x, y));
		cpBodySetPosition(b, cpv(x + 2.5, y + 0.5));
		cpBodySetAngle(a, 0.15);
		cpBodySetAngle(b, -0.2);
		cpBodySetVelocity(a, cpv(0.5, 0.25));
		cpBodySetVelocity(b, cpv(-0.25, -0.1));
		cpBodySetAngularVelocity(a, 0.4);
		cpBodySetAngularVelocity(b, -0.3);

		cpConstraint *constraint = NULL;
		switch(kind){
			case CONSTRAINT_SLIDE:
				constraint = cpSlideJointNew(a, b, cpvzero, cpvzero, 1.0, 2.0);
				break;
			case CONSTRAINT_PIVOT:
				constraint = cpPivotJointNew2(a, b, cpv(1.0, 0.0), cpv(-1.0, 0.0));
				break;
			case CONSTRAINT_GROOVE:
				constraint = cpGrooveJointNew(a, b, cpv(-1.0, 0.0), cpv(1.0, 0.0), cpv(-1.0, 0.0));
				break;
			case CONSTRAINT_GEAR:
				constraint = cpGearJointNew(a, b, 0.0, 2.0);
				break;
			case CONSTRAINT_RATCHET:
				constraint = cpRatchetJointNew(a, b, 0.0, CP_PI/4.0);
				break;
			case CONSTRAINT_ROTARY_LIMIT:
				constraint = cpRotaryLimitJointNew(a, b, -0.1, 0.1);
				break;
			case CONSTRAINT_DAMPED_SPRING:
				constraint = cpDampedSpringNew(a, b, cpvzero, cpvzero, 2.0, 40.0, 2.0);
				break;
			case CONSTRAINT_DAMPED_ROTARY_SPRING:
				constraint = cpDampedRotarySpringNew(a, b, 0.0, 20.0, 1.5);
				break;
		}
		constraint_state->constraints[i] = cpSpaceAddConstraint(space, constraint);
	}
	return space;
}

#define CONSTRAINT_INIT(name, kind) \
static cpSpace *name(int size, void **state){return init_constraint_benchmark(size, state, kind);}

CONSTRAINT_INIT(init_slide_constraints, CONSTRAINT_SLIDE)
CONSTRAINT_INIT(init_pivot_constraints, CONSTRAINT_PIVOT)
CONSTRAINT_INIT(init_groove_constraints, CONSTRAINT_GROOVE)
CONSTRAINT_INIT(init_gear_constraints, CONSTRAINT_GEAR)
CONSTRAINT_INIT(init_ratchet_constraints, CONSTRAINT_RATCHET)
CONSTRAINT_INIT(init_rotary_limit_constraints, CONSTRAINT_ROTARY_LIMIT)
CONSTRAINT_INIT(init_damped_spring_constraints, CONSTRAINT_DAMPED_SPRING)
CONSTRAINT_INIT(init_damped_rotary_spring_constraints, CONSTRAINT_DAMPED_ROTARY_SPRING)

static cpFloat
point_line_segment_distance(cpVect p, cpVect a, cpVect b)
{
	return cpvdist(p, cpClosetPointOnSegment(p, a, b));
}

static cpFloat
constraint_error_for_kind(ConstraintKind kind, cpConstraint *constraint)
{
	cpBody *a = cpConstraintGetBodyA(constraint);
	cpBody *b = cpConstraintGetBodyB(constraint);
	switch(kind){
		case CONSTRAINT_SLIDE: {
			cpFloat distance = cpvdist(cpBodyLocalToWorld(a, cpSlideJointGetAnchorA(constraint)), cpBodyLocalToWorld(b, cpSlideJointGetAnchorB(constraint)));
			return cpfmax(cpSlideJointGetMin(constraint) - distance, cpfmax(distance - cpSlideJointGetMax(constraint), 0.0));
		} case CONSTRAINT_PIVOT:
			return cpvdist(cpBodyLocalToWorld(a, cpPivotJointGetAnchorA(constraint)), cpBodyLocalToWorld(b, cpPivotJointGetAnchorB(constraint)));
		case CONSTRAINT_GROOVE: {
			cpVect anchor = cpBodyLocalToWorld(b, cpGrooveJointGetAnchorB(constraint));
			cpVect ga = cpBodyLocalToWorld(a, cpGrooveJointGetGrooveA(constraint));
			cpVect gb = cpBodyLocalToWorld(a, cpGrooveJointGetGrooveB(constraint));
			return point_line_segment_distance(anchor, ga, gb);
		} case CONSTRAINT_GEAR:
			return cpfabs(cpBodyGetAngle(b)*cpGearJointGetRatio(constraint) - cpBodyGetAngle(a) - cpGearJointGetPhase(constraint));
		case CONSTRAINT_RATCHET: {
			cpFloat ratchet = cpRatchetJointGetRatchet(constraint);
			cpFloat delta = cpBodyGetAngle(b) - cpBodyGetAngle(a) - cpRatchetJointGetPhase(constraint);
			return cpfabs(delta - (cpFloat)round((double)(delta/ratchet))*ratchet);
		} case CONSTRAINT_ROTARY_LIMIT: {
			cpFloat angle = cpBodyGetAngle(b) - cpBodyGetAngle(a);
			return cpfmax(cpRotaryLimitJointGetMin(constraint) - angle, cpfmax(angle - cpRotaryLimitJointGetMax(constraint), 0.0));
		} case CONSTRAINT_DAMPED_SPRING: {
			cpFloat distance = cpvdist(cpBodyLocalToWorld(a, cpDampedSpringGetAnchorA(constraint)), cpBodyLocalToWorld(b, cpDampedSpringGetAnchorB(constraint)));
			return cpfabs(distance - cpDampedSpringGetRestLength(constraint));
		} case CONSTRAINT_DAMPED_ROTARY_SPRING:
			return cpfabs((cpBodyGetAngle(a) - cpBodyGetAngle(b)) - cpDampedRotarySpringGetRestAngle(constraint));
		case CONSTRAINT_PIN: {
			cpFloat distance = cpvdist(cpBodyLocalToWorld(a, cpPinJointGetAnchorA(constraint)), cpBodyLocalToWorld(b, cpPinJointGetAnchorB(constraint)));
			return cpfabs(distance - cpPinJointGetDist(constraint));
		} case CONSTRAINT_SIMPLE_MOTOR:
			return cpfabs((cpBodyGetAngularVelocity(b) - cpBodyGetAngularVelocity(a)) + cpSimpleMotorGetRate(constraint));
		case CONSTRAINT_KIND_COUNT:
			break;
	}
	return INFINITY;
}

static cpFloat
constraint_error(const ConstraintBenchState *state, cpConstraint *constraint)
{
	return constraint_error_for_kind(state->kind, constraint);
}

static void
constraint_collect_metrics(ConstraintBenchState *state)
{
	for(int i=0; i<state->count; i++){
		cpConstraint *constraint = state->constraints[i];
		state->peak_error = cpfmax(state->peak_error, constraint_error(state, constraint));
		state->peak_impulse = cpfmax(state->peak_impulse, cpfabs(cpConstraintGetImpulse(constraint)));
	}
}

static void
constraint_print_metrics(void *data)
{
	ConstraintBenchState *state = (ConstraintBenchState *)data;
	cpFloat max_error = 0.0;
	cpFloat sum_error = 0.0;
	for(int i=0; i<state->count; i++){
		cpFloat error = constraint_error(state, state->constraints[i]);
		max_error = cpfmax(max_error, error);
		sum_error += error;
	}
	printf(",\"benchmark_metrics\":{\"constraint_kind\":%d,\"constraint_count\":%d,\"max_error\":%.17g,\"sum_error\":%.17g,\"peak_error\":%.17g,\"peak_impulse\":%.17g}",
		(int)state->kind, state->count, (double)max_error, (double)sum_error, (double)state->peak_error, (double)state->peak_impulse);
}

static void
constraint_destroy_state(void *data)
{
	ConstraintBenchState *state = (ConstraintBenchState *)data;
	free(state->constraints);
	free(state);
}

static cpConstraint *
new_constraint_for_kind(ConstraintKind kind, cpBody *a, cpBody *b)
{
	switch(kind){
		case CONSTRAINT_SLIDE: return cpSlideJointNew(a, b, cpvzero, cpvzero, 0.8, 1.4);
		case CONSTRAINT_PIVOT: return cpPivotJointNew2(a, b, cpv(0.6, 0.0), cpv(-0.6, 0.0));
		case CONSTRAINT_GROOVE: return cpGrooveJointNew(a, b, cpv(-0.8, 0.0), cpv(0.8, 0.0), cpv(-0.6, 0.0));
		case CONSTRAINT_GEAR: return cpGearJointNew(a, b, 0.0, 2.0);
		case CONSTRAINT_RATCHET: return cpRatchetJointNew(a, b, 0.0, CP_PI/4.0);
		case CONSTRAINT_ROTARY_LIMIT: return cpRotaryLimitJointNew(a, b, -0.1, 0.1);
		case CONSTRAINT_DAMPED_SPRING: return cpDampedSpringNew(a, b, cpvzero, cpvzero, 1.0, 35.0, 2.0);
		case CONSTRAINT_DAMPED_ROTARY_SPRING: return cpDampedRotarySpringNew(a, b, 0.0, 18.0, 1.5);
		case CONSTRAINT_PIN: return cpPinJointNew(a, b, cpvzero, cpvzero);
		case CONSTRAINT_SIMPLE_MOTOR: return cpSimpleMotorNew(a, b, 0.8);
		case CONSTRAINT_KIND_COUNT: break;
	}
	return NULL;
}

static cpSpace *
init_constraint_mix(int size, void **state)
{
	cpSpace *space = new_space(0.0, -12.0);
	cpSpaceSetSleepTimeThreshold(space, 0.4);
	cpSpaceSetIdleSpeedThreshold(space, 0.2);
	cpShape *ground = add_static_segment(space, cpv(-100.0, 0.0), cpv(100.0, 0.0), 0.5);
	cpShapeSetFriction(ground, 0.8);

	ConstraintMixState *mix = (ConstraintMixState *)calloc(1, sizeof(ConstraintMixState));
	if(!mix){fprintf(stderr, "Out of memory.\n"); exit(1);}
	mix->count = size;
	mix->constraints = (cpConstraint **)calloc((size_t)size, sizeof(cpConstraint *));
	mix->kinds = (ConstraintKind *)calloc((size_t)size, sizeof(ConstraintKind));
	mix->dynamic_bodies = (cpBody **)calloc((size_t)size, sizeof(cpBody *));
	if(!mix->constraints || !mix->kinds || !mix->dynamic_bodies){fprintf(stderr, "Out of memory.\n"); exit(1);}
	*state = mix;

	for(int i=0; i<size; i++){
		ConstraintKind kind = (ConstraintKind)(i % CONSTRAINT_KIND_COUNT);
		cpFloat x = (cpFloat)(i % 25)*3.5 - 42.0;
		cpFloat y = 2.0 + (cpFloat)(i/25)*3.0;
		cpBody *a;
		if(i % 3 == 1){
			a = cpSpaceAddBody(space, cpBodyNewStatic());
		} else if(i % 3 == 2){
			a = cpSpaceAddBody(space, cpBodyNewKinematic());
			cpBodySetVelocity(a, cpv(0.15, 0.0));
			cpBodySetAngularVelocity(a, 0.1);
		} else {
			a = cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
			cpBodySetVelocity(a, cpv(0.2, 0.0));
			cpBodySetAngularVelocity(a, 0.25);
		}
		cpBody *b = cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
		cpBodySetPosition(a, cpv(x, y));
		cpBodySetPosition(b, cpv(x + 1.7, y + 0.25));
		cpBodySetAngle(a, 0.2);
		cpBodySetAngle(b, -0.25);
		cpBodySetVelocity(b, cpv(-0.3, -0.05));
		cpBodySetAngularVelocity(b, -0.35);

		cpShape *shapeA = cpSpaceAddShape(space, cpCircleShapeNew(a, 0.45, cpvzero));
		cpShape *shapeB = cpSpaceAddShape(space, cpBoxShapeNew(b, 0.9, 0.9, 0.0));
		cpShapeSetFriction(shapeA, 0.7);
		cpShapeSetFriction(shapeB, 0.7);
		cpShapeSetDensity(shapeB, 1.0);

		mix->kinds[i] = kind;
		mix->dynamic_bodies[i] = b;
		mix->constraints[i] = cpSpaceAddConstraint(space, new_constraint_for_kind(kind, a, b));
	}
	return space;
}

static void
constraint_mix_collect_metrics(ConstraintMixState *mix)
{
	for(int i=0; i<mix->count; i++){
		ConstraintKind kind = mix->kinds[i];
		cpConstraint *constraint = mix->constraints[i];
		mix->peak_error[kind] = cpfmax(mix->peak_error[kind], constraint_error_for_kind(kind, constraint));
		mix->peak_impulse[kind] = cpfmax(mix->peak_impulse[kind], cpfabs(cpConstraintGetImpulse(constraint)));
	}
}

static void
constraint_mix_update(cpSpace *space, void *data, cpFloat dt)
{
	ConstraintMixState *mix = (ConstraintMixState *)data;
	cpSpaceStep(space, dt);
	mix->step++;
	if(mix->step == 179){
		for(int i=0; i<mix->count; i++){
			if(cpBodyIsSleeping(mix->dynamic_bodies[i])){
				mix->wake_body = mix->dynamic_bodies[i];
				mix->wake_body_was_sleeping = cpTrue;
				break;
			}
		}
	}
	if(mix->step == 180 && mix->wake_body) cpBodyApplyImpulseAtLocalPoint(mix->wake_body, cpv(20.0, 10.0), cpvzero);
	if(mix->step == 181 && mix->wake_body) mix->wake_body_is_awake = !cpBodyIsSleeping(mix->wake_body);
}

static void
constraint_mix_print_metrics(void *data)
{
	ConstraintMixState *mix = (ConstraintMixState *)data;
	int counts[CONSTRAINT_KIND_COUNT] = {0};
	cpFloat max_error[CONSTRAINT_KIND_COUNT] = {0};
	cpFloat sum_error[CONSTRAINT_KIND_COUNT] = {0};
	for(int i=0; i<mix->count; i++){
		ConstraintKind kind = mix->kinds[i];
		cpFloat error = constraint_error_for_kind(kind, mix->constraints[i]);
		counts[kind]++;
		max_error[kind] = cpfmax(max_error[kind], error);
		sum_error[kind] += error;
	}
	printf(",\"benchmark_metrics\":{\"constraint_count\":%d,\"type_count\":%d,\"wake_body_was_sleeping\":%s,\"wake_body_is_awake\":%s,\"per_type\":[", mix->count, CONSTRAINT_KIND_COUNT, mix->wake_body_was_sleeping ? "true" : "false", mix->wake_body_is_awake ? "true" : "false");
	for(int kind=0; kind<CONSTRAINT_KIND_COUNT; kind++){
		if(kind) printf(",");
		printf("{\"constraint_kind\":%d,\"constraint_count\":%d,\"max_error\":%.17g,\"sum_error\":%.17g,\"peak_error\":%.17g,\"peak_impulse\":%.17g}",
			kind, counts[kind], (double)max_error[kind], (double)sum_error[kind], (double)mix->peak_error[kind], (double)mix->peak_impulse[kind]);
	}
	printf("]}");
}

static void
constraint_mix_destroy_state(void *data)
{
	ConstraintMixState *mix = (ConstraintMixState *)data;
	free(mix->constraints);
	free(mix->kinds);
	free(mix->dynamic_bodies);
	free(mix);
}

// Smoke and reference batches target practical sample durations on the reference host.
static Benchmark benchmarks[] = {
	{"FallingSquares", 1300, 300, 10, 50, 300, 10, 1, 1, init_falling_squares, default_update, NULL, NULL},
	{"FallingCircles", 1300, 300, 10, 50, 300, 10, 2, 2, init_falling_circles, default_update, NULL, NULL},
	{"Tumbler", 1500, 1000, 50, 250, 1000, 50, 1, 1, init_tumbler, tumbler_update, tumbler_destroy_state, NULL},
	{"AddPair", 1000, 2000, 100, 500, 2500, 100, 4, 1, init_add_pair, add_pair_update, NULL, NULL},
	{"MildN2", 100, 200, 10, 50, 200, 10, 10, 4, init_mild_n2, default_update, NULL, NULL},
	{"N2", 100, 750, 25, 100, 750, 25, 60, 20, init_n2, default_update, NULL, NULL},
	{"Multifixture", 500, 100, 5, 25, 100, 5, 3, 1, init_multifixture, default_update, NULL, NULL},
	{"MostlyStaticSingleBody", 400, 200, 10, 50, 200, 5, 160, 80, init_mostly_static_single_body, default_update, NULL, NULL},
	{"MostlyStaticMultiBody", 400, 200, 10, 50, 200, 5, 160, 80, init_mostly_static_multi_body, default_update, NULL, NULL},
	{"Diagonal", 1000, 50, 2, 10, 50, 2, 10, 5, init_diagonal, default_update, NULL, NULL},
	{"MixedStaticDynamic", 400, 6000, 100, 500, 6000, 100, 7, 5, init_mixed_static_dynamic, default_update, NULL, NULL},
	{"BigMobile", 1000, 11, 1, 6, 11, 1, 150, 10, init_big_mobile, default_update, NULL, NULL},
	{"SlowExplosion", 1000, 6000, 100, 1000, 6000, 100, 25, 10, init_slow_explosion, default_update, NULL, NULL},
	{"FrictionalPyramid", 900, 45, 8, 16, 45, 1, 4, 4, init_frictional_pyramid, default_update, NULL, NULL},
	{"CollisionCallbacks", 600, 240, 24, 60, 240, 12, 20, 20, init_collision_callbacks, default_update, callback_destroy_state, callback_print_metrics},
	{"SleepWake", 420, 200, 20, 50, 200, 10, 200, 100, init_sleep_wake, sleep_wake_update, sleep_wake_destroy_state, NULL},
	{"SurfaceVelocity", 600, 240, 24, 60, 240, 12, 20, 20, init_surface_velocity, default_update, NULL, NULL},
	{"SlideConstraints", 600, 2000, 50, 500, 2000, 50, 20, 4, init_slide_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"PivotConstraints", 600, 2000, 50, 500, 2000, 50, 20, 4, init_pivot_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"GrooveConstraints", 600, 2000, 50, 500, 2000, 50, 20, 4, init_groove_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"GearConstraints", 600, 4000, 100, 1000, 4000, 100, 30, 8, init_gear_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"RatchetConstraints", 600, 4000, 100, 1000, 4000, 100, 30, 8, init_ratchet_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"RotaryLimitConstraints", 600, 4000, 100, 1000, 4000, 100, 30, 8, init_rotary_limit_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"DampedSpringConstraints", 600, 2000, 50, 500, 2000, 50, 20, 4, init_damped_spring_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"DampedRotarySpringConstraints", 600, 4000, 100, 1000, 4000, 100, 30, 8, init_damped_rotary_spring_constraints, default_update, constraint_destroy_state, constraint_print_metrics},
	{"ConstraintMix", 600, 2000, 100, 500, 2000, 100, 10, 2, init_constraint_mix, constraint_mix_update, constraint_mix_destroy_state, constraint_mix_print_metrics},
};

static int benchmark_count = (int)(sizeof(benchmarks)/sizeof(benchmarks[0]));

static Benchmark *
find_benchmark(const char *name)
{
	for(int i = 0; i < benchmark_count; i++){
		if(strcmp(benchmarks[i].name, name) == 0) return &benchmarks[i];
	}
	return NULL;
}


typedef struct Summary {
	int step;
	int bodies;
	int dynamic_bodies;
	int static_bodies;
	int kinematic_bodies;
	int sleeping_bodies;
	int shapes;
	int constraints;
	int invalid_values;
	cpFloat total_mass;
	cpFloat total_kinetic_energy;
	cpFloat max_linear_velocity;
	cpFloat max_angular_velocity;
	cpFloat sum_position_x;
	cpFloat sum_position_y;
	cpFloat sum_velocity_x;
	cpFloat sum_velocity_y;
	cpBB body_bounds;
	cpBB shape_bounds;
	cpBool has_body_bounds;
	cpBool has_shape_bounds;
	int contact_pairs;
	int contact_points;
} Summary;

struct ArbiterSummaryData {
	Summary *summary;
	cpArbiter **arbiters;
	int count;
	int capacity;
};

static int
is_bad(cpFloat value)
{
	return isnan((double)value) || isinf((double)value);
}

static void
summary_add_bad_vect(Summary *summary, cpVect v)
{
	if(is_bad(v.x)) summary->invalid_values++;
	if(is_bad(v.y)) summary->invalid_values++;
}

static void
summary_body(cpBody *body, void *data)
{
	Summary *summary = (Summary *)data;
	cpBodyType type = cpBodyGetType(body);
	cpVect p = cpBodyGetPosition(body);
	cpVect v = cpBodyGetVelocity(body);
	cpFloat w = cpBodyGetAngularVelocity(body);
	cpFloat mass = cpBodyGetMass(body);
	cpFloat moment = cpBodyGetMoment(body);
	cpFloat speed = cpvlength(v);
	cpFloat angular_speed = cpfabs(w);

	summary->bodies++;
	if(type == CP_BODY_TYPE_DYNAMIC){
		summary->dynamic_bodies++;
		summary->sum_position_x += p.x;
		summary->sum_position_y += p.y;
		summary->sum_velocity_x += v.x;
		summary->sum_velocity_y += v.y;
		summary->total_mass += mass;
		if(!isinf((double)mass)) summary->total_kinetic_energy += 0.5*mass*cpvdot(v, v);
		if(!isinf((double)moment)) summary->total_kinetic_energy += 0.5*moment*w*w;
		if(speed > summary->max_linear_velocity) summary->max_linear_velocity = speed;
		if(angular_speed > summary->max_angular_velocity) summary->max_angular_velocity = angular_speed;

		cpBB bb = cpBBNewForExtents(p, 0.0, 0.0);
		summary->body_bounds = summary->has_body_bounds ? cpBBMerge(summary->body_bounds, bb) : bb;
		summary->has_body_bounds = cpTrue;
	} else if(type == CP_BODY_TYPE_STATIC){
		summary->static_bodies++;
	} else if(type == CP_BODY_TYPE_KINEMATIC){
		summary->kinematic_bodies++;
	}

	if(cpBodyIsSleeping(body)) summary->sleeping_bodies++;
	// Infinite mass and moment are expected for static and kinematic bodies.
	if(type == CP_BODY_TYPE_DYNAMIC && is_bad(mass)) summary->invalid_values++;
	if(type == CP_BODY_TYPE_DYNAMIC && is_bad(moment)) summary->invalid_values++;
	if(is_bad(w)) summary->invalid_values++;
	summary_add_bad_vect(summary, p);
	summary_add_bad_vect(summary, v);
}

static void
summary_shape(cpShape *shape, void *data)
{
	Summary *summary = (Summary *)data;
	cpBB bb = cpShapeGetBB(shape);
	summary->shapes++;
	summary->shape_bounds = summary->has_shape_bounds ? cpBBMerge(summary->shape_bounds, bb) : bb;
	summary->has_shape_bounds = cpTrue;
	if(is_bad(bb.l)) summary->invalid_values++;
	if(is_bad(bb.b)) summary->invalid_values++;
	if(is_bad(bb.r)) summary->invalid_values++;
	if(is_bad(bb.t)) summary->invalid_values++;
}

static void
summary_constraint(cpConstraint *constraint, void *data)
{
	(void)constraint;
	Summary *summary = (Summary *)data;
	summary->constraints++;
}

static void
summary_arbiter(cpBody *body, cpArbiter *arbiter, void *data)
{
	(void)body;
	struct ArbiterSummaryData *arbiter_data = (struct ArbiterSummaryData *)data;
	if(arbiter_data->count == arbiter_data->capacity){
		int capacity = arbiter_data->capacity ? arbiter_data->capacity*2 : 64;
		cpArbiter **arbiters = (cpArbiter **)realloc(arbiter_data->arbiters, sizeof(cpArbiter *)*(size_t)capacity);
		if(!arbiters){
			fprintf(stderr, "Out of memory while collecting arbiters.\n");
			exit(1);
		}
		arbiter_data->arbiters = arbiters;
		arbiter_data->capacity = capacity;
	}

	arbiter_data->arbiters[arbiter_data->count++] = arbiter;
}

static int
compare_arbiter_ptrs(const void *a, const void *b)
{
	uintptr_t pa = (uintptr_t)*(cpArbiter * const *)a;
	uintptr_t pb = (uintptr_t)*(cpArbiter * const *)b;
	return (pa > pb) - (pa < pb);
}

static void
summary_body_arbiters(cpBody *body, void *data)
{
	cpBodyEachArbiter(body, summary_arbiter, data);
}

static Summary
collect_summary(cpSpace *space, int step)
{
	Summary summary;
	memset(&summary, 0, sizeof(summary));
	summary.step = step;
	cpSpaceEachBody(space, summary_body, &summary);
	cpSpaceEachShape(space, summary_shape, &summary);
	cpSpaceEachConstraint(space, summary_constraint, &summary);

	struct ArbiterSummaryData arbiter_data;
	memset(&arbiter_data, 0, sizeof(arbiter_data));
	arbiter_data.summary = &summary;
	cpSpaceEachBody(space, summary_body_arbiters, &arbiter_data);
	if(arbiter_data.count > 1){
		qsort(arbiter_data.arbiters, (size_t)arbiter_data.count, sizeof(cpArbiter *), compare_arbiter_ptrs);
	}
	for(int i = 0; i < arbiter_data.count; i++){
		if(i > 0 && arbiter_data.arbiters[i] == arbiter_data.arbiters[i - 1]) continue;
		summary.contact_pairs++;
		summary.contact_points += cpArbiterGetCount(arbiter_data.arbiters[i]);
	}
	free(arbiter_data.arbiters);

	return summary;
}


static void
print_number_json(cpFloat value)
{
	if(is_bad(value)){
		printf("null");
	} else {
		printf("%.17g", (double)value);
	}
}

static void
print_bb_json(cpBB bb, cpBool valid)
{
	if(valid){
		printf("[");
		print_number_json(bb.l);
		printf(",");
		print_number_json(bb.b);
		printf(",");
		print_number_json(bb.r);
		printf(",");
		print_number_json(bb.t);
		printf("]");
	} else {
		printf("null");
	}
}

static void
print_summary_json(const Summary *summary)
{
	printf("{\"step\":%d", summary->step);
	printf(",\"bodies\":%d", summary->bodies);
	printf(",\"dynamic_bodies\":%d", summary->dynamic_bodies);
	printf(",\"static_bodies\":%d", summary->static_bodies);
	printf(",\"kinematic_bodies\":%d", summary->kinematic_bodies);
	printf(",\"sleeping_bodies\":%d", summary->sleeping_bodies);
	printf(",\"shapes\":%d", summary->shapes);
	printf(",\"constraints\":%d", summary->constraints);
	printf(",\"contact_pairs\":%d", summary->contact_pairs);
	printf(",\"contact_points\":%d", summary->contact_points);
	printf(",\"invalid_values\":%d", summary->invalid_values);
	printf(",\"total_mass\":");
	print_number_json(summary->total_mass);
	printf(",\"total_kinetic_energy\":");
	print_number_json(summary->total_kinetic_energy);
	printf(",\"max_linear_velocity\":");
	print_number_json(summary->max_linear_velocity);
	printf(",\"max_angular_velocity\":");
	print_number_json(summary->max_angular_velocity);
	printf(",\"sum_position\":[");
	print_number_json(summary->sum_position_x);
	printf(",");
	print_number_json(summary->sum_position_y);
	printf("]");
	printf(",\"sum_velocity\":[");
	print_number_json(summary->sum_velocity_x);
	printf(",");
	print_number_json(summary->sum_velocity_y);
	printf("]");
	printf(",\"body_bounds\":");
	print_bb_json(summary->body_bounds, summary->has_body_bounds);
	printf(",\"shape_bounds\":");
	print_bb_json(summary->shape_bounds, summary->has_shape_bounds);
	printf("}");
}

static int
compare_ints(const void *a, const void *b)
{
	int ia = *(const int *)a;
	int ib = *(const int *)b;
	return (ia > ib) - (ia < ib);
}

static int
parse_checkpoints(const char *arg, int *checkpoints, int max_checkpoints)
{
	if(!arg) return 0;
	char *copy = (char *)malloc(strlen(arg) + 1);
	if(!copy){
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	strcpy(copy, arg);

	int count = 0;
	for(char *token = strtok(copy, ","); token; token = strtok(NULL, ",")){
		if(count >= max_checkpoints){
			fprintf(stderr, "Too many checkpoints; max is %d.\n", max_checkpoints);
			free(copy);
			exit(2);
		}
		if(strcmp(token, "final") == 0){
			checkpoints[count++] = -1;
			continue;
		}

		char *end = NULL;
		long checkpoint = strtol(token, &end, 10);
		if(!token[0] || (end && *end) || checkpoint < 0 || checkpoint > INT_MAX){
			fprintf(stderr, "Invalid checkpoint: %s. Use a non-negative integer or 'final'.\n", token);
			free(copy);
			exit(2);
		}
		checkpoints[count++] = (int)checkpoint;
	}
	free(copy);
	return count;
}

static int
prepare_checkpoints(const Benchmark *benchmark, int *checkpoints, int checkpoint_count, int max_checkpoints)
{
	if(checkpoint_count == 0){
		int defaults[] = {0, 1, 10, 100, benchmark->steps};
		for(size_t i = 0; i < sizeof(defaults)/sizeof(defaults[0]); i++){
			if(defaults[i] <= benchmark->steps) checkpoints[checkpoint_count++] = defaults[i];
		}
	} else {
		for(int i = 0; i < checkpoint_count; i++){
			if(checkpoints[i] == -1 || checkpoints[i] > benchmark->steps) checkpoints[i] = benchmark->steps;
		}
	}

	qsort(checkpoints, (size_t)checkpoint_count, sizeof(int), compare_ints);
	int out = 0;
	for(int i = 0; i < checkpoint_count; i++){
		if(out == 0 || checkpoints[i] != checkpoints[out - 1]) checkpoints[out++] = checkpoints[i];
	}
	(void)max_checkpoints;
	return out;
}

static void
run_summary_json(Benchmark *benchmark, int size, const int *input_checkpoints, int input_checkpoint_count, int max_checkpoints)
{
	int *checkpoints = (int *)calloc((size_t)max_checkpoints, sizeof(int));
	if(!checkpoints){
		fprintf(stderr, "Out of memory.\n");
		exit(1);
	}
	for(int i = 0; i < input_checkpoint_count; i++) checkpoints[i] = input_checkpoints[i];
	int checkpoint_count = prepare_checkpoints(benchmark, checkpoints, input_checkpoint_count, max_checkpoints);
	int checkpoint_index = 0;
	int simulated_steps = checkpoints[checkpoint_count - 1];
	void *state = NULL;

	cpSpace *space = benchmark->init(size, &state);

	printf("{\"benchmark\":\"%s\",\"size\":%d,\"steps\":%d,\"simulated_steps\":%d,\"checkpoints\":[", benchmark->name, size, benchmark->steps, simulated_steps);
	if(checkpoint_index < checkpoint_count && checkpoints[checkpoint_index] == 0){
		Summary summary = collect_summary(space, 0);
		print_summary_json(&summary);
		checkpoint_index++;
	}

	for(int step = 1; step <= simulated_steps; step++){
		benchmark->update(space, state, (cpFloat)(1.0/FPS));
		if(benchmark->print_metrics == constraint_print_metrics) constraint_collect_metrics((ConstraintBenchState *)state);
		if(benchmark->print_metrics == constraint_mix_print_metrics) constraint_mix_collect_metrics((ConstraintMixState *)state);
		if(checkpoint_index < checkpoint_count && checkpoints[checkpoint_index] == step){
			if(checkpoint_index > 0) printf(",");
			Summary summary = collect_summary(space, step);
			print_summary_json(&summary);
			checkpoint_index++;
		}
	}
	printf("]");
	if(benchmark->print_metrics) benchmark->print_metrics(state);
	printf("}");

	free_space_children(space);
	cpSpaceFree(space);
	if(benchmark->destroy_state) benchmark->destroy_state(state);
	free(checkpoints);
}

typedef struct SvgContext {
	FILE *file;
} SvgContext;

static void
svg_color(char *buffer, size_t size, cpSpaceDebugColor color)
{
	int r = (int)(cpfclamp(color.r, 0.0, 1.0)*255.0);
	int g = (int)(cpfclamp(color.g, 0.0, 1.0)*255.0);
	int b = (int)(cpfclamp(color.b, 0.0, 1.0)*255.0);
	snprintf(buffer, size, "#%02x%02x%02x", r, g, b);
}

static cpSpaceDebugColor
svg_color_for_shape(cpShape *shape, cpDataPointer data)
{
	(void)shape;
	(void)data;
	cpSpaceDebugColor color = {0.35f, 0.55f, 0.95f, 0.55f};
	return color;
}

static void
svg_draw_circle(cpVect pos, cpFloat angle, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	SvgContext *ctx = (SvgContext *)data;
	char stroke[16], fill_color[16];
	svg_color(stroke, sizeof(stroke), outline);
	svg_color(fill_color, sizeof(fill_color), fill);
	fprintf(ctx->file, "  <circle cx=\"%.17g\" cy=\"%.17g\" r=\"%.17g\" fill=\"%s\" fill-opacity=\"%.3g\" stroke=\"%s\" stroke-width=\"0.03\"/>\n",
		(double)pos.x, (double)-pos.y, (double)radius, fill_color, (double)fill.a, stroke);
	fprintf(ctx->file, "  <line x1=\"%.17g\" y1=\"%.17g\" x2=\"%.17g\" y2=\"%.17g\" stroke=\"%s\" stroke-width=\"0.02\"/>\n",
		(double)pos.x, (double)-pos.y, (double)(pos.x + cos(angle)*radius), (double)-(pos.y + sin(angle)*radius), stroke);
}

static void
svg_draw_segment(cpVect a, cpVect b, cpSpaceDebugColor color, cpDataPointer data)
{
	SvgContext *ctx = (SvgContext *)data;
	char stroke[16];
	svg_color(stroke, sizeof(stroke), color);
	fprintf(ctx->file, "  <line x1=\"%.17g\" y1=\"%.17g\" x2=\"%.17g\" y2=\"%.17g\" stroke=\"%s\" stroke-width=\"0.05\" stroke-linecap=\"round\"/>\n",
		(double)a.x, (double)-a.y, (double)b.x, (double)-b.y, stroke);
}

static void
svg_draw_fat_segment(cpVect a, cpVect b, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	SvgContext *ctx = (SvgContext *)data;
	char stroke[16];
	(void)outline;
	svg_color(stroke, sizeof(stroke), fill);
	fprintf(ctx->file, "  <line x1=\"%.17g\" y1=\"%.17g\" x2=\"%.17g\" y2=\"%.17g\" stroke=\"%s\" stroke-opacity=\"%.3g\" stroke-width=\"%.17g\" stroke-linecap=\"round\"/>\n",
		(double)a.x, (double)-a.y, (double)b.x, (double)-b.y, stroke, (double)fill.a, (double)(2.0*radius));
}

static void
svg_draw_polygon(int count, const cpVect *verts, cpFloat radius, cpSpaceDebugColor outline, cpSpaceDebugColor fill, cpDataPointer data)
{
	SvgContext *ctx = (SvgContext *)data;
	char stroke[16], fill_color[16];
	(void)radius;
	svg_color(stroke, sizeof(stroke), outline);
	svg_color(fill_color, sizeof(fill_color), fill);
	fprintf(ctx->file, "  <polygon points=\"");
	for(int i = 0; i < count; i++) fprintf(ctx->file, "%.17g,%.17g ", (double)verts[i].x, (double)-verts[i].y);
	fprintf(ctx->file, "\" fill=\"%s\" fill-opacity=\"%.3g\" stroke=\"%s\" stroke-width=\"0.03\"/>\n", fill_color, (double)fill.a, stroke);
}

static void
svg_draw_dot(cpFloat size, cpVect pos, cpSpaceDebugColor color, cpDataPointer data)
{
	SvgContext *ctx = (SvgContext *)data;
	char fill_color[16];
	svg_color(fill_color, sizeof(fill_color), color);
	fprintf(ctx->file, "  <circle cx=\"%.17g\" cy=\"%.17g\" r=\"%.17g\" fill=\"%s\"/>\n", (double)pos.x, (double)-pos.y, (double)(size/2.0), fill_color);
}

static void
write_svg_snapshot(Benchmark *benchmark, int size, int step, const char *path)
{
	void *state = NULL;
	cpSpace *space = benchmark->init(size, &state);
	if(step < 0 || step > benchmark->steps) step = benchmark->steps;
	for(int i = 0; i < step; i++) benchmark->update(space, state, (cpFloat)(1.0/FPS));

	Summary summary = collect_summary(space, step);
	cpBB bb = summary.has_shape_bounds ? summary.shape_bounds : cpBBNew(-1.0, -1.0, 1.0, 1.0);
	cpFloat margin = cpfmax(1.0, cpfmax(bb.r - bb.l, bb.t - bb.b)*0.05);
	cpFloat min_x = bb.l - margin;
	cpFloat max_x = bb.r + margin;
	cpFloat min_y = -bb.t - margin;
	cpFloat max_y = -bb.b + margin;
	cpFloat width = cpfmax(1.0, max_x - min_x);
	cpFloat height = cpfmax(1.0, max_y - min_y);

	FILE *file = (strcmp(path, "-") == 0 ? stdout : fopen(path, "w"));
	if(!file){
		fprintf(stderr, "Could not open SVG output path: %s\n", path);
		exit(1);
	}

	SvgContext ctx = {file};
	fprintf(file, "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"%.17g %.17g %.17g %.17g\">\n", (double)min_x, (double)min_y, (double)width, (double)height);
	fprintf(file, "  <rect x=\"%.17g\" y=\"%.17g\" width=\"%.17g\" height=\"%.17g\" fill=\"white\"/>\n", (double)min_x, (double)min_y, (double)width, (double)height);
	fprintf(file, "  <title>MunkBench %s size %d step %d</title>\n", benchmark->name, size, step);

	cpSpaceDebugColor outline_color = {0.10f, 0.10f, 0.12f, 1.0f};
	cpSpaceDebugColor constraint_color = {0.80f, 0.25f, 0.25f, 1.0f};
	cpSpaceDebugColor collision_color = {0.90f, 0.20f, 0.20f, 1.0f};
	cpSpaceDebugDrawOptions options = {
		svg_draw_circle,
		svg_draw_segment,
		svg_draw_fat_segment,
		svg_draw_polygon,
		svg_draw_dot,
		CP_SPACE_DEBUG_DRAW_SHAPES,
		outline_color,
		svg_color_for_shape,
		constraint_color,
		collision_color,
		cpTransformIdentity,
		&ctx,
	};
	cpSpaceDebugDraw(space, &options);
	fprintf(file, "</svg>\n");

	if(file != stdout) fclose(file);
	free_space_children(space);
	cpSpaceFree(space);
	if(benchmark->destroy_state) benchmark->destroy_state(state);
}

static int
profile_size(const Benchmark *benchmark, BenchmarkProfile profile)
{
	switch(profile){
		case BENCHMARK_PROFILE_SMOKE: return benchmark->smoke_size;
		case BENCHMARK_PROFILE_REFERENCE: return benchmark->reference_size;
		case BENCHMARK_PROFILE_EXTENDED: return benchmark->extended_size;
	}
	return benchmark->reference_size;
}

static int
profile_batch(const Benchmark *benchmark, BenchmarkProfile profile)
{
	switch(profile){
		case BENCHMARK_PROFILE_SMOKE: return benchmark->smoke_batch;
		case BENCHMARK_PROFILE_REFERENCE: return benchmark->reference_batch;
		case BENCHMARK_PROFILE_EXTENDED: return 1;
	}
	return benchmark->reference_batch;
}

static RunResult
run_benchmark(Benchmark *benchmark, int size, int sample, int batch)
{
	RunResult result = {benchmark->name, size, sample, batch, 0.0, 0.0};

	for(int run = 0; run < batch; run++){
		void *state = NULL;
		double init_start_time = now_seconds();
		cpSpace *space = benchmark->init(size, &state);
		double sim_start_time = now_seconds();

		for(int steps = 0; steps < benchmark->steps; steps++){
			benchmark->update(space, state, (cpFloat)(1.0/FPS));
		}

		double end_time = now_seconds();
		result.init_time += sim_start_time - init_start_time;
		result.run_time += end_time - sim_start_time;

		free_space_children(space);
		cpSpaceFree(space);
		if(benchmark->destroy_state) benchmark->destroy_state(state);
	}

	return result;
}

static void
run_timing_samples(Benchmark *benchmark, int size, int warmup, int samples, int batch)
{
	for(int i = 0; i < warmup; i++) (void)run_benchmark(benchmark, size, 0, batch);
	for(int sample = 1; sample <= samples; sample++){
		RunResult result = run_benchmark(benchmark, size, sample, batch);
		printf("%s,%s,%d,%d,%d,%.9f,%.9f\n", MUNK2D_VERSION, result.benchmark, result.size, result.sample, result.batch, result.init_time, result.run_time);
		fflush(stdout);
	}
}

static int
name_selected(Benchmark *benchmark, char **names, int name_count)
{
	if(name_count == 0) return 1;
	for(int i = 0; i < name_count; i++){
		if(strcmp(benchmark->name, names[i]) == 0) return 1;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	benchmarkContactSolver = CP_CONTACT_SOLVER_BASELINE;
	int size_arg_set = 0;
	int size_arg = 0;
	int summary_json = 0;
	const char *checkpoints_arg = NULL;
	const char *svg_path = NULL;
	int svg_step = -1;
	int warmup = 0;
	int samples = 1;
	int batch = 1;
	int auto_batch = 0;
	BenchmarkProfile profile = BENCHMARK_PROFILE_REFERENCE;
	char **selected_names = NULL;
	int selected_name_count = 0;

	for(int i = 1; i < argc; i++){
		if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0){
			usage(argv[0]);
			return 0;
		} else if(strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--size") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for %s.\n", argv[i]);
				return 2;
			}
			size_arg = atoi(argv[++i]);
			size_arg_set = 1;
		} else if(strcmp(argv[i], "-b") == 0 || strcmp(argv[i], "--benchmarks") == 0){
			while(i + 1 < argc && argv[i + 1][0] != '-'){
				selected_names = (char **)realloc(selected_names, sizeof(char *)*(selected_name_count + 1));
				if(!selected_names){
					fprintf(stderr, "Out of memory.\n");
					return 1;
				}
				selected_names[selected_name_count++] = argv[++i];
			}
		} else if(strcmp(argv[i], "--contact-solver") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for --contact-solver.\n");
				free(selected_names);
				return 2;
			}
			const char *value = argv[++i];
			if(strcmp(value, "baseline") == 0) benchmarkContactSolver = CP_CONTACT_SOLVER_BASELINE;
			else if(strcmp(value, "avx2") == 0) benchmarkContactSolver = CP_CONTACT_SOLVER_AVX2;
			else if(strcmp(value, "wasm-simd") == 0) benchmarkContactSolver = CP_CONTACT_SOLVER_WASM_SIMD;
			else {
				fprintf(stderr, "Invalid contact solver: %s. Use baseline, avx2 or wasm-simd.\n", value);
				free(selected_names);
				return 2;
			}
			if(!cpContactSolverIsAvailable(benchmarkContactSolver)){
				fprintf(stderr, "Requested contact solver is unavailable in this build or on this CPU/OS.\n");
				free(selected_names);
				return 2;
			}
		} else if(strcmp(argv[i], "--profile") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for --profile.\n");
				return 2;
			}
			const char *value = argv[++i];
			if(strcmp(value, "smoke") == 0) profile = BENCHMARK_PROFILE_SMOKE;
			else if(strcmp(value, "reference") == 0) profile = BENCHMARK_PROFILE_REFERENCE;
			else if(strcmp(value, "extended") == 0) profile = BENCHMARK_PROFILE_EXTENDED;
			else {
				fprintf(stderr, "Invalid profile: %s. Use smoke, reference, or extended.\n", value);
				return 2;
			}
		} else if(strcmp(argv[i], "--warmup") == 0 || strcmp(argv[i], "--samples") == 0 || strcmp(argv[i], "--batch") == 0){
			const char *option = argv[i];
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for %s.\n", option);
				free(selected_names);
				return 2;
			}
			const char *value_arg = argv[++i];
			if(strcmp(option, "--batch") == 0 && strcmp(value_arg, "auto") == 0){
				auto_batch = 1;
				continue;
			}
			char *end = NULL;
			long value = strtol(value_arg, &end, 10);
			if(!value_arg[0] || (end && *end) || value < 0 || value > INT_MAX || (strcmp(option, "--warmup") != 0 && value == 0)){
				fprintf(stderr, "Invalid value for %s: %s.\n", option, value_arg);
				free(selected_names);
				return 2;
			}
			if(strcmp(option, "--warmup") == 0) warmup = (int)value;
			else if(strcmp(option, "--samples") == 0) samples = (int)value;
			else batch = (int)value;
		} else if(strcmp(argv[i], "--summary-json") == 0){
			summary_json = 1;
		} else if(strcmp(argv[i], "--checkpoints") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for %s.\n", argv[i]);
				return 2;
			}
			checkpoints_arg = argv[++i];
		} else if(strcmp(argv[i], "--svg") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for %s.\n", argv[i]);
				return 2;
			}
			svg_path = argv[++i];
		} else if(strcmp(argv[i], "--step") == 0){
			if(i + 1 >= argc){
				fprintf(stderr, "Missing value for %s.\n", argv[i]);
				return 2;
			}
			svg_step = atoi(argv[++i]);
		} else {
			fprintf(stderr, "Unknown argument: %s\n", argv[i]);
			usage(argv[0]);
			free(selected_names);
			return 2;
		}
	}

	for(int i = 0; i < selected_name_count; i++){
		if(!find_benchmark(selected_names[i])){
			fprintf(stderr, "Unknown benchmark: %s\n", selected_names[i]);
			fprintf(stderr, "Known benchmarks:\n");
			for(int j = 0; j < benchmark_count; j++) fprintf(stderr, "  %s\n", benchmarks[j].name);
			free(selected_names);
			return 2;
		}
	}

	if((svg_path || summary_json) && (warmup != 0 || samples != 1 || batch != 1 || auto_batch)){
		fprintf(stderr, "--warmup, --samples, and --batch are only supported in timing CSV mode.\n");
		free(selected_names);
		return 2;
	}
	if(auto_batch && size_arg_set){
		fprintf(stderr, "--batch auto is calibrated only for named profiles; use an explicit batch with --size.\n");
		free(selected_names);
		return 2;
	}

	if(svg_path){
		if(selected_name_count != 1){
			fprintf(stderr, "--svg requires exactly one benchmark selected with -b/--benchmarks.\n");
			free(selected_names);
			return 2;
		}
		if(size_arg_set && size_arg == -1){
			fprintf(stderr, "--svg does not support size sweeps.\n");
			free(selected_names);
			return 2;
		}
		Benchmark *benchmark = find_benchmark(selected_names[0]);
		int size = size_arg_set ? size_arg : profile_size(benchmark, profile);
		write_svg_snapshot(benchmark, size, svg_step, svg_path);
		free(selected_names);
		return 0;
	}

	if(summary_json){
		if(size_arg_set && size_arg == -1){
			fprintf(stderr, "--summary-json does not support size sweeps.\n");
			free(selected_names);
			return 2;
		}
		int max_checkpoints = 128;
		int checkpoints[128];
		int checkpoint_count = parse_checkpoints(checkpoints_arg, checkpoints, max_checkpoints);
		printf("{\"version\":\"%s\",\"benchmarks\":[", MUNK2D_VERSION);
		int emitted = 0;
		for(int i = 0; i < benchmark_count; i++){
			Benchmark *benchmark = &benchmarks[i];
			if(!name_selected(benchmark, selected_names, selected_name_count)) continue;
			if(emitted++) printf(",");
			int size = size_arg_set ? size_arg : profile_size(benchmark, profile);
			run_summary_json(benchmark, size, checkpoints, checkpoint_count, max_checkpoints);
		}
		printf("]}\n");
		free(selected_names);
		return 0;
	}

	printf("version,benchmark,size,sample,batch,init_time,run_time\n");
	for(int i = 0; i < benchmark_count; i++){
		Benchmark *benchmark = &benchmarks[i];
		if(!name_selected(benchmark, selected_names, selected_name_count)) continue;

		int benchmark_batch = auto_batch ? profile_batch(benchmark, profile) : batch;
		if(!size_arg_set){
			run_timing_samples(benchmark, profile_size(benchmark, profile), warmup, samples, benchmark_batch);
		} else if(size_arg == -1){
			int step = (benchmark->size_end + 1 - benchmark->smoke_size)/11;
			if(step <= 0) step = benchmark->size_inc;
			for(int size = benchmark->smoke_size; size <= benchmark->size_end; size += step){
				run_timing_samples(benchmark, size, warmup, samples, benchmark_batch);
			}
		} else {
			run_timing_samples(benchmark, size_arg, warmup, samples, benchmark_batch);
		}
	}

	free(selected_names);
	return 0;
}
