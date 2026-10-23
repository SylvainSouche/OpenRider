/// @file
/// Conformal latitude helpers shared by the conformal projections
/// (module-private). Latitudes are handled through their tangents, which
/// keeps full precision near the poles (Karney 2011).
#pragma once

namespace reproj {

/// tan of the conformal latitude from tan of the geographic latitude.
double ConformalTau(double tau, double e);

/// tan of the geographic latitude from tan of the conformal latitude
/// (Newton's method; converges in two or three steps).
double GeographicTau(double tau_prime, double e);

} // namespace reproj
