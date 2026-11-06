/* Whole-step mixed contact/joint workloads, independent of the 26-scene
 * MunkBench/browser protocol. Uses only public APIs so the identical driver
 * can be linked against an older library for a controlled comparison. */
#include "chipmunk/chipmunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif

#define REQUIRE(x) do { if(!(x)){fprintf(stderr,"mixedbench: %s\n",#x);exit(1);} } while(0)
typedef struct Scene {
 cpSpace *space;
 cpBody **bodies;
 cpShape **shapes;
 cpConstraint **joints;
 int bodyCount, shapeCount, jointCount;
} Scene;
static void addShape(Scene *s,cpShape *shape)
{
 cpShapeSetFriction(shape,0.7);cpShapeSetElasticity(shape,0);
 s->shapes[s->shapeCount++]=cpSpaceAddShape(s->space,shape);
}
static void addJoint(Scene *s,cpConstraint *joint)
{
 cpConstraintSetMaxForce(joint,500);
 s->joints[s->jointCount++]=cpSpaceAddConstraint(s->space,joint);
}
static Scene makeScene(const char *name,int count,cpContactSolverType solver)
{
 Scene s;memset(&s,0,sizeof(s));
 s.bodies=(cpBody **)calloc((size_t)count,sizeof(cpBody *));
 s.shapes=(cpShape **)calloc((size_t)count+1,sizeof(cpShape *));
 s.joints=(cpConstraint **)calloc(3*(size_t)count,sizeof(cpConstraint *));
 REQUIRE(s.bodies&&s.shapes&&s.joints);
 s.space=cpSpaceNew();REQUIRE(cpSpaceSetContactSolver(s.space,solver));
 cpSpaceSetGravity(s.space,cpv(0,-10));cpSpaceSetIterations(s.space,10);
 cpSpaceSetCollisionSlop(s.space,0.005);
 addShape(&s,cpSegmentShapeNew(cpSpaceGetStaticBody(s.space),cpv(-1000,0),cpv(1000,0),0));
 int mechanism=!strcmp(name,"mechanisms");
 for(int i=0;i<count;i++){
  int group=i/8,member=i%8;
  cpFloat x,y;
  if(mechanism){
   x=(group%16)*4.0+(member?((member%3)-1)*0.45:0);
   y=(group/16)*6.0+(member?3.0+(member/3)*0.5:1.3);
  }else{x=(i%32)*1.05;y=0.5+(i/32)*1.02;}
  cpFloat mass=mechanism&&member==0?5:1;
  cpFloat moment=mechanism&&member==0?cpMomentForBox(mass,2,0.2):cpMomentForBox(mass,1,1);
  cpBody *body=cpSpaceAddBody(s.space,cpBodyNew(mass,moment));
  cpBodySetPosition(body,cpv(x,y));s.bodies[s.bodyCount++]=body;
  if(mechanism){
   addShape(&s,member?cpCircleShapeNew(body,0.2,cpvzero):cpBoxShapeNew(body,2,0.2,0));
   if(!member){
    addJoint(&s,cpPivotJointNew(cpSpaceGetStaticBody(s.space),body,cpv(x,y)));
    addJoint(&s,cpSimpleMotorNew(cpSpaceGetStaticBody(s.space),body,0.6));
   }
  }else addShape(&s,cpBoxShapeNew(body,1,1,0));
 }
 if(!strcmp(name,"light")){
  for(int i=0;i+1<count;i+=16) addJoint(&s,cpPinJointNew(s.bodies[i],s.bodies[i+1],cpvzero,cpvzero));
 }else if(!strcmp(name,"dense")){
  for(int i=0;i<count;i++){
   if(i%32) addJoint(&s,cpPinJointNew(s.bodies[i-1],s.bodies[i],cpvzero,cpvzero));
   if(i>=32) addJoint(&s,cpPinJointNew(s.bodies[i-32],s.bodies[i],cpvzero,cpvzero));
   if(i%8==1) addJoint(&s,cpDampedRotarySpringNew(s.bodies[i-1],s.bodies[i],0,3,0.3));
  }
 }
 return s;
}
static void destroyScene(Scene *s)
{
 for(int i=0;i<s->jointCount;i++){cpSpaceRemoveConstraint(s->space,s->joints[i]);cpConstraintFree(s->joints[i]);}
 for(int i=0;i<s->shapeCount;i++){cpSpaceRemoveShape(s->space,s->shapes[i]);cpShapeFree(s->shapes[i]);}
 for(int i=0;i<s->bodyCount;i++){cpSpaceRemoveBody(s->space,s->bodies[i]);cpBodyFree(s->bodies[i]);}
 cpSpaceFree(s->space);free(s->bodies);free(s->shapes);free(s->joints);
}
static uint64_t hashBytes(uint64_t hash,const void *data,size_t size)
{
 const unsigned char *p=(const unsigned char *)data;
 for(size_t i=0;i<size;i++){hash^=p[i];hash*=UINT64_C(1099511628211);}return hash;
}
static uint64_t stateHash(Scene *s,int *invalid)
{
 uint64_t hash=UINT64_C(14695981039346656037);
 for(int i=0;i<s->bodyCount;i++){
  cpBody *b=s->bodies[i];cpVect p=cpBodyGetPosition(b),v=cpBodyGetVelocity(b);
  cpFloat values[6]={p.x,p.y,cpBodyGetAngle(b),v.x,v.y,cpBodyGetAngularVelocity(b)};
  for(int k=0;k<6;k++)if(!isfinite(values[k]))(*invalid)++;
  hash=hashBytes(hash,values,sizeof(values));
 }
 for(int i=0;i<s->jointCount;i++){cpFloat j=cpConstraintGetImpulse(s->joints[i]);if(!isfinite(j))(*invalid)++;hash=hashBytes(hash,&j,sizeof(j));}
 return hash;
}
static double seconds(void)
{
#ifdef __EMSCRIPTEN__
 /* Monotonic browser/Node wall time, in the same units as native CPU time. */
 return emscripten_get_now()/1000.0;
#elif defined(_WIN32)
 /* MSVC clock() reports elapsed wall time, unlike the POSIX CPU clock. */
 FILETIME created,exited,kernel,user;
 REQUIRE(GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user));
 uint64_t kernelTicks=((uint64_t)kernel.dwHighDateTime<<32)|kernel.dwLowDateTime;
 uint64_t userTicks=((uint64_t)user.dwHighDateTime<<32)|user.dwLowDateTime;
 return (double)(kernelTicks+userTicks)/10000000.0;
#else
 return (double)clock()/CLOCKS_PER_SEC;
#endif
}
int main(int argc,char **argv)
{
 const char *name="light",*solverName="original";
 int count=256,steps=1200,batch=1,samples=1,warmup=0;
 for(int i=1;i<argc;i++){
  REQUIRE(i+1<argc);const char *key=argv[i++],*value=argv[i];
  if(!strcmp(key,"--scene"))name=value;
  else if(!strcmp(key,"--solver"))solverName=value;
  else if(!strcmp(key,"--count"))count=atoi(value);
  else if(!strcmp(key,"--steps"))steps=atoi(value);
  else if(!strcmp(key,"--batch"))batch=atoi(value);
  else if(!strcmp(key,"--samples"))samples=atoi(value);
  else if(!strcmp(key,"--warmup"))warmup=atoi(value);
  else{fprintf(stderr,"Unknown argument %s\n",key);return 1;}
 }
 REQUIRE(count>=8&&count<=16384&&steps>0&&steps<=100000&&batch>0&&batch<=4096&&samples>0&&samples<=100&&warmup>=0&&warmup<=20);
 REQUIRE(!strcmp(name,"light")||!strcmp(name,"dense")||!strcmp(name,"mechanisms"));
 REQUIRE(!strcmp(solverName,"original")||!strcmp(solverName,"avx2")||!strcmp(solverName,"wasm-simd"));
 cpContactSolverType solver=!strcmp(solverName,"avx2")?CP_CONTACT_SOLVER_AVX2:
  !strcmp(solverName,"wasm-simd")?CP_CONTACT_SOLVER_WASM_SIMD:CP_CONTACT_SOLVER_ORIGINAL;
 if(!cpContactSolverIsAvailable(solver)){fprintf(stderr,"mixedbench: requested solver unavailable: %s\n",solverName);return 2;}
 for(int sample=-warmup;sample<samples;sample++){
  double initSeconds=0,runSeconds=0;uint64_t hash=0;int invalid=0,joints=0,shapes=0;
  for(int repetition=0;repetition<batch;repetition++){
   double started=seconds();Scene s=makeScene(name,count,solver);initSeconds+=seconds()-started;
   started=seconds();for(int step=0;step<steps;step++)cpSpaceStep(s.space,1.0/120);runSeconds+=seconds()-started;
   uint64_t currentHash=stateHash(&s,&invalid);
   if(repetition)REQUIRE(hash==currentHash);hash=currentHash;joints=s.jointCount;shapes=s.shapeCount;
   destroyScene(&s);
  }
  REQUIRE(!invalid);
  if(sample>=0)printf("{\"scene\":\"%s\",\"solver\":\"%s\",\"bodies\":%d,\"shapes\":%d,\"joints\":%d,\"steps\":%d,\"batch\":%d,\"sample\":%d,\"initSeconds\":%.9g,\"runSeconds\":%.9g,\"stateHash\":\"%016llx\",\"invalid\":%d}\n",name,solverName,count,shapes,joints,steps,batch,sample,initSeconds,runSeconds,(unsigned long long)hash,invalid);
 }
 return 0;
}
