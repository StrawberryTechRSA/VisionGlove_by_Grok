#include "visionglove/config.hpp"
#include "visionglove/logger.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace vg {
namespace {

// Minimal JSON subset parser for our config schema (objects, arrays, strings, numbers, bools, null).
// Avoids heavy deps; sufficient and fully under our control.

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<JsonValue> a;
    std::unordered_map<std::string, JsonValue> o;

    const JsonValue* get(const std::string& key) const {
        if (type != Type::Object) return nullptr;
        auto it = o.find(key);
        return it == o.end() ? nullptr : &it->second;
    }
    double num(const std::string& key, double def) const {
        const auto* v = get(key);
        return (v && v->type == Type::Number) ? v->n : def;
    }
    int integer(const std::string& key, int def) const {
        return static_cast<int>(num(key, def));
    }
    bool boolean(const std::string& key, bool def) const {
        const auto* v = get(key);
        if (!v) return def;
        if (v->type == Type::Bool) return v->b;
        if (v->type == Type::Number) return v->n != 0;
        return def;
    }
    std::string str(const std::string& key, const std::string& def) const {
        const auto* v = get(key);
        return (v && v->type == Type::String) ? v->s : def;
    }
};

class JsonParser {
public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    JsonValue parse() {
        skip_ws();
        return parse_value();
    }

private:
    std::string text_;
    std::size_t i_ = 0;

    void skip_ws() {
        while (i_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[i_]))) ++i_;
    }
    char peek() const { return i_ < text_.size() ? text_[i_] : '\0'; }
    char get() { return i_ < text_.size() ? text_[i_++] : '\0'; }

    JsonValue parse_value() {
        skip_ws();
        char c = peek();
        if (c == '{') return parse_object();
        if (c == '[') return parse_array();
        if (c == '"') return parse_string();
        if (c == 't' || c == 'f') return parse_bool();
        if (c == 'n') { parse_null(); return {}; }
        if (c == '-' || std::isdigit(static_cast<unsigned char>(c))) return parse_number();
        return {};
    }

    JsonValue parse_object() {
        JsonValue v;
        v.type = JsonValue::Type::Object;
        get();  // {
        skip_ws();
        if (peek() == '}') { get(); return v; }
        while (true) {
            skip_ws();
            JsonValue key = parse_string();
            skip_ws();
            if (get() != ':') break;
            JsonValue val = parse_value();
            v.o[key.s] = std::move(val);
            skip_ws();
            char c = get();
            if (c == '}') break;
            if (c != ',') break;
        }
        return v;
    }

    JsonValue parse_array() {
        JsonValue v;
        v.type = JsonValue::Type::Array;
        get();  // [
        skip_ws();
        if (peek() == ']') { get(); return v; }
        while (true) {
            v.a.push_back(parse_value());
            skip_ws();
            char c = get();
            if (c == ']') break;
            if (c != ',') break;
        }
        return v;
    }

    JsonValue parse_string() {
        JsonValue v;
        v.type = JsonValue::Type::String;
        get();  // "
        while (i_ < text_.size()) {
            char c = get();
            if (c == '"') break;
            if (c == '\\' && i_ < text_.size()) {
                char e = get();
                switch (e) {
                    case 'n': v.s.push_back('\n'); break;
                    case 't': v.s.push_back('\t'); break;
                    case 'r': v.s.push_back('\r'); break;
                    case '"': v.s.push_back('"'); break;
                    case '\\': v.s.push_back('\\'); break;
                    default: v.s.push_back(e); break;
                }
            } else {
                v.s.push_back(c);
            }
        }
        return v;
    }

    JsonValue parse_number() {
        JsonValue v;
        v.type = JsonValue::Type::Number;
        std::size_t start = i_;
        if (peek() == '-') ++i_;
        while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        if (peek() == '.') {
            ++i_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        }
        if (peek() == 'e' || peek() == 'E') {
            ++i_;
            if (peek() == '+' || peek() == '-') ++i_;
            while (std::isdigit(static_cast<unsigned char>(peek()))) ++i_;
        }
        v.n = std::stod(text_.substr(start, i_ - start));
        return v;
    }

    JsonValue parse_bool() {
        JsonValue v;
        v.type = JsonValue::Type::Bool;
        if (text_.compare(i_, 4, "true") == 0) { i_ += 4; v.b = true; }
        else if (text_.compare(i_, 5, "false") == 0) { i_ += 5; v.b = false; }
        return v;
    }

    void parse_null() {
        if (text_.compare(i_, 4, "null") == 0) i_ += 4;
    }
};

void apply_json(const JsonValue& root, AppConfig& cfg) {
    if (root.type != JsonValue::Type::Object) return;

    if (const auto* s = root.get("system")) {
        cfg.system.name = s->str("name", cfg.system.name);
        cfg.system.version = s->str("version", cfg.system.version);
        cfg.system.debug_mode = s->boolean("debug_mode", cfg.system.debug_mode);
        cfg.system.log_level = s->str("log_level", cfg.system.log_level);
    }
    if (const auto* s = root.get("sensors")) {
        cfg.sensors.enabled = s->boolean("enabled", cfg.sensors.enabled);
        cfg.sensors.sample_rate = s->integer("sample_rate", cfg.sensors.sample_rate);
        cfg.sensors.calibration_required = s->boolean("calibration_required", cfg.sensors.calibration_required);
        cfg.sensors.timeout_s = s->num("timeout", cfg.sensors.timeout_s);
        cfg.sensors.unusual_accel_threshold = s->num("unusual_accel_threshold", cfg.sensors.unusual_accel_threshold);
        cfg.sensors.closed_threshold = s->num("closed_threshold", cfg.sensors.closed_threshold);
        cfg.sensors.open_threshold = s->num("open_threshold", cfg.sensors.open_threshold);
        cfg.sensors.panic_window_s = s->num("panic_window_s", cfg.sensors.panic_window_s);
        cfg.sensors.panic_fist_count = s->integer("panic_fist_count", cfg.sensors.panic_fist_count);
    }
    if (const auto* s = root.get("vision")) {
        cfg.vision.enabled = s->boolean("enabled", cfg.vision.enabled);
        cfg.vision.camera_index = s->integer("camera_index", cfg.vision.camera_index);
        cfg.vision.fps = s->integer("fps", cfg.vision.fps);
        cfg.vision.detection_threshold = s->num("detection_threshold", cfg.vision.detection_threshold);
        cfg.vision.person_threshold = s->integer("person_threshold", cfg.vision.person_threshold);
        cfg.vision.simulate = s->boolean("simulate", cfg.vision.simulate);
        if (const auto* r = s->get("resolution"); r && r->type == JsonValue::Type::Array && r->a.size() >= 2) {
            cfg.vision.resolution[0] = static_cast<int>(r->a[0].n);
            cfg.vision.resolution[1] = static_cast<int>(r->a[1].n);
        }
    }
    if (const auto* s = root.get("haptics")) {
        cfg.haptics.enabled = s->boolean("enabled", cfg.haptics.enabled);
        cfg.haptics.intensity = s->num("intensity", cfg.haptics.intensity);
        cfg.haptics.duration_s = s->num("duration", cfg.haptics.duration_s);
        cfg.haptics.pattern = s->str("pattern", cfg.haptics.pattern);
    }
    if (const auto* s = root.get("communications")) {
        cfg.communications.emergency_contact = s->str("emergency_contact", cfg.communications.emergency_contact);
        cfg.communications.police_number = s->str("police_number", cfg.communications.police_number);
        cfg.communications.dry_run = s->boolean("dry_run", cfg.communications.dry_run);
        if (const auto* sms = s->get("sms_service")) {
            cfg.communications.sms.provider = sms->str("provider", cfg.communications.sms.provider);
            cfg.communications.sms.account_sid = sms->str("account_sid", cfg.communications.sms.account_sid);
            cfg.communications.sms.auth_token = sms->str("auth_token", cfg.communications.sms.auth_token);
            cfg.communications.sms.from_number = sms->str("from_number", cfg.communications.sms.from_number);
        }
    }
    if (const auto* s = root.get("security")) {
        cfg.security.encryption_enabled = s->boolean("encryption_enabled", cfg.security.encryption_enabled);
        cfg.security.key_rotation_interval_s = s->integer("key_rotation_interval", cfg.security.key_rotation_interval_s);
        cfg.security.max_failed_attempts = s->integer("max_failed_attempts", cfg.security.max_failed_attempts);
    }
    if (const auto* s = root.get("livestream")) {
        cfg.livestream.enabled = s->boolean("enabled", cfg.livestream.enabled);
        cfg.livestream.quality = s->str("quality", cfg.livestream.quality);
        cfg.livestream.platform = s->str("platform", cfg.livestream.platform);
        cfg.livestream.stream_key = s->str("stream_key", cfg.livestream.stream_key);
        cfg.livestream.max_duration_s = s->integer("max_duration", cfg.livestream.max_duration_s);
        cfg.livestream.dry_run = s->boolean("dry_run", cfg.livestream.dry_run);
    }
}

}  // namespace

AppConfig load_config(const std::string& path, bool* ok) {
    AppConfig cfg;
    std::ifstream in(path);
    if (!in) {
        VG_LOG_WARN("Config", "Config file not found, using defaults: " + path);
        if (ok) *ok = false;
        return cfg;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    try {
        JsonParser parser(ss.str());
        apply_json(parser.parse(), cfg);
        if (ok) *ok = true;
        VG_LOG_INFO("Config", "Loaded configuration from " + path);
    } catch (const std::exception& e) {
        VG_LOG_ERROR("Config", std::string("Parse failed: ") + e.what());
        if (ok) *ok = false;
    }
    return cfg;
}

bool save_config(const std::string& path, const AppConfig& cfg) {
    std::ofstream out(path);
    if (!out) return false;
    out << "{\n"
        << "    \"system\": {\n"
        << "        \"name\": \"" << cfg.system.name << "\",\n"
        << "        \"version\": \"" << cfg.system.version << "\",\n"
        << "        \"debug_mode\": " << (cfg.system.debug_mode ? "true" : "false") << ",\n"
        << "        \"log_level\": \"" << cfg.system.log_level << "\"\n"
        << "    },\n"
        << "    \"sensors\": {\n"
        << "        \"enabled\": " << (cfg.sensors.enabled ? "true" : "false") << ",\n"
        << "        \"sample_rate\": " << cfg.sensors.sample_rate << ",\n"
        << "        \"calibration_required\": " << (cfg.sensors.calibration_required ? "true" : "false") << ",\n"
        << "        \"timeout\": " << cfg.sensors.timeout_s << ",\n"
        << "        \"unusual_accel_threshold\": " << cfg.sensors.unusual_accel_threshold << ",\n"
        << "        \"closed_threshold\": " << cfg.sensors.closed_threshold << ",\n"
        << "        \"open_threshold\": " << cfg.sensors.open_threshold << ",\n"
        << "        \"panic_window_s\": " << cfg.sensors.panic_window_s << ",\n"
        << "        \"panic_fist_count\": " << cfg.sensors.panic_fist_count << "\n"
        << "    },\n"
        << "    \"vision\": {\n"
        << "        \"enabled\": " << (cfg.vision.enabled ? "true" : "false") << ",\n"
        << "        \"camera_index\": " << cfg.vision.camera_index << ",\n"
        << "        \"resolution\": [" << cfg.vision.resolution[0] << ", " << cfg.vision.resolution[1] << "],\n"
        << "        \"fps\": " << cfg.vision.fps << ",\n"
        << "        \"detection_threshold\": " << cfg.vision.detection_threshold << ",\n"
        << "        \"person_threshold\": " << cfg.vision.person_threshold << ",\n"
        << "        \"simulate\": " << (cfg.vision.simulate ? "true" : "false") << "\n"
        << "    },\n"
        << "    \"haptics\": {\n"
        << "        \"enabled\": " << (cfg.haptics.enabled ? "true" : "false") << ",\n"
        << "        \"intensity\": " << cfg.haptics.intensity << ",\n"
        << "        \"duration\": " << cfg.haptics.duration_s << ",\n"
        << "        \"pattern\": \"" << cfg.haptics.pattern << "\"\n"
        << "    },\n"
        << "    \"communications\": {\n"
        << "        \"emergency_contact\": \"" << cfg.communications.emergency_contact << "\",\n"
        << "        \"police_number\": \"" << cfg.communications.police_number << "\",\n"
        << "        \"dry_run\": " << (cfg.communications.dry_run ? "true" : "false") << ",\n"
        << "        \"sms_service\": {\n"
        << "            \"provider\": \"" << cfg.communications.sms.provider << "\",\n"
        << "            \"account_sid\": \"" << cfg.communications.sms.account_sid << "\",\n"
        << "            \"auth_token\": \"" << cfg.communications.sms.auth_token << "\",\n"
        << "            \"from_number\": \"" << cfg.communications.sms.from_number << "\"\n"
        << "        }\n"
        << "    },\n"
        << "    \"security\": {\n"
        << "        \"encryption_enabled\": " << (cfg.security.encryption_enabled ? "true" : "false") << ",\n"
        << "        \"key_rotation_interval\": " << cfg.security.key_rotation_interval_s << ",\n"
        << "        \"max_failed_attempts\": " << cfg.security.max_failed_attempts << "\n"
        << "    },\n"
        << "    \"livestream\": {\n"
        << "        \"enabled\": " << (cfg.livestream.enabled ? "true" : "false") << ",\n"
        << "        \"quality\": \"" << cfg.livestream.quality << "\",\n"
        << "        \"platform\": \"" << cfg.livestream.platform << "\",\n"
        << "        \"stream_key\": \"" << cfg.livestream.stream_key << "\",\n"
        << "        \"max_duration\": " << cfg.livestream.max_duration_s << ",\n"
        << "        \"dry_run\": " << (cfg.livestream.dry_run ? "true" : "false") << "\n"
        << "    }\n"
        << "}\n";
    return true;
}

bool validate_config(const AppConfig& cfg, std::string* error) {
    if (cfg.sensors.sample_rate <= 0) {
        if (error) *error = "sensors.sample_rate must be positive";
        return false;
    }
    if (cfg.vision.resolution[0] <= 0 || cfg.vision.resolution[1] <= 0) {
        if (error) *error = "vision.resolution must be positive [w,h]";
        return false;
    }
    if (cfg.vision.fps <= 0) {
        if (error) *error = "vision.fps must be positive";
        return false;
    }
    if (cfg.sensors.closed_threshold <= cfg.sensors.open_threshold) {
        if (error) *error = "closed_threshold must be > open_threshold";
        return false;
    }
    return true;
}

}  // namespace vg
