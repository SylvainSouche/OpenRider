// Help > About and CREDITS.md credit the same components and sources.

#include <atf-c++.hpp>

#include "app/credits.h"

#include <fstream>
#include <set>
#include <string>

namespace {

// The names CREDITS.md's tables link: "| [name](url) ...".
std::set<std::string> CreditedInFile()
{
    std::set<std::string> names;
    std::ifstream file(OPENRIDER_CREDITS_FILE);
    std::string line;
    while (std::getline(file, line))
        if (line.compare(0, 3, "| [") == 0) {
            const size_t end = line.find(']');
            if (end != std::string::npos)
                names.insert(line.substr(3, end - 3));
        }
    return names;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(about_matches_credits_file);
ATF_TEST_CASE_BODY(about_matches_credits_file)
{
    const std::set<std::string> in_file = CreditedInFile();
    ATF_REQUIRE(in_file.size() > 10);
    std::set<std::string> in_about;
    for (const app::Credit &credit : app::kLibraryCredits)
        in_about.insert(credit.name);
    for (const app::Credit &credit : app::kDataCredits)
        in_about.insert(credit.name);
    for (const std::string &name : in_file)
        if (!in_about.count(name))
            ATF_FAIL("credited in CREDITS.md, missing from About: " + name);
    for (const std::string &name : in_about)
        if (!in_file.count(name))
            ATF_FAIL("in About, missing from CREDITS.md: " + name);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, about_matches_credits_file);
}
