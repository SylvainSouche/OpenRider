/// @file
/// Transformation between two coordinate systems, one point or a batch of
/// vertices at a time.
#pragma once

#include "reproj/crs.h"

#include <cstddef>

namespace reproj {

/// Converts coordinates from a source to a target system, through
/// geographic coordinates and, between different datums, WGS 84.
/// Immutable: safe to share between threads.
class Transformer {
public:
    /// From `source` to `target`; both must be Valid().
    Transformer(Crs source, Crs target);

    /// The source system.
    const Crs &Source() const { return source_; }
    /// The target system.
    const Crs &Target() const { return target_; }
    /// The transformer from target to source.
    Transformer Inverse() const { return Transformer(target_, source_); }

    /// Transforms one point.
    /// @return false where either projection is undefined (x, y untouched)
    bool Transform(double x, double y, double &out_x, double &out_y) const;

    /// Transforms `count` vertices in place; those that cannot be
    /// transformed become NaN, as do NaN inputs. Large batches are spread
    /// over the CPU's cores.
    /// @return the number of vertices that became NaN
    size_t TransformPoints(double *xs, double *ys, size_t count) const;

private:
    /// TransformPoints on the calling thread.
    size_t TransformRange(double *xs, double *ys, size_t count) const;

    Crs source_;     ///< from
    Crs target_;     ///< to
    bool identity_;  ///< same system: copies
};

} // namespace reproj
