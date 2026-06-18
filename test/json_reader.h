// SPDX-License-Identifier: AGPL-3.0-only
// Copyright (c) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// https://github.com/dstroy0/XPoint

/*
 * Minimal JSON reader for XPoint test data.
 *
 * Handles a strict subset of JSON sufficient for the files in test/data/:
 *   - Top-level array of objects: [{...}, {...}, ...]
 *   - Object fields: string keys, integer values, string values,
 *     and flat integer arrays
 *
 * ***NOT*** a general-purpose parser. Strings must not contain escaped quotes.
 * All values are returned as their C++ types; missing keys return defaults.
 */

#ifndef JSON_READER_H
#define JSON_READER_H

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace json
{

/* Read an entire file into a string. Returns empty string on failure. */
inline std::string read_file(const std::string &path)
{
    std::ifstream f(path.c_str());
    if (!f.is_open())
        return "";
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

/*
 * Split the top-level JSON array into individual raw object strings.
 * Input: "[{...}, {...}]"
 * Output: ["{...}", "{...}"]
 *
 * Depth tracking handles nested {} and [] within each object.
 */
inline std::vector<std::string> split_objects(const std::string &src)
{
    std::vector<std::string> result;
    int depth = 0;
    std::size_t start = std::string::npos;

    for (std::size_t i = 0; i < src.size(); ++i)
    {
        char ch = src[i];
        if (ch == '{')
        {
            if (depth == 0)
                start = i;
            ++depth;
        }
        else if (ch == '}')
        {
            if (depth > 0)
            {
                --depth;
                if (depth == 0 && start != std::string::npos)
                {
                    result.push_back(src.substr(start, i - start + 1));
                    start = std::string::npos;
                }
            }
        }
    }
    return result;
}

/* Skip whitespace characters starting at pos. */
inline std::size_t skip_ws(const std::string &s, std::size_t pos)
{
    while (pos < s.size() && (s[pos] == ' ' || s[pos] == '\t' || s[pos] == '\n' || s[pos] == '\r'))
        ++pos;
    return pos;
}

/*
 * Find the value position for "key" inside an object string.
 * Searches for the exact quoted key followed by optional whitespace and ':'.
 * Returns std::string::npos if not found.
 */
inline std::size_t find_key(const std::string &obj, const std::string &key)
{
    std::string needle = "\"" + key + "\"";
    std::size_t pos = 0;
    while (pos < obj.size())
    {
        std::size_t kp = obj.find(needle, pos);
        if (kp == std::string::npos)
            return std::string::npos;

        /* Advance past the key and colon to the value start. */
        std::size_t vp = kp + needle.size();
        vp = skip_ws(obj, vp);
        if (vp < obj.size() && obj[vp] == ':')
        {
            vp = skip_ws(obj, vp + 1);
            return vp;
        }
        pos = kp + 1; // key found but no colon — keep searching
    }
    return std::string::npos;
}

/* Read an integer (optionally negative) at position pos. */
inline int read_int(const std::string &s, std::size_t pos, int def = 0, bool *ok = nullptr)
{
    if (ok)
        *ok = false;
    if (pos >= s.size())
        return def;
    bool neg = (s[pos] == '-');
    if (neg)
        ++pos;
    if (pos >= s.size() || s[pos] < '0' || s[pos] > '9')
        return def;
    int val = 0;
    while (pos < s.size() && s[pos] >= '0' && s[pos] <= '9')
        val = val * 10 + (s[pos++] - '0');
    if (ok)
        *ok = true;
    return neg ? -val : val;
}

/* Get an integer field from an object string. Returns def if not found. */
inline int get_int(const std::string &obj, const std::string &key, int def = 0)
{
    std::size_t vp = find_key(obj, key);
    if (vp == std::string::npos)
        return def;
    return read_int(obj, vp, def);
}

/* Get a string field from an object string. Returns def if not found. */
inline std::string get_str(const std::string &obj, const std::string &key, const std::string &def = "")
{
    std::size_t vp = find_key(obj, key);
    if (vp == std::string::npos || obj[vp] != '"')
        return def;
    ++vp; // skip opening "
    std::size_t end = obj.find('"', vp);
    if (end == std::string::npos)
        return def;
    return obj.substr(vp, end - vp);
}

/* Get a flat integer array field from an object string: "key": [1, 2, 3]. */
inline std::vector<int> get_int_array(const std::string &obj, const std::string &key)
{
    std::vector<int> result;
    std::size_t vp = find_key(obj, key);
    if (vp == std::string::npos || obj[vp] != '[')
        return result;
    ++vp; // skip '['
    while (vp < obj.size() && obj[vp] != ']')
    {
        vp = skip_ws(obj, vp);
        if (vp >= obj.size() || obj[vp] == ']')
            break;
        if (obj[vp] == ',')
        {
            ++vp;
            continue;
        }
        bool ok = false;
        int val = read_int(obj, vp, 0, &ok);
        if (ok)
        {
            result.push_back(val);
            /* Advance past the number. */
            if (obj[vp] == '-')
                ++vp;
            while (vp < obj.size() && obj[vp] >= '0' && obj[vp] <= '9')
                ++vp;
        }
        else
        {
            ++vp; // skip unrecognised character
        }
    }
    return result;
}

/*
 * Load all objects from a JSON array file.
 * Returns an empty vector on read failure.
 * Prints an error to stderr if the file cannot be opened.
 */
inline std::vector<std::string> load(const std::string &path)
{
    std::string src = read_file(path);
    if (src.empty())
    {
        std::cerr << "json::load: cannot read " << path << "\n";
        return {};
    }
    return split_objects(src);
}

} // namespace json

#endif // JSON_READER_H
