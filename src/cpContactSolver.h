/* Private contact-solver state. No ISA types escape the backend translation units. */
#ifndef CP_CONTACT_SOLVER_INTERNAL_H
#define CP_CONTACT_SOLVER_INTERNAL_H
#include "chipmunk/chipmunk_private.h"
#include <stdint.h>
#include <stddef.h>

#ifndef CP_AVX2_CONTACT_SOLVER
#define CP_AVX2_CONTACT_SOLVER 0
#endif
#ifndef CP_WASM_SIMD_CONTACT_SOLVER
#define CP_WASM_SIMD_CONTACT_SOLVER 0
#endif
/* Logical packet width, not ISA width. Wasm solves a four-contact packet in
 * independent two-double halves. A two-contact layout was also evaluated. */
#ifndef CP_CONTACT_SOLVER_LANES
#define CP_CONTACT_SOLVER_LANES 4
#endif
#define CP_CONTACT_SOLVER_COLORS 12
#define CP_CONTACT_SOLVER_KINDS 12

typedef struct cpContactSolverContact {
 double r1x[CP_CONTACT_SOLVER_LANES], r1y[CP_CONTACT_SOLVER_LANES], r2x[CP_CONTACT_SOLVER_LANES], r2y[CP_CONTACT_SOLVER_LANES];
 double nMass[CP_CONTACT_SOLVER_LANES], tMass[CP_CONTACT_SOLVER_LANES], bias[CP_CONTACT_SOLVER_LANES], bounce[CP_CONTACT_SOLVER_LANES];
 double jBias[CP_CONTACT_SOLVER_LANES], jnAcc[CP_CONTACT_SOLVER_LANES], jtAcc[CP_CONTACT_SOLVER_LANES];
 struct cpContact *original[CP_CONTACT_SOLVER_LANES];
} cpContactSolverContact;

typedef struct cpContactSolverPacket {
 int a[CP_CONTACT_SOLVER_LANES], b[CP_CONTACT_SOLVER_LANES], lanes, count, mode, friction;
 double nx[CP_CONTACT_SOLVER_LANES], ny[CP_CONTACT_SOLVER_LANES], sx[CP_CONTACT_SOLVER_LANES], sy[CP_CONTACT_SOLVER_LANES], u[CP_CONTACT_SOLVER_LANES];
 double am[CP_CONTACT_SOLVER_LANES], ai[CP_CONTACT_SOLVER_LANES], bm[CP_CONTACT_SOLVER_LANES], bi[CP_CONTACT_SOLVER_LANES];
 cpContactSolverContact con[2];
} cpContactSolverPacket;

typedef struct cpContactSolverLink { int a, b, kind, next; } cpContactSolverLink;
typedef struct cpContactSolverDependency { int pending, successors[2]; } cpContactSolverDependency;
typedef struct cpContactSolverContext cpContactSolverContext;
typedef void (*cpContactSolverKernel)(cpContactSolverContext *, int);

struct cpContactSolverContext {
 void *graphMemory, *solverMemory;
 int graphCapacity, solverBodyCapacity, solverPacketCapacity;
 cpBody **bodies;
 int *hash;
 unsigned int *colors;
 cpContactSolverLink *links;
 /* Body-major vx, vy, w, bias vx, bias vy, bias w (six doubles/body). */
 double *velocity;
 cpContactSolverPacket *packets;
 int bodyCount, packetCount, hashMask;
 /* Mixed steps preserve contact dependencies and synchronize only bodies
  * shared by contacts and joints. This lazy arena leaves joint-free graph
  * and packet allocations unchanged. Per-step counts are cleared on return. */
 void *mixedMemory;
 int mixedCapacity;
 cpContactSolverDependency *dependencies;
 int *mixedOrder, *jointBodies;
 unsigned char *mixedLanes;
 int jointBodyCount;
 cpContactSolverKernel kernel;
 cpContactSolverType type;
 cpBool usedLastStep;
};

/* A private extension of the allocation ledger, not of cpSpace or cpArray.
 * The first member remains an ordinary cpArray: its contents/growth/freeing
 * retain their original semantics. The optional context is owned separately. */
typedef struct cpSpaceBufferStorage {
 cpArray array;
 cpContactSolverContext *solver;
} cpSpaceBufferStorage;
typedef char cpSpaceBufferStoragePrefixCheck[(offsetof(cpSpaceBufferStorage, array) == 0) ? 1 : -1];

static inline cpContactSolverContext *
cpContactSolverGet(const cpSpace *space)
{
 return ((const cpSpaceBufferStorage *)space->allocatedBuffers)->solver;
}

#if defined(__GNUC__) || defined(__clang__)
#define CP_CONTACT_SOLVER_PRIVATE __attribute__((visibility("hidden")))
#else
#define CP_CONTACT_SOLVER_PRIVATE
#endif
CP_CONTACT_SOLVER_PRIVATE cpArray *cpSpaceBufferArrayNew(void);
CP_CONTACT_SOLVER_PRIVATE void cpContactSolverDestroy(cpSpace *space);
CP_CONTACT_SOLVER_PRIVATE cpBool cpContactSolverStep(cpSpace *space, cpContactSolverContext *context);
CP_CONTACT_SOLVER_PRIVATE void cpContactSolverKernelScalar(cpContactSolverContext *context, int iterations);
#if CP_AVX2_CONTACT_SOLVER && CP_USE_DOUBLES
CP_CONTACT_SOLVER_PRIVATE void cpContactSolverKernelAvx2(cpContactSolverContext *context, int iterations);
#endif
#if CP_WASM_SIMD_CONTACT_SOLVER && CP_USE_DOUBLES
CP_CONTACT_SOLVER_PRIVATE void cpContactSolverKernelWasm(cpContactSolverContext *context, int iterations);
#endif
CP_CONTACT_SOLVER_PRIVATE cpBool cpContactSolverCpuSupported(void);
/* Pure decision function also permits testing unavailable CPU/OS combinations. */
CP_CONTACT_SOLVER_PRIVATE cpBool cpContactSolverCheckFeatures(unsigned int maxLeaf, unsigned int ecx1,
 unsigned int ebx7, uint64_t xcr0);
#endif
