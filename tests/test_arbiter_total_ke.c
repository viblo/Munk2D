#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "chipmunk/chipmunk_private.h"

static void
check(int condition, const char *message)
{
	if(!condition){
		fprintf(stderr, "arbiter total KE regression: %s\n", message);
		exit(1);
	}
}

static void
near(cpFloat actual, cpFloat expected, const char *message)
{
	check(isfinite(actual) && isfinite(expected), "energy must be finite");
	check(cpfabs(actual - expected) <= 1e-10*(1.0 + cpfabs(expected)), message);
}

/* Exercise the arithmetic independently of contact generation and solvers. */
static void
test_contact_terms(void)
{
	struct cpContact contacts[2] = {{0}};
	cpArbiter arb = {0};
	arb.state = CP_ARBITER_STATE_NORMAL;
	arb.contacts = contacts;
	arb.count = 2;
	arb.e = 0.25;
	contacts[0].nMass = 0.5;
	contacts[0].jnAcc = 2.0;
	contacts[1].nMass = 0.25;
	contacts[1].jnAcc = 3.0;

	cpFloat normal = (1.0 - arb.e)/(1.0 + arb.e)*(4.0/0.5 + 9.0/0.25);
	near(cpArbiterTotalKE(&arb), normal, "disabled friction must retain normal energy");

	/* One disabled contact must not discard another contact's friction loss. */
	contacts[1].tMass = 0.125;
	contacts[1].jtAcc = 0.5;
	near(cpArbiterTotalKE(&arb), normal + 0.25/0.125, "mixed tangential terms");
	contacts[0].tMass = 0.75;
	contacts[0].jtAcc = -0.25;
	near(cpArbiterTotalKE(&arb), normal + 0.25/0.125 + 0.0625/0.75,
		"nonzero-friction formula must be unchanged");

	arb.e = 1.0;
	contacts[0].tMass = contacts[1].tMass = 0.0;
	contacts[0].jtAcc = contacts[1].jtAcc = 0.0;
	near(cpArbiterTotalKE(&arb), 0.0, "elastic frictionless contact energy");
	arb.count = 0;
	near(cpArbiterTotalKE(&arb), 0.0, "empty arbiter energy");
	arb.count = 2;
	arb.state = CP_ARBITER_STATE_CACHED;
	near(cpArbiterTotalKE(&arb), 0.0, "cached arbiter has no active contacts");
}

typedef struct EnergyResult {
	int calls, count;
	cpFloat energy, normal, tangent;
} EnergyResult;

static void
post_solve(cpArbiter *arb, cpSpace *space, void *data)
{
	EnergyResult *result = (EnergyResult *)data;
	(void)space;
	result->calls++;
	result->count = cpArbiterGetCount(arb);
	result->energy = cpArbiterTotalKE(arb);
	cpFloat eCoef = (1.0 - arb->e)/(1.0 + arb->e);
	for(int i = 0; i < result->count; i++){
		const struct cpContact *con = &arb->contacts[i];
		check(con->nMass > 0.0, "solved contact normal mass");
		result->normal += eCoef*con->jnAcc*con->jnAcc/con->nMass;
		if(con->tMass != 0.0){
			result->tangent += con->jtAcc*con->jtAcc/con->tMass;
		} else {
			near(con->jtAcc, 0.0, "frictionless impulse");
		}
	}
	near(result->energy, result->normal + result->tangent, "resolved contact energy");
}

static EnergyResult
collision(cpContactSolverType solver, int static_target, int box, cpFloat friction,
	cpFloat elasticity, cpFloat tangent_velocity)
{
	cpSpace *space = cpSpaceNew();
	check(cpSpaceSetContactSolver(space, solver), "select available solver");
	cpBody *a = cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
	cpBody *b = static_target ? cpSpaceGetStaticBody(space) :
		cpSpaceAddBody(space, cpBodyNew(1.0, 1.0));
	cpBodySetPosition(b, cpv(1.9, 0.0));
	cpBodySetVelocity(a, cpv(10.0, tangent_velocity));
	cpShape *sa = box ? cpBoxShapeNew(a, 2.0, 2.0, 0.0) : cpCircleShapeNew(a, 1.0, cpvzero);
	cpShape *sb = box ? cpBoxShapeNew(b, 2.0, 2.0, 0.0) : cpCircleShapeNew(b, 1.0, cpvzero);
	cpShapeSetFriction(sa, friction);
	cpShapeSetFriction(sb, friction);
	cpShapeSetElasticity(sa, elasticity);
	cpShapeSetElasticity(sb, elasticity);
	cpSpaceAddShape(space, sa);
	cpSpaceAddShape(space, sb);
	EnergyResult result = {0};
	cpCollisionHandler *handler = cpSpaceAddGlobalCollisionHandler(space);
	handler->postSolveFunc = post_solve;
	handler->userData = &result;
	cpSpaceStep(space, 0.001);
	check(result.calls == 1, "one resolved shape pair");
	check(result.count == (box ? 2 : 1), "contact manifold size");
	check(isfinite(cpBodyGetVelocity(a).x) && isfinite(cpBodyGetVelocity(b).x),
		"collision velocities remain finite");
	cpSpaceRemoveShape(space, sa);
	cpSpaceRemoveShape(space, sb);
	cpShapeFree(sa);
	cpShapeFree(sb);
	cpSpaceRemoveBody(space, a);
	cpBodyFree(a);
	if(!static_target){
		cpSpaceRemoveBody(space, b);
		cpBodyFree(b);
	}
	cpSpaceFree(space);
	return result;
}

int
main(void)
{
	test_contact_terms();
	const cpContactSolverType solvers[] = {
		CP_CONTACT_SOLVER_BASELINE, CP_CONTACT_SOLVER_AVX2, CP_CONTACT_SOLVER_WASM_SIMD
	};
	for(unsigned int i = 0; i < sizeof(solvers)/sizeof(solvers[0]); i++){
		if(!cpContactSolverIsAvailable(solvers[i])) continue;
		for(int static_target = 0; static_target <= 1; static_target++){
			for(int box = 0; box <= 1; box++){
				for(int elastic = 0; elastic <= 1; elastic++){
					EnergyResult zero = collision(solvers[i], static_target, box, 0.0, elastic*0.5, 0.0);
					EnergyResult control = collision(solvers[i], static_target, box, 0.5, elastic*0.5, 0.0);
					near(zero.tangent, 0.0, "frictionless tangential energy");
					check(zero.normal > 0.0, "normal energy loss must not be discarded");
					// Two-contact manifolds can develop transient tangential impulses
					// during iterative solving, even for symmetric initial velocities.
					if(!box) near(zero.energy, control.energy, "head-on friction consistency");
				}
			}
		}
		EnergyResult friction = collision(solvers[i], 0, 0, 0.5, 0.0, 2.0);
		check(friction.tangent > 0.0, "nonzero sliding friction energy is retained");
		printf("arbiter total KE: solver %d passed\n", (int)solvers[i]);
	}
	return 0;
}
