#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fenv.h>
#include "../src/cpContactSolver.h"
#include "chipmunk/cpHastySpace.h"
#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)

typedef struct Fixture {cpSpace *space;cpBody *body[256];cpShape *shape[257];int count;cpConstraint *joint;cpBool hasty;} Fixture;
static void addBody(Fixture *f, int index, cpBool stack)
{
 cpBody *b=cpSpaceAddBody(f->space,cpBodyNew(1,cpMomentForBox(1,1,1)));
 cpBodySetPosition(b,stack?cpv((index%2)*1.1,0.4+(index/2)*1.01):cpv(-200+2*index,0.4));
 cpShape *shape=cpSpaceAddShape(f->space,cpBoxShapeNew(b,1,1,0));
 cpShapeSetFriction(shape,0.7);f->body[index]=b;f->shape[index]=shape;f->count++;
}
static void makeWithSpace(Fixture *f,int count,cpBool stack,cpSpace *space)
{
 memset(f,0,sizeof(*f));f->space=space;cpSpaceSetGravity(f->space,cpv(0,-10));cpSpaceSetCollisionSlop(f->space,0.005);
 f->shape[256]=cpSpaceAddShape(f->space,cpSegmentShapeNew(cpSpaceGetStaticBody(f->space),cpv(-1000,0),cpv(1000,0),0));
 cpShapeSetFriction(f->shape[256],1);
 for(int i=0;i<count;i++)addBody(f,i,stack);
}
static void make(Fixture *f,int count,cpBool stack){makeWithSpace(f,count,stack,cpSpaceNew());}
static void removeLast(Fixture *f)
{
 int i=--f->count;cpSpaceRemoveShape(f->space,f->shape[i]);cpShapeFree(f->shape[i]);cpSpaceRemoveBody(f->space,f->body[i]);cpBodyFree(f->body[i]);
}
static void cleanup(Fixture *f)
{
 if(f->joint){cpSpaceRemoveConstraint(f->space,f->joint);cpConstraintFree(f->joint);}
 while(f->count)removeLast(f);
 cpSpaceRemoveShape(f->space,f->shape[256]);cpShapeFree(f->shape[256]);if(f->hasty)cpHastySpaceFree(f->space);else cpSpaceFree(f->space);
}
static void compare(Fixture *a,Fixture *b)
{
 CHECK(a->count==b->count);
 for(int i=0;i<a->count;i++){
  cpBody *x=a->body[i],*y=b->body[i];
  CHECK(isfinite(x->p.x)&&isfinite(x->p.y)&&isfinite(x->v.x)&&isfinite(x->v.y)&&isfinite(x->w));
  CHECK(memcmp(&x->p,&y->p,sizeof(cpVect))==0);CHECK(memcmp(&x->v,&y->v,sizeof(cpVect))==0);
  CHECK(memcmp(&x->v_bias,&y->v_bias,sizeof(cpVect))==0);CHECK(memcmp(&x->a,&y->a,sizeof(cpFloat))==0);
  CHECK(memcmp(&x->w,&y->w,sizeof(cpFloat))==0);CHECK(memcmp(&x->w_bias,&y->w_bias,sizeof(cpFloat))==0);
 }
 CHECK(a->space->arbiters->num==b->space->arbiters->num);
 for(int i=0;i<a->space->arbiters->num;i++){
  cpArbiter *x=(cpArbiter *)a->space->arbiters->arr[i],*y=(cpArbiter *)b->space->arbiters->arr[i];CHECK(x->count==y->count);
  for(int j=0;j<x->count;j++){CHECK(x->contacts[j].jnAcc==y->contacts[j].jnAcc);CHECK(x->contacts[j].jtAcc==y->contacts[j].jtAcc);CHECK(x->contacts[j].jBias==y->contacts[j].jBias);}
 }
}
static void features(void)
{
 unsigned int required=(1u<<26)|(1u<<27)|(1u<<28);
 CHECK(cpContactSolverCheckFeatures(7,required,1u<<5,6));
 for(int bit=26;bit<=28;bit++)CHECK(!cpContactSolverCheckFeatures(7,required&~(1u<<bit),1u<<5,6));
 CHECK(!cpContactSolverCheckFeatures(6,required,1u<<5,6));CHECK(!cpContactSolverCheckFeatures(7,required,0,6));
 CHECK(!cpContactSolverCheckFeatures(7,required,1u<<5,0));CHECK(!cpContactSolverCheckFeatures(7,required,1u<<5,2));CHECK(!cpContactSolverCheckFeatures(7,required,1u<<5,4));
}
/* Exercise the runtime-selected kernel without relying on scheduler packing.
 * Exact-sized allocations put the last body's angular bias at the allocation
 * boundary, so ASan also checks the four-plus-two-double body loads. */
static void packetKernels(cpContactSolverKernel kernel)
{
 enum { STRIDE = 19, COMPONENTS = 6 };
 static const int ids[2][4] = {{18,3,12,6},{0,15,8,2}};
 static const int iterations[] = {0,1,3,17};
 const size_t bytes = COMPONENTS*STRIDE*sizeof(double);
 double initial[COMPONENTS*STRIDE];
 double *actual = (double *)malloc(bytes), *expected = (double *)malloc(bytes);
 CHECK(actual && expected && kernel);
 for(int component=0;component<COMPONENTS;component++)for(int body=0;body<STRIDE;body++){
  /* Include signed zero in frozen/unreferenced storage as well as live input. */
  initial[body*COMPONENTS+component] = (body+component)%7==0 ? -0.0 :
   ((body*7+component*11)%23-11)*0.125;
 }
 for(int layout=0;layout<2;layout++)for(int lanes=1;lanes<=4;lanes++)
 for(int count=1;count<=2;count++)for(int mode=0;mode<=2;mode++)
 for(int friction=0;friction<=1;friction++){
  cpContactSolverPacket seed;
  cpBool writable[STRIDE] = {0};
  memset(&seed,0,sizeof(seed));
  seed.lanes=lanes;seed.count=count;seed.mode=mode;seed.friction=friction;
  for(int lane=0;lane<lanes;lane++){
   /* All writable indices are distinct. Frozen sides deliberately alias;
    * swapping layouts tests the allocation boundary on either side, frozen
    * or writable. The gaps in the index sets must remain untouched. */
   seed.a[lane]=ids[layout][mode==1 ? 0 : lane];
   seed.b[lane]=ids[1-layout][mode==2 ? 0 : lane];
   if(mode!=1)writable[seed.a[lane]]=cpTrue;
   if(mode!=2)writable[seed.b[lane]]=cpTrue;
   seed.nx[lane]=(lane&1) ? -0.8 : 0.6;
   seed.ny[lane]=(lane&1) ? 0.6 : 0.8;
   seed.sx[lane]=(lane-2)*0.0625;seed.sy[lane]=(1-lane)*0.125;
   seed.u[lane]=friction ? 0.5+lane*0.125 : 0.0;
   seed.am[lane]=mode==1 ? 0.0 : 0.5+lane*0.0625;
   seed.ai[lane]=mode==1 ? 0.0 : 0.25+lane*0.03125;
   seed.bm[lane]=mode==2 ? 0.0 : 0.375+lane*0.03125;
   seed.bi[lane]=mode==2 ? 0.0 : 0.1875+lane*0.0625;
   for(int contact=0;contact<count;contact++){
    cpContactSolverContact *c=&seed.con[contact];
    c->r1x[lane]=(lane+1)*0.125-contact*0.25;
    c->r1y[lane]=(contact+1)*-0.1875+lane*0.0625;
    c->r2x[lane]=(contact+1)*0.25-lane*0.0625;
    c->r2y[lane]=(lane-2)*0.125+contact*0.1875;
    c->nMass[lane]=0.25+lane*0.03125+contact*0.0625;
    c->tMass[lane]=0.1875+lane*0.03125+contact*0.0625;
    c->bias[lane]=(lane+1)*0.125-contact*0.0625;
    c->bounce[lane]=(lane-1)*0.0625+contact*0.125;
    c->jBias[lane]=lane==0 ? -0.0 : (lane+contact)*0.03125;
    c->jnAcc[lane]=0.25+(lane+contact)*0.125;
    c->jtAcc[lane]=friction ? (lane-2)*0.03125 : -0.0;
   }
  }
  /* Match scheduler padding: valid duplicate indices, zero coefficients.
   * A full-width scatter would overwrite lane zero with its dummy velocity. */
  for(int lane=lanes;lane<4;lane++){
   seed.a[lane]=seed.a[0];seed.b[lane]=seed.b[0];
  }
  for(size_t run=0;run<sizeof(iterations)/sizeof(iterations[0]);run++){
   cpContactSolverPacket vectorPacket, scalarPacket;
   cpContactSolverContext vectorContext, scalarContext;
   memcpy(&vectorPacket,&seed,sizeof(seed));memcpy(&scalarPacket,&seed,sizeof(seed));
   memcpy(actual,initial,bytes);memcpy(expected,initial,bytes);
   memset(&vectorContext,0,sizeof(vectorContext));
   vectorContext.velocity=actual;vectorContext.packets=&vectorPacket;
   vectorContext.bodyCount=STRIDE;
   vectorContext.packetCount=1;vectorContext.kernel=kernel;
   scalarContext=vectorContext;scalarContext.velocity=expected;
   scalarContext.packets=&scalarPacket;scalarContext.kernel=cpContactSolverKernelScalar;
   vectorContext.kernel(&vectorContext,iterations[run]);
   scalarContext.kernel(&scalarContext,iterations[run]);
   if(memcmp(actual,expected,bytes)!=0){
    fprintf(stderr,"packet velocity mismatch: layout=%d lanes=%d contacts=%d mode=%d friction=%d iterations=%d\n",
     layout,lanes,count,mode,friction,iterations[run]);
    CHECK(memcmp(actual,expected,bytes)==0);
   }
   for(int component=0;component<COMPONENTS;component++)for(int body=0;body<STRIDE;body++){
    int index=body*COMPONENTS+component;
    CHECK(isfinite(actual[index]));
    if(!writable[body])CHECK(memcmp(actual+index,initial+index,sizeof(double))==0);
   }
   /* SIMD computes dummy impulse slots, whereas scalar visits active lanes
    * only. Compare active accumulators bitwise (including signed zero). */
   for(int contact=0;contact<count;contact++){
    cpContactSolverContact *a=&vectorPacket.con[contact],*b=&scalarPacket.con[contact];
    CHECK(memcmp(a->jBias,b->jBias,lanes*sizeof(double))==0);
    CHECK(memcmp(a->jnAcc,b->jnAcc,lanes*sizeof(double))==0);
    CHECK(memcmp(a->jtAcc,b->jtAcc,lanes*sizeof(double))==0);
    for(int lane=0;lane<lanes;lane++){
     CHECK(isfinite(a->jBias[lane])&&isfinite(a->jnAcc[lane])&&isfinite(a->jtAcc[lane]));
    }
   }
  }
 }
 free(actual);free(expected);
}
static int callbackCount,postStepCount;
static void switchAfterStep(cpSpace *s,void *key,void *data){(void)key;(void)data;CHECK(!cpSpaceIsLocked(s));CHECK(cpSpaceSetContactSolver(s,CP_CONTACT_SOLVER_ORIGINAL));postStepCount++;}
static void postSolve(cpArbiter *arb,cpSpace *s,void *data)
{
 (void)arb;(void)data;CHECK(cpSpaceIsLocked(s));CHECK(!cpSpaceSetContactSolver(s,CP_CONTACT_SOLVER_ORIGINAL));CHECK(cpSpaceGetContactSolver(s)==CP_CONTACT_SOLVER_AVX2);callbackCount++;
 cpSpaceAddPostStepCallback(s,switchAfterStep,&callbackCount,NULL);
}
static cpContactSolverKernel poisonKernel;
static int poisonKernelCalls;
static void checkedPoisonKernel(cpContactSolverContext *s,int iterations)
{
 feclearexcept(FE_INVALID|FE_DIVBYZERO);
 poisonKernel(s,iterations);
 CHECK(!(fetestexcept(FE_INVALID|FE_DIVBYZERO)));poisonKernelCalls++;
}
static void poisonScratch(cpContactSolverContext *s)
{
 if(!s->solverMemory)return;
 const uint64_t poison=UINT64_C(0x7ff0000000000001); /* signaling NaN */
 for(size_t i=0;i<(size_t)s->solverBodyCapacity*6*sizeof(double);i+=sizeof(poison))memcpy((char *)s->velocity+i,&poison,sizeof(poison));
 for(size_t i=0;i<(size_t)s->solverPacketCapacity*sizeof(cpContactSolverPacket);i+=sizeof(poison))memcpy((char *)s->packets+i,&poison,sizeof(poison));
}
static void poisonedPackets(void)
{
 Fixture a,b;make(&a,4,cpFalse);make(&b,4,cpFalse);
 CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));CHECK(cpSpaceSetContactSolver(b.space,CP_CONTACT_SOLVER_AVX2));
 cpContactSolverContext *s=cpContactSolverGet(a.space);poisonKernel=s->kernel;s->kernel=checkedPoisonKernel;
 cpContactSolverGet(b.space)->kernel=cpContactSolverKernelScalar;
 for(int cycle=0;cycle<20;cycle++){
  int count=4+cycle%10;
  for(int which=0;which<2;which++){
   Fixture *f=which?&b:&a;
   while(f->count>count)removeLast(f);while(f->count<count)addBody(f,f->count,cpFalse);
   for(int i=0;i<count;i++){
    cpShapeSetFriction(f->shape[i],cycle%2?0:0.7);
    if(cycle%3==0){
     cpSpaceRemoveShape(f->space,f->shape[i]);cpShapeFree(f->shape[i]);
     cpShape *shape=i%2?cpCircleShapeNew(f->body[i],0.5,cpvzero):cpBoxShapeNew(f->body[i],1,1,0);
     f->shape[i]=cpSpaceAddShape(f->space,shape);cpShapeSetFriction(shape,cycle%2?0:0.7);
    }
   }
  }
  for(int step=0;step<16;step++){
   poisonScratch(s);cpFloat dt=step%2?1.0/120:1.0/60;
   cpSpaceStep(a.space,dt);cpSpaceStep(b.space,dt);compare(&a,&b);
  }
 }
 CHECK(poisonKernelCalls>0);cleanup(&a);cleanup(&b);
 printf("PASS: poisoned reusable packet/velocity arenas, mixed shapes/friction, partial lanes, %d checked kernel calls.\n",poisonKernelCalls);
}

int main(void)
{
 features();CHECK(cpContactSolverIsAvailable(CP_CONTACT_SOLVER_ORIGINAL));CHECK(!cpContactSolverIsAvailable((cpContactSolverType)99));
 Fixture a,b;make(&a,8,cpFalse);CHECK(cpSpaceGetContactSolver(a.space)==CP_CONTACT_SOLVER_ORIGINAL);CHECK(!cpSpaceSetContactSolver(a.space,(cpContactSolverType)99));
 cpSpaceLock(a.space);CHECK(!cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_ORIGINAL));CHECK(!cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));cpSpaceUnlock(a.space,cpTrue);
 if(!cpContactSolverIsAvailable(CP_CONTACT_SOLVER_AVX2)){
  CHECK(!cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));CHECK(cpSpaceGetContactSolver(a.space)==CP_CONTACT_SOLVER_ORIGINAL);cpSpaceStep(a.space,1.0/60);cleanup(&a);
  puts("PASS: CPU/OS feature matrix, unavailable build/CPU and original-default API.");return 0;
 }
 CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));cpContactSolverContext *ctx=cpContactSolverGet(a.space);CHECK(ctx&&!ctx->graphMemory&&!ctx->solverMemory);
 packetKernels(ctx->kernel);
 make(&b,8,cpFalse);
 for(int i=0;i<8;i++){cpSpaceStep(a.space,1.0/60);cpSpaceStep(b.space,1.0/60);compare(&a,&b);}CHECK(ctx->usedLastStep);
 void *graph=ctx->graphMemory,*solver=ctx->solverMemory;int capacity=ctx->graphCapacity;
 cpSpaceStep(a.space,1.0/60);CHECK(ctx->graphMemory==graph&&ctx->solverMemory==solver);
 for(int i=8;i<160;i++)addBody(&a,i,cpFalse);cpSpaceStep(a.space,1.0/60);CHECK(ctx->graphCapacity>capacity);CHECK(ctx->solverBodyCapacity>=160);
 while(a.count>8)removeLast(&a);cpSpaceStep(a.space,1.0/60);CHECK(ctx->usedLastStep);CHECK(ctx->bodyCount==0&&ctx->packetCount==0);
 /* Free shapes/bodies while context still exists: destroy must not dereference them. */
 cleanup(&a);cleanup(&b);

 make(&a,16,cpTrue);make(&b,16,cpTrue);CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));CHECK(cpSpaceSetContactSolver(b.space,CP_CONTACT_SOLVER_AVX2));cpContactSolverGet(b.space)->kernel=cpContactSolverKernelScalar;
 for(int step=0;step<600;step++){int iterations=step%3==0?1:step%3==1?10:20;cpSpaceSetIterations(a.space,iterations);cpSpaceSetIterations(b.space,iterations);cpFloat dt=step%2?1.0/60:1.0/120;cpSpaceStep(a.space,dt);cpSpaceStep(b.space,dt);compare(&a,&b);}cleanup(&a);cleanup(&b);

 make(&a,8,cpFalse);make(&b,8,cpFalse);a.joint=cpSpaceAddConstraint(a.space,cpPinJointNew(a.body[0],a.body[1],cpvzero,cpvzero));b.joint=cpSpaceAddConstraint(b.space,cpPinJointNew(b.body[0],b.body[1],cpvzero,cpvzero));CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));
 for(int i=0;i<30;i++){cpSpaceStep(a.space,1.0/60);cpSpaceStep(b.space,1.0/60);compare(&a,&b);CHECK(!cpContactSolverGet(a.space)->usedLastStep);}
 CHECK(!cpContactSolverGet(a.space)->solverMemory);CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_ORIGINAL));CHECK(cpContactSolverGet(a.space)==NULL);cleanup(&a);cleanup(&b);

 /*13 contacts on one writable body cannot fit in12 colors; no data is committed. */
 cpSpace *space=cpSpaceNew();CHECK(cpSpaceSetContactSolver(space,CP_CONTACT_SOLVER_AVX2));cpBody *body=cpBodyNew(1,1);cpArbiter arbs[13];struct cpContact contacts[13];memset(arbs,0,sizeof(arbs));memset(contacts,0,sizeof(contacts));
 for(int i=0;i<13;i++){arbs[i].body_a=body;arbs[i].body_b=cpSpaceGetStaticBody(space);arbs[i].count=1;arbs[i].contacts=&contacts[i];cpArrayPush(space->arbiters,&arbs[i]);}
 ctx=cpContactSolverGet(space);CHECK(!cpContactSolverStep(space,ctx));CHECK(!ctx->solverMemory);CHECK(ctx->bodyCount==0&&ctx->packetCount==0);CHECK(body->v.x==0&&body->v.y==0);space->arbiters->num=0;cpBodyFree(body);cpSpaceFree(space);

 make(&a,8,cpFalse);CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));cpSpaceAddGlobalCollisionHandler(a.space)->postSolveFunc=postSolve;cpSpaceStep(a.space,1.0/60);CHECK(callbackCount>0&&postStepCount==1);CHECK(cpSpaceGetContactSolver(a.space)==CP_CONTACT_SOLVER_ORIGINAL);cleanup(&a);
 space=cpSpaceNew();CHECK(cpSpaceSetContactSolver(space,CP_CONTACT_SOLVER_AVX2));cpSpaceDestroy(space);memset(space,0,sizeof(*space));cpSpaceInit(space);CHECK(cpSpaceGetContactSolver(space)==CP_CONTACT_SOLVER_ORIGINAL);cpSpaceFree(space);
 make(&a,8,cpFalse);cpSpaceSetSleepTimeThreshold(a.space,0.5);cpBodySleep(a.body[0]);CHECK(cpBodyIsSleeping(a.body[0]));CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));CHECK(cpBodyIsSleeping(a.body[0]));CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_ORIGINAL));CHECK(cpBodyIsSleeping(a.body[0]));cleanup(&a);
 makeWithSpace(&a,8,cpFalse,cpHastySpaceNew());makeWithSpace(&b,8,cpFalse,cpHastySpaceNew());a.hasty=b.hasty=cpTrue;cpHastySpaceSetThreads(a.space,1);cpHastySpaceSetThreads(b.space,1);CHECK(cpSpaceSetContactSolver(a.space,CP_CONTACT_SOLVER_AVX2));
 for(int i=0;i<12;i++){cpHastySpaceStep(a.space,1.0/60);cpHastySpaceStep(b.space,1.0/60);compare(&a,&b);}CHECK(!cpContactSolverGet(a.space)->graphMemory);cleanup(&a);cleanup(&b);
 poisonedPackets();
 puts("PASS: CPU/OS gating, API, original default, arena reuse/grow/shrink, direct packet/kernel and scalar equivalence, poisoned buffers, varying dt/iterations, fallback, callback locks, sleep, HastySpace independence and lifecycle.");return 0;
}
