// SPDX-License-Identifier: MIT
// Tiny JSON reader/writer: objects, arrays, strings, numbers, booleans, null.
// Enough for preset and config files; no dependencies.  Parsing is bounded
// (depth and size limits) because preset files may come from strangers.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace lstrip {

struct Json {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Json> a;
    std::vector<std::pair<std::string, Json>> o;

    static Json number(double v) { Json j; j.type = Num; j.n = v; return j; }
    static Json boolean(bool v) { Json j; j.type = Bool; j.b = v; return j; }
    static Json string(std::string v) { Json j; j.type = Str; j.s = std::move(v); return j; }
    static Json array() { Json j; j.type = Arr; return j; }
    static Json object() { Json j; j.type = Obj; return j; }

    Json& set(const std::string& k, Json v) { o.emplace_back(k, std::move(v)); return *this; }
    Json& push(Json v) { a.push_back(std::move(v)); return *this; }
    const Json* get(const std::string& k) const {
        for (const auto& kv : o) if (kv.first == k) return &kv.second;
        return nullptr;
    }

    // ------------------------------------------------------------------ parse
    static bool parse(const std::string& text, Json& out) {
        if (text.size() > (4u << 20)) return false;
        Parser p{text.c_str(), text.c_str() + text.size()};
        p.ws();
        if (!p.value(out, 0)) return false;
        p.ws();
        return p.cur == p.end;
    }

    // ------------------------------------------------------------------- dump
    std::string dump(int indent = 0) const { std::string r; write(r, indent, 0); r += '\n'; return r; }

private:
    struct Parser {
        const char* cur; const char* end;
        void ws() { while (cur < end && (*cur == ' ' || *cur == '\n' || *cur == '\r' || *cur == '\t')) ++cur; }
        bool lit(const char* w) {
            const size_t n = std::char_traits<char>::length(w);
            if (static_cast<size_t>(end - cur) < n || std::string(cur, n) != w) return false;
            cur += n; return true;
        }
        bool value(Json& j, int depth) {
            if (depth > 32 || cur >= end) return false;
            switch (*cur) {
            case '{': return object(j, depth);
            case '[': return array(j, depth);
            case '"': j.type = Str; return str(j.s);
            case 't': j.type = Bool; j.b = true; return lit("true");
            case 'f': j.type = Bool; j.b = false; return lit("false");
            case 'n': j.type = Null; return lit("null");
            default: return number(j);
            }
        }
        bool number(Json& j) {
            char* e = nullptr;
            const double v = std::strtod(cur, &e);
            if (e == cur || e > end || !std::isfinite(v)) return false;
            j.type = Num; j.n = v; cur = e; return true;
        }
        bool str(std::string& out) {
            ++cur; out.clear();
            while (cur < end && *cur != '"') {
                char c = *cur++;
                if (c == '\\') {
                    if (cur >= end) return false;
                    c = *cur++;
                    switch (c) {
                    case 'n': out += '\n'; break; case 't': out += '\t'; break; case 'r': out += '\r'; break;
                    case 'b': out += '\b'; break; case 'f': out += '\f'; break;
                    case 'u': { // BMP code point -> UTF-8
                        if (end - cur < 4) return false;
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            const char h = *cur++; cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= h - '0'; else if (h >= 'a' && h <= 'f') cp |= h - 'a' + 10;
                            else if (h >= 'A' && h <= 'F') cp |= h - 'A' + 10; else return false;
                        }
                        if (cp < 0x80) out += static_cast<char>(cp);
                        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                        else { out += static_cast<char>(0xE0 | (cp >> 12)); out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
                        break; }
                    default: out += c; break;
                    }
                } else out += c;
                if (out.size() > 65536) return false;
            }
            if (cur >= end) return false;
            ++cur; return true;
        }
        bool array(Json& j, int depth) {
            j.type = Arr; ++cur; ws();
            if (cur < end && *cur == ']') { ++cur; return true; }
            while (true) {
                Json v; ws();
                if (!value(v, depth + 1)) return false;
                j.a.push_back(std::move(v)); ws();
                if (cur < end && *cur == ',') { ++cur; continue; }
                if (cur < end && *cur == ']') { ++cur; return true; }
                return false;
            }
        }
        bool object(Json& j, int depth) {
            j.type = Obj; ++cur; ws();
            if (cur < end && *cur == '}') { ++cur; return true; }
            while (true) {
                ws(); std::string k;
                if (cur >= end || *cur != '"' || !str(k)) return false;
                ws(); if (cur >= end || *cur != ':') return false; ++cur; ws();
                Json v;
                if (!value(v, depth + 1)) return false;
                j.o.emplace_back(std::move(k), std::move(v)); ws();
                if (cur < end && *cur == ',') { ++cur; continue; }
                if (cur < end && *cur == '}') { ++cur; return true; }
                return false;
            }
        }
    };

    static void esc(std::string& r, const std::string& s) {
        r += '"';
        for (unsigned char c : s) {
            switch (c) {
            case '"': r += "\\\""; break; case '\\': r += "\\\\"; break; case '\n': r += "\\n"; break;
            case '\t': r += "\\t"; break; case '\r': r += "\\r"; break;
            default:
                if (c < 0x20) { char b[8]; std::snprintf(b, sizeof b, "\\u%04x", c); r += b; } else r += static_cast<char>(c);
            }
        }
        r += '"';
    }
    void write(std::string& r, int indent, int level) const {
        const std::string pad(indent > 0 ? (level + 1) * indent : 0, ' '), padEnd(indent > 0 ? level * indent : 0, ' ');
        const char* nl = indent > 0 ? "\n" : "";
        switch (type) {
        case Null: r += "null"; break;
        case Bool: r += b ? "true" : "false"; break;
        case Num: { char buf[40]; std::snprintf(buf, sizeof buf, "%.9g", n); r += buf; break; }
        case Str: esc(r, s); break;
        case Arr:
            if (a.empty()) { r += "[]"; break; }
            r += "["; r += nl;
            for (size_t i = 0; i < a.size(); ++i) { r += pad; a[i].write(r, indent, level + 1); if (i + 1 < a.size()) r += ","; r += nl; }
            r += padEnd; r += "]"; break;
        case Obj:
            if (o.empty()) { r += "{}"; break; }
            r += "{"; r += nl;
            for (size_t i = 0; i < o.size(); ++i) {
                r += pad; esc(r, o[i].first); r += indent > 0 ? ": " : ":"; o[i].second.write(r, indent, level + 1);
                if (i + 1 < o.size()) r += ",";
                r += nl;
            }
            r += padEnd; r += "}"; break;
        }
    }
};

} // namespace lstrip
