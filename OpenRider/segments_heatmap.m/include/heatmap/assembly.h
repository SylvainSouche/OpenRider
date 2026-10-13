/// @file
/// The network assembly of the heat-map method: the state its stages share
/// while turning the analysis into a segment network.
#pragma once

#include "heatmap/analysis.h"
#include "heatmap/geometry.h"
#include "heatmap/heatmap.h"

#include <map>
#include <set>
#include <utility>
#include <vector>

namespace heatmap {

/// A traversal: a track's run along one skeleton line in one direction, or
/// one of its own parts.
struct Trav {
    int line = -1;      ///< skeleton line; -1: the route's own part
    int from_v = 0;     ///< entry vertex on the line
    int to_v = 0;       ///< exit vertex on the line
    size_t first = 0;   ///< first track point
    size_t last = 0;    ///< last track point
};

/// A run of a track along a proto: its point indices, and whether it goes
/// the proto's way (from -> to).
struct Run {
    size_t track;   ///< track index
    size_t first;   ///< first point
    size_t last;    ///< last point
    bool forward;   ///< travelled from `from` to `to`
};

/// A segment in the making, between two provisional nodes.
struct Proto {
    int from;                  ///< start node (provisional id)
    int to;                    ///< end node (provisional id)
    std::vector<XY> pts;       ///< its line, metres
    std::set<size_t> routes;   ///< tracks following it
    std::vector<Run> runs;     ///< the tracks' runs along it
    bool own = false;          ///< made from one route's own part
};

/// One step of a track's chain, before the output.
struct Item {
    int proto = -1;              ///< proto travelled
    bool forward = true;         ///< along it from `from` to `to`
    int from = -1;               ///< node entered (travel order)
    int to = -1;                 ///< node left
    size_t first = 0;            ///< first track point along it
    size_t last = 0;             ///< last track point along it
};

/// The state shared by the stages of the network assembly (stages.h).
/// Nodes are provisional ids merged by union-find (`parent`, Find()).
struct Assembly {
    /// The assembly of `tracks`, from their `analysis`.
    Assembly(const std::vector<OverviewTrack> &tracks, const SegmentParams &params, Analysis analysis)
        : tracks(tracks), params(params), analysis(std::move(analysis))
    {
    }

    const std::vector<OverviewTrack> &tracks;  ///< input tracks
    const SegmentParams &params;               ///< parameters
    Analysis analysis;                         ///< heat map, lines, matches, pieces
    double cut_resolution_m = 0.0;             ///< entry points closer than this are one cut
    int margin = 0;                            ///< the same, in skeleton vertices
    double end_radius = 0.0;                   ///< a route end joins a node this close
    std::vector<std::vector<Trav>> travs;      ///< per track: its traversals
    std::vector<std::vector<int>> cut_candidates;  ///< per line: entry/exit vertices
    std::vector<std::vector<int>> cuts;        ///< per line: cut vertices, ends included
    std::vector<int> parent;                   ///< union-find over provisional nodes
    std::vector<std::vector<int>> node_at;     ///< per line, per cut: provisional node
    std::vector<XY> node_pos;                  ///< per provisional node: position
    std::vector<Proto> protos;                 ///< segments in the making
    std::vector<std::vector<int>> proto_of;    ///< per line, per range: proto
    std::vector<std::vector<Item>> items;      ///< per track: its chain
    std::map<std::pair<int, int>, int> connectors;  ///< node pair -> connecting proto
    SegmentNetwork net;                        ///< the output
};

/// Representative of provisional node `x` (union-find, path halving).
int Find(Assembly &assembly, int x);

/// A new provisional node at `q`.
int NewNode(Assembly &assembly, const XY &q);

/// A route's start or end joins the node already there, if any within
/// `end_radius` (outings starting from home share one node instead of one
/// each); otherwise it is a new node.
int EndNode(Assembly &assembly, const XY &q);

} // namespace heatmap
