// Checks the GLSL #include preprocessor: inlining, #line bookkeeping, error paths and the
// mapping of driver log locations back to file names.

#include "gfx/ShaderPreprocessor.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <source_location>
#include <string>
#include <string_view>

namespace {

int g_failures {0};

void expect(const bool condition, const std::string_view what,
            const std::source_location where = std::source_location::current())
{
    if (!condition) {
        ++g_failures;
        std::cerr << where.file_name() << ':' << where.line() << ": " << what << '\n';
    }
}

void writeFile(const std::filesystem::path& path, const std::string_view contents)
{
    std::filesystem::create_directories(path.parent_path());
    std::ofstream {path} << contents;
}

[[nodiscard]] bool contains(const std::string_view text, const std::string_view part)
{
    return text.find(part) != std::string_view::npos;
}

} // namespace

int main()
{
    const std::filesystem::path root {std::filesystem::temp_directory_path() / "blackhole_test_shader_preprocessor"};
    std::filesystem::remove_all(root);

    writeFile(root / "main.comp", "#version 430 core\n"
                                  "#extension GL_GOOGLE_include_directive : enable\n"
                                  "#include \"common/a.glsl\"\n"
                                  "void main() {}\n");
    writeFile(root / "common/a.glsl", "#include \"b.glsl\"\n"
                                      "float a() { return b(); }\n");
    writeFile(root / "common/b.glsl", "float b() { return 1.0; }\n");

    const auto result {bh::gl::preprocessShader(root / "main.comp")};
    expect(result.has_value(), "preprocessing succeeds");
    if (result) {
        const std::string expected {"#version 430 core\n"
                                    "\n"
                                    "#line 1 1\n"
                                    "#line 1 2\n"
                                    "float b() { return 1.0; }\n"
                                    "#line 2 1\n"
                                    "float a() { return b(); }\n"
                                    "#line 4 0\n"
                                    "void main() {}\n"};
        expect(result->source == expected, "inlined source with #line directives:\n" + result->source);
        expect(result->files.size() == 3, "three source strings");

        const std::string log {bh::gl::annotateShaderLog("1:2(5): error: Mesa style\n"
                                                         "2(1) : error C0000: NVIDIA style\n"
                                                         "ERROR: 0:4: 'x' : glslang style\n"
                                                         "7:1(1): unknown file stays\n",
                                                         result->files)};
        expect(contains(log, "common/a.glsl:2(5): error: Mesa style"), "Mesa location annotated:\n" + log);
        expect(contains(log, "common/b.glsl:1 : error C0000"), "NVIDIA location annotated:\n" + log);
        expect(contains(log, "ERROR: main.comp:4: 'x'"), "glslang location annotated:\n" + log);
        expect(contains(log, "7:1(1): unknown file stays"), "out-of-range index untouched:\n" + log);
    }

    writeFile(root / "missing.comp", "#version 430 core\n#include \"nope.glsl\"\n");
    const auto missing {bh::gl::preprocessShader(root / "missing.comp")};
    expect(!missing && contains(missing.error(), "nope.glsl") && contains(missing.error(), "missing.comp:2"),
           "missing include reports both files");

    writeFile(root / "cycle.glsl", "#include \"cycle.glsl\"\n");
    const auto cycle {bh::gl::preprocessShader(root / "cycle.glsl")};
    expect(!cycle && contains(cycle.error(), "cycle"), "include cycle is reported");

    writeFile(root / "malformed.comp", "#include <angle.glsl>\n");
    const auto malformed {bh::gl::preprocessShader(root / "malformed.comp")};
    expect(!malformed && contains(malformed.error(), "malformed.comp:1"), "malformed #include is reported");

    std::filesystem::remove_all(root);
    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "all shader preprocessor checks passed\n";
    return 0;
}
