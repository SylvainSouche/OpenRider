// The XML reader.

#include <atf-c++.hpp>

#include "../src/xml_node.h"

ATF_TEST_CASE_WITHOUT_HEAD(elements_attributes_text);
ATF_TEST_CASE_BODY(elements_attributes_text)
{
    std::string error;
    const auto root = ParseXml("<?xml version='1.0'?><!-- c --><a:root x:id=\"1\" b='two &amp; three'>"
                               "<child>  &lt;text&gt; &#233;&#x20AC; </child><child/><![CDATA[<raw>]]></a:root>",
                               error);
    ATF_REQUIRE(root != nullptr);
    ATF_REQUIRE_EQ(root->name, "root");
    ATF_REQUIRE_EQ(root->Attribute("id"), "1");
    ATF_REQUIRE_EQ(root->Attribute("b"), "two & three");
    ATF_REQUIRE_EQ(root->Children("child").size(), 2u);
    ATF_REQUIRE_EQ(root->ChildText("child"), "<text> \xC3\xA9\xE2\x82\xAC");
    ATF_REQUIRE_EQ(root->text, "<raw>");
}

ATF_TEST_CASE_WITHOUT_HEAD(malformed_documents);
ATF_TEST_CASE_BODY(malformed_documents)
{
    std::string error;
    ATF_REQUIRE(ParseXml("<a><b></a>", error) == nullptr);
    ATF_REQUIRE(!error.empty());
    ATF_REQUIRE(ParseXml("<a>", error) == nullptr);
    ATF_REQUIRE(ParseXml("<a x=1/>", error) == nullptr);
    ATF_REQUIRE(ParseXml("", error) == nullptr);
    ATF_REQUIRE(ParseXml("<a/><b/>", error) == nullptr);
}

ATF_INIT_TEST_CASES(tcs)
{
    ATF_ADD_TEST_CASE(tcs, elements_attributes_text);
    ATF_ADD_TEST_CASE(tcs, malformed_documents);
}
