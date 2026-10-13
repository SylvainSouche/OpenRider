/// @file
/// Session log (module-level utilities): one file per application run,
/// named with its creation date and time, in the user's log directory.
/// Files older than the retention period are deleted when a session starts.
#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace core {

/// Days a session log is kept.
constexpr int kLogRetentionDays = 10;

/// "OpenRider-YYYY-MM-DD_HH-MM-SS.log" for the given local time.
std::string LogFileName(std::time_t when);

/// Creation time of a log named by LogFileName(); -1 if `name` is not one.
std::time_t LogFileTime(const std::string &name);

/// Names among `names` that are log files older than `days` relative to `now`.
std::vector<std::string> ExpiredLogs(const std::vector<std::string> &names, std::time_t now, int days);

/// Opens this session's log file in `dir`, pruning expired ones first.
/// Safe to call once; LogLine() is a no-op until then. Thread-safe.
/// @return the log file is open
bool LogOpenSession(const std::string &dir);

/// Appends a timestamped printf-formatted line to the session log.
void LogLine(const char *format, ...) __attribute__((format(printf, 1, 2)));

/// Path of the session log file, "" before LogOpenSession().
std::string LogSessionPath();

} // namespace core
