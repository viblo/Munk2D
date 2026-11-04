#define PACKET_SCALAR
#define V double
#define Velocity ScalarVelocity
#define getVelocity getScalarVelocity
#define putVelocity putScalarVelocity
#define dot scalarDot
#define impulse scalarImpulse
#define solvePacket solveScalarPacket
#define load(p) ((p)[lane])
#define store(p,v) ((p)[lane]=(v))
#define add(a,b) ((a)+(b))
#define sub(a,b) ((a)-(b))
#define mul(a,b) ((a)*(b))
#define neg(a) (-(a))
#define vmax(a,b) cpfmax((a),(b))
#define vmin(a,b) cpfmin((a),(b))
#define zero() (0.0)
#include "cpContactSolverKernel.inc"
#undef PACKET_SCALAR
#undef V
#undef Velocity
#undef getVelocity
#undef putVelocity
#undef dot
#undef impulse
#undef solvePacket
#undef load
#undef store
#undef add
#undef sub
#undef mul
#undef neg
#undef vmax
#undef vmin
#undef zero
