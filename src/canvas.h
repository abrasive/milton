//    Milton Paint
//    Copyright (C) 2015  Sergio Gonzalez
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License along
//    with this program; if not, write to the Free Software Foundation, Inc.,
//    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

#pragma once

typedef struct Brush_s {
    i32     radius;  // This should be replaced by a BrushType and some union containing brush info.
    v4f     color;
    f32     alpha;
} Brush;

typedef struct Stroke_s {
    Brush           brush;
    i32*            points_x;
    i32*            points_y;
    f32*            pressures;
    i32             num_points;
} Stroke;

// From cord template
#include "StrokeCord.generated.h"

typedef struct CanvasView_s {
    v2i     screen_size;            // Size in pixels
    f32     aspect_ratio;
    i32     scale;                  // Zoom
    v2i     screen_center;          // In pixels
    v2i     pan_vector;             // In canvas scale
    i32     downsampling_factor;
    i32     canvas_radius_limit;
} CanvasView;


v2i canvas_to_raster(CanvasView* view, v2i canvas_point);

v2i raster_to_canvas(CanvasView* view, v2i raster_point);

// Returns an array of `num_strokes` b32's, masking strokes to the rect.
b32* filter_strokes_to_rect(Arena* arena,
                            StrokeCord* strokes,
                            const Rect rect);

// Does point p0 with radius r0 contain point p1 with radius r1?
b32 stroke_point_contains_point(v2i p0, i32 r0,
                                     v2i p1, i32 r1);

Rect bounding_box_for_stroke(Stroke* stroke);

Rect bounding_box_for_last_n_points(Stroke* stroke, i32 last_n);

Rect canvas_rect_to_raster_rect(CanvasView* view, Rect canvas_rect);

