# Changelog

## 2.0.1

This is the third release of Munk2D, the friendly Chipmunk2D fork!

Changes:

- BUG: The wildcard CollisionHandler will be invoked twice when two shapes of
  same type collides (one time in swapped order).

## 2.0.0

This is the second release of Munk2D, the friendly Chipmunk2D fork!

This release maps to Pymunk 7.0, and contains breaking changes compared to 1.0
and the latest Chipmunk2D version.

Changes:

- API: Sensor shapes are now included in all cpSpaceQuery functions
  (cpSpacePointQuery, cpSpacePointQueryNearest, cpSpaceSegmentQuery,
  cpSpaceSegmentQueryFirst, cpSpaceBBQuery, cpSpaceShapeQuery)
- API: CollisionHandler for A,B is distinct from B,A.
- API: begin, preSolve and separate collision callbacks will always be called
- API: Removed cpArbiterIgnore.
- API: Added new cpArbiterGetProcessCollision/cpArbiterSetProcessCollision that
  can be used to ignore a collision from collision callbacks.
- API: The ProcessCollision value is not reset on each step even if set from a
  preSolve callback.
- API: The ordering of CollisionHandler callbacks is that the most specific
  callback is invoked first.

## 1.0.0 The first Munk2D release!

This is the first release of Munk2D, the friendly Chipmunk2D fork!

The main purpose of this fork is to be a companion for the Python 2D physics
library [Pymunk](https://www.pymunk.org) which is built on Chipmunk2D. Given the
slow pace of development of Chipmunk2D, and some unique requirements and
opportunities of Pymunk this is something that have grown over a long time.

New in this release compared to latest stable Chipmunk is mainly a number of bug
fixes together with documentation updates which became a requirement to have
documentation of this fork.

Note that especially the documentation is very much work in progress, but good
enough to not hold back this release anymore.

Changes:

- API: Now possible to to override cpMessage by defining CP_OVERRIDE_MESSAGE
- API: The maxForce property now have effect on DampedSpring and
  DampedRotarySpring
- API: Add transform property to cpSpaceDebugDrawOptions
- API: Use CGFloat as the float type if CP_USE_CGTYPES is set.
- BUG: Fixed total impulse calculation of DampedRotarySpring
- BUG: Fix for compiling cpPolyline with MSVC
- BUG: Fix FFI symbols not exported on windows under mingw
- BUG: Fix a potential divide by zero in ClosestT
- BUG: Fix division by zero in cpPolyShapeSegmentQuery() function.
- BUG: Fix planetmath url (by ccgargantua)
- BUG: Optimized memory allocation of cpPolylineSet (by richardgroves)
- BUG: Fix cast between incompatible function type to reduce warnings (by aganm)
- BUG: Rename body local variable to fix shadow warning (by aganm)
- BUG: Fixes bug in cpSpaceShapeQuery to not miss collisions (by alanmillard)
- BUG: Fix for cpSpaceShapeQuery using cpSegmentShape with null body (by
  maniek2332)
- MISC: Fix spelling of positive (by skitt)
- MISC: Github actions that build on Windows/Linux/MacOS with MSVC/GCC/CLang
- MISC: Fix warning for INFINITY on CLang 18+
- DOC: Reworked the documentation to be easier to maintain
- DOC: Unified license texts everywhere

## Chipmunk versions

### 7.0.3

- MISC: Replacing GLFW with Sokol in the demo application. No need to push GLFW
  binaries and has a nice x-platform renderer to build on.
- MISC: Fixed some 'const' warnings for MSCV.

### 7.0.2

- MISC: Merging pull requests. Build fixes and the like.

### 7.0.1

- BUG: Remove references to M_PI sinces it's not actually part of C and causes
  problems with MSVC.
- BUG: Build fixes for Mac/CMake and MSVC 13.
- BUG: Move to using \_\_declspec(dllexport) for Windows builds.
- BUG: Fixed a precision issue with the EPA algorithm that would cause excessive
  iteration.
- BUG: cpPolyshapeNewRaw() was undefined.
- BUG: Changing gravity will wake up all objects in a space.

### 7.0.0

- All features from Chipmunk Pro are now free and open source! (threaded and
  NEON solver, autogeometry)
- API: Lots of cleanup to the API naming for better consistency.
- API: Renamed nearest point queries to simply point queries.
- API: Removed many deprecated functions.
- API: Struct definitions have become fully opaque instead of mangling names
  with the CP_PRIVATE() macro.
- API: Replaced templated accessor functions with concrete ones. Should be
  simpler to deal with for FFIs.
- API: Optional automatic mass properties for shapes. Calculates the moment of
  inertia and center of gravity for you.
- API: Optional anchor point for bodies that is separate from the center of
  gravity.
- API: Added radius parameters to many functions dealing with shapes (moment
  calculation, initialization, etc).
- API: The convex hull and winding is automatically calculated when creating a
  poly shape.
- API: Added a cpShapesCollide() function to check overlap of arbitrary shapes.
- API: cpShape filter property to supersede layers and groups.
- API: Collision handlers now return a collision handler struct to make it
  simpler to set up callbacks.
- API: Wildcard collision types.
- API: The cpArbiterTotalImpulseWithFriction() function was renamed to
  cpArbiterTotalImpulse(). The old useless cpArbiterTotalImpulse()
  implementation was removed.
- API: Contacts now store the colliding point on the surface of both shapes.
- API: cpArbiterIsRemoval() to check if a separate callback is called due to a
  removal and not a true separating collision.
- API: Arbiters now only store one normal per pair of colliding shapes.
- API: cpBBNewForExtents().
- API: Added a concrete kinematic body type to replace the confusing "rogue"
  body concept.
- API: Added a 2x3 affine transform type, cpTransform.
- API: Added a new debug rendering API.
- MISC: Numerous improvements to the collision detection.
- MISC: cpPolyline structs are passed by reference instead of value. (I've
  regretted that decision for years!)

### 6.2.2

- Fixed some issues on arm64.
- PRO: Added a 64 bit NEON solver to use on arm64.

### 6.2.1

- Added Android support to the CMake files. (Thanks Eric Wing!)
- Added a MSVC 2012 project file. (Thanks Leonid Usov!)
- Merged a fix for VAOs on Windows. (Thanks Leonid Usov!)
- Merged a couple of other minor fixes.
- BUG: Fixed a crash issue with the ChipmunkTileCache and
  ChipmunkPointCloudSampler classes. (Pro only).

### 6.2.0

- Collision detection now primarily uses the GJK and EPA algorithms instead of
  SAT. Internally this was a rather huge change. o_O
- Improved collision point quality and better collision point identification.
- All shape types can now be given a rounding radius.
- Collisions are now guaranteed to have a maximum of 2 collision points.
- Poly to poly collision performance is slightly better when they have a radius.
  Slightly worse with none.
- Implemented smoothed segment collisions to prevent colliding with the "cracks"
  between segment shapes.
- API: (Officially) added cpSegmentShapeSetNeighbors() used to enable smoothed
  line collisions.
- API: Added cpBBCenter() to get the center of a bounding box.
- API: Added cpPolyShapeInit2() and cpPolyShapeNew2() to create poly shapes with
  a radius. (Horrible names yes, but it will go away in Chipmunk 7)
- API: Added cpBoxShapeInit3() and cpBoxShapeNew3() to create boxes with a
  radius.
- API: Added cpPolyShapeGetRadius() and cpPolyShapeSetRadius() (the latter only
  in chipmunk_unsafe.h).
- API: Added cpNearestPointQueryInfo.g which returns the gradient of the signed
  distance field for the shape.
- BUG: cpMomentForPoly() will now return a correct value for degenerate 2 vertex
  polygons.
- BUG: Fixed an issue where certain segment query calls would return a t value
  of 0 instead of 1 for a missed query.
- MISC: Passing cpvzero to cpvnormalize() will now return cpvzero. No need to
  worry about NaNs or cpvnormalize_safe().
- MISC: Demo app now uses GLFW instead of GLUT, and has improved drawing and
  text rendering routines.

### 6.1.5

- API: Added cpArbiter\*SurfaceVelocity() to allow for custom surface velocity
  calculation.
- API: Added cpArbiteSetContactPointSet() to allow changing the contact geometry
  on the fly.
- API: Added cpSpaceConvertBodyToStatic() and cpSpaceConvertBodyToDynamic().
- API: Added [ChipmunkBody velocityAt*Point:] methods to wrap their C
  equivalents. (Pro only)
- API: Added overridable [ChipmunkBody updateVelocity:...] and [ChipmunkBody
  updatePosition:] methods. (Pro only)
- API: Added .space properties to ChipmunkBody, ChipmunkShape and
  ChipmunkConstaint to wrap their C equivalents. (Pro only)
- API: Added overridable [ChipmunkConstraint preSolve:] and [ChipmunkConstraint
  postSolve:] methods. (Pro only)
- API: Added an ChipmunkMultiGrab.grabSort property that allows you to
  prioritize which shape is grabbed when there is overlap. (Pro only)
- MISC: Segment queries started inside of a shape now return t=0 and n=cpvzero
  instead of being undefined.
- MISC: Cleaned up a lot of common assertion messages to be more clear.
- MISC: Added a new demo called Shatter.
- MISC: Added a crushing force estimation example to the ContactGraph demo and a
  static/dynamic conversion example to Plink.
- MISC: Modified the Sticky demo to use the new cpArbiteSetContactPointSet() to
  avoid the use of unnecessary sensor shapes.
- MISC: [ChipmunkSpace addBounds:...] now returns a NSArray of the bounding
  segments. (Pro only)

### 6.1.4

- MISC: Fixed a build script issue that was preventing the documentation from
  being generated.

### 6.1.3

- BUG: Fixed a couple of very specific but fatal bugs that occur when sleeping
  is enabled and filtering collisions.
- BUG: Fixed an issue with cpvslerp() between very similar vectors.
- BUG: Fixed an issue with grab friction in ChipmunkMultiGrab. (Pro only)
- MISC: Implemented the cpConstraintGetImpulse() functionality for spring
  joints.
- MISC: Added more functions to chipmunk_ffi.h

### 6.1.2

- API: Added a cpArbiter.data pointer. Now you can tag collisions with custom
  persistent data.
- API: Added segment to segment collisions (thanks to LegoCylon)
- API: cpSpaceAddPostStepCallback() now returns false if the callback was a
  duplicate.
- API: Added the ChipmunkAbstractSampler.marchThreshold property instead of
  hardcoding it to 0.5.
- API: Added ChipmunkGrooveJoint properties for the groove and joint anchors.
- API: ChipmunkMultiGrab now returns information about grabbed shapes.
- BUG: Fixed a minor (non-crashing, non-leaking) memory pooling issue with
  reindexing lots of static shapes.
- BUG: Fixed an issue with the slerp functions that would cause them to return
  incorrect results when given non-unit length input.
- BUG: Fixed a precision bug with the ChipmunkImage sampler classes that could
  cause artifacts or miss small features.
- BUG: Fixed a number of properties in Objective-Chipmunk that should have been
  nonatomic.
- BUG: Fixed a number of types in Objective-Chipmunk that were incorrectly id
  that should have been cpGroup, cpCollisionType etc. It's now possible to
  redefine them at compile time if you wish.
- MISC: Dropped armv6 support in favor of armv7s on iOS. (You can switch it back
  easily if you need.)
- MISC: Updated iOS build scripts to guess the latest SDK.
- MISC: Added the "Sticky Surfaces" demo as a cpArbiter.data example.
- MISC: Updated Objective-Chipmunk build scripts to always use the latest iOS
  SDK.

### 6.1.1

- API: Renamed the new block based iterators as soon as possible to match the
  Apple convention ("\_b" suffix).

### 6.1.0

- API: Added a pthread based, multi-threaded solver to accelerate the game on
  multi-core systems. (Pro only)
- API: Added cpConvexHull() and CP_CONVEX_HULL() for generating convex hulls.
- API: Added cpPolylineConvexDecomposition_BETA() to generate an approximate
  concave decomposition of a polyline. (Pro only)
- API: Added [ChipmunkPolyline toConvexHull:] to generate approximate convex
  hulls. (Pro only).
- API: Added [ChipmunkPolylineSet toConvexHulls_BETA:]. (Pro only)
- API: Added nearest point queries.
- API: Added a push mode to ChipmunkMultiGrab so touches can interact with the
  scene even if they didn't initially touch a shape. (Pro only)
- API: Added optional block based iterators.
- API: Added a space property to cpBody, cpShape and cpConstraint types.
- BUG: Fixed an issue with changing the floating point and vector type on OS X.
- BUG: Fixed a pixel offset in ChipmunkImageSampler that could cause minor
  sampling artifacts. (Pro only)
- BUG: Fixed an issue where cpShape and cpConstraint structs could have garbage
  space pointers if cpcalloc() was redefined.
- BUG: Fixed assertions in cpArbiter getters to correctly reflect a contact
  count of 0 from separate() callbacks.
- BUG: Fixed a regression relating to registering post-step() callbacks from
  other post-step() callbacks.
- BUG: Fixed a minor memory leak for sleeping bodies when destroying a space.
- MISC: Point queries are now deprecated in preference to point queries.
- MISC: cpSpatialIndexPointQuery() was redundant and has been removed. Use
  cpSpatialIndexQuery() instead.
- MISC: cpShape\*Query() functions now accept a NULL info pointer if you don't
  want detailed query info.
- MISC: The enableContactGraph property of cpSpace is deprecated and always be
  true.
- MISC: Added a new demos of the convex hull functions and a self balancing
  Unicycle.

### 6.0.3

- API: Added a cpBBForCircle() convenience function.
- API: Added cpBBSegmentQuery() to check where a segment hits a cpBB.
- API: Added cpBodyGetVelAtWorldPoint() and cpBodyGetVelAtLocalPoint() to get
  point velocities on a body.
- API: Added cpArbiterTotalKE() to calculate the energy lost due to a collision.
  Great for calculating damage accurately.
- API: Added methods to get an ObjC pointer from a C chipmunk struct.
- API: Added a CHIPMUNK_ARBITER_GET_BODIES() macro for Objective-Chipmunk.
- API: The Objective-Chipmunk headers are now ARC compatible.
- API: Added a [ChipmunkSpace contains:] method to check if a ChipmunkObject has
  been added to the space or not.
- API: Added a cpBBNewForCircle() function.
- API: Added a cpBBSegmentQuery() function for raycasting againsts AABBs.
- BUG: Fixed a regression with ChipmunkSpace.bodies and ChipmunkSpace.shapes
  that caused crashes.
- BUG: Fixed a rare bug with postStep() callbacks and iterators.
- BUG: Fixed a border case in cpBBIntersectsSegment() that could cause missed
  segment queries.
- MISC: Added some new assertions for error conditions that were previously
  uncaught.
- MISC: Accelerated segment queries in cpBBTree by sorting the nodes.
- MISC: Added a new "Slice" demo that lets you cut up a polygon.
- MISC: Added NEON optimizations for Chipmunk Pro. Expect running on most ARM
  platforms to be 25-35% faster for contact heavy simulations.
- MISC: All ChipmunkObject instances added to a space are now retained, even
  composite ones.

### 6.0.2

- API: Added cpSpaceIsLocked() to check if you are in a callback or not.
- API: Removed the long deprecated [ChipmunkSpace addShapeAHandler:] and
  [ChipmunkSpace addShapeBHandler:] methods.
- API: The ChipmunkObject protocol now can return any id<NSFastEnumeration>
  object instead of just an NSSet.
- API: The largely useless [ChipmunkSpace addBaseObjects:] and [ChipmunkSpace
  removeBaseObjects:] methods were removed.
- API: Added [ChipmunkSpace smartAdd:] and [ChipmunkSpace smartRemove:] methods
  for a consistent API to remove objects inside and out of callbacks.
- API: Added [ChipmunkSpace addPostStepBlock:key:] to complement [ChipmunkSpace
  addPostStepCallback:selector:key:].
- API: Added [ChipmunkSpace addPostStepAddition:].
- API: Objective-Chipmunk collision handlers no longer retain their target to
  avoid reference cycles.
- API: Added callbacks to joints.
- BUG: Soft errors (only checked when debug mode is enabled) and warnings were
  disabled. Whoops.
- BUG: cpShapeIsSensor() was incorrectly named in chipmunk_ffi.h.
- BUG: It should be safe to call cpActivateBody() from an space iterator
  callback now.
- MISC: Very nice bouyancy demo added based on callbacks.
- MISC: Breakable Joints demo showing how to use the new joint callbacks.
- MISC: Player demo updated and greatly enhanced by Chipmunk 6 features.
- MISC: Changed adding a static body to a space from a warning to a hard error.
- MISC: cpGroup and cpCollisionType now default to uintptr_t so you can safely
  use pointers instead of ints for these types.
- MISC: Updated the MSVC10 project file.
- MISC: Updated the FFI defs.

### 6.0.1

- BUG: Calling cpBodySetPos() on a sleeping body was delaying the Separate()
  handler callback if one existed.
- BUG: Fixed a bug where Separate() handler callbacks were not occuring when
  removing shapes.
- BUG: Calling cpBodyApplyForce() or cpBodyResetForces() was not activating
  sleeping bodies.
- API: Added cpSpaceEachConstraint().
- API: Added a "CurrentTimeStep" property to cpSpace to retrieve the current (or
  most recent) timestep.
- MISC: Got rid of anonymous unions so that it is C99 clean again.

### 6.0.0

Chipmunk 6.x's API is not quite 100% compatible with 5.x. Make sure you read the
list of changes carefully. Keep in mind that this is a x.0.0 release and that
it's likely there are still some bugs I don't know about yet. I've spent a lot
of effort rewriting the collision detection, sleeping, and contact graph
algorithms that have required large changes and cleanup to the 5.x codebase.
I've ironed out all the bugs that I know of, and the beta test went well. So
it's finally time for 6!

- API: Chipmunk now has hard runtime assertions that aren't disabled in release
  mode for many error conditions. Most people have been using release builds of
  Chipmunk during development and were missing out on very important error
  checking.
- API: Access to the private API has been disabled by default now and much of
  the private API has changed. I've added official APIs for all the uses of the
  private API I knew of.
- API: Added accessor functions for every property on every type. As Chipmunk's
  complexity has grown, it's become more difficult to ignore accessors. You are
  encouraged to use them, but are not required to.
- API: Added cpSpaceEachBody() and cpSpaceEachShape() to iterate bodies/shapes
  in a space.
- API: Added cpSpaceReindexShapesForBody() to reindex all the shapes attached to
  a particular body.
- API: Added a 'data' pointer to spaces now too.
- API: cpSpace.staticBody is a pointer to the static body instead of a static
  reference.
- API: The globals cp_bias_coef, cp_collision_slop, cp_contact_persistence have
  been moved to properties of a space. (collisionBias, collisionSlop,
  collisionPersistence respectively)
- API: Added cpBodyActivateStatic() to wake up bodies touching a static body
  with an optional shape filter parameter.
- API: Added cpBodyEachShape() and cpBodyEachConstraint() iterators to iterate
  the active shapes/constraints attached to a body.
- API: Added cpBodyEeachArbiter() to iterate the collision pairs a body is
  involved in. This makes it easy to perform grounding checks or find how much
  collision force is being applied to an object.
- API: The error correction applied by the collision bias and joint bias is now
  timestep independent and the units have completely changed.
- FIX: Units of damping for springs are correct regardless of the number of
  iterations. Previously they were only correct if you had 1 or 2 iterations.
- MISC: Numerous changes to help make Chipmunk work better with variable
  timesteps. Use of constant timesteps is still highly recommended, but it is
  now easier to change the time scale without introducing artifacts.
- MISC: Performance! Chipmunk 6 should be way faster than Chipmunk 5 for almost
  any game.
- MISC: Chipmunk supports multiple spatial indexes and uses a bounding box tree
  similar to the one found in the Bullet physics library by default. This should
  provide much better performance for scenes with objects of differening size
  and works without any tuning for any scale.

### 5.3.5

- FIX: Fixed spelling of cpArbiterGetDepth(). Was cpArbiteGetDepth() before.
  Apparently nobody ever used this function.
- FIX: Added defines for M_PI and M_E. Apparently these values were never part
  of the C standard math library. Who knew!?
- FIX: Added a guard to cpBodyActivate() so that it's a noop for rogue bodies.
- FIX: Shape queries now work with (and against) sensor shapes.
- FIX: Fixed an issue where removing a collision handler while a separate()
  callback was waiting to fire the next step would cause crashes.
- FIX: Fixed an issue where the default callback would not be called for sensor
  shapes.
- FIX: Resetting or applying forces or impulses on a body causes it to wake up
  now.
- MISC: Added a check that a space was not locked when adding or removing a
  callback.
- MISC: Removed cpmalloc from the API and replaced all occurences with cpcalloc
- MISC: Added a benchmarking mode to the demo app. -trial runs it in time trial
  mode and -bench makes it run some benchmarking demos.

### 5.3.4

- FIX: cpBodyActivate() can now be called from collision and query callbacks.
  This way you can use the setter functions to change properties without
  indirectly calling cpBodyActivate() and causing an assertion.
- FIX: cpArbiterGetContactPointSet() was returning the collision points for the
  normals.
- FIX: cpSpaceEachBody() now includes sleeping bodies.
- FIX: Shapes attached to static rogue bodies created with cpBodyNewStatic() are
  added as static shapes.
- MISC: Applied a user patch to update the MSVC project and add a .def file.

### 5.3.3

- API: Added cpArbiteGetCount() to return the number of contact points.
- API: Added helper functions for calculating areas of Chipmunk shapes as well
  as calculating polygon centroids and centering polygons on their centroid.
- API: Shape queries. Query a shape to test for collisions if it were to be
  inserted into a space.
- API: cpBodyInitStatic() and cpBodyNewStatic() for creating additional static
  (rogue) bodies.
- API: cpBodySleepWithGroup() to allow you to create groups of sleeping objects
  that are woken up together.
- API: Added overloaded \*, +, - and == operators for C++ users.
- API: Added cpSpaceActivateShapesTouchingShape() to query for and activate any
  shapes touching a given shape. Useful if you ever need to move a static body.
- FIX: Fixed an extremely rare memory bug in the collision cache.
- FIX: Fixed a memory leak in Objective-Chipmunk that could cause ChipmunkSpace
  objects to be leaked.
- MISC: C struct fields and function that are considered private have been
  explicitly marked as such. Defining CP_ALLOW_PRIVATE_ACCESS to 0 in Chipmunk.h
  will let you test which parts of the private API that you are using and give
  me feedback about how to build proper APIs in Chipmunk 6 for what you are
  trying to do.
- MISC: Allow CGPoints to be used as cpVect on Mac OS X as well as iOS.

### 5.3.2

- FIX: Collision begin callbacks were being called continuously for sensors or
  collisions rejected from the pre-solve callback.
- FIX: Plugged a nasty memory leak when adding post-step callbacks.
- FIX: Shapes were being added to the spatial hash using an uninitialized
  bounding box in some cases.
- FIX: Perfectly aligned circle shapes now push each other apart.
- FIX: cpBody setter functions now call cpBodyActivate().
- FIX: Collision handler targets are released in Objective-Chipmunk when they
  are no longer needed instead of waiting for the space to be deallocated.
- API: cpSpaceSegmentQuery() no longer returns a boolean. Use
  cpSpaceSegmentQueryFirst() instead as it's more efficient.
- NEW: cpSpaceRehashShape() Rehash an individual shape, active or static.
- NEW: cpBodySleep() Force a body to fall asleep immediately.
- NEW: cpConstraintGetImpulse() Return the most recent impulse applied by a
  constraint.
- NEW: Added setter functions for the groove joint endpoints.
- MISC: A number of other minor optimizations and fixes.

### 5.3.1

- NEW: Added a brand new tutorial for Objective-Chipmunk:
  SimpleObjectiveChipmunk that can be found in the Objective-Chipmunk folder.
- NEW: Proper API docs for Objective-Chipmunk.
- NEW: Updated the included Objective-Chipmunk library.
- FIX: Fixed a rare memory crash in the sensor demo.
- FIX: Fixed some warnings that users submitted.

### 5.3.0

- FIX: Fixed the source so it can compile as C, C++, Objective-C, and
  Objective-C++.
- FIX: Fixed cp_contact_persistence. It was broken so that it would forget
  collision solutions after 1 frame instead of respecting the value set.
- OPTIMIZATION: Several minor optimizations have been added. Though performance
  should only differ by a few percent.
- OPTIMIZATION: Chipmunk now supports putting bodies to sleep when they become
  inactive.
- API: Elastic iterations are now deprecated as they should no longer be
  necessary.
- API: Added API elements to support body sleeping.
- API: Added a statically allocated static body to each space for attaching
  static shapes to.
- API: Static shapes attached to the space's static body can simply be added to
  the space using cpSpaceAddShape().
- NEW: New MSVC projects.
- NEW: Added boolean and time stamp types for clarity.

### 5.2.0

- OPTIMIZATION: Chipmunk structs used within the solver are now allocated
  linearly in large blocks. This is much more CPU cache friendly. Programs have
  seen up to 50% performance improvements though 15-20% should be expected.
- API: Shape references in cpArbiter structs changed to private_a and private_b
  to discourage accessing the fields directly and getting them out of order. You
  should be using cpArbiterGetShapes() or CP_ARBITER_GET_SHAPES() to access the
  shapes in the correct order.
- API: Added assertion error messages as well as warnings and covered many new
  assertion cases.
- FIX: separate() callbacks are called before shapes are removed from the space
  to prevent dangling pointers.
- NEW: Added convenience functions for creating box shapes and calculating
  moments.

### 5.1.0

- FIX: fixed a NaN issue that was causing raycasts for horizontal or vertical
  lines to end up in an infinite loop
- FIX: fixed a number of memory leaks
- FIX: fixed warnings for various compiler/OS combinations
- API: Rejecting a collision from a begin() callback permanently rejects the
  collision until separation
- API: Erroneous collision type parameterns removed from
  cpSpaceDefaulteCollisionHandler()
- MOVE: FFI declarations of inlined functions into their own header
- MOVE: Rearranged the project structure to separate out the header files into a
  separate include/ directory.
- NEW: Added a static library target for the iPhone.
- NEW: Type changes when building on the iPhone to make it friendlier to other
  iPhone APIs
- NEW: Added an AABB query to complement point and segment queries
- NEW: CP_NO_GROUP and CP_ALL_LAYERS constants

### 5.0.0

- Brand new Joint/Constraint API: New constraints can be added easily and are
  much more flexible than the old joint system
- Efficient Segment Queries - Like raycasting, but with line segments.
- Brand new collision callback API: Collision begin/separate events, API for
  removal of objects within callbacks, more programable control over collision
  handling.
