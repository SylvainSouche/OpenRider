#include "xml_node.h"

#include <cctype>
#include <cstdlib>

namespace {

// `name` without its namespace prefix.
std::string LocalName(const std::string &name)
{
    const size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
}

// `text` with the predefined entities and character references decoded.
std::string Decoded(const std::string &text)
{
    std::string out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '&') {
            out += text[i];
            continue;
        }
        const size_t end = text.find(';', i);
        if (end == std::string::npos) {
            out += text[i];
            continue;
        }
        const std::string entity = text.substr(i + 1, end - i - 1);
        if (entity == "amp")
            out += '&';
        else if (entity == "lt")
            out += '<';
        else if (entity == "gt")
            out += '>';
        else if (entity == "quot")
            out += '"';
        else if (entity == "apos")
            out += '\'';
        else if (!entity.empty() && entity[0] == '#') {
            const bool hexadecimal = entity.size() > 1 && (entity[1] == 'x' || entity[1] == 'X');
            const unsigned long code = std::strtoul(entity.c_str() + (hexadecimal ? 2 : 1), nullptr, hexadecimal ? 16 : 10);
            // UTF-8 encoding of the code point.
            if (code < 0x80) {
                out += static_cast<char>(code);
            } else if (code < 0x800) {
                out += static_cast<char>(0xC0 | (code >> 6));
                out += static_cast<char>(0x80 | (code & 0x3F));
            } else if (code < 0x10000) {
                out += static_cast<char>(0xE0 | (code >> 12));
                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            } else {
                out += static_cast<char>(0xF0 | (code >> 18));
                out += static_cast<char>(0x80 | ((code >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (code & 0x3F));
            }
        } else {
            out += text.substr(i, end - i + 1);  // unknown: kept as is
        }
        i = end;
    }
    return out;
}

// `text` without leading and trailing white space.
std::string Trimmed(const std::string &text)
{
    size_t first = 0, last = text.size();
    while (first < last && std::isspace(static_cast<unsigned char>(text[first])))
        ++first;
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1])))
        --last;
    return text.substr(first, last - first);
}

} // namespace

const XmlNode *XmlNode::Child(const std::string &child) const
{
    for (const std::unique_ptr<XmlNode> &node : children)
        if (node->name == child)
            return node.get();
    return nullptr;
}

std::vector<const XmlNode *> XmlNode::Children(const std::string &child) const
{
    std::vector<const XmlNode *> found;
    for (const std::unique_ptr<XmlNode> &node : children)
        if (node->name == child)
            found.push_back(node.get());
    return found;
}

std::string XmlNode::ChildText(const std::string &child) const
{
    const XmlNode *node = Child(child);
    return node ? node->text : "";
}

std::string XmlNode::Attribute(const std::string &attribute) const
{
    auto found = attributes.find(attribute);
    return found == attributes.end() ? "" : found->second;
}

std::unique_ptr<XmlNode> ParseXml(const std::string &document, std::string &error)
{
    std::unique_ptr<XmlNode> root;
    std::vector<XmlNode *> open;  // elements not yet closed, innermost last
    std::vector<std::string> texts;
    size_t i = 0;
    const size_t n = document.size();
    while (i < n) {
        if (document[i] != '<') {
            const size_t next = document.find('<', i);
            const size_t end = next == std::string::npos ? n : next;
            if (!open.empty())
                texts.back() += Decoded(document.substr(i, end - i));
            i = end;
            continue;
        }
        if (document.compare(i, 4, "<!--") == 0) {
            const size_t end = document.find("-->", i + 4);
            i = end == std::string::npos ? n : end + 3;
            continue;
        }
        if (document.compare(i, 9, "<![CDATA[") == 0) {
            const size_t end = document.find("]]>", i + 9);
            if (end == std::string::npos) {
                error = "unterminated CDATA";
                return nullptr;
            }
            if (!open.empty())
                texts.back() += document.substr(i + 9, end - i - 9);
            i = end + 3;
            continue;
        }
        if (document.compare(i, 2, "<?") == 0 || document.compare(i, 2, "<!") == 0) {
            const size_t end = document.find('>', i);
            i = end == std::string::npos ? n : end + 1;
            continue;
        }
        if (document.compare(i, 2, "</") == 0) {
            const size_t end = document.find('>', i);
            if (end == std::string::npos || open.empty()) {
                error = "unexpected closing tag";
                return nullptr;
            }
            const std::string name = LocalName(Trimmed(document.substr(i + 2, end - i - 2)));
            if (name != open.back()->name) {
                error = "closing tag " + name + " does not match " + open.back()->name;
                return nullptr;
            }
            open.back()->text = Trimmed(texts.back());
            open.pop_back();
            texts.pop_back();
            i = end + 1;
            continue;
        }
        // An opening tag: name, attributes, then '>' or '/>'.
        size_t j = i + 1;
        while (j < n && !std::isspace(static_cast<unsigned char>(document[j])) && document[j] != '>' && document[j] != '/')
            ++j;
        auto node = std::make_unique<XmlNode>();
        node->name = LocalName(document.substr(i + 1, j - i - 1));
        bool self_closing = false;
        for (;;) {
            while (j < n && std::isspace(static_cast<unsigned char>(document[j])))
                ++j;
            if (j >= n) {
                error = "unterminated tag " + node->name;
                return nullptr;
            }
            if (document[j] == '>') {
                ++j;
                break;
            }
            if (document[j] == '/' && j + 1 < n && document[j + 1] == '>') {
                self_closing = true;
                j += 2;
                break;
            }
            const size_t equals = document.find('=', j);
            if (equals == std::string::npos) {
                error = "malformed attribute in " + node->name;
                return nullptr;
            }
            const std::string attribute = LocalName(Trimmed(document.substr(j, equals - j)));
            size_t quote = equals + 1;
            while (quote < n && std::isspace(static_cast<unsigned char>(document[quote])))
                ++quote;
            if (quote >= n || (document[quote] != '"' && document[quote] != '\'')) {
                error = "unquoted attribute in " + node->name;
                return nullptr;
            }
            const size_t close = document.find(document[quote], quote + 1);
            if (close == std::string::npos) {
                error = "unterminated attribute in " + node->name;
                return nullptr;
            }
            node->attributes[attribute] = Decoded(document.substr(quote + 1, close - quote - 1));
            j = close + 1;
        }
        XmlNode *raw = node.get();
        if (open.empty()) {
            if (root) {
                error = "more than one root element";
                return nullptr;
            }
            root = std::move(node);
        } else {
            open.back()->children.push_back(std::move(node));
        }
        if (!self_closing) {
            open.push_back(raw);
            texts.emplace_back();
        }
        i = j;
    }
    if (!root || !open.empty()) {
        error = root ? "unclosed element " + open.back()->name : "no element";
        return nullptr;
    }
    return root;
}
