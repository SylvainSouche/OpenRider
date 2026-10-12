#include "maptiles/screen.h"

namespace maptiles {

// @impl 0f87-6acd-5e67-4b57
std::vector<size_t> simplify_screen(const std::vector<float> &xs, const std::vector<float> &ys,
                                    float min_px)
{
    std::vector<size_t> kept;
    const size_t n = xs.size() < ys.size() ? xs.size() : ys.size();
    if (n == 0)
        return kept;
    kept.push_back(0);
    const float min2 = min_px * min_px;
    for (size_t i = 1; i < n; ++i) {
        const size_t last = kept.back();
        const float dx = xs[i] - xs[last], dy = ys[i] - ys[last];
        if (dx * dx + dy * dy >= min2)
            kept.push_back(i);
    }
    if (kept.back() != n - 1)
        kept.push_back(n - 1);
    return kept;
}

} // namespace maptiles
