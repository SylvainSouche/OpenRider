#include "reproj/crs_registry.h"

#include "reproj/proj_string.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace reproj {

namespace {

// `text` without leading and trailing blanks.
std::string Trimmed(const std::string &text)
{
    const size_t first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos)
        return "";
    return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
}

// The code of "EPSG:<code>", or 0.
int CodeOf(const std::string &id)
{
    if (id.size() < 6)
        return 0;
    std::string prefix = id.substr(0, 5);
    for (char &c : prefix)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (prefix != "EPSG:")
        return 0;
    char *end = nullptr;
    const long code = std::strtol(id.c_str() + 5, &end, 10);
    return *end == '\0' && code > 0 ? static_cast<int>(code) : 0;
}

} // namespace

CrsRegistry::CrsRegistry()
{
    std::string error;
    Add("EPSG:4326 | WGS 84 | -180,-90,180,90 | +proj=longlat +datum=WGS84", error);
    Add("EPSG:3857 | WGS 84 / Pseudo-Mercator | -180,-85.06,180,85.06 | +proj=webmerc +a=6378137 +b=6378137", error);
}

bool CrsRegistry::Add(const std::string &line, std::string &error)
{
    std::vector<std::string> fields;
    std::istringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '|'))
        fields.push_back(Trimmed(field));
    if (fields.size() != 4) {
        error = "expected 4 fields separated by '|'";
        return false;
    }
    Crs crs;
    crs.epsg = CodeOf(fields[0]);
    if (crs.epsg == 0) {
        error = "not an EPSG code: " + fields[0];
        return false;
    }
    crs.name = fields[1];
    double area[4];
    std::istringstream bounds(fields[2]);
    int count = 0;
    while (count < 4 && std::getline(bounds, field, ','))
        area[count++] = std::strtod(field.c_str(), nullptr);
    if (count != 4 || !(area[0] < area[2]) || !(area[1] < area[3])) {
        error = "area of use must be west,south,east,north";
        return false;
    }
    crs.area = {area[0], area[1], area[2], area[3]};
    crs.definition = fields[3];
    crs.projection = ParseProjString(crs.definition, crs.datum, crs.geographic, error);
    if (!crs.projection) {
        error = crs.Id() + ": " + error;
        return false;
    }
    systems_[crs.epsg] = crs;
    return true;
}

std::vector<std::string> CrsRegistry::Parse(const std::string &text)
{
    std::vector<std::string> errors;
    std::istringstream lines(text);
    std::string line;
    int number = 0;
    while (std::getline(lines, line)) {
        ++number;
        const std::string content = Trimmed(line);
        if (content.empty() || content[0] == '#')
            continue;
        std::string error;
        if (!Add(content, error))
            errors.push_back("line " + std::to_string(number) + ": " + error);
    }
    return errors;
}

std::vector<std::string> CrsRegistry::LoadFile(const std::string &path)
{
    std::ifstream file(path);
    if (!file)
        return {"cannot read " + path};
    std::ostringstream text;
    text << file.rdbuf();
    return Parse(text.str());
}

Crs CrsRegistry::Find(int code) const
{
    auto found = systems_.find(code);
    return found == systems_.end() ? Crs{} : found->second;
}

Crs CrsRegistry::Find(const std::string &id) const
{
    return Find(CodeOf(id));
}

std::vector<int> CrsRegistry::Codes() const
{
    std::vector<int> codes;
    for (const auto &[code, crs] : systems_)
        codes.push_back(code);
    return codes;
}

} // namespace reproj
