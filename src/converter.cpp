#include "converter.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

namespace dataconv {
namespace {

using json = nlohmann::json;

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string normalize_ext(std::string ext) {
    ext = lower(ext);
    if (!ext.empty() && ext.front() == '.') ext.erase(ext.begin());
    if (ext == "yml") ext = "yaml";
    if (ext != "csv" && ext != "json" && ext != "yaml") {
        throw std::runtime_error("Unsupported file extension: " + ext);
    }
    return ext;
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

std::vector<std::vector<std::string>> parse_csv(const std::string& input) {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> row;
    std::string field;
    bool quoted = false;
    bool field_started = false;

    for (std::size_t i = 0; i < input.size(); ++i) {
        const char c = input[i];
        if (quoted) {
            if (c == '"') {
                if (i + 1 < input.size() && input[i + 1] == '"') {
                    field += '"';
                    ++i;
                } else {
                    quoted = false;
                }
            } else {
                field += c;
            }
            continue;
        }

        if (c == '"' && !field_started && field.empty()) {
            quoted = true;
            field_started = true;
        } else if (c == ',') {
            row.push_back(field);
            field.clear();
            field_started = false;
        } else if (c == '\n' || c == '\r') {
            if (c == '\r' && i + 1 < input.size() && input[i + 1] == '\n') ++i;
            row.push_back(field);
            field.clear();
            field_started = false;
            rows.push_back(row);
            row.clear();
        } else {
            field += c;
            field_started = true;
        }
    }
    if (quoted) throw std::runtime_error("Malformed CSV: unterminated quoted field");
    if (!field.empty() || !row.empty() || field_started) {
        row.push_back(field);
        rows.push_back(row);
    }
    return rows;
}

json csv_to_json(const std::string& input) {
    auto rows = parse_csv(input);
    if (rows.empty()) return json::array();
    const auto headers = rows.front();
    if (headers.empty()) throw std::runtime_error("CSV header is empty");
    json result = json::array();
    for (std::size_t r = 1; r < rows.size(); ++r) {
        if (rows[r].size() != headers.size()) {
            throw std::runtime_error("CSV row has a different number of fields than the header");
        }
        json item = json::object();
        for (std::size_t c = 0; c < headers.size(); ++c) item[headers[c]] = rows[r][c];
        result.push_back(std::move(item));
    }
    return result;
}

json yaml_node_to_json(const YAML::Node& node) {
    if (!node || node.IsNull()) return nullptr;
    if (node.IsSequence()) {
        json out = json::array();
        for (const auto& child : node) out.push_back(yaml_node_to_json(child));
        return out;
    }
    if (node.IsMap()) {
        json out = json::object();
        for (const auto& kv : node) {
            if (!kv.first.IsScalar()) throw std::runtime_error("YAML mapping keys must be scalars");
            out[kv.first.as<std::string>()] = yaml_node_to_json(kv.second);
        }
        return out;
    }
    const std::string s = node.Scalar();
    const std::string ls = lower(s);
    if (ls == "null" || s == "~") return nullptr;
    if (ls == "true") return true;
    if (ls == "false") return false;
    try {
        std::size_t used = 0;
        long long v = std::stoll(s, &used);
        if (used == s.size()) return v;
    } catch (...) {}
    try {
        std::size_t used = 0;
        double v = std::stod(s, &used);
        if (used == s.size()) return v;
    } catch (...) {}
    return s;
}

json parse_input(const std::string& input, const std::string& ext) {
    if (ext == "csv") return csv_to_json(input);
    if (ext == "json") return json::parse(input);
    YAML::Node root = YAML::Load(input);
    return yaml_node_to_json(root);
}

void validate_records(const json& value) {
    if (!value.is_array()) throw std::runtime_error("Expected an array/sequence of records");
    for (const auto& item : value) {
        if (!item.is_object()) throw std::runtime_error("Each record must be an object/map");
    }
}

std::string csv_escape(const std::string& s) {
    if (s.find_first_of(",\"\r\n") == std::string::npos) return s;
    std::string out = "\"";
    for (char c : s) {
        if (c == '"') out += "\"\"";
        else out += c;
    }
    return out + "\"";
}

std::string to_csv(const json& value) {
    validate_records(value);
    std::vector<std::string> headers;
    if (!value.empty()) {
        for (auto it = value[0].begin(); it != value[0].end(); ++it) headers.push_back(it.key());
        for (const auto& item : value) {
            if (item.size() != headers.size()) throw std::runtime_error("Records must have identical keys");
            for (const auto& h : headers) {
                if (!item.contains(h)) throw std::runtime_error("Records must have identical keys");
                if (item[h].is_structured()) throw std::runtime_error("Nested values cannot be exported to CSV");
            }
        }
    }
    std::ostringstream out;
    for (std::size_t i = 0; i < headers.size(); ++i) {
        if (i) out << ',';
        out << csv_escape(headers[i]);
    }
    if (!headers.empty()) out << '\n';
    for (const auto& item : value) {
        for (std::size_t i = 0; i < headers.size(); ++i) {
            if (i) out << ',';
            const auto& v = item.at(headers[i]);
            std::string text;
            if (v.is_string()) text = v.get<std::string>();
            else if (v.is_null()) text = "null";
            else if (v.is_boolean()) text = v.get<bool>() ? "true" : "false";
            else text = v.dump();
            out << csv_escape(text);
        }
        out << '\n';
    }
    return out.str();
}

std::string to_yaml(const json& value) {
    YAML::Node node;
    if (value.is_array()) {
        node = YAML::Node(YAML::NodeType::Sequence);
        for (const auto& item : value) node.push_back(YAML::Load(item.dump()));
    } else {
        node = YAML::Load(value.dump());
    }
    YAML::Emitter emitter;
    emitter << node;
    if (!emitter.good()) throw std::runtime_error("Failed to serialize YAML");
    return std::string(emitter.c_str()) + "\n";
}

}  // namespace

std::string convert_text(const std::string& input,
                         const std::string& input_extension,
                         const std::string& output_extension) {
    const auto in_ext = normalize_ext(input_extension);
    const auto out_ext = normalize_ext(output_extension);
    if (in_ext == out_ext) throw std::runtime_error("Input and output formats must differ");
    json value = parse_input(input, in_ext);
    if (out_ext == "json") return value.dump(2) + "\n";
    if (out_ext == "yaml") return to_yaml(value);
    return to_csv(value);
}

void convert_file(const std::string& input_path, const std::string& output_path) {
    auto extension = [](const std::string& path) {
        const auto pos = path.find_last_of('.');
        if (pos == std::string::npos) throw std::runtime_error("File has no extension: " + path);
        return path.substr(pos + 1);
    };

    std::ifstream in(input_path, std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open input file: " + input_path);
    std::ostringstream buffer;
    buffer << in.rdbuf();

    const auto converted = convert_text(buffer.str(), extension(input_path), extension(output_path));
    std::ofstream out(output_path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot open output file: " + output_path);
    out << converted;
    if (!out) throw std::runtime_error("Failed to write output file: " + output_path);
}

}  // namespace dataconv
