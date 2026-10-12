/* OpenRider filter abstraction.
 *
 * A filter plugin lives in plugins/filters/. It declares its parameters
 * (thresholds are parameters, never constants) and processes one outing.
 * Filters never modify their input: a track filter returns which points to
 * keep, and the application records and re-applies it over the original
 * data (non-destructive).
 */
#ifndef OPENRIDER_FILTER_H
#define OPENRIDER_FILTER_H

#include <openrider/input.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* @impl 0f87-6acd-33a9-8156 */
enum or_filter_kind {
    OR_FILTER_TRACK = 1  /* changes the outing's record points */
};

struct or_filter_param {
    const char *id;           /* e.g. "ref_distance_m" */
    const char *label;        /* e.g. "Reference distance (m)" */
    double default_value;
    double min_value;
    double max_value;
};

/* Functions return 0 on success; on failure, non-zero with a message
 * written to err (errlen bytes, always NUL-terminated). */
struct or_filter_vtable {
    uint32_t kind;            /* enum or_filter_kind */
    const char *id;           /* e.g. "jitter" */
    const char *label;        /* e.g. "Jitter while not moving" */
    size_t param_count;
    const struct or_filter_param *params;
    /* Track filter: sets keep[i] to 1 for every point to keep, 0 otherwise.
     * `values` holds one value per declared parameter, in order. */
    int (*apply_track)(const struct or_point *points, size_t count, const double *values,
                       unsigned char *keep, char *err, size_t errlen);
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_FILTER_H */
