#include <atf-c++.hpp>

#include "core/types.h"

namespace {

core::TypeCatalog catalog()
{
    core::TypeCatalog c;
    c.load_source("garmin_connect",
                  "{\"backcountry_skiing\":{\"sport\":\"alpine_skiing\",\"sub_sport\":\"backcountry\"},"
                  " \"roller_ski\":{\"sport\":\"cross_country_skiing\",\"tag\":\"roller-ski\"}}");
    c.load_labels("{\"alpine_skiing/backcountry\":{\"en\":\"Ski touring\",\"fr\":\"Ski de randonnée\"},"
                  " \"running\":{\"en\":\"Running\",\"fr\":\"Course à pied\"}}");
    return c;
}

} // namespace

ATF_TEST_CASE_WITHOUT_HEAD(source_mapping_wins_over_device_type);
ATF_TEST_CASE_BODY(source_mapping_wins_over_device_type)
{
    const core::TypeCatalog c = catalog();
    // A ski tour the watch recorded as a trail run, corrected in Garmin Connect.
    const core::ActivityType t = c.resolve("garmin_connect", "backcountry_skiing", "running", "trail");
    ATF_REQUIRE_EQ(t.key(), "alpine_skiing/backcountry");
}

ATF_TEST_CASE_WITHOUT_HEAD(unknown_source_type_falls_back_to_fit);
ATF_TEST_CASE_BODY(unknown_source_type_falls_back_to_fit)
{
    const core::TypeCatalog c = catalog();
    ATF_REQUIRE_EQ(c.resolve("garmin_connect", "unheard_of", "running", "trail").key(), "running/trail");
    ATF_REQUIRE_EQ(c.resolve("garmin_connect", "", "hiking", "").key(), "hiking/generic");
}

ATF_TEST_CASE_WITHOUT_HEAD(type_tag_for_what_fit_lacks);
ATF_TEST_CASE_BODY(type_tag_for_what_fit_lacks)
{
    const core::TypeCatalog c = catalog();
    const core::ActivityType t = c.resolve("garmin_connect", "roller_ski", "", "");
    ATF_REQUIRE_EQ(t.sport, "cross_country_skiing");
    ATF_REQUIRE_EQ(t.tag, "roller-ski");
    ATF_REQUIRE_EQ(t.key(), "cross_country_skiing/generic+roller-ski");
}

ATF_TEST_CASE_WITHOUT_HEAD(labels_by_language_with_sport_fallback);
ATF_TEST_CASE_BODY(labels_by_language_with_sport_fallback)
{
    const core::TypeCatalog c = catalog();
    const core::ActivityType tour{"alpine_skiing", "backcountry", ""};
    ATF_REQUIRE_EQ(c.label(tour, "fr"), "Ski de randonnée");
    ATF_REQUIRE_EQ(c.label(tour, "en"), "Ski touring");
    ATF_REQUIRE_EQ(c.label(core::ActivityType{"running", "street", ""}, "fr"), "Course à pied");
    ATF_REQUIRE_EQ(c.label(core::ActivityType{"golf", "generic", ""}, "en"), "golf/generic");
}

ATF_TEST_CASE_WITHOUT_HEAD(icons_with_sport_and_generic_fallback);
ATF_TEST_CASE_BODY(icons_with_sport_and_generic_fallback)
{
    core::TypeCatalog c;
    c.load_labels("{\"cross_country_skiing\":{\"en\":\"Nordic\",\"icon\":\"e71c\"},"
                  " \"cycling/mountain\":{\"en\":\"MTB\",\"icon\":\"e734\"}}");
    // Skating has no icon of its own: the nordic (sport) icon is used.
    ATF_REQUIRE_EQ(c.icon(core::ActivityType{"cross_country_skiing", "skate_skiing", ""}), "\xee\x9c\x9c");
    ATF_REQUIRE_EQ(c.icon(core::ActivityType{"cycling", "mountain", ""}), "\xee\x9c\xb4");
    ATF_REQUIRE_EQ(c.icon(core::ActivityType{"golf", "generic", ""}), "\xee\x9c\xae");  // generic person
}

ATF_TEST_CASE_WITHOUT_HEAD(colours_with_sport_and_grey_fallback);
ATF_TEST_CASE_BODY(colours_with_sport_and_grey_fallback)
{
    core::TypeCatalog c;
    c.load_labels("{\"running\":{\"en\":\"Running\",\"colour\":\"#d62728\"},"
                  " \"cycling\":{\"en\":\"Cycling\",\"colour\":\"#1f3fbf\"}}");
    ATF_REQUIRE_EQ(c.colour(core::ActivityType{"running", "trail", ""}), 0xd62728u);
    ATF_REQUIRE_EQ(c.colour(core::ActivityType{"cycling", "mountain", ""}), 0x1f3fbfu);
    ATF_REQUIRE_EQ(c.colour(core::ActivityType{"golf", "generic", ""}), 0x8c8c8cu);
}

ATF_TEST_CASE_WITHOUT_HEAD(invalid_json_is_an_error);
ATF_TEST_CASE_BODY(invalid_json_is_an_error)
{
    core::TypeCatalog c;
    ATF_REQUIRE(!c.load_source("x", "{not json").ok());
    ATF_REQUIRE(!c.load_labels("[]").ok());
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, source_mapping_wins_over_device_type);
    ATF_ADD_TEST_CASE(tcs, unknown_source_type_falls_back_to_fit);
    ATF_ADD_TEST_CASE(tcs, type_tag_for_what_fit_lacks);
    ATF_ADD_TEST_CASE(tcs, labels_by_language_with_sport_fallback);
    ATF_ADD_TEST_CASE(tcs, icons_with_sport_and_generic_fallback);
    ATF_ADD_TEST_CASE(tcs, colours_with_sport_and_grey_fallback);
    ATF_ADD_TEST_CASE(tcs, invalid_json_is_an_error);
}
