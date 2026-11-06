/* Native mixed contact/joint integration. The Wasm policy is unchanged. */
#include "../src/cpContactSolver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(x) do { if(!(x)){fprintf(stderr,"FAIL mixed %s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);} } while(0)
#define MAX_BODIES 64
#define MAX_JOINTS 96

typedef struct World {
 cpSpace *space;
 cpBody *bodies[MAX_BODIES];
 cpShape *shapes[MAX_BODIES + 1];
 cpConstraint *joints[MAX_JOINTS];
 int count, jointCount, mode, preCalls, postCalls, springCalls, customCalls;
 int mixedCalls, mixedSteps, maxOverlap, maxPackets;
 cpContactSolverKernel packetKernel;
} World;
static World *current;

static void checkSchedule(cpContactSolverContext *s)
{
 int lastArb[2*MAX_BODIES + 2], lastPacket[2*MAX_BODIES + 2];
 CHECK(s->bodyCount <= 2*MAX_BODIES + 2);
 for(int i = 0; i < s->bodyCount; ++i) lastArb[i] = lastPacket[i] = -1;
 int done = 0;
 for(int packet = 0; packet < s->packetCount; ++packet){
  cpContactSolverPacket *p = &s->packets[packet];
  CHECK(p->lanes == s->mixedLanes[packet]);
  for(int lane = 0; lane < p->lanes; ++lane){
   int arb = s->mixedOrder[done++];
   int ids[2] = {p->a[lane], p->b[lane]};
   for(int side = 0; side < 2; ++side){
    if((side == 0 && p->mode == 1) || (side == 1 && p->mode == 2)) continue;
    int id = ids[side];
    CHECK(arb > lastArb[id]);
    CHECK(packet > lastPacket[id]); /* no dependent lanes in one packet */
    lastArb[id] = arb; lastPacket[id] = packet;
   }
  }
 }
 CHECK(done == current->space->arbiters->num);
}

static void checkedKernel(cpContactSolverContext *s, int iterations)
{
 CHECK(current);
 if(!current->space->constraints->num){
  CHECK(s->jointBodyCount == 0);
  current->packetKernel(s, iterations);
  return;
 }
 CHECK(iterations == 1 && s->mixedMemory && s->jointBodyCount >= 0);
 CHECK(s->jointBodyCount <= s->bodyCount);
 checkSchedule(s);
 current->mixedCalls++;
 if(s->jointBodyCount > current->maxOverlap) current->maxOverlap = s->jointBodyCount;
 if(s->packetCount > current->maxPackets) current->maxPackets = s->packetCount;
 current->packetKernel(s, iterations);
}

static void preSolve(cpConstraint *joint, cpSpace *space)
{
 World *w = (World *)cpConstraintGetUserData(joint);
 CHECK(cpSpaceIsLocked(space));
 CHECK(!cpSpaceSetContactSolver(space, CP_CONTACT_SOLVER_ORIGINAL));
 w->preCalls++;
}
static void postSolve(cpConstraint *joint, cpSpace *space)
{
 World *w = (World *)cpConstraintGetUserData(joint);
 CHECK(cpSpaceIsLocked(space));
 CHECK(isfinite(cpConstraintGetImpulse(joint)));
 for(int i = 0; i < w->count; ++i) CHECK(isfinite(w->bodies[i]->v.x) && isfinite(w->bodies[i]->w));
 w->postCalls++;
}
static cpFloat springForce(cpConstraint *joint, cpFloat distance)
{
 World *w = (World *)cpConstraintGetUserData(joint);
 /* A user callback may touch unrelated state in preStep, before snapshots. */
 w->bodies[w->count - 1]->v.x += 0.00001;
 w->springCalls++;
 return (cpDampedSpringGetRestLength(joint) - distance)*cpDampedSpringGetStiffness(joint);
}

static cpConstraint *newJoint(int kind, cpBody *a, cpBody *b)
{
 cpFloat distance = cpvdist(a->p, b->p);
 switch(kind){
  case 0: return cpPinJointNew(a, b, cpvzero, cpvzero);
  case 1: return cpSlideJointNew(a, b, cpvzero, cpvzero, distance*0.9, distance*1.1);
  case 2: return cpPivotJointNew(a, b, cpvmult(cpvadd(a->p, b->p), 0.5));
  case 3: return cpGrooveJointNew(a, b, cpv(0, -0.3), cpv(distance + 0.3, 0), cpvzero);
  case 4: return cpGearJointNew(a, b, 0, 1);
  case 5: return cpRatchetJointNew(a, b, 0, 0.25);
  case 6: return cpRotaryLimitJointNew(a, b, -0.4, 0.4);
  case 7: return cpDampedSpringNew(a, b, cpvzero, cpvzero, distance, 30, 2);
  case 8: return cpDampedRotarySpringNew(a, b, 0, 10, 2);
  default: return cpSimpleMotorNew(a, b, 0.25);
 }
}
static void addJoint(World *w, cpConstraint *joint)
{
 CHECK(w->jointCount < MAX_JOINTS);
 cpConstraintSetUserData(joint, w);
 cpConstraintSetPreSolveFunc(joint, preSolve);
 cpConstraintSetPostSolveFunc(joint, postSolve);
 cpConstraintSetMaxForce(joint, 50);
 cpSpaceAddConstraint(w->space, joint);
 w->joints[w->jointCount++] = joint;
}
static void make(World *w, int kind, int mode, cpBool stack)
{
 memset(w, 0, sizeof(*w));
 w->space = cpSpaceNew(); w->mode = mode; w->count = 24;
 cpSpaceSetGravity(w->space, cpv(0, -10));
 cpSpaceSetCollisionSlop(w->space, 0.005);
 w->shapes[MAX_BODIES] = cpSpaceAddShape(w->space, cpSegmentShapeNew(cpSpaceGetStaticBody(w->space), cpv(-100,0), cpv(100,0), 0));
 cpShapeSetFriction(w->shapes[MAX_BODIES], 1);
 for(int i = 0; i < w->count; ++i){
  cpBody *b = cpSpaceAddBody(w->space, cpBodyNew(1, cpMomentForBox(1,1,1)));
  cpBodySetPosition(b, stack ? cpv((i%4)*1.1, 0.5 + (i/4)*1.02) : cpv(-23 + i*2, 0.45));
  cpBodySetVelocity(b, cpv((i%3 - 1)*0.1, 0));
  cpShape *shape = cpSpaceAddShape(w->space, cpBoxShapeNew(b,1,1,0));
  cpShapeSetFriction(shape, 0.7);
  w->bodies[i] = b; w->shapes[i] = shape;
 }
 for(int i = 0; i < w->count; i += 2){
  cpConstraint *joint = newJoint(kind, w->bodies[i], w->bodies[i+1]);
  addJoint(w, joint);
  if(kind == 7) cpDampedSpringSetSpringForceFunc(joint, springForce);
 }
 if(mode){
  CHECK(cpSpaceSetContactSolver(w->space, CP_CONTACT_SOLVER_AVX2));
  cpContactSolverContext *s = cpContactSolverGet(w->space);
  w->packetKernel = mode == 1 ? cpContactSolverKernelScalar : s->kernel;
  s->kernel = checkedKernel;
 }
}
static void checkFinite(World *w)
{
 for(int i = 0; i < w->count; ++i){
  cpBody *b = w->bodies[i];
  CHECK(isfinite(b->p.x) && isfinite(b->p.y) && isfinite(b->a));
  CHECK(isfinite(b->v.x) && isfinite(b->v.y) && isfinite(b->w));
  CHECK(isfinite(b->v_bias.x) && isfinite(b->v_bias.y) && isfinite(b->w_bias));
 }
}
static void compare(World *a, World *b)
{
 CHECK(a->count == b->count && a->jointCount == b->jointCount);
 checkFinite(a); checkFinite(b);
 for(int i = 0; i < a->count; ++i){
  cpBody *x = a->bodies[i], *y = b->bodies[i];
  CHECK(!memcmp(&x->p, &y->p, sizeof(cpVect)));
  CHECK(!memcmp(&x->v, &y->v, sizeof(cpVect)));
  CHECK(!memcmp(&x->v_bias, &y->v_bias, sizeof(cpVect)));
  CHECK(!memcmp(&x->a, &y->a, sizeof(cpFloat)));
  CHECK(!memcmp(&x->w, &y->w, sizeof(cpFloat)));
  CHECK(!memcmp(&x->w_bias, &y->w_bias, sizeof(cpFloat)));
 }
 CHECK(a->space->arbiters->num == b->space->arbiters->num);
 for(int i = 0; i < a->space->arbiters->num; ++i){
  cpArbiter *x = (cpArbiter *)a->space->arbiters->arr[i], *y = (cpArbiter *)b->space->arbiters->arr[i];
  CHECK(x->count == y->count);
  for(int j = 0; j < x->count; ++j){
   CHECK(!memcmp(&x->contacts[j].jBias, &y->contacts[j].jBias, sizeof(cpFloat)));
   CHECK(!memcmp(&x->contacts[j].jnAcc, &y->contacts[j].jnAcc, sizeof(cpFloat)));
   CHECK(!memcmp(&x->contacts[j].jtAcc, &y->contacts[j].jtAcc, sizeof(cpFloat)));
  }
 }
 for(int i = 0; i < a->jointCount; ++i){
  cpFloat x = cpConstraintGetImpulse(a->joints[i]), y = cpConstraintGetImpulse(b->joints[i]);
  CHECK(!memcmp(&x, &y, sizeof(cpFloat)));
 }
 CHECK(a->preCalls == b->preCalls && a->postCalls == b->postCalls && a->springCalls == b->springCalls);
}
static void step(World *w, cpFloat dt, int iterations)
{
 current = w;
 if(iterations > 0) cpSpaceSetIterations(w->space, iterations);
 else w->space->iterations = iterations; /* defensive private-state fallback; public setter rejects zero */
 cpSpaceStep(w->space, dt);
 if(w->mode){
  cpContactSolverContext *s = cpContactSolverGet(w->space);
  if(s->usedLastStep) w->mixedSteps++;
  CHECK(s->bodyCount == 0 && s->packetCount == 0 && s->jointBodyCount == 0);
 }
 current = NULL;
}
static void destroy(World *w)
{
 for(int i = 0; i < w->jointCount; ++i){cpSpaceRemoveConstraint(w->space,w->joints[i]);cpConstraintFree(w->joints[i]);}
 for(int i = 0; i < w->count; ++i){cpSpaceRemoveShape(w->space,w->shapes[i]);cpShapeFree(w->shapes[i]);cpSpaceRemoveBody(w->space,w->bodies[i]);cpBodyFree(w->bodies[i]);}
 cpSpaceRemoveShape(w->space,w->shapes[MAX_BODIES]);cpShapeFree(w->shapes[MAX_BODIES]);cpSpaceFree(w->space);
}

static void customPre(cpConstraint *c, cpFloat dt){(void)c;(void)dt;}
static void customCached(cpConstraint *c, cpFloat dt){(void)c;(void)dt;}
static void customImpulse(cpConstraint *c, cpFloat dt)
{
 World *w = (World *)cpConstraintGetUserData(c); (void)dt;
 w->customCalls++;
 /* Deliberately outside the joint endpoints, and mutating packed data. */
 w->bodies[w->count-1]->v.x += 0.0001;
 if(w->space->arbiters->num){
  cpArbiter *arb = (cpArbiter *)w->space->arbiters->arr[0];
  arb->contacts[0].bounce += 0.00001;
 }
}
static cpFloat customGetImpulse(cpConstraint *c){(void)c;return 0;}
static const cpConstraintClass customClass = {customPre, customCached, customImpulse, customGetImpulse};

static void customFallback(void)
{
 World a,b; make(&a,0,0,cpFalse); make(&b,0,2,cpFalse);
 World *worlds[2] = {&a,&b};
 for(int i = 0; i < 2; ++i){
  World *w = worlds[i];
  cpConstraint *c = (cpConstraint *)calloc(1,sizeof(cpConstraint)); CHECK(c);
  cpConstraintInit(c,&customClass,w->bodies[0],w->bodies[1]); addJoint(w,c);
 }
 for(int i = 0; i < 40; ++i){step(&a,1.0/60,10);step(&b,1.0/60,10);compare(&a,&b);}
 CHECK(b.customCalls == 400 && !b.mixedCalls && !b.mixedSteps);
 CHECK(!cpContactSolverGet(b.space)->mixedMemory && !cpContactSolverGet(b.space)->solverMemory);
 destroy(&a);destroy(&b);
}

static void poisonMixedArena(World *w)
{
 cpContactSolverContext *s = cpContactSolverGet(w->space);
 if(!s->mixedMemory) return;
 memset(s->dependencies,0xa5,(size_t)s->mixedCapacity*sizeof(cpContactSolverDependency));
 memset(s->mixedOrder,0xa5,(size_t)s->mixedCapacity*sizeof(int));
 memset(s->mixedLanes,0xa5,(size_t)s->mixedCapacity*sizeof(unsigned char));
 memset(s->jointBodies,0xa5,2*(size_t)s->mixedCapacity*sizeof(int));
}
static void transitions(void)
{
 World a,b;make(&a,0,1,cpFalse);make(&b,0,2,cpFalse);
 for(int i=0;i<10;i++){step(&a,1.0/60,10);step(&b,1.0/60,10);compare(&a,&b);}
 void *oldArena=cpContactSolverGet(b.space)->mixedMemory;CHECK(oldArena);
 World *worlds[2]={&a,&b};
 for(int which=0;which<2;which++){
  World *w=worlds[which];
  for(int i=0;i<w->jointCount;i++){cpSpaceRemoveConstraint(w->space,w->joints[i]);cpConstraintFree(w->joints[i]);}
  w->jointCount=0;
 }
 for(int i=0;i<10;i++){step(&a,1.0/60,10);step(&b,1.0/60,10);compare(&a,&b);}
 CHECK(cpContactSolverGet(b.space)->mixedMemory==oldArena);
 for(int which=0;which<2;which++){
  World *w=worlds[which];
  /* Some endpoints have no contacts; some are shared by several joints. */
  cpBodySetPosition(w->bodies[22],cpv(0,20));cpBodySetPosition(w->bodies[23],cpv(2,20));
  addJoint(w,cpPinJointNew(w->bodies[0],w->bodies[22],cpvzero,cpvzero));
  addJoint(w,cpDampedSpringNew(w->bodies[22],w->bodies[23],cpvzero,cpvzero,2,5,1));
  addJoint(w,cpPivotJointNew(cpSpaceGetStaticBody(w->space),w->bodies[1],w->bodies[1]->p));
  addJoint(w,cpSimpleMotorNew(cpSpaceGetStaticBody(w->space),w->bodies[1],0.1));
  cpBodySetType(w->bodies[20],CP_BODY_TYPE_KINEMATIC);
  cpBodySetVelocity(w->bodies[20],cpv(0.05,0));cpBodySetAngularVelocity(w->bodies[20],-0.1);
  addJoint(w,cpPinJointNew(w->bodies[19],w->bodies[20],cpvzero,cpvzero));
  cpBodySetMoment(w->bodies[18],INFINITY);
  addJoint(w,cpPinJointNew(w->bodies[17],w->bodies[18],cpvzero,cpvzero));
 }
 for(int i=0;i<120;i++){
  if(i%7==0)poisonMixedArena(&b);
  int iterations=i%11==0?0:10;
  step(&a,i%2?1.0/60:1.0/120,iterations);step(&b,i%2?1.0/60:1.0/120,iterations);compare(&a,&b);
 }
 CHECK(cpContactSolverGet(b.space)->mixedMemory==oldArena);
 destroy(&a);destroy(&b);
 /* Sleep/wake changes the active constraint list between steps. */
 make(&a,0,1,cpFalse);make(&b,0,2,cpFalse);
 cpSpaceSetSleepTimeThreshold(a.space,0.2);cpSpaceSetSleepTimeThreshold(b.space,0.2);
 for(int i=0;i<180;i++){
  if(i==120){
   CHECK(cpBodyIsSleeping(a.bodies[0])&&cpBodyIsSleeping(b.bodies[0]));
   cpBodyApplyImpulseAtLocalPoint(a.bodies[0],cpv(0,3),cpvzero);cpBodyApplyImpulseAtLocalPoint(b.bodies[0],cpv(0,3),cpvzero);
   CHECK(!cpBodyIsSleeping(a.bodies[0])&&!cpBodyIsSleeping(b.bodies[0]));
  }
  step(&a,1.0/60,10);step(&b,1.0/60,10);compare(&a,&b);
 }
 destroy(&a);destroy(&b);
}

static void growShrink(void)
{
 World a,b;make(&a,0,1,cpFalse);make(&b,0,2,cpFalse);
 step(&a,1.0/60,10);step(&b,1.0/60,10);compare(&a,&b);
 cpContactSolverContext *context=cpContactSolverGet(b.space);
 int capacity=context->mixedCapacity;void *old=context->mixedMemory;
 World *worlds[2]={&a,&b};
 for(int which=0;which<2;which++){
  World *w=worlds[which];
  for(int i=w->count;i<56;i++){
   cpBody *body=cpSpaceAddBody(w->space,cpBodyNew(1,cpMomentForBox(1,1,1)));
   cpBodySetPosition(body,cpv(40+(i-24)*0.9,0.45));
   w->bodies[i]=body;w->shapes[i]=cpSpaceAddShape(w->space,cpBoxShapeNew(body,1,1,0));
   cpShapeSetFriction(w->shapes[i],0.7);
   addJoint(w,cpPinJointNew(w->bodies[i-1],body,cpvzero,cpvzero));
   w->count++;
  }
 }
 for(int i=0;i<20;i++){step(&a,1.0/120,10);step(&b,1.0/120,10);compare(&a,&b);}
 CHECK(context->mixedCapacity>capacity&&context->mixedMemory!=old);
 void *grown=context->mixedMemory;
 for(int which=0;which<2;which++){
  World *w=worlds[which];
  while(w->jointCount>12){cpConstraint *j=w->joints[--w->jointCount];cpSpaceRemoveConstraint(w->space,j);cpConstraintFree(j);}
  while(w->count>24){int i=--w->count;cpSpaceRemoveShape(w->space,w->shapes[i]);cpShapeFree(w->shapes[i]);cpSpaceRemoveBody(w->space,w->bodies[i]);cpBodyFree(w->bodies[i]);}
 }
 for(int i=0;i<20;i++){poisonMixedArena(&b);step(&a,1.0/120,10);step(&b,1.0/120,10);compare(&a,&b);}
 CHECK(context->mixedMemory==grown);
 destroy(&a);destroy(&b);
}

int main(void)
{
 if(!cpContactSolverIsAvailable(CP_CONTACT_SOLVER_AVX2)){
  puts("PASS: native mixed contact/joint backend unavailable; existing fallback retained.");return 0;
 }
 long mixedSteps = 0;
 for(int kind = 0; kind < 10; ++kind) for(int stack = 0; stack < 2; ++stack){
  World original,scalar,vector; make(&original,kind,0,stack);make(&scalar,kind,1,stack);make(&vector,kind,2,stack);
  for(int i = 0; i < 360; ++i){
   cpFloat dt = i%2 ? 1.0/60 : 1.0/120;
   int iterations = i%3 == 0 ? 1 : i%3 == 1 ? 10 : 20;
   if(i%17==0)poisonMixedArena(&vector);
   step(&original,dt,iterations);step(&scalar,dt,iterations);step(&vector,dt,iterations);
   compare(&scalar,&vector);compare(&original,&vector);
  }
  CHECK(vector.mixedSteps > 0 && vector.mixedCalls > 0 && vector.maxOverlap > 0);
  CHECK(scalar.mixedSteps == vector.mixedSteps);
  CHECK(vector.preCalls == vector.postCalls && vector.preCalls == 360*vector.jointCount);
  if(kind == 7) CHECK(vector.springCalls == 360*vector.jointCount);
  mixedSteps += vector.mixedSteps;
  destroy(&original);destroy(&scalar);destroy(&vector);
 }
 customFallback();transitions();growShrink();
 printf("PASS: all10 built-in joints, flat/stacked connected contacts, dependency order, sparse overlap, original/scalar/SIMD exact bodies/contact/joint impulses, varying dt/iterations, callbacks, poisoned reuse, add/remove, joint-only endpoints, static/kinematic/partly frozen bodies, sleep/wake and custom-class fallback; %ld mixed steps.\n",mixedSteps);
 return 0;
}
