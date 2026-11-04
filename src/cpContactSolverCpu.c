/* Baseline-ISA runtime guard. Never compile this file with AVX2-only flags. */
#include "cpContactSolver.h"

cpBool
cpContactSolverCheckFeatures(unsigned int maxLeaf, unsigned int ecx1,
 unsigned int ebx7, uint64_t xcr0)
{
 const unsigned int required = (1u << 26) | (1u << 27) | (1u << 28);
 return maxLeaf >= 7 && (ecx1 & required) == required &&
  (xcr0 & 6) == 6 && (ebx7 & (1u << 5)) != 0;
}

#if CP_AVX2_CONTACT_SOLVER && CP_USE_DOUBLES
#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
cpBool
cpContactSolverCpuSupported(void)
{
 int r[4];
 __cpuidex(r, 0, 0);
 unsigned int maxLeaf = (unsigned int)r[0];
 if(maxLeaf < 7) return cpFalse;
 __cpuidex(r, 1, 0);
 unsigned int ecx = (unsigned int)r[2];
 const unsigned int required = (1u << 26) | (1u << 27) | (1u << 28);
 if((ecx & required) != required) return cpFalse;
 uint64_t xcr0 = _xgetbv(0);
 __cpuidex(r, 7, 0);
 return cpContactSolverCheckFeatures(maxLeaf, ecx, (unsigned int)r[1], xcr0);
}
#elif (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
#include <cpuid.h>
cpBool
cpContactSolverCpuSupported(void)
{
 unsigned int a, b, c, d;
 unsigned int maxLeaf = __get_cpuid_max(0, NULL);
 if(maxLeaf < 7) return cpFalse;
 __cpuid_count(1, 0, a, b, c, d);
 unsigned int ecx = c;
 const unsigned int required = (1u << 26) | (1u << 27) | (1u << 28);
 if((ecx & required) != required) return cpFalse;
 /* OSXSAVE must be checked BEFORE executing XGETBV. */
 unsigned int low, high;
 __asm__ volatile("xgetbv" : "=a"(low), "=d"(high) : "c"(0));
 uint64_t xcr0 = ((uint64_t)high << 32) | low;
 __cpuid_count(7, 0, a, b, c, d);
 return cpContactSolverCheckFeatures(maxLeaf, ecx, b, xcr0);
}
#else
cpBool cpContactSolverCpuSupported(void){ return cpFalse; }
#endif
#else
cpBool cpContactSolverCpuSupported(void){ return cpFalse; }
#endif

cpBool
cpContactSolverIsAvailable(cpContactSolverType solver)
{
 if(solver == CP_CONTACT_SOLVER_ORIGINAL) return cpTrue;
 if(solver == CP_CONTACT_SOLVER_AVX2) return cpContactSolverCpuSupported();
#if CP_WASM_SIMD_CONTACT_SOLVER && CP_USE_DOUBLES
 /* A SIMD-enabled Wasm module has already passed runtime validation at load. */
 if(solver == CP_CONTACT_SOLVER_WASM_SIMD) return cpTrue;
#endif
 return cpFalse;
}
