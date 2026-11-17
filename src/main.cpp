#include "app/Application.hpp"

#include <exception>
#include <iostream>
#include <span>
#include <string_view>

namespace {

constexpr std::string_view usage {
    "usage: blackhole [--validate]\n"
    "  (no option)   interactive renderer\n"
    "  --validate    trace fixed scenes on the GPU and with the CPU reference, compare, exit 0 if they agree\n"};

} // namespace

int main(const int argc, const char* const* const argv)
{
    // argv is the C entry point's array; a span is the bounds-checked way in.
    const std::span<const char* const> arguments {argv, static_cast<std::size_t>(argc)};
    try {
        if (arguments.size() == 2 && std::string_view {arguments[1]} == "--validate") {
            bh::Application app {bh::ApplicationOptions {.hiddenWindow = true}};
            return app.validate();
        }
        if (arguments.size() > 1) {
            std::cerr << usage;
            return 2;
        }
        bh::Application app {};
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
