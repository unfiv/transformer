#include "io/json_readers.hpp"
#include "core/profiler.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include <numeric>
#include <cassert>
#include <cmath>

namespace transformer
{

namespace
{

struct JsonValue;
using JsonObject = std::unordered_map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue
{
    std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject> value;
};

class JsonLiteParser
{
public:
    explicit JsonLiteParser(std::string text)
        : text_(std::move(text))
    {
    }

    JsonValue parse_value()
    {
        skip_ws();
        if (position_ >= text_.size())
        {
            throw std::runtime_error("JSON parse error: unexpected end of input");
        }

        const char c = text_[position_];
        if (c == '{')
        {
            return JsonValue{ parse_object() };
        }
        if (c == '[')
        {
            return JsonValue{ parse_array() };
        }
        if (c == '"')
        {
            return JsonValue{ parse_string() };
        }
        if (c == 't' || c == 'f')
        {
            return JsonValue{ parse_bool() };
        }
        if (c == 'n')
        {
            parse_null();
            return JsonValue{ nullptr };
        }
        return JsonValue{ parse_number() };
    }

private:
    JsonObject parse_object()
    {
        expect_char('{');
        JsonObject object;
        skip_ws();

        while (!consume_char('}'))
        {
            const std::string key = parse_string();
            expect_char(':');
            object[key] = parse_value();
            skip_ws();
            if (!consume_char(','))
            {
                expect_char('}');
                break;
            }
            skip_ws();
        }

        return object;
    }

    JsonArray parse_array()
    {
        expect_char('[');
        JsonArray array;
        skip_ws();

        while (!consume_char(']'))
        {
            array.push_back(parse_value());
            skip_ws();
            if (!consume_char(','))
            {
                expect_char(']');
                break;
            }
            skip_ws();
        }

        return array;
    }

    std::string parse_string()
    {
        expect_char('"');
        std::string result;
        while (position_ < text_.size())
        {
            const char c = text_[position_++];
            if (c == '"')
            {
                return result;
            }
            if (c == '\\')
            {
                if (position_ >= text_.size())
                {
                    throw std::runtime_error("JSON parse error: unfinished escape sequence");
                }
                const char escaped = text_[position_++];
                switch (escaped)
                {
                case '"':
                case '\\':
                case '/':
                    result.push_back(escaped);
                    break;
                case 'b':
                    result.push_back('\b');
                    break;
                case 'f':
                    result.push_back('\f');
                    break;
                case 'n':
                    result.push_back('\n');
                    break;
                case 'r':
                    result.push_back('\r');
                    break;
                case 't':
                    result.push_back('\t');
                    break;
                default:
                    throw std::runtime_error("JSON parse error: unsupported escape sequence");
                }
                continue;
            }

            result.push_back(c);
        }

        throw std::runtime_error("JSON parse error: unterminated string");
    }

    bool parse_bool()
    {
        if (match_token("true"))
        {
            return true;
        }
        if (match_token("false"))
        {
            return false;
        }
        throw std::runtime_error("JSON parse error: expected boolean");
    }

    void parse_null()
    {
        if (!match_token("null"))
        {
            throw std::runtime_error("JSON parse error: expected null");
        }
    }

    double parse_number()
    {
        skip_ws();
        const std::size_t start = position_;
        while (position_ < text_.size())
        {
            const char ch = text_[position_];
            if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e'
                || ch == 'E')
            {
                ++position_;
                continue;
            }
            break;
        }

        if (start == position_)
        {
            throw std::runtime_error("JSON parse error: expected number");
        }

        return std::stod(text_.substr(start, position_ - start));
    }

    bool match_token(const std::string& token)
    {
        skip_ws();
        if (text_.compare(position_, token.size(), token) == 0)
        {
            position_ += token.size();
            return true;
        }
        return false;
    }

    void skip_ws()
    {
        while (position_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[position_])) != 0)
        {
            ++position_;
        }
    }

    void expect_char(char c)
    {
        skip_ws();
        if (position_ >= text_.size() || text_[position_] != c)
        {
            throw std::runtime_error(std::string("JSON parse error: expected '") + c + "'");
        }
        ++position_;
    }

    bool consume_char(char c)
    {
        skip_ws();
        if (position_ < text_.size() && text_[position_] == c)
        {
            ++position_;
            return true;
        }
        return false;
    }

    std::string text_;
    std::size_t position_ = 0;
};

std::string read_file_to_string(const std::string& path)
{
    std::ifstream input(path);

    if (!input)
    {
        std::string fallback = path;
        constexpr char kCommonTypo[] = "asserts/";
        const std::size_t typo_pos = fallback.find(kCommonTypo);
        if (typo_pos != std::string::npos)
        {
            fallback.replace(typo_pos, std::char_traits<char>::length(kCommonTypo), "assets/");
            input.open(fallback);
        }
    }

    if (!input)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    return ss.str();
}

const JsonObject& as_object(const JsonValue& value, const std::string& error)
{
    const auto object = std::get_if<JsonObject>(&value.value);
    if (object == nullptr)
    {
        throw std::runtime_error(error);
    }
    return *object;
}

const JsonArray& as_array(const JsonValue& value, const std::string& error)
{
    const auto array = std::get_if<JsonArray>(&value.value);
    if (array == nullptr)
    {
        throw std::runtime_error(error);
    }
    return *array;
}

double as_number(const JsonValue& value, const std::string& error)
{
    const auto number = std::get_if<double>(&value.value);
    if (number == nullptr)
    {
        throw std::runtime_error(error);
    }
    return *number;
}

const JsonValue* find_key(const JsonObject& object, std::initializer_list<const char*> keys)
{
    for (const char* key : keys)
    {
        const auto it = object.find(key);
        if (it != object.end())
        {
            return &it->second;
        }
    }
    return nullptr;
}

std::array<std::int8_t, 4> parse_bone_indices(const JsonValue& value)
{
    std::array<std::int8_t, 4> indices{ -1, -1, -1, -1 };
    const JsonArray& index_values = as_array(value, "Weights JSON parse error: bone indices must be an array");

    for (std::size_t i = 0; i < index_values.size() && i < indices.size(); ++i)
    {
        indices[i] = static_cast<std::int8_t>(as_number(index_values[i], "Weights JSON parse error: bone index must be numeric"));
    }

    return indices;
}

std::array<float, 4> parse_weights(const JsonValue& value)
{
    std::array<float, 4> weights{ 0.0F, 0.0F, 0.0F, 0.0F };
    const JsonArray& weight_values = as_array(value, "Weights JSON parse error: weights must be an array");

    for (std::size_t i = 0; i < weight_values.size() && i < weights.size(); ++i)
    {
        weights[i] = static_cast<float>(as_number(weight_values[i], "Weights JSON parse error: weight must be numeric"));
    }

    return weights;
}

VertexBoneWeights parse_vertex_weights(const JsonValue& value)
{
    const JsonObject& vertex_object = as_object(value, "Weights JSON parse error: each vertex must be an object");
    VertexBoneWeights vertex_bone_weights;

    const JsonValue* indices_value = find_key(vertex_object, { "bone_indices", "boneIndices", "indices", "joints", "index" });
    const JsonValue* weights_value = find_key(vertex_object, { "weights", "bone_weights", "boneWeights", "weight" });

    if (indices_value != nullptr && weights_value != nullptr)
    {
        vertex_bone_weights.bone_indices = parse_bone_indices(*indices_value);
        vertex_bone_weights.weights = parse_weights(*weights_value);
        return vertex_bone_weights;
    }

    const JsonValue* influences_value = find_key(vertex_object, { "influences", "bones", "bone_influences" });
    if (influences_value == nullptr)
    {
        throw std::runtime_error("Weights JSON parse error: vertex must contain either indices/weights or influences array");
    }

    const JsonArray& influences = as_array(*influences_value, "Weights JSON parse error: influences must be an array");
    for (std::size_t i = 0; i < influences.size() && i < 4; ++i)
    {
        const JsonObject& influence_object = as_object(influences[i], "Weights JSON parse error: influence must be an object");
        const JsonValue* influence_index = find_key(influence_object, { "bone_index", "boneIndex", "index", "joint" });
        const JsonValue* influence_weight = find_key(influence_object, { "weight", "value" });

        if (influence_index == nullptr || influence_weight == nullptr)
        {
            throw std::runtime_error("Weights JSON parse error: influence must contain bone index and weight");
        }

        vertex_bone_weights.bone_indices[i] =
            static_cast<std::int8_t>(as_number(*influence_index, "Weights JSON parse error: influence index must be numeric"));
        vertex_bone_weights.weights[i] =
            static_cast<float>(as_number(*influence_weight, "Weights JSON parse error: influence weight must be numeric"));
    }

    // 2. Validate normalization to ensure the hot loop doesn't need to divide by sum.
    // We use std::accumulate to sum all weights (including zeroed-out unused slots).
    const float weights_sum = std::accumulate(vertex_bone_weights.weights.begin(), vertex_bone_weights.weights.end(), 0.0F);
    
    // Floating point epsilon check (0.001 is standard for skinning data validation).
    assert(std::abs(weights_sum - 1.0F) < 0.001F && "Weights sum must be 1.0 for branchless optimization!");

    return vertex_bone_weights;
}

BoneWeightsData parse_bone_weights_data(const JsonValue& root)
{
    BoneWeightsData data;

    const JsonArray* vertices = nullptr;
    if (const auto* root_array = std::get_if<JsonArray>(&root.value))
    {
        vertices = root_array;
    }
    else
    {
        const JsonObject& root_object = as_object(root, "Weights JSON parse error: root value must be object or array");
        const JsonValue* vertices_value = find_key(root_object, { "vertices", "vertex_weights", "weights", "skin" });
        if (vertices_value == nullptr)
        {
            throw std::runtime_error("Weights JSON parse error: root object must contain vertices array");
        }
        vertices = &as_array(*vertices_value, "Weights JSON parse error: vertices must be an array");
    }

    data.per_vertex_weights.reserve(vertices->size());
    for (const JsonValue& vertex_value : *vertices)
    {
        data.per_vertex_weights.push_back(parse_vertex_weights(vertex_value));
    }

    return data;
}

std::vector<Mat4> parse_bone_matrices(const JsonValue& root)
{
    const JsonArray* bones = nullptr;
    if (const auto* root_array = std::get_if<JsonArray>(&root.value))
    {
        bones = root_array;
    }
    else
    {
        const JsonObject& root_object = as_object(root, "Pose JSON parse error: root value must be object or array");
        const JsonValue* bones_value = find_key(root_object, { "bones" });
        if (bones_value == nullptr)
        {
            throw std::runtime_error("Pose JSON parse error: expected 'bones' array in root object");
        }
        bones = &as_array(*bones_value, "Pose JSON parse error: bones must be an array");
    }

    std::vector<Mat4> matrices;
    matrices.reserve(bones->size());

    for (const JsonValue& bone_value : *bones)
    {
        const JsonArray* matrix_values = nullptr;
        if (const auto* direct_matrix = std::get_if<JsonArray>(&bone_value.value))
        {
            matrix_values = direct_matrix;
        }
        else
        {
            const JsonObject& bone_object = as_object(bone_value, "Pose JSON parse error: each bone must be an object or matrix array");
            const JsonValue* matrix_value = find_key(bone_object, { "matrix" });
            if (matrix_value == nullptr)
            {
                throw std::runtime_error("Pose JSON parse error: bone object must contain 'matrix' array");
            }
            matrix_values = &as_array(*matrix_value, "Pose JSON parse error: matrix must be an array");
        }

        if (matrix_values->size() != 16)
        {
            throw std::runtime_error("Pose JSON parse error: matrix must contain exactly 16 numeric values");
        }

        Mat4 matrix{};
        for (std::size_t i = 0; i < 16; ++i)
        {
            matrix.m[i] = static_cast<float>(as_number((*matrix_values)[i], "Pose JSON parse error: matrix element must be numeric"));
        }
        matrices.push_back(matrix);
    }

    return matrices;
}

} // namespace

BoneWeightsData JsonBoneWeightsReader::read(const std::string& weights_file, Profiler& profiler) const
{
    const auto scope = profiler.stage("read_weights_json");
    const JsonValue parsed_root = JsonLiteParser(read_file_to_string(weights_file)).parse_value();
    return parse_bone_weights_data(parsed_root);
}

std::vector<Mat4> JsonBonePoseReader::read_matrices(const std::string& file_path, Profiler& profiler,
                                                    const std::string& stage_name) const
{
    const auto scope = profiler.stage(stage_name);
    const JsonValue parsed_root = JsonLiteParser(read_file_to_string(file_path)).parse_value();
    return parse_bone_matrices(parsed_root);
}

void JsonStatsWriter::write(const std::string& output_file, const StatsReport& stats) const
{
    std::ofstream output(output_file);
    if (!output)
    {
        throw std::runtime_error("Failed to open stats output file: " + output_file);
    }

    output.setf(std::ios::fixed);
    output.precision(3);

    output << "{\n  \"unit\": \"microseconds\",\n  \"stages\": [\n";
    for (std::size_t i = 0; i < stats.stages.size(); ++i)
    {
        output << "    { \"stage\": \"" << stats.stages[i].stage << "\", \"microseconds\": "
               << stats.stages[i].microseconds << " }";
        if (i + 1 < stats.stages.size())
        {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]";

    if (stats.bench_summary.has_value())
    {
        const BenchSummary& bench = stats.bench_summary.value();
        output << ",\n  \"bench\": {\n"
               << "    \"runs\": " << bench.runs << ",\n"
               << "    \"min_microseconds\": " << bench.min_microseconds << ",\n"
               << "    \"max_microseconds\": " << bench.max_microseconds << ",\n"
               << "    \"mean_microseconds\": " << bench.mean_microseconds << ",\n"
               << "    \"median_microseconds\": " << bench.median_microseconds << ",\n"
               << "    \"stddev_microseconds\": " << bench.stddev_microseconds << "\n"
               << "  }";
    }

    output << "\n}\n";
}

} // namespace transformer
