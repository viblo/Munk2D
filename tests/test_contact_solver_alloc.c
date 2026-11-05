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
int main(void)
{
 cpContactSolverContext s;memset(&s,0,sizeof(s));
 failAllocation=1;CHECK(!reserveGraph(&s,4));CHECK(!s.graphMemory&&s.graphCapacity==0);
 failAllocation=0;CHECK(reserveGraph(&s,4));void *graph=s.graphMemory;int cap=s.graphCapacity;
 failAllocation=1;CHECK(!reserveGraph(&s,cap+1));CHECK(s.graphMemory==graph&&s.graphCapacity==cap);
 CHECK(!reserveSolver(&s,8,2));CHECK(!s.solverMemory);
 failAllocation=0;CHECK(reserveSolver(&s,8,2));void *solver=s.solverMemory;int bodies=s.solverBodyCapacity,packets=s.solverPacketCapacity;
 failAllocation=1;CHECK(!reserveSolver(&s,bodies+1,packets+1));CHECK(s.solverMemory==solver&&s.solverBodyCapacity==bodies&&s.solverPacketCapacity==packets);
 size_t bytes=SIZE_MAX-3;CHECK(!addRegion(&bytes,1,8));bytes=0;CHECK(!addRegion(&bytes,SIZE_MAX,8));
 free(s.graphMemory);free(s.solverMemory);
 failAllocation=0;cpSpace *space=cpSpaceNew();
 if(cpContactSolverIsAvailable(TEST_CONTACT_SOLVER)){
  failAllocation=1;CHECK(!cpSpaceSetContactSolver(space,TEST_CONTACT_SOLVER));CHECK(cpSpaceGetContactSolver(space)==CP_CONTACT_SOLVER_ORIGINAL);
  failAllocation=0;CHECK(cpSpaceSetContactSolver(space,TEST_CONTACT_SOLVER));
  for(int i=0;i<4;i++)cpArrayPush(space->arbiters,NULL);
  failAllocation=1;CHECK(!cpContactSolverStep(space,cpContactSolverGet(space)));CHECK(!cpContactSolverGet(space)->usedLastStep);CHECK(!cpContactSolverGet(space)->graphMemory);
  CHECK(cpSpaceGetContactSolver(space)==TEST_CONTACT_SOLVER);space->arbiters->num=0;
 }
 failAllocation=0;cpSpaceFree(space);
 puts("PASS: scratch allocation failure preserves previous arenas; checked sizing; failed selection and step allocation leave state consistent.");return 0;
}
