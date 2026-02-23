#include <cstdint>
#include <iostream>

#include <lightpath/lightpath.hpp>

int main() {
    lightpath::EngineConfig config;
    config.object_type = lightpath::ObjectType::Line;
    config.pixel_count = 64;

    lightpath::Engine engine(config);

    lightpath::EmitCommand command;
    command.model = 0;
    command.speed = 1.0f;
    command.length = 6;
    command.color = 0x33CC99;

    if (!engine.emit(command)) {
        std::cerr << "Failed to emit a light list" << std::endl;
        return 1;
    }

    for (uint8_t frame = 0; frame < 8; ++frame) {
        engine.tick(16);
    }

    const auto pixel_result = engine.pixel(0);
    if (!pixel_result) {
        std::cerr << "Failed to fetch pixel 0" << std::endl;
        return 1;
    }

    const lightpath::Color pixel = pixel_result.value();
    std::cout << "Pixel(0): " << static_cast<int>(pixel.r) << "," << static_cast<int>(pixel.g)
              << "," << static_cast<int>(pixel.b) << std::endl;

    return 0;
}
