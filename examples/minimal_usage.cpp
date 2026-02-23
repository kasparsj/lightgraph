#include <cstdint>
#include <iostream>

#include <lightpath/lightpath.hpp>

int main() {
    auto object = lightpath::makeObject(lightpath::BuiltinObjectType::Line, lightpath::kLinePixelCount);
    lightpath::Engine engine(std::move(object));

    lightpath::EmitParams params(0, 1.0f, 0x33CC99);
    params.setLength(6);
    if (engine.state().emit(params) < 0) {
        std::cerr << "Failed to emit a light list" << std::endl;
        return 1;
    }

    lightpath::millis() = 0;
    for (uint8_t frame = 0; frame < 8; ++frame) {
        lightpath::millis() += 16;
        engine.update(lightpath::millis());
    }

    const lightpath::Color pixel = engine.state().getPixel(0);
    std::cout << "Pixel(0): "
              << static_cast<int>(pixel.R) << ","
              << static_cast<int>(pixel.G) << ","
              << static_cast<int>(pixel.B) << std::endl;

    return 0;
}
