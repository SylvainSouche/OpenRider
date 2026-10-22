#include "conformal.h"

#include <algorithm>
#include <cmath>

namespace reproj {

double ConformalTau(double tau, double e)
{
    const double tau1 = std::hypot(1.0, tau);
    const double sigma = std::sinh(e * std::atanh(e * tau / tau1));
    return std::hypot(1.0, sigma) * tau - sigma * tau1;
}

double GeographicTau(double tau_prime, double e)
{
    const double e2m = 1.0 - e * e;
    double tau = tau_prime / e2m;
    for (int i = 0; i < 10; ++i) {
        const double tau_prime_i = ConformalTau(tau, e);
        const double step = (tau_prime - tau_prime_i) / std::hypot(1.0, tau_prime_i) * (1.0 + e2m * tau * tau) /
                            (e2m * std::hypot(1.0, tau));
        tau += step;
        if (std::fabs(step) <= 1e-15 * std::max(1.0, std::fabs(tau)))
            break;
    }
    return tau;
}

} // namespace reproj
