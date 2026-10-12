/* OpenRider input (data backend) abstraction.
 *
 * An input plugin provides outings and their recorded points. It is the
 * only way the application reads activity data; GarminDB is the first
 * implementation. Plugins only read: they never modify their source.
 *
 * Units are SI throughout: metres, seconds, metres per second. A value the
 * source does not have is NaN (floating point) or -1 (integers).
 */
#ifndef OPENRIDER_INPUT_H
#define OPENRIDER_INPUT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Summary of one outing. Strings are NUL-terminated and truncated to fit. */
struct or_outing {
    char id[32];               /* stable source identifier (Garmin activity_id) */
    char name[128];
    char start_local[24];      /* "YYYY-MM-DD HH:MM:SS", local time */
    char fit_sport[32];        /* FIT sport recorded by the device */
    char fit_sub_sport[32];    /* FIT sub_sport recorded by the device */
    char source_type[48];      /* source's own type key (Garmin Connect typeKey), "" if none */
    double duration_s;
    double moving_s;
    double distance_m;
    double ascent_m;
    double descent_m;
    double avg_speed_ms;
    double max_speed_ms;
    int32_t avg_hr;
    int32_t max_hr;
    double start_lat;
    double start_lon;
    double min_alt_m;
    double max_alt_m;
    double avg_temp_c;
    double min_temp_c;
    double max_temp_c;
    double calories;
    double aerobic_te;         /* training effect, 0-5 */
    double anaerobic_te;
};

/* One recorded sample. */
struct or_point {
    double t_s;                /* seconds since the outing's start */
    double lat;                /* degrees */
    double lon;                /* degrees */
    double alt_m;
    double dist_m;             /* cumulative distance */
    double speed_ms;
    double hr;
    double cadence;
    double temp_c;
};

/* Callbacks return 0 to continue, non-zero to stop early. */
typedef int (*or_outing_cb)(void *user, const struct or_outing *outing,
                            size_t index, size_t total);
typedef int (*or_point_cb)(void *user, const struct or_point *point);

/* Functions return 0 on success; on failure, non-zero with a message
 * written to err (errlen bytes, always NUL-terminated). */
struct or_input_vtable {
    /* config: plugin-specific JSON text (may be NULL for defaults). */
    void *(*open)(const char *config, char *err, size_t errlen);
    void (*close)(void *handle);
    int (*list_outings)(void *handle, or_outing_cb cb, void *user,
                        char *err, size_t errlen);
    int (*load_points)(void *handle, const char *outing_id, or_point_cb cb,
                       void *user, char *err, size_t errlen);
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_INPUT_H */
