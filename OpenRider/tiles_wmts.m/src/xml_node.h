/// @file
/// A small XML reader, enough for service descriptions: elements,
/// attributes, text and the five predefined entities. Namespace prefixes
/// are dropped ("ows:Identifier" is "Identifier"); comments, processing
/// instructions and DOCTYPE are skipped, CDATA kept as text.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

/// An element: its name, attributes, text and children.
struct XmlNode {
    std::string name;                                ///< local name, prefix dropped
    std::map<std::string, std::string> attributes;   ///< by local name
    std::string text;                                ///< its own text, trimmed
    std::vector<std::unique_ptr<XmlNode>> children;  ///< in document order

    /// The first child named `child`, or null.
    const XmlNode *Child(const std::string &child) const;

    /// Every child named `child`.
    std::vector<const XmlNode *> Children(const std::string &child) const;

    /// The text of the first child named `child`, or "".
    std::string ChildText(const std::string &child) const;

    /// The attribute `attribute`, or "".
    std::string Attribute(const std::string &attribute) const;
};

/// Parses `document`.
/// @return the root element, or null with `error` set
std::unique_ptr<XmlNode> ParseXml(const std::string &document, std::string &error);
