#include <math.h>
#include <stdio.h>
#include <string.h>

#include "chipmunk/chipmunk.h"

static int failures = 0;

static void
fail(const char *message)
{
  ++failures;
  fprintf(stderr, "FAIL: %s\n", message);
}

static void
assert_true(cpBool condition, const char *message)
{
  if(!condition) fail(message);
}

static void
assert_near(cpFloat actual, cpFloat expected, cpFloat tolerance, const char *message)
{
  if(cpfabs(actual - expected) > tolerance) {
    char buffer[512];
    snprintf(
      buffer,
      sizeof(buffer),
      "%s (expected %.17g, got %.17g, tolerance %.17g)",
      message,
      (double)expected,
      (double)actual,
      (double)tolerance
    );
    fail(buffer);
  }
}

static void
assert_vect_near(cpVect actual, cpVect expected, cpFloat tolerance, const char *message)
{
  if(cpvdist(actual, expected) > tolerance) {
    char buffer[512];
    snprintf(
      buffer,
      sizeof(buffer),
      "%s (expected {%.17g, %.17g}, got {%.17g, %.17g})",
      message,
      (double)expected.x,
      (double)expected.y,
      (double)actual.x,
      (double)actual.y
    );
    fail(buffer);
  }
}

static void
test_cpvslerp(void)
{
  cpVect a = cpvmult(cpvforangle(0.0), 1.0);
  cpVect b = cpvmult(cpvforangle(1.0), 1.0);
  cpVect halfway = cpvslerp(a, b, 0.5);
  cpVect expected = cpvmult(cpvforangle(0.5), 1.0);

  assert_vect_near(cpvslerp(a, b, 0.0), a, 1e-9, "cpvslerp returns the start vector at t=0");
  assert_vect_near(cpvslerp(a, b, 1.0), b, 1e-9, "cpvslerp returns the end vector at t=1");
  assert_vect_near(halfway, expected, 1e-9, "cpvslerp returns the expected midpoint");
}

static void
test_cpbbsegmentquery(void)
{
  cpBB bb = cpBBNew(-1, -1, 1, 1);
  cpFloat miss = cpBBSegmentQuery(bb, cpv(2, -2), cpv(0, -2));

  assert_near(
    cpBBSegmentQuery(bb, cpv(-2, 0), cpv(0, 0)),
    0.5,
    1e-9,
    "cpBBSegmentQuery hits the left edge halfway"
  );
  assert_near(
    cpBBSegmentQuery(bb, cpv(-2, -2), cpv(0, 0)),
    0.5,
    1e-9,
    "cpBBSegmentQuery hits a diagonal corner halfway"
  );
  assert_true(
    miss > 1.0,
    "cpBBSegmentQuery returns a no-hit result for a miss"
  );
}

static void
test_core_library_functions(void)
{
  assert_true(cpVersionString != NULL, "cpVersionString is exposed");
  assert_true(strlen(cpVersionString) > 0u, "cpVersionString is not empty");

  assert_near(
    cpAreaForCircle(0.0, 2.0),
    4.0*CP_PI,
    1e-9,
    "cpAreaForCircle matches the area of a radius-2 circle"
  );
  assert_near(
    cpMomentForCircle(3.0, 0.0, 2.0, cpvzero),
    6.0,
    1e-9,
    "cpMomentForCircle matches the solid circle moment of inertia"
  );
}

int
main(void)
{
  test_cpvslerp();
  test_cpbbsegmentquery();
  test_core_library_functions();

  if(failures != 0) {
    fprintf(stderr, "%d test assertion(s) failed.\n", failures);
    return 1;
  }

  printf("All basic chipmunk tests passed.\n");
  return 0;
}
