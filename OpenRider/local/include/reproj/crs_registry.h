/// @file
/// The coordinate systems available, declared as text: one per line,
///
///     EPSG:<code> | <name> | <west>,<south>,<east>,<north> | <PROJ string>
///
/// with '#' starting a comment. Adding a system takes a line, no code, as
/// long as its projection method is one ParseProjString knows.
#pragma once

#include "reproj/crs.h"

#include <map>
#include <string>
#include <vector>

namespace reproj {

/// Coordinate systems by EPSG code. Starts with WGS 84 (EPSG:4326) and Web
/// Mercator (EPSG:3857), which map tiles need whatever else is declared.
class CrsRegistry {
public:
    /// The registry holding the two built-in systems.
    CrsRegistry();

    /// Adds (or replaces) the system one definition line declares.
    /// @return false with `error` set when the line is malformed or its
    ///         PROJ string unsupported
    bool Add(const std::string &line, std::string &error);

    /// Adds every system of a definition text; blank and comment lines are
    /// skipped.
    /// @return one message per refused line ("line N: ..."), empty if all
    ///         were added
    std::vector<std::string> Parse(const std::string &text);

    /// Parse() on the contents of the file at `path`.
    /// @return as Parse(), or a single message if the file cannot be read
    std::vector<std::string> LoadFile(const std::string &path);

    /// The system of `code`; an invalid Crs when unknown.
    Crs Find(int code) const;

    /// The system of "EPSG:<code>" (case-insensitive prefix); an invalid Crs
    /// when unknown.
    Crs Find(const std::string &id) const;

    /// Every code held, in increasing order.
    std::vector<int> Codes() const;

private:
    std::map<int, Crs> systems_;  ///< by EPSG code
};

} // namespace reproj
