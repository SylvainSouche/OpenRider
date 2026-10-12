// Session log: one file per application run, named with its creation date
// and time, in the user's log directory. Files older than the retention
// period are deleted when a session starts.
#pragma once

#include <ctime>
#include <string>
#include <vector>

namespace core {

constexpr int kLogRetentionDays = 10;

// "OpenRider-YYYY-MM-DD_HH-MM-SS.log" for the given local time.
std::string log_file_name(std::time_t when);

// Parses a name produced by log_file_name(); returns -1 if it is not one.
std::time_t log_file_time(const std::string &name);

// Names among `names` that are log files older than `days` relative to `now`.
std::vector<std::string> expired_logs(const std::vector<std::string> &names,
                                      std::time_t now, int days);

// Opens this session's log file (pruning expired ones first). Safe to call
// once; log_line() is a no-op until then. Thread-safe.
bool log_open_session(const std::string &dir);
void log_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
std::string log_session_path();

} // namespace core
