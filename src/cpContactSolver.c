#include "cpContactSolver.h"
#include <limits.h>
#include <string.h>

cpArray *
cpSpaceBufferArrayNew(void)
{
 cpSpaceBufferStorage *storage = (cpSpaceBufferStorage *)cpcalloc(1, sizeof(*storage));
 cpAssertHard(storage != NULL, "Cannot allocate space buffer storage.");
 storage->array.num = 0;
 storage->array.max = 4;
 storage->solver = NULL;
 storage->array.arr = (void **)cpcalloc(4, sizeof(void *));
 cpAssertHard(storage->array.arr != NULL, "Cannot allocate space buffer array.");
 return &storage->array;
}

/* Checked, naturally aligned regions within an allocation owned by this module. */
static cpBool
addRegion(size_t *bytes, size_t count, size_t itemSize)
{
 if(*bytes > SIZE_MAX - 7) return cpFalse;
 size_t aligned = (*bytes + 7) & ~(size_t)7;
 if(count > (SIZE_MAX - aligned)/itemSize) return cpFalse;
 *bytes = aligned + count*itemSize;
 return cpTrue;
}

static void *
region(void *memory, size_t *offset, size_t count, size_t itemSize)
{
 *offset = (*offset + 7) & ~(size_t)7;
 void *result = (char *)memory + *offset;
 *offset += count*itemSize;
 return result;
}

static int
capacityFor(int count)
{
 int capacity = 64;
 while(capacity < count) capacity *= 2;
 return capacity;
}

static cpBool
reserveGraph(cpContactSolverContext *s, int count)
{
 if(count <= s->graphCapacity) return cpTrue;
 int capacity = capacityFor(count);
 size_t bytes = 0;
 if(!addRegion(&bytes, 2*(size_t)capacity, sizeof(cpBody *)) ||
    !addRegion(&bytes, 4*(size_t)capacity, sizeof(int)) ||
    !addRegion(&bytes, 2*(size_t)capacity, sizeof(unsigned int)) ||
    !addRegion(&bytes, (size_t)capacity, sizeof(cpContactSolverLink))) return cpFalse;
 void *memory = cpcalloc(1, bytes);
 if(!memory) return cpFalse;
 if(s->graphMemory) cpfree(s->graphMemory);
 s->graphMemory = memory;
 s->graphCapacity = capacity;
 size_t offset = 0;
 s->bodies = (cpBody **)region(memory, &offset, 2*(size_t)capacity, sizeof(cpBody *));
 s->hash = (int *)region(memory, &offset, 4*(size_t)capacity, sizeof(int));
 s->colors = (unsigned int *)region(memory, &offset, 2*(size_t)capacity, sizeof(unsigned int));
 s->links = (cpContactSolverLink *)region(memory, &offset, (size_t)capacity, sizeof(cpContactSolverLink));
 return cpTrue;
}

static cpBool
reserveSolver(cpContactSolverContext *s, int bodyCount, int packetCount)
{
 if(bodyCount <= s->solverBodyCapacity && packetCount <= s->solverPacketCapacity) return cpTrue;
 int bodies = capacityFor(bodyCount), packets = capacityFor(packetCount);
 if(bodies < s->solverBodyCapacity) bodies = s->solverBodyCapacity;
 if(packets < s->solverPacketCapacity) packets = s->solverPacketCapacity;
 size_t bytes = 0;
 if(!addRegion(&bytes, 6*(size_t)bodies, sizeof(double)) ||
    !addRegion(&bytes, (size_t)packets, sizeof(cpContactSolverPacket))) return cpFalse;
 void *memory = cpcalloc(1, bytes);
 if(!memory) return cpFalse;
 if(s->solverMemory) cpfree(s->solverMemory);
 s->solverMemory = memory;
 s->solverBodyCapacity = bodies;
 s->solverPacketCapacity = packets;
 size_t offset = 0;
 s->velocity = (double *)region(memory, &offset, 6*(size_t)bodies, sizeof(double));
 s->packets = (cpContactSolverPacket *)region(memory, &offset, (size_t)packets, sizeof(cpContactSolverPacket));
 return cpTrue;
}

void
cpContactSolverDestroy(cpSpace *space)
{
 cpContactSolverContext *s = cpContactSolverGet(space);
 if(!s) return;
 ((cpSpaceBufferStorage *)space->allocatedBuffers)->solver = NULL;
 /* A previous step may have been followed by removal/freeing of any body.
  * Destruction must never commit or dereference old scratch references. */
 if(s->graphMemory) cpfree(s->graphMemory);
 if(s->solverMemory) cpfree(s->solverMemory);
 if(s->mixedMemory) cpfree(s->mixedMemory);
 cpfree(s);
}

cpContactSolverType
cpSpaceGetContactSolver(const cpSpace *space)
{
 const cpContactSolverContext *s = cpContactSolverGet(space);
 return s ? s->type : CP_CONTACT_SOLVER_ORIGINAL;
}

cpBool
cpSpaceSetContactSolver(cpSpace *space, cpContactSolverType solver)
{
 if(space->locked || !cpContactSolverIsAvailable(solver)) return cpFalse;
 if(solver == CP_CONTACT_SOLVER_ORIGINAL){
  cpContactSolverDestroy(space);
  return cpTrue;
 }
 if(cpContactSolverGet(space)) return cpTrue;
#if (CP_AVX2_CONTACT_SOLVER || CP_WASM_SIMD_CONTACT_SOLVER) && CP_USE_DOUBLES
 cpContactSolverContext *s = (cpContactSolverContext *)cpcalloc(1, sizeof(*s));
 if(!s) return cpFalse;
 s->type = solver;
#if CP_WASM_SIMD_CONTACT_SOLVER
 s->kernel = cpContactSolverKernelWasm;
#else
 s->kernel = cpContactSolverKernelAvx2;
#endif
 ((cpSpaceBufferStorage *)space->allocatedBuffers)->solver = s;
 return cpTrue;
#else
 return cpFalse;
#endif
}

static int
bodyIndex(cpContactSolverContext *s, cpBody *body)
{
 uintptr_t hash = ((uintptr_t)body >> 4)*UINT64_C(11400714819323198485);
 int slot = (int)(hash & (unsigned int)s->hashMask);
 while(s->hash[slot] >= 0){
  int index = s->hash[slot];
  if(s->bodies[index] == body) return index;
  slot = (slot + 1) & s->hashMask;
 }
 int index = s->bodyCount++;
 s->hash[slot] = index;
 s->bodies[index] = body;
 s->colors[index] = 0;
 return index;
}

static void
packPacket(cpContactSolverContext *s, cpContactSolverPacket *p, cpArray *arbiters,
 const int *indices, int lanes, int kind)
{
 p->lanes = lanes;
 p->mode = kind/4;
 p->friction = (kind/2)%2;
 p->count = kind%2 + 1;
 for(int lane = 0; lane < lanes; ++lane){
  int index = indices[lane];
  cpContactSolverLink *link = &s->links[index];
  cpArbiter *arb = (cpArbiter *)arbiters->arr[index];
  cpBody *a = arb->body_a, *b = arb->body_b;
  p->a[lane] = link->a; p->b[lane] = link->b;
  p->nx[lane] = arb->n.x; p->ny[lane] = arb->n.y;
  p->sx[lane] = arb->surface_vr.x; p->sy[lane] = arb->surface_vr.y;
  p->u[lane] = arb->u;
  p->am[lane] = a->m_inv; p->ai[lane] = a->i_inv;
  p->bm[lane] = b->m_inv; p->bi[lane] = b->i_inv;
  for(int c = 0; c < p->count; ++c){
   struct cpContact *from = &arb->contacts[c];
   cpContactSolverContact *to = &p->con[c];
   to->original[lane] = from;
   to->r1x[lane] = from->r1.x; to->r1y[lane] = from->r1.y;
   to->r2x[lane] = from->r2.x; to->r2y[lane] = from->r2.y;
   to->nMass[lane] = from->nMass; to->tMass[lane] = from->tMass;
   to->bias[lane] = from->bias; to->bounce[lane] = from->bounce;
   to->jBias[lane] = from->jBias; to->jnAcc[lane] = from->jnAcc; to->jtAcc[lane] = from->jtAcc;
  }
 }
 /* Active lanes are fully overwritten above. Initialize only inactive lanes;
  * never let reused arena poison enter a vector calculation. */
 for(int lane = lanes; lane < CP_CONTACT_SOLVER_LANES; ++lane){
  p->a[lane] = p->a[0]; p->b[lane] = p->b[0];
  p->nx[lane] = p->ny[lane] = p->sx[lane] = p->sy[lane] = p->u[lane] = 0;
  p->am[lane] = p->ai[lane] = p->bm[lane] = p->bi[lane] = 0;
  for(int c = 0; c < p->count; ++c){
   cpContactSolverContact *to = &p->con[c];
   to->original[lane] = NULL;
   to->r1x[lane] = to->r1y[lane] = to->r2x[lane] = to->r2y[lane] = 0;
   to->nMass[lane] = to->tMass[lane] = to->bias[lane] = to->bounce[lane] = 0;
   to->jBias[lane] = to->jnAcc[lane] = to->jtAcc[lane] = 0;
  }
 }
}

static cpBool
prepare(cpContactSolverContext *s, cpArray *arbiters)
{
 int count = arbiters->num;
 if(count < 4 || count > INT_MAX/64 || !reserveGraph(s, count)) return cpFalse;
 int hashSize = 1;
 while(hashSize < 4*count) hashSize *= 2;
 s->hashMask = hashSize - 1;
 for(int i = 0; i < hashSize; ++i) s->hash[i] = -1;

 enum { groupCount = CP_CONTACT_SOLVER_COLORS*CP_CONTACT_SOLVER_KINDS };
 int heads[groupCount], tails[groupCount], sizes[groupCount];
 unsigned int usedKinds[CP_CONTACT_SOLVER_COLORS] = {0};
 for(int i = 0; i < count; ++i){
  cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
  cpBody *a = arb->body_a, *b = arb->body_b;
  cpBool staticA = (a->m_inv == 0 && a->i_inv == 0);
  cpBool staticB = (b->m_inv == 0 && b->i_inv == 0);
  if(a == b || (staticA && staticB) || arb->count < 1 || arb->count > 2) return cpFalse;
  cpContactSolverLink *link = &s->links[i];
  link->a = bodyIndex(s, a);
  link->b = bodyIndex(s, b);
  int mode = staticA ? 1 : (staticB ? 2 : 0);
  unsigned int used = (staticA ? 0 : s->colors[link->a]) | (staticB ? 0 : s->colors[link->b]);
  int color = 0;
  while(color < CP_CONTACT_SOLVER_COLORS && (used & (1u << color))) ++color;
  if(color == CP_CONTACT_SOLVER_COLORS) return cpFalse;
  if(!staticA) s->colors[link->a] |= 1u << color;
  if(!staticB) s->colors[link->b] |= 1u << color;
  link->kind = (mode*2 + (arb->u != 0))*2 + arb->count - 1;
  link->next = -1;
  int group = color*CP_CONTACT_SOLVER_KINDS + link->kind;
  unsigned int bit = 1u << link->kind;
  if(usedKinds[color] & bit) s->links[tails[group]].next = i;
  else {usedKinds[color] |= bit; heads[group] = i; sizes[group] = 0;}
  tails[group] = i;
  ++sizes[group];
 }

 int packets = 0;
 for(int color = 0; color < CP_CONTACT_SOLVER_COLORS; ++color)
  for(int kind = 0, bits = (int)usedKinds[color]; bits; ++kind, bits >>= 1)
   if(bits & 1) packets += (sizes[color*CP_CONTACT_SOLVER_KINDS + kind] + CP_CONTACT_SOLVER_LANES - 1)/CP_CONTACT_SOLVER_LANES;
 if(!reserveSolver(s, s->bodyCount, packets)) return cpFalse;
 /* Enumerate in the same color/kind order as the original full scan. */
 for(int color = 0; color < CP_CONTACT_SOLVER_COLORS; ++color)
  for(int groupKind = 0, bits = (int)usedKinds[color]; bits; ++groupKind, bits >>= 1){
   if(!(bits & 1)) continue;
   int group = color*CP_CONTACT_SOLVER_KINDS + groupKind;
   while(heads[group] >= 0){
    int indices[CP_CONTACT_SOLVER_LANES], lanes = 0;
    while(lanes < CP_CONTACT_SOLVER_LANES && heads[group] >= 0){
     int index = heads[group];
     heads[group] = s->links[index].next;
     indices[lanes++] = index;
    }
    packPacket(s, &s->packets[s->packetCount++], arbiters, indices, lanes, group % CP_CONTACT_SOLVER_KINDS);
   }
  }
 for(int i = 0; i < s->bodyCount; ++i){
  cpBody *b = s->bodies[i]; double *v = s->velocity + 6*i;
  v[0] = b->v.x; v[1] = b->v.y; v[2] = b->w;
  v[3] = b->v_bias.x; v[4] = b->v_bias.y; v[5] = b->w_bias;
 }
 return cpTrue;
}

/* Built-in impulse sweeps touch only their endpoint velocities and private
 * joint state. Custom classes may also change unrelated bodies or contacts;
 * they must retain the complete original loop, not receive stale snapshots.
 * Spring force/torque callbacks run in preStep, before packet preparation. */
static cpBool
constraintsSupported(cpArray *constraints)
{
 for(int i = 0; i < constraints->num; ++i){
  cpConstraint *c = (cpConstraint *)constraints->arr[i];
  if(!(cpConstraintIsPinJoint(c) || cpConstraintIsSlideJoint(c) ||
       cpConstraintIsPivotJoint(c) || cpConstraintIsGrooveJoint(c) ||
       cpConstraintIsGearJoint(c) || cpConstraintIsRatchetJoint(c) ||
       cpConstraintIsRotaryLimitJoint(c) || cpConstraintIsSimpleMotor(c) ||
       cpConstraintIsDampedSpring(c) || cpConstraintIsDampedRotarySpring(c))) return cpFalse;
 }
 return cpTrue;
}

static int
findBody(cpContactSolverContext *s, cpBody *body)
{
 uintptr_t hash = ((uintptr_t)body >> 4)*UINT64_C(11400714819323198485);
 int slot = (int)(hash & (unsigned int)s->hashMask);
 while(s->hash[slot] >= 0){
  int index = s->hash[slot];
  if(s->bodies[index] == body) return index;
  slot = (slot + 1) & s->hashMask;
 }
 return -1;
}

static cpBool
reserveMixedWork(cpContactSolverContext *s, int count)
{
 if(count <= s->mixedCapacity) return cpTrue;
 int capacity = capacityFor(count);
 size_t bytes = 0;
 if(!addRegion(&bytes, 2*(size_t)capacity, sizeof(int)) ||
    !addRegion(&bytes, (size_t)capacity, sizeof(cpContactSolverDependency)) ||
    !addRegion(&bytes, (size_t)capacity, sizeof(int)) ||
    !addRegion(&bytes, (size_t)capacity, sizeof(unsigned char))) return cpFalse;
 void *memory = cpcalloc(1, bytes);
 if(!memory) return cpFalse;
 if(s->mixedMemory) cpfree(s->mixedMemory);
 s->mixedMemory = memory;
 s->mixedCapacity = capacity;
 size_t offset = 0;
 s->jointBodies = (int *)region(memory, &offset, 2*(size_t)capacity, sizeof(int));
 s->dependencies = (cpContactSolverDependency *)region(memory, &offset, (size_t)capacity, sizeof(cpContactSolverDependency));
 s->mixedOrder = (int *)region(memory, &offset, (size_t)capacity, sizeof(int));
 s->mixedLanes = (unsigned char *)region(memory, &offset, (size_t)capacity, sizeof(unsigned char));
 return cpTrue;
}

/* The colored contact order can change convergence when coupled to joints.
 * Preserve the original order on every writable body in mixed steps instead.
 * Ready queues only reorder independent arbiters; a whole packet is removed
 * before successors become ready, so dependent contacts cannot share a lane. */
static cpBool
prepareOrdered(cpContactSolverContext *s, cpArray *arbiters)
{
 int count = arbiters->num;
 if(count < 4 || count > INT_MAX/64 || !reserveGraph(s, count) || !reserveMixedWork(s, count)) return cpFalse;
 int hashSize = 1;
 while(hashSize < 4*count) hashSize *= 2;
 s->hashMask = hashSize - 1;
 for(int i = 0; i < hashSize; ++i) s->hash[i] = -1;
 int heads[CP_CONTACT_SOLVER_KINDS], tails[CP_CONTACT_SOLVER_KINDS];
 int ready[CP_CONTACT_SOLVER_KINDS] = {0};
 for(int kind = 0; kind < CP_CONTACT_SOLVER_KINDS; ++kind) heads[kind] = tails[kind] = -1;
 for(int i = 0; i < count; ++i){
  cpArbiter *arb = (cpArbiter *)arbiters->arr[i];
  cpBody *a = arb->body_a, *b = arb->body_b;
  cpBool staticA = (a->m_inv == 0 && a->i_inv == 0);
  cpBool staticB = (b->m_inv == 0 && b->i_inv == 0);
  if(a == b || (staticA && staticB) || arb->count < 1 || arb->count > 2) return cpFalse;
  cpContactSolverLink *link = &s->links[i];
  cpContactSolverDependency *node = &s->dependencies[i];
  link->a = bodyIndex(s, a); link->b = bodyIndex(s, b);
  link->kind = ((staticA ? 1 : (staticB ? 2 : 0))*2 + (arb->u != 0))*2 + arb->count - 1;
  link->next = -1;
  node->pending = 0; node->successors[0] = node->successors[1] = -1;
  /* The unused color flags hold the preceding arbiter index plus one. */
  int predecessors[2] = {staticA || !s->colors[link->a] ? -1 : (int)s->colors[link->a] - 1,
                        staticB || !s->colors[link->b] ? -1 : (int)s->colors[link->b] - 1};
  for(int side = 0; side < 2; ++side) if(predecessors[side] >= 0){
   cpContactSolverDependency *prev = &s->dependencies[predecessors[side]];
   int slot = (prev->successors[0] >= 0);
   if(prev->successors[slot] >= 0) return cpFalse;
   prev->successors[slot] = i;
   ++node->pending;
  }
  if(!staticA) s->colors[link->a] = (unsigned int)i + 1;
  if(!staticB) s->colors[link->b] = (unsigned int)i + 1;
  if(!node->pending){
   int kind = link->kind;
   if(tails[kind] >= 0) s->links[tails[kind]].next = i;
   else heads[kind] = i;
   tails[kind] = i; ++ready[kind];
  }
 }
 int packets = 0;
 for(int done = 0; done < count;){
  int kind = 0;
  for(int k = 1; k < CP_CONTACT_SOLVER_KINDS; ++k) if(ready[k] > ready[kind]) kind = k;
  if(!ready[kind]) return cpFalse;
  int lanes = ready[kind] < CP_CONTACT_SOLVER_LANES ? ready[kind] : CP_CONTACT_SOLVER_LANES;
  for(int lane = 0; lane < lanes; ++lane){
   int index = heads[kind];
   s->mixedOrder[done + lane] = index;
   heads[kind] = s->links[index].next;
  }
  ready[kind] -= lanes;
  if(!ready[kind]) tails[kind] = -1;
  for(int lane = 0; lane < lanes; ++lane){
   cpContactSolverDependency *node = &s->dependencies[s->mixedOrder[done + lane]];
   for(int side = 0; side < 2; ++side) if(node->successors[side] >= 0){
    int next = node->successors[side];
    if(!--s->dependencies[next].pending){
     int type = s->links[next].kind;
     s->links[next].next = -1;
     if(tails[type] >= 0) s->links[tails[type]].next = next;
     else heads[type] = next;
     tails[type] = next; ++ready[type];
    }
   }
  }
  s->mixedLanes[packets++] = (unsigned char)lanes;
  done += lanes;
 }
 if(!reserveSolver(s, s->bodyCount, packets)) return cpFalse;
 int done = 0;
 for(int i = 0; i < packets; ++i){
  int lanes = s->mixedLanes[i];
  packPacket(s, &s->packets[s->packetCount++], arbiters, s->mixedOrder + done, lanes, s->links[s->mixedOrder[done]].kind);
  done += lanes;
 }
 for(int i = 0; i < s->bodyCount; ++i){
  cpBody *b = s->bodies[i]; double *v = s->velocity + 6*i;
  v[0] = b->v.x; v[1] = b->v.y; v[2] = b->w;
  v[3] = b->v_bias.x; v[4] = b->v_bias.y; v[5] = b->w_bias;
 }
 return cpTrue;
}

static void
prepareJointBodies(cpContactSolverContext *s, cpArray *constraints)
{
 /* Scheduling is complete. Reuse its flags to deduplicate the overlap list;
  * do not insert joint-only endpoints into the contact body's hash table. */
 for(int i = 0; i < s->bodyCount; ++i) s->colors[i] = 0;
 for(int i = 0; i < constraints->num; ++i){
  cpConstraint *c = (cpConstraint *)constraints->arr[i];
  cpBody *endpoints[2] = {c->a, c->b};
  for(int side = 0; side < 2; ++side){
   int index = findBody(s, endpoints[side]);
   if(index >= 0 && !s->colors[index]){
    s->colors[index] = 1;
    s->jointBodies[s->jointBodyCount++] = index;
   }
  }
 }
}

static void
solveMixed(cpSpace *space, cpContactSolverContext *s)
{
 for(int iteration = 0; iteration < space->iterations; ++iteration){
  s->kernel(s, 1);
  for(int i = 0; i < s->jointBodyCount; ++i){
   int index = s->jointBodies[i];
   cpBody *b = s->bodies[index]; const double *v = s->velocity + 6*index;
   /* Frozen endpoints can be shared by lanes. Never export into them, but
    * do import them after the scalar sweep (including signed-zero changes). */
   if(b->m_inv == 0 && b->i_inv == 0) continue;
   /* Built-in joints neither read nor write split-impulse bias velocities.
    * Those stay in scratch until final commit; only physical velocities
    * cross the contact/joint boundary each iteration. */
   b->v = cpv(v[0], v[1]); b->w = v[2];
  }
  for(int i = 0; i < space->constraints->num; ++i){
   cpConstraint *c = (cpConstraint *)space->constraints->arr[i];
   c->klass->applyImpulse(c, space->curr_dt);
  }
  for(int i = 0; i < s->jointBodyCount; ++i){
   int index = s->jointBodies[i];
   cpBody *b = s->bodies[index]; double *v = s->velocity + 6*index;
   v[0] = b->v.x; v[1] = b->v.y; v[2] = b->w;
  }
 }
}

cpBool
cpContactSolverStep(cpSpace *space, cpContactSolverContext *s)
{
 s->usedLastStep = cpFalse;
 s->bodyCount = s->packetCount = s->jointBodyCount = 0;
 cpBool mixed = (space->constraints->num != 0);
 /* Wasm's existing joint fallback is intentionally unchanged. No state is
  * committed until every allocation and eligibility check has succeeded. */
 if(space->iterations <= 0 || space->arbiters->num < 4 ||
    (mixed && (s->type != CP_CONTACT_SOLVER_AVX2 || !constraintsSupported(space->constraints))) ||
    !(mixed ? prepareOrdered(s, space->arbiters) : prepare(s, space->arbiters))){
  s->bodyCount = s->packetCount = s->jointBodyCount = 0;
  return cpFalse;
 }
 if(mixed){
  prepareJointBodies(s, space->constraints);
  solveMixed(space, s);
 }else s->kernel(s, space->iterations);
 for(int i = 0; i < s->bodyCount; ++i){
  cpBody *b = s->bodies[i]; const double *v = s->velocity + 6*i;
  if(b->m_inv == 0 && b->i_inv == 0) continue;
  b->v = cpv(v[0], v[1]); b->w = v[2];
  b->v_bias = cpv(v[3], v[4]); b->w_bias = v[5];
 }
 for(int i = 0; i < s->packetCount; ++i){
  cpContactSolverPacket *p = &s->packets[i];
  for(int c = 0; c < p->count; ++c) for(int lane = 0; lane < p->lanes; ++lane){
   cpContactSolverContact *from = &p->con[c]; struct cpContact *to = from->original[lane];
   to->jBias = from->jBias[lane]; to->jnAcc = from->jnAcc[lane]; to->jtAcc = from->jtAcc[lane];
  }
 }
 s->bodyCount = s->packetCount = s->jointBodyCount = 0;
 s->usedLastStep = cpTrue;
 return cpTrue;
}
