/* This translation unit alone is built for AVX2, and is excluded from LTO.
 * Entry is reachable only after the generic CPU/OS capability check. */
#include "cpContactSolver.h"
#if CP_AVX2_CONTACT_SOLVER && CP_USE_DOUBLES
#include <immintrin.h>

typedef __m256d V;
static inline V load(const double *p){ return _mm256_loadu_pd(p); }
static inline void store(double *p, V v){ _mm256_storeu_pd(p, v); }
static inline V add(V a, V b){ return _mm256_add_pd(a, b); }
static inline V sub(V a, V b){ return _mm256_sub_pd(a, b); }
static inline V mul(V a, V b){ return _mm256_mul_pd(a, b); }
static inline V neg(V a){ return _mm256_xor_pd(a, _mm256_set1_pd(-0.0)); }
static inline V vmax(V a, V b){ return _mm256_max_pd(a, b); }
static inline V vmin(V a, V b){ return _mm256_min_pd(a, b); }
static inline V zero(void){ return _mm256_setzero_pd(); }
/* Six adjacent doubles per body: vx, vy, w, bias vx, bias vy, bias w.
 * Load four plus two doubles, then transpose into the kernel's SIMD lanes.
 * In particular, never read eight doubles at the final body in the arena. */
static inline void transpose4(V a,V b,V c,V d,V *x,V *y,V *z,V *w){
 V ab0=_mm256_unpacklo_pd(a,b),ab1=_mm256_unpackhi_pd(a,b);
 V cd0=_mm256_unpacklo_pd(c,d),cd1=_mm256_unpackhi_pd(c,d);
 *x=_mm256_permute2f128_pd(ab0,cd0,0x20);*y=_mm256_permute2f128_pd(ab1,cd1,0x20);
 *z=_mm256_permute2f128_pd(ab0,cd0,0x31);*w=_mm256_permute2f128_pd(ab1,cd1,0x31);
}
static inline void getAoS(const double *v,const int *ids,V *x,V *y,V *w,V *bx,V *by,V *bw){
 const double *a=v+6*ids[0],*b=v+6*ids[1],*c=v+6*ids[2],*d=v+6*ids[3];
 transpose4(load(a),load(b),load(c),load(d),x,y,w,bx);
 __m128d at=_mm_loadu_pd(a+4),bt=_mm_loadu_pd(b+4),ct=_mm_loadu_pd(c+4),dt=_mm_loadu_pd(d+4);
 *by=_mm256_insertf128_pd(_mm256_castpd128_pd256(_mm_unpacklo_pd(at,bt)),_mm_unpacklo_pd(ct,dt),1);
 *bw=_mm256_insertf128_pd(_mm256_castpd128_pd256(_mm_unpackhi_pd(at,bt)),_mm_unpackhi_pd(ct,dt),1);
}
static inline void putAoS(double *v,const int *ids,int lanes,V x,V y,V w,V bx,V by,V bw){
 /* Only active lanes may be written: dummy lanes alias lane zero, and
  * frozen bodies can be shared by multiple contacts in the same color. */
 V a,b,c,d;transpose4(x,y,w,bx,&a,&b,&c,&d);
 __m128d by0=_mm256_castpd256_pd128(by),bw0=_mm256_castpd256_pd128(bw);
 __m128d by1=_mm256_extractf128_pd(by,1),bw1=_mm256_extractf128_pd(bw,1);
 store(v+6*ids[0],a);_mm_storeu_pd(v+6*ids[0]+4,_mm_unpacklo_pd(by0,bw0));
 if(lanes>1){store(v+6*ids[1],b);_mm_storeu_pd(v+6*ids[1]+4,_mm_unpackhi_pd(by0,bw0));}
 if(lanes>2){store(v+6*ids[2],c);_mm_storeu_pd(v+6*ids[2]+4,_mm_unpacklo_pd(by1,bw1));}
 if(lanes>3){store(v+6*ids[3],d);_mm_storeu_pd(v+6*ids[3]+4,_mm_unpackhi_pd(by1,bw1));}
}
#include "cpContactSolverKernel.inc"
#include "cpContactSolverScalar.inc"

void
cpContactSolverKernelAvx2(cpContactSolverContext *s, int iterations)
{
 for(int iteration = 0; iteration < iterations; ++iteration){
  for(int i = 0; i < s->packetCount; ++i){
   cpContactSolverPacket *p = &s->packets[i];
   if(p->lanes == 1) solveScalarPacket(s, p, 0);
   else solvePacket(s, p, 0);
  }
 }
}
#endif
