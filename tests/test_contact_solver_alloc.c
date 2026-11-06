/* Compile the allocator/scheduler with a failing allocator. The archive's copy
 * of this translation unit is not pulled in: this test supplies its symbols. */
#include <stdio.h>
#include <stdlib.h>
#include "../src/cpContactSolver.h"
#ifdef __EMSCRIPTEN__
#define TEST_CONTACT_SOLVER CP_CONTACT_SOLVER_WASM_SIMD
#else
#define TEST_CONTACT_SOLVER CP_CONTACT_SOLVER_AVX2
#endif
static int failAllocation;
static void *testCalloc(size_t count,size_t size){return failAllocation?NULL:calloc(count,size);}
#undef cpcalloc
#define cpcalloc testCalloc
#include "../src/cpContactSolver.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL allocation test line%d\n",__LINE__);exit(1);}}while(0)
static void mixedStepFailures(void)
{
 if(!cpContactSolverIsAvailable(CP_CONTACT_SOLVER_AVX2))return;
 failAllocation=0;
 cpSpace *space=cpSpaceNew();CHECK(cpSpaceSetContactSolver(space,CP_CONTACT_SOLVER_AVX2));
 cpContactSolverContext *s=cpContactSolverGet(space);
 cpBody *bodies[4];cpArbiter arbiters[4];struct cpContact contacts[4];
 memset(arbiters,0,sizeof(arbiters));memset(contacts,0,sizeof(contacts));
 for(int i=0;i<4;i++){
  bodies[i]=cpSpaceAddBody(space,cpBodyNew(1,1));
  cpBodySetPosition(bodies[i],cpv(i*2,0));cpBodySetVelocity(bodies[i],cpv(i+1,-i));
  arbiters[i].body_a=bodies[i];arbiters[i].body_b=cpSpaceGetStaticBody(space);
  arbiters[i].count=1;arbiters[i].contacts=&contacts[i];
  cpArrayPush(space->arbiters,&arbiters[i]);
 }
 cpConstraint *joint=cpSpaceAddConstraint(space,cpPinJointNew(bodies[0],bodies[1],cpvzero,cpvzero));
 CHECK(reserveGraph(s,4));
 failAllocation=1;CHECK(!cpContactSolverStep(space,s));CHECK(!s->mixedMemory&&!s->solverMemory);
 CHECK(!s->usedLastStep&&s->bodyCount==0&&s->packetCount==0&&s->jointBodyCount==0);
 failAllocation=0;CHECK(reserveMixedWork(s,4));void *mixed=s->mixedMemory;
 failAllocation=1;CHECK(!cpContactSolverStep(space,s));CHECK(s->mixedMemory==mixed&&!s->solverMemory);
 CHECK(!s->usedLastStep&&s->bodyCount==0&&s->packetCount==0&&s->jointBodyCount==0);
 for(int i=0;i<4;i++){
  CHECK(bodies[i]->v.x==i+1&&bodies[i]->v.y==-i);
  CHECK(contacts[i].jBias==0&&contacts[i].jnAcc==0&&contacts[i].jtAcc==0);
 }
 failAllocation=0;space->arbiters->num=0;
 cpSpaceRemoveConstraint(space,joint);cpConstraintFree(joint);
 for(int i=0;i<4;i++){cpSpaceRemoveBody(space,bodies[i]);cpBodyFree(bodies[i]);}
 cpSpaceFree(space);
}
int main(void)
{
 cpContactSolverContext s;memset(&s,0,sizeof(s));
 failAllocation=1;CHECK(!reserveGraph(&s,4));CHECK(!s.graphMemory&&s.graphCapacity==0);
 failAllocation=0;CHECK(reserveGraph(&s,4));void *graph=s.graphMemory;int cap=s.graphCapacity;
 failAllocation=1;CHECK(!reserveGraph(&s,cap+1));CHECK(s.graphMemory==graph&&s.graphCapacity==cap);
 CHECK(!reserveSolver(&s,8,2));CHECK(!s.solverMemory);
 failAllocation=0;CHECK(reserveSolver(&s,8,2));void *solver=s.solverMemory;int bodies=s.solverBodyCapacity,packets=s.solverPacketCapacity;
 failAllocation=1;CHECK(!reserveSolver(&s,bodies+1,packets+1));CHECK(s.solverMemory==solver&&s.solverBodyCapacity==bodies&&s.solverPacketCapacity==packets);
 CHECK(!reserveMixedWork(&s,4));CHECK(!s.mixedMemory&&s.mixedCapacity==0);
 failAllocation=0;CHECK(reserveMixedWork(&s,4));void *mixed=s.mixedMemory;int mixedCapacity=s.mixedCapacity;
 int *jointBodies=s.jointBodies;cpContactSolverDependency *dependencies=s.dependencies;
 failAllocation=1;CHECK(!reserveMixedWork(&s,mixedCapacity+1));
 CHECK(s.mixedMemory==mixed&&s.mixedCapacity==mixedCapacity&&s.jointBodies==jointBodies&&s.dependencies==dependencies);
 size_t bytes=SIZE_MAX-3;CHECK(!addRegion(&bytes,1,8));bytes=0;CHECK(!addRegion(&bytes,SIZE_MAX,8));
 free(s.graphMemory);free(s.solverMemory);free(s.mixedMemory);
 failAllocation=0;cpSpace *space=cpSpaceNew();
 if(cpContactSolverIsAvailable(TEST_CONTACT_SOLVER)){
  failAllocation=1;CHECK(!cpSpaceSetContactSolver(space,TEST_CONTACT_SOLVER));CHECK(cpSpaceGetContactSolver(space)==CP_CONTACT_SOLVER_ORIGINAL);
  failAllocation=0;CHECK(cpSpaceSetContactSolver(space,TEST_CONTACT_SOLVER));
  for(int i=0;i<4;i++)cpArrayPush(space->arbiters,NULL);
  failAllocation=1;CHECK(!cpContactSolverStep(space,cpContactSolverGet(space)));CHECK(!cpContactSolverGet(space)->usedLastStep);CHECK(!cpContactSolverGet(space)->graphMemory);
  CHECK(cpSpaceGetContactSolver(space)==TEST_CONTACT_SOLVER);space->arbiters->num=0;
 }
 failAllocation=0;cpSpaceFree(space);mixedStepFailures();
 puts("PASS: scratch allocation failure preserves previous arenas; checked sizing; failed selection and graph/mixed/packet step allocations leave bodies and contacts uncommitted.");return 0;
}
