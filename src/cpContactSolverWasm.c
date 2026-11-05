/* WebAssembly SIMD is a module-level requirement, not a runtime CPU dispatch.
 * Only this translation unit needs -msimd128 during compilation. */
#include "cpContactSolver.h"
#if CP_WASM_SIMD_CONTACT_SOLVER && CP_USE_DOUBLES
#include <wasm_simd128.h>

typedef v128_t V;
static inline V add(V a, V b){ return wasm_f64x2_add(a, b); }
static inline V sub(V a, V b){ return wasm_f64x2_sub(a, b); }
static inline V mul(V a, V b){ return wasm_f64x2_mul(a, b); }
static inline V neg(V a){ return wasm_f64x2_neg(a); }
/* Match cpfmin/cpfmax, including equal signed zeros and unordered operands.
 * Wasm's IEEE min/max and pseudo-min/max have different tie/NaN rules. */
static inline V vmax(V a, V b){ return wasm_v128_bitselect(a, b, wasm_f64x2_gt(a, b)); }
static inline V vmin(V a, V b){ return wasm_v128_bitselect(a, b, wasm_f64x2_lt(a, b)); }
static inline V zero(void){ return wasm_f64x2_splat(0.0); }

/* No gather/scatter: load exactly six adjacent doubles per body and transpose
 * two bodies into double lanes. The final body never needs a padded allocation. */
static inline void getAoS(const double *v,const int *ids,V *x,V *y,V *w,V *bx,V *by,V *bw){
 const double *a=v+6*ids[0],*b=v+6*ids[1];
 V a0=wasm_v128_load(a),b0=wasm_v128_load(b);
 V a1=wasm_v128_load(a+2),b1=wasm_v128_load(b+2);
 V a2=wasm_v128_load(a+4),b2=wasm_v128_load(b+4);
 *x=wasm_i64x2_shuffle(a0,b0,0,2);*y=wasm_i64x2_shuffle(a0,b0,1,3);
 *w=wasm_i64x2_shuffle(a1,b1,0,2);*bx=wasm_i64x2_shuffle(a1,b1,1,3);
 *by=wasm_i64x2_shuffle(a2,b2,0,2);*bw=wasm_i64x2_shuffle(a2,b2,1,3);
}
static inline void putAoS(double *v,const int *ids,int lanes,V x,V y,V w,V bx,V by,V bw){
 double *a=v+6*ids[0];
 wasm_v128_store(a,wasm_i64x2_shuffle(x,y,0,2));
 wasm_v128_store(a+2,wasm_i64x2_shuffle(w,bx,0,2));
 wasm_v128_store(a+4,wasm_i64x2_shuffle(by,bw,0,2));
 /* Never write dummy lanes or the shared frozen side of a contact. */
 if(lanes>1){
  double *b=v+6*ids[1];
  wasm_v128_store(b,wasm_i64x2_shuffle(x,y,1,3));
  wasm_v128_store(b+2,wasm_i64x2_shuffle(w,bx,1,3));
  wasm_v128_store(b+4,wasm_i64x2_shuffle(by,bw,1,3));
 }
}
#define load(p) wasm_v128_load((p)+lane)
#define store(p,v) wasm_v128_store((p)+lane,(v))
#include "cpContactSolverKernel.inc"
#undef load
#undef store
#include "cpContactSolverScalar.inc"

void
cpContactSolverKernelWasm(cpContactSolverContext *s, int iterations)
{
 for(int iteration=0;iteration<iterations;++iteration){
  for(int i=0;i<s->packetCount;++i){
   cpContactSolverPacket *p=&s->packets[i];
   for(int lane=0;lane<p->lanes;lane+=2){
    if(p->lanes-lane==1) solveScalarPacket(s,p,lane);
    else solvePacket(s,p,lane);
   }
  }
 }
}
#endif
