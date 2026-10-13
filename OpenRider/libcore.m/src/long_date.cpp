#include "core/long_date.h"

#include <cstdio>

namespace core {

std::string LongDate(const char *start_local, const std::string &lang)
{
    static const char *months_en[] = {"January", "February", "March",     "April",   "May",      "June",
                               "July",    "August",   "September", "October", "November", "December"};
    static const char *months_fr[] = {"janvier", "février", "mars",      "avril",   "mai",      "juin",
                               "juillet", "août",    "septembre", "octobre", "novembre", "décembre"};
    int year = 0, month = 0, day = 0;
    if (!start_local || std::sscanf(start_local, "%d-%d-%d", &year, &month, &day) != 3 || month < 1 || month > 12)
        return start_local ? start_local : "";
    char text[48];
    std::snprintf(text, sizeof text, "%d %s %d", day, (lang == "fr" ? months_fr : months_en)[month - 1], year);
    return text;
}

} // namespace core
