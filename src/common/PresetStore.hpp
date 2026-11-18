// SPDX-License-Identifier: MIT
// User preset + UI config persistence (UI-side only, plain std::filesystem).
//
//   presets : $XDG_DATA_HOME/L-STRIP/presets/<name>.lstrip.json   (default ~/.local/share)
//   config  : $XDG_CONFIG_HOME/L-STRIP/ui.json   {theme, stars[]}   (default ~/.config)
//
// Preset files store values by parameter *symbol*, so adding parameters later
// never invalidates old files; unknown symbols are ignored and missing ones get defaults.
#pragma once
#include "MiniJson.hpp"
#include "Presets.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace lstrip {

struct UserPreset {
    std::string name;
    std::string category = "User";
    float values[kParamCount];   // indexed by id; entries for outputs are unused
};

class PresetStore {
public:
    static constexpr const char* kFormat = "lstrip-preset";
    static constexpr int kVersion = 1;

    // dataDir/configDir may be overridden (tests); empty = XDG defaults.
    explicit PresetStore(std::string dataDir = {}, std::string configDir = {}) {
        namespace fs = std::filesystem;
        auto home = []() -> std::string { const char* h = std::getenv("HOME"); return h && *h ? h : "/tmp"; };
        auto env = [](const char* k) -> std::string { const char* v = std::getenv(k); return v && *v ? v : ""; };
        if (dataDir.empty()) { std::string b = env("XDG_DATA_HOME"); dataDir = (b.empty() ? home() + "/.local/share" : b) + "/L-STRIP/presets"; }
        if (configDir.empty()) { std::string b = env("XDG_CONFIG_HOME"); configDir = (b.empty() ? home() + "/.config" : b) + "/L-STRIP"; }
        dataDir_ = std::move(dataDir); configDir_ = std::move(configDir);
    }

    const std::string& dataDir() const { return dataDir_; }

    // Folder used by Export: <XDG documents dir>/L-STRIP (default ~/Documents/L-STRIP).
    // The save dialog of DPF's X11 file browser is not implemented, so export never asks for a path.
    static std::string exportDir() {
        const char* h = std::getenv("HOME");
        const std::string home = h && *h ? h : "/tmp";
        const char* cfg = std::getenv("XDG_CONFIG_HOME");
        const std::string cfgDir = cfg && *cfg ? cfg : home + "/.config";
        std::string docs = home + "/Documents";
        std::ifstream f(cfgDir + "/user-dirs.dirs");
        std::string line;
        while (std::getline(f, line)) {
            const std::string key = "XDG_DOCUMENTS_DIR=";
            if (line.compare(0, key.size(), key) != 0) continue;
            std::string v = line.substr(key.size());
            if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
            const std::string tok = "$HOME";
            if (v.compare(0, tok.size(), tok) == 0) v = home + v.substr(tok.size());
            if (!v.empty() && v[0] == '/' && v != home + "/") docs = v;
        }
        return docs + "/L-STRIP";
    }
    const std::vector<UserPreset>& user() const { return user_; }

    // ---------------------------------------------------------------- presets
    void reload() {
        namespace fs = std::filesystem;
        user_.clear();
        std::error_code ec;
        if (!fs::is_directory(dataDir_, ec)) return;
        for (const auto& e : fs::directory_iterator(dataDir_, ec)) {
            if (!e.is_regular_file()) continue;
            const std::string fn = e.path().filename().string();
            if (fn.size() < 12 || fn.compare(fn.size() - 12, 12, ".lstrip.json") != 0) continue;
            UserPreset p;
            if (readFile(e.path().string(), p)) user_.push_back(p);
        }
        std::sort(user_.begin(), user_.end(), [](const UserPreset& a, const UserPreset& b) { return a.name < b.name; });
    }

    bool exists(const std::string& name) const { return find(name) != nullptr; }
    const UserPreset* find(const std::string& name) const {
        for (const auto& p : user_) if (p.name == name) return &p;
        return nullptr;
    }

    static std::string cleanName(std::string n) {
        for (char& c : n) if (static_cast<unsigned char>(c) < 0x20) c = ' ';
        const auto a = n.find_first_not_of(' ');
        if (a == std::string::npos) return {};
        n = n.substr(a, n.find_last_not_of(' ') - a + 1);
        if (n.size() > 48) n.resize(48);
        return n;
    }
    static std::string fileNameFor(const std::string& name) {
        std::string f;
        for (unsigned char c : name) f += (std::isalnum(c) || c == '-' || c == '_' || c == ' ' || c >= 0x80) ? static_cast<char>(c) : '_';
        if (f.empty() || f == "." || f == "..") f = "preset";
        return f + ".lstrip.json";
    }

    // Next free "User N" style name.
    std::string nextName(const std::string& base = "User") const {
        for (int i = 1; i < 10000; ++i) {
            const std::string n = base + " " + std::to_string(i);
            if (!find(n)) { bool clash = false; for (uint32_t k = 0; k < kNumFactoryPresets; ++k) clash |= n == kFactoryPresets[k].name; if (!clash) return n; }
        }
        return base;
    }

    bool save(const std::string& rawName, const float* values, std::string* err = nullptr) {
        namespace fs = std::filesystem;
        const std::string name = cleanName(rawName);
        if (name.empty()) { if (err) *err = "Name is empty"; return false; }
        for (uint32_t i = 0; i < kNumFactoryPresets; ++i)
            if (name == kFactoryPresets[i].name) { if (err) *err = "That name belongs to a factory preset"; return false; }
        std::error_code ec;
        fs::create_directories(dataDir_, ec);
        UserPreset p; p.name = name; std::copy(values, values + kParamCount, p.values);
        const std::string path = dataDir_ + "/" + fileNameFor(name);
        if (!writeFile(path, p)) { if (err) *err = "Could not write " + path; return false; }
        reload();
        return true;
    }

    bool remove(const std::string& name) {
        std::error_code ec;
        const bool ok = std::filesystem::remove(dataDir_ + "/" + fileNameFor(name), ec);
        reload();
        return ok;
    }

    bool rename(const std::string& oldName, const std::string& newRaw, std::string* err = nullptr) {
        const UserPreset* p = find(oldName);
        if (!p) { if (err) *err = "Preset not found"; return false; }
        const std::string nn = cleanName(newRaw);
        if (nn.empty()) { if (err) *err = "Name is empty"; return false; }
        if (nn == oldName) return true;
        if (find(nn)) { if (err) *err = "A preset with that name exists"; return false; }
        float v[kParamCount]; std::copy(p->values, p->values + kParamCount, v);
        if (!save(nn, v, err)) return false;
        remove(oldName);
        if (stars_.erase(oldName)) { stars_.insert(nn); saveConfig(); }
        return true;
    }

    bool importFile(const std::string& path, std::string* nameOut = nullptr, std::string* err = nullptr) {
        UserPreset p;
        if (!readFile(path, p)) { if (err) *err = "Not a valid L-STRIP preset file"; return false; }
        std::string name = p.name;
        for (uint32_t i = 0; i < kNumFactoryPresets; ++i) if (name == kFactoryPresets[i].name) name += " (imported)";
        if (find(name)) name = nextName(name);
        if (!save(name, p.values, err)) return false;
        if (nameOut) *nameOut = name;
        return true;
    }

    // Export a user preset or an arbitrary parameter set to <dir>/<name>.lstrip.json.
    bool exportTo(const std::string& name, const float* values, const std::string& dir, std::string* pathOut = nullptr) const {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        UserPreset p; p.name = name; std::copy(values, values + kParamCount, p.values);
        const std::string path = dir + "/" + fileNameFor(name);
        if (!writeFile(path, p)) return false;
        if (pathOut) *pathOut = path;
        return true;
    }

    // ----------------------------------------------------------------- config
    void loadConfig() {
        std::string text;
        if (!slurp(configDir_ + "/ui.json", text)) return;
        Json j;
        if (!Json::parse(text, j) || j.type != Json::Obj) return;
        if (const Json* t = j.get("theme")) if (t->type == Json::Str) dark_ = t->s != "light";
        if (const Json* s = j.get("stars")) if (s->type == Json::Arr) for (const auto& e : s->a) if (e.type == Json::Str) stars_.insert(e.s);
    }
    void saveConfig() const {
        std::error_code ec;
        std::filesystem::create_directories(configDir_, ec);
        Json j = Json::object();
        j.set("theme", Json::string(dark_ ? "dark" : "light"));
        Json st = Json::array();
        for (const auto& s : stars_) st.push(Json::string(s));
        j.set("stars", st);
        std::ofstream f(configDir_ + "/ui.json", std::ios::trunc);
        if (f) f << j.dump(2);
    }
    bool dark() const { return dark_; }
    void setDark(bool d) { dark_ = d; saveConfig(); }
    bool starred(const std::string& n) const { return stars_.count(n) != 0; }
    void toggleStar(const std::string& n) { if (!stars_.erase(n)) stars_.insert(n); saveConfig(); }

    // -------------------------------------------------------------------- I/O
    static bool readFile(const std::string& path, UserPreset& out) {
        std::string text;
        if (!slurp(path, text)) return false;
        Json j;
        if (!Json::parse(text, j) || j.type != Json::Obj) return false;
        const Json* fmt = j.get("format"); const Json* nm = j.get("name"); const Json* ps = j.get("params");
        if (!fmt || fmt->type != Json::Str || fmt->s != kFormat) return false;
        if (!nm || nm->type != Json::Str || !ps || ps->type != Json::Obj) return false;
        out.name = cleanName(nm->s);
        if (out.name.empty()) return false;
        if (const Json* c = j.get("category")) if (c->type == Json::Str) out.category = c->s.empty() ? "User" : c->s.substr(0, 24);
        for (uint32_t i = 0; i < kParamCount; ++i) out.values[i] = kParams[i].def;
        for (const auto& kv : ps->o) {
            const int id = findParamBySymbol(kv.first.c_str());
            if (id < 0 || isOutput(static_cast<uint32_t>(id))) continue;
            double v = kv.second.type == Json::Num ? kv.second.n : (kv.second.type == Json::Bool ? (kv.second.b ? 1.0 : 0.0) : kParams[id].def);
            v = std::min<double>(std::max<double>(v, kParams[id].min), kParams[id].max);
            out.values[id] = static_cast<float>(v);
        }
        return true;
    }

private:
    static bool writeFile(const std::string& path, const UserPreset& p) {
        Json j = Json::object();
        j.set("format", Json::string(kFormat)).set("version", Json::number(kVersion));
        j.set("name", Json::string(p.name)).set("category", Json::string(p.category));
        Json ps = Json::object();
        for (uint32_t i = 0; i < kParamCount; ++i) if (!isOutput(i)) ps.set(kParams[i].symbol, Json::number(p.values[i]));
        j.set("params", ps);
        std::ofstream f(path, std::ios::trunc);
        if (!f) return false;
        f << j.dump(2);
        return static_cast<bool>(f);
    }
    static bool slurp(const std::string& path, std::string& out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::ostringstream ss; ss << f.rdbuf();
        out = ss.str();
        return out.size() < (4u << 20);
    }

    std::string dataDir_, configDir_;
    std::vector<UserPreset> user_;
    std::set<std::string> stars_;
    bool dark_ = true;
};

} // namespace lstrip
