/* Copyright (c) 2025 Victor Blomqvist
 * Copyright (c) 2007-2024 Scott Lembcke and Howling Moon Software
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
*/

// This was previsoly part of the non-open source Chipmunk Pro.
// See https://github.com/slembcke/Chipmunk2D/issues/221 for the change of license
// and http://chipmunk2d.net/legal.php for more information on previous licenses.

#import "ObjectiveChipmunk/ObjectiveChipmunk.h"
#import "ChipmunkAutoGeometry.h"

/**
	A point cloud sampler allows you to perform deformable terrain like with a bitmap backed sampler,
	but without any bounds. It only requires memory for the points you add instead of large RAM chewing bitmap.
	However, unlike a bitmap, the deformation can only go one way. (i.e. You can add or remove terrain, but not both).
	Without any points, the sampler will return 1.0. Adding points will put "holes" in it causing it to return lower values.
*/
@interface ChipmunkPointCloudSampler : ChipmunkAbstractSampler {
@private
	cpFloat _cellSize;
	cpSpatialIndex *_index;
}

/// Initialize the sampler with the given cell size,
/// which should roughly match the size of the points added to the sampler.
- (id)initWithCellSize:(cpFloat)cellSize;

/// Add a point to the cloud and return the dirty rect for the point.
-(cpBB)addPoint:(cpVect)pos radius:(cpFloat)radius fuzz:(cpFloat)fuzz;

@end
