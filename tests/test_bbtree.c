#include <stdio.h>
#include <stdlib.h>

#if !defined(_WIN32)
#include <sys/resource.h>
#endif

#include "chipmunk/chipmunk.h"

static void
limit_stack_for_regression(void)
{
#if !defined(_WIN32)
  struct rlimit limit;

  limit.rlim_cur = 256 * 1024;
  limit.rlim_max = 256 * 1024;
  setrlimit(RLIMIT_STACK, &limit);
#endif
}

static void
count_shape(cpShape *shape, void *data)
{
  int *count = (int *)data;
  (void)shape;
  (*count)++;
}

static void
free_space_shapes(cpSpace *space, cpShape **shapes, int count)
{
  int i;
  for(i = 0; i < count; i++) {
    cpSpaceRemoveShape(space, shapes[i]);
    cpShapeFree(shapes[i]);
  }
}

int
main(void)
{
  const int count = 4000;
  int i;
  int hits = 0;
  cpSpace *space = cpSpaceNew();
  cpShape **shapes = (cpShape **)calloc((size_t)count, sizeof(cpShape *));
  cpBody *body = cpSpaceGetStaticBody(space);

  if(space == NULL || shapes == NULL) {
    fprintf(stderr, "Failed to allocate bb tree test resources.\n");
    free(shapes);
    cpSpaceFree(space);
    return 1;
  }

  limit_stack_for_regression();

  for(i = 0; i < count; i++) {
    shapes[i] = cpCircleShapeNew(body, 1.0, cpvzero);
    cpSpaceAddShape(space, shapes[i]);
  }

  cpSpaceBBQuery(space, cpBBNew(-2, -2, 2, 2), CP_SHAPE_FILTER_ALL, count_shape, &hits);
  if(hits != count) {
    fprintf(stderr, "Expected %d query hits, got %d.\n", count, hits);
    free_space_shapes(space, shapes, count);
    free(shapes);
    cpSpaceFree(space);
    return 1;
  }

  free_space_shapes(space, shapes, count);
  free(shapes);
  cpSpaceFree(space);

  printf("Inserted and queried %d degenerate shapes without overflowing the BB tree traversal stack.\n", count);
  return 0;
}
