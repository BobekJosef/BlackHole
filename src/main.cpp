#include "app/Application.hpp"

#include <exception>
#include <iostream>

int main()
{
    try {
        bh::Application app {};
        app.run();
    } catch (const std::exception& error) {
        std::cerr << "fatal: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
