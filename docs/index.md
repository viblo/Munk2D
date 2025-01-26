> [!note] These documents are not yet fully updated to reference Munk2D. Links
> will still point to Chipmunk2D, the text will still be written where "I"
> references Slembcke, the creator of Chipmunk2D etc.

## What is Chipmunk2D?

First of all, [Chipmunk2D](http://chipmunk2d.net) is a 2D rigid body physics
library distributed under the MIT license. It is intended to be fast, portable,
numerically stable, and easy to use. For this reason it's been used in hundreds
of games across every recent system you can name. I've put thousands of hours of
work over many years to make Chipmunk2D what it is today. I hope you enjoy
working with it! Chipmunk2D Pro builds on what we've made with C Chipmunk2D and
extended it with more features, platform specific performance tuning, and new
features. Currently, we offer a Objective-C binding, ARM NEON optimizations for
mobile CPUs, and a very high performance and flexible automatic geometry
library. Objective-Chipmunk for iPhone/Mac provides extended APIs and a first
rate Objective-C wrapper with support for properties, blocks and ARC (automatic
reference counting) to make Chipmunk2D coding more efficient with your time on
Apple platforms. The ARM optimizations use the NEON SIMD coprocessor found in
any smartphone using an ARM Cortex A\* CPU including all iOS hardware since the
3GS and many Android phones. The auto geometry library can generate geometry
from bitmap images or procedural data. You can use it for generating convex hull
shapes from sprites, or create an entire level's collision shapes from a
background image. It can also be easily set up to be used for high-performance
deformable terrain or infinite procedural terrain. If you'd like to hear more
about Chipmunk2D Pro or download a trial library check out the
[Chipmunk2D Pro](http://chipmunk-physics.net/chipmunkPro.php).

## Where do I start?

You might want to start by looking at some
[sample code](http://chipmunk-physics.net/downloads.php). From there, you
probably want to skim over the latest
[documentatio](http://files.slembcke.net/chipmunk/release/ChipmunkLatest-Docs/).
While geared towards C developers, it introduces all the important building
blocks. The Objective-C API simply takes things to the next logical step.

## Note

These Doxygen docs cover both the C Chipmunk2D API as well as
Objective-Chipmunk. The Objective-Chipmunk classes (Chipmunk*) are not available
except with Chipmunk2D Pro. */
