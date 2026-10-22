/// @file
/// Fast transformation of many points within a known region: through a
/// grid of exactly transformed nodes, interpolated bilinearly. Drawing
/// tracks costs a few nanoseconds per point instead of a full projection.
#pragma once

#include "reproj/transformer.h"
#include "reproj/warp_mesh.h"

namespace reproj {

/// Transforms points through an interpolation grid over `box` (in the
/// transformer's source system), refined until it errs by at most a
/// tolerance; points outside the box, and grids that could not reach the
/// tolerance, are transformed exactly. Immutable once built.
class GridProjector {
public:
    /// A grid over `box` (top-left (x0, y0), bottom-right (x1, y1)) for
    /// `transformer`, within `tolerance` target units.
    GridProjector(Transformer transformer, const Box &box, double tolerance, int max_cells = 64);

    /// Transforms one point.
    /// @return false where the transformation is undefined
    bool Project(double x, double y, double &out_x, double &out_y) const;

    /// The transformer, for exact transformations.
    const Transformer &Exact() const { return transformer_; }

private:
    Transformer transformer_;  ///< exact transformation
    Box box_;                  ///< region the grid covers
    WarpMesh mesh_;            ///< its nodes
    bool usable_;              ///< the grid reached the tolerance
};

/// Scale of `crs` at (lon, lat) WGS84 degrees: its units per metre on the
/// ground along the meridian (1 on a UTM central meridian is 0.9996;
/// 1/cos(lat) for Mercator; for a geographic system, degrees per metre).
double ScaleFactor(const Crs &crs, double lon, double lat);

} // namespace reproj
