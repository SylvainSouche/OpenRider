/** @file
 * OpenRider input (data backend) abstraction.
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

/** Summary of one outing. Strings are NUL-terminated and truncated to fit. */
struct or_outing {
    char id[32];               /**< stable source identifier (Garmin activity_id) */
    char name[128];            /**< the outing's name */
    char start_local[24];      /**< "YYYY-MM-DD HH:MM:SS", local time */
    char fit_sport[32];        /**< FIT sport recorded by the device */
    char fit_sub_sport[32];    /**< FIT sub_sport recorded by the device */
    char source_type[48];      /**< source's own type key (Garmin Connect typeKey), "" if none */
    double duration_s;         /**< elapsed time */
    double moving_s;           /**< moving time */
    double distance_m;         /**< distance */
    double ascent_m;           /**< total ascent (D+) */
    double descent_m;          /**< total descent (D-) */
    double avg_speed_ms;       /**< average speed */
    double max_speed_ms;       /**< maximum speed */
    int32_t avg_hr;            /**< average heart rate, bpm */
    int32_t max_hr;            /**< maximum heart rate, bpm */
    double start_lat;          /**< start position, degrees */
    double start_lon;          /**< start position, degrees */
    double min_alt_m;          /**< lowest altitude */
    double max_alt_m;          /**< highest altitude */
    double avg_temp_c;         /**< average temperature, degrees C */
    double min_temp_c;         /**< lowest temperature, degrees C */
    double max_temp_c;         /**< highest temperature, degrees C */
    double calories;           /**< energy, kcal */
    double aerobic_te;         /**< aerobic training effect, 0-5 */
    double anaerobic_te;       /**< anaerobic training effect, 0-5 */
};

/** One recorded sample. */
struct or_point {
    double t_s;                /**< seconds since the outing's start */
    double lat;                /**< latitude, degrees */
    double lon;                /**< longitude, degrees */
    double alt_m;              /**< altitude */
    double dist_m;             /**< cumulative distance */
    double speed_ms;           /**< recorded speed */
    double hr;                 /**< heart rate, bpm */
    double cadence;            /**< rpm or steps per minute */
    double temp_c;             /**< temperature, degrees C */
};

/** Receives one outing of list_outings(); returns 0 to continue, non-zero
 * to stop early. `index` counts from 0; `total` is the number announced. */
typedef int (*or_outing_cb)(void *user, const struct or_outing *outing,
                            size_t index, size_t total);
/** Receives one point of load_points(); returns 0 to continue, non-zero to
 * stop early. */
typedef int (*or_point_cb)(void *user, const struct or_point *point);

/** Functions of an input plugin. They return 0 on success; on failure,
 * non-zero with a message written to err (errlen bytes, always
 * NUL-terminated). */
struct or_input_vtable {
    /** Opens the source. `config`: plugin-specific JSON text (may be NULL
     * for defaults). Returns a handle, or NULL with a message in err. */
    void *(*open)(const char *config, char *err, size_t errlen);
    /** Closes a handle returned by open(). */
    void (*close)(void *handle);
    /** Calls `cb` for every outing, passing `user` along. */
    int (*list_outings)(void *handle, or_outing_cb cb, void *user,
                        char *err, size_t errlen);
    /** Calls `cb` for every recorded point of an outing, in time order. */
    int (*load_points)(void *handle, const char *outing_id, or_point_cb cb,
                       void *user, char *err, size_t errlen);
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_INPUT_H */
