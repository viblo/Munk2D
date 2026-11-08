/* Copyright (c) 2026 Victor Blomqvist
 * SPDX-License-Identifier: MIT
 * Regression coverage for endpoint-aligned and degenerate capsule collisions.
 * Large translated float cases allow for their coarser coordinate resolution.
 */
#include <chipmunk/chipmunk.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static cpVect world(cpVect p, cpFloat angle, cpVect shift)
{
	return cpvadd(cpvrotate(p, cpvforangle(angle)), shift);
}
static cpShape *capsule(cpBody *body, int reverse)
{
	return cpSegmentShapeNew(body, cpv(reverse ? 80 : -80, 0), cpv(reverse ? -80 : 80, 0), 80);
}
static int static_probes(void)
{
	const double ys[] = {120,	  280,			120.001,	  119.999,		279.999,
						 280.001, 120.00000001, 119.99999999, 279.99999999, 280.00000001};
	const double rotations[] = {0,
								0.5235987755982988,
								0.7853981633974483,
								1.5707963267948966,
								3.141592653589793,
								-0.4487989505128276};
	int failures = 0, total = 0;

	for (int y = 0; y < 10; y++)
		for (int r = 0; r < 6; r++)
			for (int tr = 0; tr < 2; tr++)
				for (int rev = 0; rev < 4; rev++)
					for (int swap = 0; swap < 2; swap++) {
						cpFloat rotation = rotations[r];
						cpVect shift = tr ? cpv(1e6, -1e6) : cpvzero;
						cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
						cpBodySetAngle(a, 1.5707963267948966 + rotation);
						cpBodySetAngle(b, rotation);
						cpBodySetPosition(a, world(cpv(200, 200), rotation, shift));
						cpBodySetPosition(b, world(cpv(400, ys[y]), rotation, shift));
						cpShape *sa = capsule(a, rev & 1), *sb = capsule(b, rev & 2);
						cpShapeCacheBB(sa);
						cpShapeCacheBB(sb);
						cpContactPointSet points = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
						cpFloat dy = ys[y] - cpfclamp(ys[y], 120, 280),
								expectedDepth = sqrt(120 * 120 + dy * dy) - 160;
						cpVect expected =
							cpvrotate(cpvnormalize(cpv(120, dy)), cpvforangle(rotation));
						if (swap)
							expected = cpvneg(expected);
						cpFloat depth = points.count ? points.points[0].distance : 9999;
						int pass = points.count > 0 && cpvdot(points.normal, expected) >
														   1 - (CP_USE_DOUBLES ? 1e-8 : 1e-4);
						for (int i = 0; i < points.count; i++)
							pass = pass && fabs(points.points[i].distance - expectedDepth) <
											   (CP_USE_DOUBLES ? 1e-5 : 0.5);
						if (!pass)
							failures++;
						total++;
						if (!pass && failures < 9)
							printf("static,%.11g,%.17g,%d,%d,%d,%d,%.17g,%.17g,%.17g,%.17g,%d\n",
								   ys[y], (double)rotation, tr, rev, swap, points.count,
								   (double)points.normal.x, (double)points.normal.y, (double)depth,
								   (double)expectedDepth, pass);
						cpShapeFree(sa);
						cpShapeFree(sb);
						cpBodyFree(a);
						cpBodyFree(b);
					}
	printf("static: %d/%d failures\n", failures, total);
	return failures;
}
struct Events {
	int step, first, resolve;
	cpFloat x, impulse;
	cpVect normal;
};
static void post_solve(cpArbiter *arb, cpSpace *space, void *data)
{
	struct Events *e = (struct Events *)data;
	(void)space;
	cpFloat impulse = cpvlength(cpArbiterTotalImpulse(arb));
	if (e->first < 0)
		e->first = e->step;
	if (e->resolve < 0 && impulse > 1e-8) {
		e->resolve = e->step;
		e->impulse = impulse;
		e->normal = cpArbiterGetNormal(arb);
	}
}
static int dynamics(void)
{
	int failures = 0;
	const cpFloat ys[] = {120, 280, 120.001, 279.999};
	for (int i = 0; i < 4; i++) {
		cpSpace *space = cpSpaceNew();
		cpBody *a = cpBodyNew(0, 0), *b = cpBodyNew(0, 0);
		cpShape *sa = capsule(a, 0), *sb = capsule(b, 0);
		cpShapeSetMass(sa, 1);
		cpShapeSetMass(sb, 1);
		cpBodySetAngle(a, 1.5707963267948966);
		cpBodySetPosition(a, cpv(200, 200));
		cpBodySetPosition(b, cpv(500, ys[i]));
		cpSpaceAddBody(space, a);
		cpSpaceAddShape(space, sa);
		cpSpaceAddBody(space, b);
		cpSpaceAddShape(space, sb);
		cpBodyApplyImpulseAtLocalPoint(b, cpv(-100, 0), cpvzero);
		struct Events e = {0, -1, -1, 0, 0, {0, 0}};
		cpCollisionHandler *handler = cpSpaceAddGlobalCollisionHandler(space);
		handler->postSolveFunc = post_solve;
		handler->userData = &e;
		for (e.step = 0; e.step < 200; e.step++)
			cpSpaceStep(space, .02);
		if (e.resolve < 28 || e.resolve > 30 || fabs(e.normal.x) < .999)
			failures++;
		printf("dynamic y=%.11g first=%d resolve=%d normal=(%.17g,%.17g) impulse=%.17g\n",
			   (double)ys[i], e.first, e.resolve, (double)e.normal.x, (double)e.normal.y,
			   (double)e.impulse);
		cpSpaceRemoveShape(space, sa);
		cpSpaceRemoveShape(space, sb);
		cpSpaceRemoveBody(space, a);
		cpSpaceRemoveBody(space, b);
		cpShapeFree(sa);
		cpShapeFree(sb);
		cpBodyFree(a);
		cpBodyFree(b);
		cpSpaceFree(space);
	}
	return failures;
}

// Use double intermediates for the oracle even when the engine uses floats.
// Normalizing the float projection residual near a core intersection can
// amplify its coordinate-rounding error into an unrelated reference normal.
struct ReferenceVect {
	double x, y;
};
static struct ReferenceVect reference_project(struct ReferenceVect p, struct ReferenceVect a,
											  struct ReferenceVect b)
{
	double dx = b.x - a.x, dy = b.y - a.y;
	double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / (dx * dx + dy * dy);
	t = fmax(0, fmin(1, t));
	struct ReferenceVect projected = {a.x + t * dx, a.y + t * dy};
	return projected;
}
static cpVect reference_direction(cpVect a, cpVect b, cpVect c, cpVect d)
{
	struct ReferenceVect aa = {a.x, a.y}, bb = {b.x, b.y}, cc = {c.x, c.y}, dd = {d.x, d.y};
	struct ReferenceVect as[] = {aa, bb, reference_project(cc, aa, bb),
								 reference_project(dd, aa, bb)};
	struct ReferenceVect bs[] = {reference_project(aa, cc, dd), reference_project(bb, cc, dd), cc,
								 dd};
	double best = INFINITY, x = 0, y = 0;
	for (int i = 0; i < 4; i++) {
		double dx = bs[i].x - as[i].x, dy = bs[i].y - as[i].y, value = dx * dx + dy * dy;
		if (value < best) {
			best = value;
			x = dx;
			y = dy;
		}
	}
	double length = sqrt(best);
	return length > 0 ? cpv((cpFloat)(x / length), (cpFloat)(y / length)) : cpvzero;
}

static uint32_t seed = 19;
static double random01(void)
{
	seed = 1664525u * seed + 1013904223u;
	return (seed >> 8) / 16777216.0;
}
static cpVect project(cpVect p, cpVect a, cpVect b)
{
	cpVect d = cpvsub(b, a);
	return cpvadd(a, cpvmult(d, cpfclamp01(cpvdot(cpvsub(p, a), d) / cpvlengthsq(d))));
}
static cpFloat closest(cpVect a, cpVect b, cpVect c, cpVect d, cpVect *pa, cpVect *pb)
{
	cpVect aCandidates[] = {a, b, project(c, a, b), project(d, a, b)},
		   bCandidates[] = {project(a, c, d), project(b, c, d), c, d};
	cpFloat best = INFINITY;
	for (int i = 0; i < 4; i++) {
		cpFloat x = cpvdistsq(aCandidates[i], bCandidates[i]);
		if (x < best) {
			best = x;
			*pa = aCandidates[i];
			*pb = bCandidates[i];
		}
	}
	return sqrt(best);
}
static cpBool crosses(cpVect a, cpVect b, cpVect c, cpVect d)
{
	cpVect ab = cpvsub(b, a), cd = cpvsub(d, c);
	cpFloat den = cpvcross(ab, cd);
	if (den == 0)
		return cpFalse;
	cpFloat t = cpvcross(cpvsub(c, a), cd) / den, u = cpvcross(cpvsub(c, a), ab) / den;
	return t >= 0 && t <= 1 && u >= 0 && u <= 1;
}
static int geometry_audit(void)
{
	int fail = 0, normalFail = 0, countFail = 0, parallelFail = 0, neighborFail = 0, depthFail = 0;
	for (int i = 0; i < 20000; i++) {
		cpFloat angle = 6.283185307179586 * random01(), length = 10 + 150 * random01(),
				r1 = 1 + 79 * random01(), r2 = 1 + 79 * random01();
		cpVect unit = cpvforangle(angle), a = cpv(500 * random01() - 250, 500 * random01() - 250),
			   b = cpvadd(a, cpvmult(unit, length));
		cpVect c = cpv(500 * random01() - 250, 500 * random01() - 250),
			   d = cpvadd(
				   c, cpvmult(cpvforangle(6.283185307179586 * random01()), 10 + 150 * random01()));
		if (i % 5 == 0) {
			c = cpvadd(b, cpvmult(unit, 10 + 100 * random01()));
			d = cpvadd(c, cpvmult(unit, 10 + 150 * random01()));
		}
		cpBody *bodyA = cpBodyNew(1, 1), *bodyB = cpBodyNew(1, 1);
		cpShape *sa = cpSegmentShapeNew(bodyA, a, b, r1), *sb = cpSegmentShapeNew(bodyB, c, d, r2);
		cpShapeCacheBB(sa);
		cpShapeCacheBB(sb);
		cpContactPointSet hit = cpShapesCollide(sa, sb);
		cpVect pa, pb;
		cpBool cross = (i % 5 != 0) && crosses(a, b, c, d);
		cpFloat distance = cross ? 0 : closest(a, b, c, d, &pa, &pb);
		cpBool expected = distance < r1 + r2;
		int badCount = (hit.count > 0) != expected, badNormal = 0;
		if (hit.count) {
			badNormal = !isfinite(hit.normal.x) || !isfinite(hit.normal.y) ||
						fabs(cpvlengthsq(hit.normal) - 1) > (CP_USE_DOUBLES ? 1e-7 : 1e-4);
			if (!cross && distance > 1e-6)
				badNormal = badNormal || cpvdot(hit.normal, reference_direction(a, b, c, d)) <
											 1 - (CP_USE_DOUBLES ? 1e-7 : 1e-4);
		}
		int badDepth = 0;
		if (hit.count) {
			double expectedDepth = distance - r1 - r2;
			if (cross) {
				double best = -INFINITY;
				cpVect axes[] = {cpvnormalize(cpvperp(cpvsub(b, a))),
								 cpvnormalize(cpvperp(cpvsub(d, c)))};
				for (int k = 0; k < 2; k++) {
					cpVect n = axes[k];
					double aa = cpvdot(a, n), bb = cpvdot(b, n), cc = cpvdot(c, n),
						   dd = cpvdot(d, n);
					double gap = fmax(fmin(cc, dd) - fmax(aa, bb), fmin(aa, bb) - fmax(cc, dd));
					best = fmax(best, gap);
				}
				expectedDepth = best - r1 - r2;
			}
			double actual = INFINITY;
			for (int k = 0; k < hit.count; k++)
				actual = fmin(actual, hit.points[k].distance);
			badDepth = fabs(actual - expectedDepth) > (CP_USE_DOUBLES ? 1e-5 : 0.002);
			if (badDepth) {
				depthFail++;
				if (depthFail < 8)
					fprintf(stderr, "DEPTH case%d expected%.17g actual%.17g cross%d\n", i,
							expectedDepth, actual, cross);
			}
		}
		if (badCount || badNormal) {
			fail++;
			countFail += badCount;
			normalFail += badNormal;
			if (fail < 9)
				printf("random %d collinear=%d distance=%.17g radii=%.17g count=%d "
					   "normal=(%.17g,%.17g) countFail=%d normalFail=%d\n",
					   i, i % 5 == 0, (double)distance, (double)(r1 + r2), hit.count,
					   (double)hit.normal.x, (double)hit.normal.y, badCount, badNormal);
		}
		cpShapeFree(sa);
		cpShapeFree(sb);
		cpBodyFree(bodyA);
		cpBodyFree(bodyB);
	}
	for (int rev = 0; rev < 4; rev++)
		for (int swap = 0; swap < 2; swap++) {
			cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
			cpVect p = cpv(-80, 0), q = cpv(80, 0), u = cpv(-80, 100), v = cpv(80, 100);
			cpShape *sa = cpSegmentShapeNew(a, rev & 1 ? q : p, rev & 1 ? p : q, 80),
					*sb = cpSegmentShapeNew(b, rev & 2 ? v : u, rev & 2 ? u : v, 80);
			cpShapeCacheBB(sa);
			cpShapeCacheBB(sb);
			cpContactPointSet points = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
			if (points.count != 2)
				parallelFail++;
			cpShapeFree(sa);
			cpShapeFree(sb);
			cpBodyFree(a);
			cpBodyFree(b);
		}
	for (int rev = 0; rev < 4; rev++)
		for (int swap = 0; swap < 2; swap++) {
			cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
			cpVect p = cpv(-80, 0), q = cpv(80, 0), u = cpv(-100, -5), v = cpv(-100, 5);
			cpShape *sa = cpSegmentShapeNew(a, rev & 1 ? q : p, rev & 1 ? p : q, 20),
					*sb = cpSegmentShapeNew(b, rev & 2 ? v : u, rev & 2 ? u : v, 20);
			cpShapeCacheBB(sa);
			cpShapeCacheBB(sb);
			cpContactPointSet plain = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
			cpSegmentShapeSetNeighbors(sa, rev & 1 ? cpv(160, 0) : cpv(-160, 0),
									   rev & 1 ? cpv(-160, 0) : cpv(160, 0));
			cpContactPointSet filtered = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
			if (plain.count == 0 || filtered.count != 0)
				neighborFail++;
			cpShapeFree(sa);
			cpShapeFree(sb);
			cpBodyFree(a);
			cpBodyFree(b);
		}
	printf("oracle: %d/20000 failed; count=%d normal=%d; parallel manifolds=%d/8 failed; neighbor "
		   "filtering=%d/8 failed\n",
		   fail, countFail, normalFail, parallelFail, neighborFail);
	printf("depth failures %d\n", depthFail);
	return fail + depthFail + parallelFail + neighborFail;
}

static int degenerates(void)
{
	const cpVect endpoints[][4] = {
		{{0, 0}, {0, 0}, {100, 0}, {100, 0}},	{{0, 0}, {0, 0}, {0, 0}, {0, 0}},
		{{0, 0}, {0, 0}, {-80, 0}, {80, 0}},	{{-80, 0}, {80, 0}, {-60, 0}, {100, 0}},
		{{-80, 0}, {80, 0}, {0, -80}, {0, 80}}, {{-80, 0}, {0, 0}, {0, 0}, {0, 80}}};
	const cpFloat coreDepths[] = {100, 0, 0, 0, -80, 0};
	int failures = 0;
	for (int radiusCase = 0; radiusCase < 2; radiusCase++)
		for (int i = 0; i < 6; i++)
			for (int reverse = 0; reverse < 4; reverse++)
				for (int swap = 0; swap < 2; swap++) {
					cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
					const cpVect *v = endpoints[i];
					cpFloat radius = radiusCase ? 0 : 80;
					cpShape *sa = cpSegmentShapeNew(a, v[reverse & 1 ? 1 : 0],
													v[reverse & 1 ? 0 : 1], radius);
					cpShape *sb = cpSegmentShapeNew(b, v[reverse & 2 ? 3 : 2],
													v[reverse & 2 ? 2 : 3], radius);
					cpShapeCacheBB(sa);
					cpShapeCacheBB(sb);
					for (int repeat = 0; repeat < 3; repeat++) {
						cpContactPointSet points = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
						cpFloat depth = INFINITY;
						for (int j = 0; j < points.count; j++)
							depth = cpfmin(depth, points.points[j].distance);
						cpFloat expected = coreDepths[i] - 2 * radius;
						if ((expected > 0 && points.count != 0) ||
							(expected <= 0 &&
							 (points.count == 0 || fabs(cpvlengthsq(points.normal) - 1) > 1e-5 ||
							  fabs(depth - expected) > 1e-3))) {
							failures++;
							fprintf(stderr,
									"degenerate %d reverse %d swap %d count %d depth %.17g\n", i,
									reverse, swap, points.count, (double)depth);
						}
					}
					cpShapeFree(sa);
					cpShapeFree(sb);
					cpBodyFree(a);
					cpBodyFree(b);
				}
	return failures;
}

struct QueryResult {
	int contacts;
};
static void query_callback(cpShape *shape, cpContactPointSet *points, void *data)
{
	(void)shape;
	((struct QueryResult *)data)->contacts += points->count;
}
static int overlap_queries(void)
{
	int failures = 0;
	for (int top = 0; top < 2; top++)
		for (int reverse = 0; reverse < 4; reverse++)
			for (int swap = 0; swap < 2; swap++) {
				cpSpace *space = cpSpaceNew();
				cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
				cpBodySetAngle(a, 1.5707963267948966);
				cpBodySetPosition(a, cpv(200, 200));
				cpBodySetPosition(b, cpv(400, top ? 280 : 120));
				cpShape *sa = capsule(a, reverse & 1), *sb = capsule(b, reverse & 2);
				cpShape *installed = swap ? sb : sa, *query = swap ? sa : sb;
				cpBody *body = swap ? b : a;
				cpSpaceAddBody(space, body);
				cpSpaceAddShape(space, installed);
				struct QueryResult result = {0};
				cpSpaceShapeQuery(space, query, query_callback, &result);
				if (!result.contacts)
					failures++;
				cpSpaceRemoveShape(space, installed);
				cpSpaceRemoveBody(space, body);
				cpShapeFree(sa);
				cpShapeFree(sb);
				cpBodyFree(a);
				cpBodyFree(b);
				cpSpaceFree(space);
			}
	return failures;
}
static int persistence(void)
{
	int failures = 0;
	cpContactSolverType solvers[] = {CP_CONTACT_SOLVER_BASELINE, CP_CONTACT_SOLVER_AVX2,
									 CP_CONTACT_SOLVER_WASM_SIMD};
	for (int i = 0; i < 3; i++)
		if (cpContactSolverIsAvailable(solvers[i])) {
			cpSpace *space = cpSpaceNew();
			cpSpaceSetGravity(space, cpv(0, -100));
			cpSpaceSetSleepTimeThreshold(space, .5);
			if (!cpSpaceSetContactSolver(space, solvers[i]))
				failures++;
			cpShape *floor =
				cpSegmentShapeNew(cpSpaceGetStaticBody(space), cpv(-80, 0), cpv(80, 0), 10);
			cpBody *body = cpBodyNew(0, 0);
			cpShape *shape = cpSegmentShapeNew(body, cpv(-80, 0), cpv(80, 0), 10);
			cpShapeSetMass(shape, 1);
			cpBodySetPosition(body, cpv(0, 30));
			cpSpaceAddShape(space, floor);
			cpSpaceAddBody(space, body);
			cpSpaceAddShape(space, shape);
			for (int step = 0; step < 600; step++)
				cpSpaceStep(space, 1.0 / 120);
			if (!cpBodyIsSleeping(body) || fabs(cpBodyGetPosition(body).y - 20) > .5)
				failures++;
			cpBodyApplyImpulseAtLocalPoint(body, cpv(0, 50), cpvzero);
			if (cpBodyIsSleeping(body))
				failures++;
			for (int step = 0; step < 600; step++)
				cpSpaceStep(space, 1.0 / 120);
			if (!cpBodyIsSleeping(body) || fabs(cpBodyGetPosition(body).y - 20) > .5)
				failures++;
			cpSpaceRemoveShape(space, floor);
			cpSpaceRemoveShape(space, shape);
			cpSpaceRemoveBody(space, body);
			cpShapeFree(floor);
			cpShapeFree(shape);
			cpBodyFree(body);
			cpSpaceFree(space);
		}
	return failures;
}
static int transformed_manifolds_and_neighbors(void)
{
	const cpFloat angles[] = {0,
							  .5235987755982988,
							  .7853981633974483,
							  1.5707963267948966,
							  3.141592653589793,
							  -.4487989505128276};
	int failures = 0;
	for (int scenario = 0; scenario < 2; scenario++)
		for (int angle = 0; angle < 6; angle++)
			for (int translated = 0; translated < 2; translated++)
				for (int reverse = 0; reverse < 4; reverse++)
					for (int swap = 0; swap < 2; swap++) {
						// Float's large-world coordinate resolution is tested by the main
						// matrix. Use a representable parallel-face fixture here.
						cpFloat offset = CP_USE_DOUBLES ? 1e6 : 1e3;
						cpVect shift = translated ? cpv(offset, -offset) : cpvzero;
						cpVect p = world(cpv(-80, 0), angles[angle], shift);
						cpVect q = world(cpv(80, 0), angles[angle], shift);
						cpVect u =
							world(scenario ? cpv(-100, -5) : cpv(-80, 100), angles[angle], shift);
						cpVect v =
							world(scenario ? cpv(-100, 5) : cpv(80, 100), angles[angle], shift);
						cpBody *a = cpBodyNew(1, 1), *b = cpBodyNew(1, 1);
						cpFloat radius = scenario ? 20 : 80;
						cpShape *sa =
							cpSegmentShapeNew(a, reverse & 1 ? q : p, reverse & 1 ? p : q, radius);
						cpShape *sb =
							cpSegmentShapeNew(b, reverse & 2 ? v : u, reverse & 2 ? u : v, radius);
						cpShapeCacheBB(sa);
						cpShapeCacheBB(sb);
						cpContactPointSet plain = cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
						if (!scenario) {
							if (plain.count != 2)
								failures++;
						} else {
							cpVect prev = world(cpv(-160, 0), angles[angle], shift);
							cpVect next = world(cpv(160, 0), angles[angle], shift);
							cpSegmentShapeSetNeighbors(sa, reverse & 1 ? next : prev,
													   reverse & 1 ? prev : next);
							cpContactPointSet filtered =
								cpShapesCollide(swap ? sb : sa, swap ? sa : sb);
							if (plain.count == 0 || filtered.count != 0)
								failures++;
						}
						cpShapeFree(sa);
						cpShapeFree(sb);
						cpBodyFree(a);
						cpBodyFree(b);
					}
	return failures;
}

static int precision_fixture(void)
{
#if !CP_USE_DOUBLES
	cpVect a = cpv(-132.83753967285156, 163.17552185058594),
		   b = cpv(-91.8529052734375, 259.976806640625);
	cpVect c = cpv(-101.35692596435547, 237.53038024902344),
		   d = cpv(-153.91110229492188, 163.99557495117188);
	cpBody *bodyA = cpBodyNew(1, 1), *bodyB = cpBodyNew(1, 1);
	cpShape *sa = cpSegmentShapeNew(bodyA, a, b, 80), *sb = cpSegmentShapeNew(bodyB, c, d, 80);
	cpShapeCacheBB(sa);
	cpShapeCacheBB(sb);
	cpContactPointSet hit = cpShapesCollide(sa, sb);
	int failed = hit.count == 0 || cpvdot(hit.normal, reference_direction(a, b, c, d)) < 1 - 1e-6;
	cpShapeFree(sa);
	cpShapeFree(sb);
	cpBodyFree(bodyA);
	cpBodyFree(bodyB);
	return failed;
#else
	return 0;
#endif
}

int main(void)
{
	int failures = static_probes() + dynamics() + geometry_audit() + degenerates() +
				   overlap_queries() + persistence() + transformed_manifolds_and_neighbors() +
				   precision_fixture();
	if (failures) {
		fprintf(stderr, "capsule regression failures: %d\n", failures);
		return 1;
	}
	puts("PASS: capsule matrix, geometry/depth oracle, degenerate cores, overlap queries, "
		 "manifolds, neighbors and persistent contacts.");
	return 0;
}
