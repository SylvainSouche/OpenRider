/** @file
 * OpenRider segment identification abstraction.
 *
 * A segments plugin lives in plugins/segments/. Given the tracks of
 * outings of similar activities, it identifies the network they follow:
 * junctions (nodes), segments joining them, and each outing as a chain of
 * segments. Methods differ (a heat map of the tracks, matching tracks to
 * each other, an external map...); the application lets the user choose
 * among the plugins found and set their declared parameters.
 *
 * Memory: the application owns the tracks it passes; the plugin allocates
 * the network it returns and frees it in free_network().
 */
#ifndef OPENRIDER_SEGMENTS_H
#define OPENRIDER_SEGMENTS_H

#include <openrider/filter.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** One outing's track: positions in recording order (WGS84 degrees). */
struct or_segment_track {
    const char *id;         /**< outing id */
    size_t count;           /**< number of points */
    const double *lon;      /**< longitudes */
    const double *lat;      /**< latitudes */
    const float *noise_m;   /**< GPS noise (m, 1 sigma) per point; NULL if unknown */
};

/** A junction of the network. */
struct or_segment_node {
    double lon;  /**< WGS84 degrees */
    double lat;  /**< WGS84 degrees */
};

/** A segment between two nodes. */
struct or_segment {
    uint32_t from;            /**< start node index */
    uint32_t to;              /**< end node index */
    size_t count;             /**< number of points of its line */
    double *lon;              /**< line, from `from` to `to` */
    double *lat;              /**< line */
    double length_m;          /**< length of the line */
    size_t route_count;       /**< number of tracks following it */
    uint32_t *routes;         /**< indices of those tracks in the input */
};

/** One step of a track along the network. */
struct or_route_step {
    uint32_t segment;  /**< segment index */
    uint32_t forward;  /**< 1: travelled from `from` to `to`; 0: backwards */
    uint32_t first;    /**< first point of the track on this step (index in its input points) */
    uint32_t last;     /**< last point of the track on this step, inclusive */
};

/** A track as a chain of segments; consecutive steps share a node, and
 * their point ranges follow each other: a step's first point is the
 * previous step's last (where the track passes the node between them).
 * The first step starts at the track's first point, the last step ends at
 * its last point. */
struct or_segment_route {
    size_t step_count;             /**< number of steps */
    struct or_route_step *steps;   /**< in travel order */
};

/** The network identified from the tracks. */
struct or_segment_network {
    size_t node_count;                  /**< number of nodes */
    struct or_segment_node *nodes;      /**< junctions */
    size_t segment_count;               /**< number of segments */
    struct or_segment *segments;        /**< segments */
    size_t route_count;                 /**< one per input track, in input order */
    struct or_segment_route *routes;    /**< each track's chain */
};

/** Polled during a build: non-zero asks the plugin to stop early (the
 * result is then discarded). */
typedef int (*or_segment_cancel_cb)(void *user);

/* @impl 0f87-6ace-2ba9-bc80 */
/** Description and functions of a segments plugin. Functions return 0 on
 * success; on failure, non-zero with a message written to err (errlen
 * bytes, always NUL-terminated). */
struct or_segments_vtable {
    size_t struct_size;                     /**< sizeof(struct or_segments_vtable) as built */
    const char *id;                         /**< e.g. "heatmap" */
    const char *label;                      /**< e.g. "Heat map of the tracks" */
    size_t param_count;                     /**< number of entries in params */
    const struct or_filter_param *params;   /**< declared parameters */
    /** Identifies the network of `count` tracks. `values` holds one value
     * per declared parameter, in order. On success *out receives a network
     * to release with free_network(). `cancel` (may be NULL) is polled now
     * and then with `user`. */
    int (*build)(const struct or_segment_track *tracks, size_t count, const double *values,
                 or_segment_cancel_cb cancel, void *user, struct or_segment_network **out,
                 char *err, size_t errlen);
    /** Releases a network returned by build(). */
    void (*free_network)(struct or_segment_network *network);
};

#ifdef __cplusplus
}
#endif

#endif /* OPENRIDER_SEGMENTS_H */
