# Munk2D

## FORK INFO

The main purpose of this fork is to be a companion for the Python 2D physics
library [Pymunk](https://www.pymunk.org) which is built on Chipmunk2D. Given the
slow pace of development of Chipmunk2D, and some unique requirements and
opportunities of Pymunk this is something that have grown over a long time. What
really made me consider making this fork more formal was the discussion
[here](https://github.com/slembcke/Chipmunk2D/issues/237) with Slembcke, the
creator of Chipmunk2D.

I do not foresee that I have the time, motivation or skills to really revive
Chipmunk2D. However, I hope to incorporate minor new features, and a bunch of
fixes. The changes are mainly driven by what make sense from the Pymunk use
case. However, I do think many of these changes are useful also to users outside
Pymunk, and you are of course free to use the fork for other projects /
languages as well.

At the moment I don't have any formal release of this fork, but I plan to make
some kind of rename and a release if/when enough changes are accumulated. Until
a proper release is out I do not recommend usage of Munk2D, since I will force
push file structure changes while I try to make the documentation ok!

/Victor 2025-01-25

## ABOUT

This is a fork of the 2D physics library
[Chipmunk2D](https://github.com/slembcke/Chipmunk2D). Munk2D is a simple,
lightweight, fast and portable 2D rigid body physics library written in C. It's
licensed under the permissive MIT license. I hope you enjoy using Munk2D!

### Expectations from Munk2D

- Munk2D will add fixes for various issues, especially issues affecting Pymunk.
- Munk2D will add or improve features useful for Pymunk.
- Munk2D does not aim for ABI stability between versions.
- Munk2D will try to avoid API breaking changes, but ultimately be less strict
  than Chipmunk2D.
- While Munk2D will not try to actively break features or targets, breakage is
  more likely than with Chipmunk2D. Testing of Munk2D will mainly happen through
  Pymunk, parts not useful from Pymunk (e.g. ObjectiveC support), will be
  untested and therefor likely to break over time.
- While Munk2D is focused on Pymunk, I will happily accept PRs to fix or improve
  other parts of the code, or improvements to tests, documentation and so on.

## FEATURES

- Designed specifically for 2D video games.
- Circle, convex polygon, and beveled line segment collision primitives.
- Multiple collision primitives can be attached to a single rigid body.
- Fast broad phase collision detection by using a bounding box tree with great
  temporal coherence or a spatial hash.
- Extremely fast impulse solving by utilizing Erin Catto's contact persistence
  algorithm.
- Supports sleeping objects that have come to rest to reduce the CPU load.
- Support for collision event callbacks based on user definable object types.
- Flexible collision filtering system with layers, exclusion groups and
  callbacks. \*\* Can be used to create all sorts of effects like one way
  platforms or buoyancy areas. (Examples included)
- Supports nearest point, segment (raycasting), shape and bounding box queries
  to the collision detection system.
- Collision impulses amounts can be retrieved for gameplay effects, sound
  effects, etc.
- Large variety of joints - easily make vehicles, ragdolls, and more.
- Joint callbacks. \*\* Can be used to easily implement breakable or animated
  joints. (Examples included)
- Maintains a contact graph of all colliding objects.
- Lightweight C99 implementation with no external dependencies outside the Std.
  C library.
- "Many language bindings available":http://chipmunk2d.net/bindingsAndPorts.php.
- Simple, read the "documentation":https://viblo.github.io/Munk2D/ and see!
- Unrestrictive MIT license

## BUILDING

Mac OS X: There is an included Xcode project file for building the static
library and demo application. Alternatively you could use the CMake files or the
macstatic.command script inside the xcode/ directory to build a static lib and
package up the headers for you.

iPhone: A native Objective-C API is included. The Xcode project can build a
static library with all the proper compiler settings. Alternatively, you can
just run iphonestatic.command in the xcode/ directory. It will build you a fat
library compiled as release for the device and debug for the simulator. After
running it, you can simply drop the Chipmunk-iOS directory into your iPhone
project!

UNIXes: A forum user was kind enough to make a set of CMake files for Chipmunk.
This will require you to have CMake installed. To build run 'cmake .' then
'make'. This should build a dynamic library, a static library, and the demo
application. A number of people have had build errors on Ubuntu due to not
having GLUT or libxmu installed.

Windows: Visual Studio projects are included in the msvc/ directory. While I try
to make sure the MSVC 10 project is up to date, I don't have MSVC 9 to keep that
project updated regularly. It may not work. I'd appreciate a hand fixing it if
that's the case.

## GET UP TO DATE

If you got the source from a point release download, you might want to consider
getting the latest source from GitHub. Bugs are fixed, and new features are
added regularly. Big changes are done in branches and tested before merging them
in it's rare for the point release downloads to be better or more bug free than
the latest code.

Head on over to "GitHub":https://github.com/viblo/Munk2D and experience the
future TODAY! (Okay, so maybe it's not that exciting.)

## GETTING STARTED

The C API documentation is in the docs/ directory.

A good starting point is to take a look at the included Demo application. The
demos all just set up a Munk2D simulation space and the demo app draws the
graphics directly out of that. This makes it easy to see how the Munk2D API
works without worrying about the graphics code. You are free to use the demo
drawing routines in your own projects, though it is certainly not the
recommended way of drawing Munk2D objects as it pokes around at the
undocumented/private APIs of Munk2D.

## SUPPORT

There is a forum for Chipmunk2D at http://chipmunk2d.net/forum/ Unfortunately
its very inactive nowadays, but much of the discussions are still relevant.

The best way to get support is to visit the "Chipmunk
Forums":http://chipmunk2d.net/forum/. There are plenty of people around using
Chipmunk on the just about every platform I've ever heard of. If you are working
on a commercial project and want some more direct help, Howling Moon Software is
also available for "contracting":http://howlingmoonsoftware.com/.

## CONTRACTING

Howling Moon Software (company of Chipmunk2D) is available for contracting if
you want to make the physics in your game really stand out. Given their unique
experience with Chipmunk2D, they can help you use Chipmunk (and likely Munk2D)
to its fullest potential. Feel free to contact them through their webpage:
http://howlingmoonsoftware.com/
