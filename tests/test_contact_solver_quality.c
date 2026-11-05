#include "../src/cpContactSolver.h"
#ifdef __EMSCRIPTEN__
#define TEST_CONTACT_SOLVER CP_CONTACT_SOLVER_WASM_SIMD
#else
#define TEST_CONTACT_SOLVER CP_CONTACT_SOLVER_AVX2
#endif
#include <stdio.h>
#if CP_USE_DOUBLES
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chipmunk/chipmunk_private.h"
#define MAX 128
static int qualityMode, qualityIndex, qualityIterations=10;
static double qualityDt=1.0/120;
static double baselineMetrics[21][10], referenceMetrics[21][10];
static long referenceContacts[21];
static int failures;

typedef struct {cpSpace *space;cpBody *body[MAX];cpShape *shape[MAX+1];cpConstraint *joint[MAX];int nb,ns,nj,step,steps;double scale,maxPen,tailPen,tailSpeed,frictionExcess,jointError;long contacts;int invalid;} World;
static double max(double a,double b){return a>b?a:b;}
static void post(cpArbiter *arb,cpSpace *space,void *data){(void)space;World *w=(World *)data;cpContactPointSet set=cpArbiterGetContactPointSet(arb);w->contacts++;
 for(int i=0;i<set.count;i++){double p=max(0,-set.points[i].distance/w->scale);w->maxPen=max(w->maxPen,p);if(w->step>=w->steps-240)w->tailPen=max(w->tailPen,p);}
 for(int i=0;i<arb->count;i++){struct cpContact *c=&arb->contacts[i];double excess=max(0,fabs(c->jtAcc)-arb->u*c->jnAcc)/(1+fabs(arb->u*c->jnAcc));w->frictionExcess=max(w->frictionExcess,excess);if(!isfinite(c->jnAcc)||!isfinite(c->jtAcc)||!isfinite(c->jBias)||c->jnAcc<0)w->invalid++;}
}
static void init(World *w,double scale,double gravity){memset(w,0,sizeof(*w));w->scale=scale;w->space=cpSpaceNew();if(qualityMode){if(!cpSpaceSetContactSolver(w->space,TEST_CONTACT_SOLVER)){fprintf(stderr,"Cannot enable solver\n");exit(1);}if(qualityMode==1)cpContactSolverGet(w->space)->kernel=cpContactSolverKernelScalar;}cpSpaceSetGravity(w->space,cpv(0,gravity*scale));cpSpaceSetIterations(w->space,qualityIterations);cpSpaceSetCollisionSlop(w->space,0.005*scale);cpSpaceSetSleepTimeThreshold(w->space,INFINITY);cpCollisionHandler *h=cpSpaceAddGlobalCollisionHandler(w->space);h->postSolveFunc=post;h->userData=w;}
static cpBody *body(World *w,double mass,double moment,cpVect p){cpBody *b=cpSpaceAddBody(w->space,cpBodyNew(mass,moment));cpBodySetPosition(b,p);w->body[w->nb++]=b;return b;}
static cpShape *shape(World *w,cpShape *s,double friction,double restitution){cpShapeSetFriction(s,friction);cpShapeSetElasticity(s,restitution);cpSpaceAddShape(w->space,s);w->shape[w->ns++]=s;return s;}
static void floorShape(World *w,double offset){shape(w,cpSegmentShapeNew(cpSpaceGetStaticBody(w->space),cpv(offset-100*w->scale,offset),cpv(offset+100*w->scale,offset),0),1,1);}
static void run(World *w,int steps){w->steps=steps;for(w->step=0;w->step<steps;w->step++){cpSpaceStep(w->space,qualityDt);for(int i=0;i<w->nb;i++){cpBody *b=w->body[i];if(!(isfinite(b->p.x)&&isfinite(b->p.y)&&isfinite(b->v.x)&&isfinite(b->v.y)&&isfinite(b->w)&&isfinite(b->a)))w->invalid++;if(w->step>=steps-240)w->tailSpeed=max(w->tailSpeed,cpvlength(b->v)/w->scale);}
 for(int i=0;i<w->nj;i++){cpConstraint *j=w->joint[i];cpVect a=cpBodyLocalToWorld(cpConstraintGetBodyA(j),cpPinJointGetAnchorA(j)),b=cpBodyLocalToWorld(cpConstraintGetBodyB(j),cpPinJointGetAnchorB(j));if(w->step>=steps-240)w->jointError=max(w->jointError,fabs(cpvdist(a,b)-cpPinJointGetDist(j))/w->scale);}}
}
static void qualityFailure(const char *name,const char *reason){fprintf(stderr,"Quality failure %s mode%d dt%.8g iterations%d: %s\n",name,qualityMode,qualityDt,qualityIterations,reason);failures++;}
static void emit(World *w,const char *name,double position,double velocity,double energy,double momentum){
 double values[10]={w->scale,w->maxPen,w->tailPen,w->tailSpeed,w->jointError,w->frictionExcess,position,velocity,energy,momentum};
 if(qualityIndex>=21){qualityFailure(name,"case count");return;}
 if(qualityMode==0)memcpy(baselineMetrics[qualityIndex],values,sizeof(values));
 double *base=baselineMetrics[qualityIndex];
 if(w->invalid||w->contacts<=0)qualityFailure(name,"invalid state or unexercised collision");
 for(int i=0;i<10;i++)if(!isfinite(values[i]))qualityFailure(name,"nonfinite metric");
 if(w->frictionExcess>1e-6)qualityFailure(name,"friction cone");
 if(strncmp(name,"impact",6)==0){
  if(velocity>1e-4||energy>1e-4||momentum>1e-5)qualityFailure(name,"analytic impact velocity/COM energy/momentum");
 }else{
  if(w->maxPen>max(0.08,base[1]*1.5+0.01)||w->tailPen>max(0.02,base[2]*1.5+0.005))qualityFailure(name,"penetration");
  if(w->tailSpeed>max(0.05,base[3]*1.5+0.01)||w->jointError>max(0.01,base[4]*1.5+0.001))qualityFailure(name,"rest velocity or joint gap");
  if(position>base[6]+0.05)qualityFailure(name,"rest position/sliding distance");
 }
 if(qualityMode==1){memcpy(referenceMetrics[qualityIndex],values,sizeof(values));referenceContacts[qualityIndex]=w->contacts;}
 if(qualityMode==2&&(memcmp(referenceMetrics[qualityIndex],values,sizeof(values))||referenceContacts[qualityIndex]!=w->contacts))qualityFailure(name,"SIMD differs from same-schedule scalar reference");
 qualityIndex++;
}
static void destroy(World *w){for(int i=0;i<w->nj;i++){cpSpaceRemoveConstraint(w->space,w->joint[i]);cpConstraintFree(w->joint[i]);}for(int i=0;i<w->ns;i++){cpSpaceRemoveShape(w->space,w->shape[i]);cpShapeFree(w->shape[i]);}for(int i=0;i<w->nb;i++){cpSpaceRemoveBody(w->space,w->body[i]);cpBodyFree(w->body[i]);}cpSpaceFree(w->space);}
static void stack(double s,double offset,int joints){World w;init(&w,s,-10);floorShape(&w,offset);for(int i=0;i<16;i++){cpBody *b=body(&w,1,cpMomentForBox(1,s,s),cpv(offset+(i%2)*1.1*s,offset+(0.6+(i/2)*1.02)*s));shape(&w,cpBoxShapeNew(b,s,s,0),0.8,0);if(joints&&i>=2){cpConstraint *j=cpSpaceAddConstraint(w.space,cpPinJointNew(w.body[i-2],b,cpvzero,cpvzero));w.joint[w.nj++]=j;}}
 run(&w,1800);double error=0;for(int i=0;i<w.nb;i++)error=max(error,fabs((w.body[i]->p.y-offset)/s-(0.5+(i/2)*(joints?1.02:1.0))));char name[100];snprintf(name,sizeof(name),"%s_offset_%.0e",joints?"joint_stack":"stack",offset);emit(&w,name,error,0,0,0);destroy(&w);}
static void slide(double s){World w;init(&w,s,-10);floorShape(&w,0);double initial[8];for(int i=0;i<8;i++){initial[i]=(-24+6*i)*s;cpBody *b=body(&w,1,cpMomentForBox(1,s,s),cpv(initial[i],0.5*s));shape(&w,cpBoxShapeNew(b,s,s,0),0.5,0);cpBodySetVelocity(b,cpv(4*s,0));}run(&w,600);double error=0,speed=0;for(int i=0;i<8;i++){error=max(error,fabs((w.body[i]->p.x-initial[i])/s-1.6));speed=max(speed,cpvlength(w.body[i]->v)/s);}emit(&w,"friction_slide",error,speed,0,0);destroy(&w);}
static void impact(double s,double ratio,double boost,double restitution){World w;init(&w,s,0);double u1=boost+2*s,u2=boost-2*s,m1=1,m2=ratio,cm=(m1*u1+m2*u2)/(m1+m2);double e1=(m1*u1+m2*u2-m2*restitution*(u1-u2))/(m1+m2),e2=(m1*u1+m2*u2+m1*restitution*(u1-u2))/(m1+m2);double initialE=0.5*m1*(u1-cm)*(u1-cm)+0.5*m2*(u2-cm)*(u2-cm);
 for(int i=0;i<8;i++){cpBody *a=body(&w,m1,cpMomentForCircle(m1,0,0.5*s,cpvzero),cpv(-s,i*3*s)),*b=body(&w,m2,cpMomentForCircle(m2,0,0.5*s,cpvzero),cpv(s,i*3*s));shape(&w,cpCircleShapeNew(a,0.5*s,cpvzero),0,restitution);shape(&w,cpCircleShapeNew(b,0.5*s,cpvzero),0,1);cpBodySetVelocity(a,cpv(u1,0));cpBodySetVelocity(b,cpv(u2,0));}
 run(&w,180);double velocity=0,energy=0,momentum=0;for(int i=0;i<8;i++){double a=w.body[2*i]->v.x,b=w.body[2*i+1]->v.x;velocity=max(velocity,max(fabs(a-e1),fabs(b-e2))/s);double e=0.5*m1*(a-cm)*(a-cm)+0.5*m2*(b-cm)*(b-cm);energy=max(energy,fabs(e-restitution*restitution*initialE)/initialE);momentum=max(momentum,fabs(m1*(a-u1)+m2*(b-u2))/(2*s*(m1+m2)));}
 char name[120];snprintf(name,sizeof(name),"impact_mass_%.0e_boost_%.0e_e%.1f",ratio,boost,restitution);emit(&w,name,0,velocity,energy,momentum);destroy(&w);}
static void runCases(void){for(int k=0;k<3;k++){double s=k==0?0.01:k==1?1:100;stack(s,0,0);stack(s,0,1);slide(s);impact(s,1,0,1);impact(s,1e6,0,1);impact(s,1,0,0.5);}stack(1,1e9,0);impact(1,1,1e8,1);impact(1,1,1e5,1);}

int main(void){
 const double steps[]={1.0/120,1.0/60,1.0/120,1.0/120};const int iterations[]={10,10,20,5};
 int modes=cpContactSolverIsAvailable(TEST_CONTACT_SOLVER)?3:1;
 for(int config=0;config<4;config++){
  qualityDt=steps[config];qualityIterations=iterations[config];
  for(qualityMode=0;qualityMode<modes;qualityMode++){qualityIndex=0;runCases();if(qualityIndex!=21)failures++;}
 }
 if(failures)return 1;
 printf("PASS:21 physical probes x4 timestep/iteration configurations x%d solver modes; scalar/SIMD exact where available.\n",modes);return 0;
}
#else
int main(void){if(cpContactSolverIsAvailable(TEST_CONTACT_SOLVER))return 1;puts("PASS: double-only backend unavailable for float builds.");return 0;}
#endif
