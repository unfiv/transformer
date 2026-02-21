#include "io/json_readers.hpp"

#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace transformer
{

namespace
{

class JsonLiteParser
{
public:
    explicit JsonLiteParser(std::string text)
        : text_(std::move(text))
    {
    }

    SkinningData parse_weights()
    {
        expect_char('{');
        expect_key("vertices");
        expect_char(':');
        expect_char('[');

        SkinningData data;
        skip_ws();
        while (!consume_char(']'))
        {
            VertexInfluence influence;
            expect_char('{');
            expect_key("bone_indices");
            expect_char(':');
            parse_int_array(influence.bone_indices);
            expect_char(',');
            expect_key("weights");
            expect_char(':');
            parse_float_array(influence.weights);
            expect_char('}');

            data.influences.push_back(influence);
            skip_ws();
            consume_char(',');
            skip_ws();
        }
        expect_char('}');
        return data;
    }

    std::vector<Mat4> parse_bones()
    {
        expect_char('{');
        expect_key("bones");
        expect_char(':');
        expect_char('[');

        std::vector<Mat4> bones;
        skip_ws();
        while (!consume_char(']'))
        {
            Mat4 matrix{};
            expect_char('{');
            expect_key("matrix");
            expect_char(':');
            expect_char('[');
            for (std::size_t i = 0; i < 16; ++i)
            {
                matrix.m[i] = parse_float();
                if (i < 15)
                {
                    expect_char(',');
                }
            }
            expect_char(']');
            expect_char('}');
            bones.push_back(matrix);

            skip_ws();
            consume_char(',');
            skip_ws();
        }
        expect_char('}');
        return bones;
    }

private:
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

    std::string parse_string()
    {
        expect_char('"');
        std::string result;
        while (position_ < text_.size() && text_[position_] != '"')
        {
            result.push_back(text_[position_]);
            ++position_;
        }
        expect_char('"');
        return result;
    }

    void expect_key(const std::string& key)
    {
        const std::string actual = parse_string();
        if (actual != key)
        {
            throw std::runtime_error("JSON parse error: expected key " + key + ", got " + actual);
        }
    }

    float parse_float()
    {
        skip_ws();
        const std::size_t start = position_;
        while (position_ < text_.size())
        {
            const char ch = text_[position_];
            if (std::isdigit(static_cast<unsigned char>(ch)) != 0 || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E')
            {
                ++position_;
                continue;
            }
            break;
        }

        if (start == position_)
        {
            throw std::runtime_error("JSON parse error: expected float value");
        }

        return std::stof(text_.substr(start, position_ - start));
    }

    std::int32_t parse_int()
    {
        return static_cast<std::int32_t>(parse_float());
    }

    template <std::size_t N>
    void parse_int_array(std::array<std::int32_t, N>& values)
    {
        values.fill(-1);
        expect_char('[');

        std::size_t i = 0;
        skip_ws();
        while (!consume_char(']'))
        {
            if (i < N)
            {
                values[i] = parse_int();
            }
            else
            {
                (void)parse_int();
            }
            ++i;
            skip_ws();
            consume_char(',');
            skip_ws();
        }
    }

    template <std::size_t N>
    void parse_float_array(std::array<float, N>& values)
    {
        values.fill(0.0F);
        expect_char('[');

        std::size_t i = 0;
        skip_ws();
        while (!consume_char(']'))
        {
            if (i < N)
            {
                values[i] = parse_float();
            }
            else
            {
                (void)parse_float();
            }
            ++i;
            skip_ws();
            consume_char(',');
            skip_ws();
        }
    }

    std::string text_;
    std::size_t position_ = 0;
};

std::string read_file_to_string(const std::string& path)
{
    std::ifstream input(path);
    if (!input)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::ostringstream ss;
    ss << input.rdbuf();
    return ss.str();
}

} // namespace

SkinningData JsonWeightsReader::read(const std::string& weights_file, Profiler& profiler) const
{
    const auto scope = profiler.stage("read_weights_json");
    return JsonLiteParser(read_file_to_string(weights_file)).parse_weights();
}

std::vector<Mat4> JsonBonePoseReader::read_matrices(const std::string& file_path, Profiler& profiler,
                                                    const std::string& stage_name) const
{
    const auto scope = profiler.stage(stage_name);
    return JsonLiteParser(read_file_to_string(file_path)).parse_bones();
}

void JsonStatsWriter::write(const std::string& output_file, const std::vector<TimingEntry>& timings) const
{
    std::ofstream output(output_file);
    if (!output)
    {
        throw std::runtime_error("Failed to open stats output file: " + output_file);
    }

    output << "{\n  \"stages\": [\n";
    for (std::size_t i = 0; i < timings.size(); ++i)
    {
        output << "    { \"stage\": \"" << timings[i].stage << "\", \"milliseconds\": " << timings[i].milliseconds << " }";
        if (i + 1 < timings.size())
        {
            output << ',';
        }
        output << '\n';
    }
    output << "  ]\n}\n";
}

} // namespace transformer
