/* Private contact-solver state. No ISA types escape the AVX2 translation unit. */
#ifndef CP_CONTACT_SOLVER_INTERNAL_H
#define CP_CONTACT_SOLVER_INTERNAL_H
#include "chipmunk/chipmunk_private.h"
#include <stdint.h>
#include <stddef.h>

#ifndef CP_AVX2_CONTACT_SOLVER
#define CP_AVX2_CONTACT_SOLVER 0
#endif
#define CP_CONTACT_SOLVER_LANES 4
#define CP_CONTACT_SOLVER_COLORS 12
#define CP_CONTACT_SOLVER_KINDS 12

typedef struct cpContactSolverContact {
 double r1x[4], r1y[4], r2x[4], r2y[4];
 double nMass[4], tMass[4], bias[4], bounce[4];
 double jBias[4], jnAcc[4], jtAcc[4];
 struct cpContact *original[4];
} cpContactSolverContact;

typedef struct cpContactSolverPacket {
 int a[4], b[4], lanes, count, mode, friction;
 double nx[4], ny[4], sx[4], sy[4], u[4];
 double am[4], ai[4], bm[4], bi[4];
 cpContactSolverContact con[2];
} cpContactSolverPacket;

typedef struct cpContactSolverLink { int a, b, kind, next; } cpContactSolverLink;
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
 cpContactSolverKernel kernel;
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
CP_CONTACT_SOLVER_PRIVATE cpBool cpContactSolverCpuSupported(void);
/* Pure decision function also permits testing unavailable CPU/OS combinations. */
CP_CONTACT_SOLVER_PRIVATE cpBool cpContactSolverCheckFeatures(unsigned int maxLeaf, unsigned int ecx1,
 unsigned int ebx7, uint64_t xcr0);
#endif
