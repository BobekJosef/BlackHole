#include "gfx/ShaderPreprocessor.hpp"

#include <charconv>
#include <format>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>
#include <utility>

namespace bh::gl {

namespace {

constexpr int maxIncludeDepth = 16;
constexpr std::string_view includeDirective = "#include";
constexpr std::string_view includeExtension = "#extension GL_GOOGLE_include_directive";

[[nodiscard]] std::expected<std::string, std::string> readFile(const std::filesystem::path& path)
{
    const std::ifstream file {path, std::ios::binary};
    if (!file) {
        return std::unexpected {"cannot open " + path.string()};
    }
    std::ostringstream contents {};
    contents << file.rdbuf();
    return contents.str();
}

[[nodiscard]] std::string_view trimLeft(const std::string_view text)
{
    const auto first {text.find_first_not_of(" \t")};
    return first == std::string_view::npos ? std::string_view {} : text.substr(first);
}

// The file name between the quotes of `#include "name"`, if the line is well-formed.
[[nodiscard]] std::expected<std::string_view, std::string> includedName(const std::string_view directive)
{
    const std::string_view rest {trimLeft(directive.substr(includeDirective.size()))};
    const auto close {rest.find('"', 1)};
    if (!rest.starts_with('"') || close == std::string_view::npos) {
        return std::unexpected {std::string {"expected #include \"file\""}};
    }
    return rest.substr(1, close - 1);
}

class Preprocessor final {
public:
    [[nodiscard]] std::expected<void, std::string> process(const std::filesystem::path& file, const int depth)
    {
        if (depth > maxIncludeDepth) {
            return std::unexpected {
                std::format("{}: includes nested deeper than {} (a cycle?)", file.string(), maxIncludeDepth)};
        }
        const auto text {readFile(file)};
        if (!text) {
            return std::unexpected {text.error()};
        }
        const std::size_t index {m_result.files.size()};
        m_result.files.push_back(file);

        std::istringstream lines {*text};
        std::string line {};
        int lineNumber {0};
        while (std::getline(lines, line)) {
            ++lineNumber;
            const std::string_view trimmed {trimLeft(line)};
            if (trimmed.starts_with(includeExtension)) {
                m_result.source += '\n'; // keep the line numbering
                continue;
            }
            if (!trimmed.starts_with(includeDirective)) {
                m_result.source += line;
                m_result.source += '\n';
                continue;
            }
            const auto name {includedName(trimmed)};
            if (!name) {
                return std::unexpected {std::format("{}:{}: {}", file.string(), lineNumber, name.error())};
            }
            m_result.source += std::format("#line 1 {}\n", m_result.files.size());
            if (auto included {process(file.parent_path() / *name, depth + 1)}; !included) {
                return std::unexpected {
                    std::format("{}\n  included from {}:{}", included.error(), file.string(), lineNumber)};
            }
            m_result.source += std::format("#line {} {}\n", lineNumber + 1, index);
        }
        return {};
    }

    [[nodiscard]] PreprocessedShader take() { return std::move(m_result); }

private:
    PreprocessedShader m_result {};
};

} // namespace

std::expected<PreprocessedShader, std::string> preprocessShader(const std::filesystem::path& mainFile)
{
    Preprocessor preprocessor {};
    if (auto result {preprocessor.process(mainFile, 0)}; !result) {
        return std::unexpected {std::move(result.error())};
    }
    return preprocessor.take();
}

std::string annotateShaderLog(const std::string_view log, const std::span<const std::filesystem::path> files)
{
    // Mesa "0:12(5): ...", NVIDIA "0(12) : ...", AMD/Intel/glslang "ERROR: 0:12: ...".
    static const std::regex location {R"(^((?:ERROR|WARNING): )?(\d+)(?::(\d+)|\((\d+)\)))"};

    // Names relative to the main file's directory, e.g. "common/disk.glsl".
    const std::filesystem::path shaderRoot {files.empty() ? std::filesystem::path {} : files.front().parent_path()};
    std::string annotated {};
    std::istringstream lines {std::string {log}};
    std::string line {};
    while (std::getline(lines, line)) {
        std::smatch match {};
        std::size_t fileIndex {};
        const auto isKnownFile = [&] {
            const std::string digits {match[2].str()};
            return std::from_chars(digits.data(), digits.data() + digits.size(), fileIndex).ec == std::errc {}
            && fileIndex < files.size();
        };
        if (std::regex_search(line, match, location) && isKnownFile()) {
            const std::string lineNumber {match[3].matched ? match[3].str() : match[4].str()};
            const std::filesystem::path name {files[fileIndex].lexically_relative(shaderRoot)};
            annotated +=
                std::format("{}{}:{}{}\n", match[1].str(), name.generic_string(), lineNumber, match.suffix().str());
        } else {
            annotated += line;
            annotated += '\n';
        }
    }
    return annotated;
}

} // namespace bh::gl
