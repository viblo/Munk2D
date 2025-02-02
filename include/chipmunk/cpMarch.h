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

#ifdef __cplusplus
extern "C" {
#endif

/// Function type used as a callback from the marching squares algorithm to sample an image function.
/// It passes you the point to sample and your context pointer, and you return the density.
typedef cpFloat (*cpMarchSampleFunc)(cpVect point, void *data);

/// Function type used as a callback from the marching squares algorithm to output a line segment.
/// It passes you the two endpoints and your context pointer.
typedef void (*cpMarchSegmentFunc)(cpVect v0, cpVect v1, void *data);

/// Trace an anti-aliased contour of an image along a particular threshold.
/// The given number of samples will be taken and spread across the bounding box area using the sampling function and context.
/// The segment function will be called for each segment detected that lies along the density contour for @c threshold.
CP_EXPORT void cpMarchSoft(
  cpBB bb, unsigned long x_samples, unsigned long y_samples, cpFloat threshold,
  cpMarchSegmentFunc segment, void *segment_data,
  cpMarchSampleFunc sample, void *sample_data
);

/// Trace an aliased curve of an image along a particular threshold.
/// The given number of samples will be taken and spread across the bounding box area using the sampling function and context.
/// The segment function will be called for each segment detected that lies along the density contour for @c threshold.
CP_EXPORT void cpMarchHard(
  cpBB bb, unsigned long x_samples, unsigned long y_samples, cpFloat threshold,
  cpMarchSegmentFunc segment, void *segment_data,
  cpMarchSampleFunc sample, void *sample_data
);

#ifdef __cplusplus
}
#endif