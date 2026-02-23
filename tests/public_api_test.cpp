#include <cstdlib>
#include <iostream>
#include <string>

#include <lightpath/lightpath.hpp>

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const lightpath::Color& color) {
    return color.r > 0 || color.g > 0 || color.b > 0;
}

}  // namespace

int main() {
    std::srand(11);

    if (lightpath::kVersionMajor < 1) {
        return fail("Version major must be >= 1");
    }
    if (std::string(lightpath::kVersionString).empty()) {
        return fail("Version string must not be empty");
    }

    lightpath::EngineConfig config;
    config.object_type = lightpath::ObjectType::Line;
    config.pixel_count = 64;
    lightpath::Engine engine(config);

    if (engine.pixelCount() != 64) {
        return fail("Engine did not respect configured pixel count");
    }

    lightpath::EmitCommand invalid;
    invalid.model = 99;
    const auto invalid_result = engine.emit(invalid);
    if (invalid_result.ok() || invalid_result.status().code() != lightpath::ErrorCode::InvalidModel) {
        return fail("Invalid model emit did not return ErrorCode::InvalidModel");
    }
    if (invalid_result.status().message().empty()) {
        return fail("Invalid model emit did not return an explanatory message");
    }

    lightpath::EmitCommand invalid_brightness;
    invalid_brightness.model = 0;
    invalid_brightness.min_brightness = 220;
    invalid_brightness.max_brightness = 120;
    const auto invalid_brightness_result = engine.emit(invalid_brightness);
    if (invalid_brightness_result.ok() ||
        invalid_brightness_result.status().code() != lightpath::ErrorCode::InvalidArgument) {
        return fail("Invalid brightness bounds did not return ErrorCode::InvalidArgument");
    }
    if (invalid_brightness_result.status().message().empty()) {
        return fail("Invalid brightness emit did not return an explanatory message");
    }

    lightpath::EmitCommand command;
    command.model = 0;
    command.speed = 1.0f;
    command.length = 5;
    command.color = 0x22AA44;
    command.note_id = 7;

    const auto emit_result = engine.emit(command);
    if (!emit_result) {
        return fail("Valid emit command failed");
    }

    for (int frame = 0; frame < 32; ++frame) {
        engine.tick(16);
    }

    int lit_pixels = 0;
    for (uint16_t i = 0; i < engine.pixelCount(); ++i) {
        const auto pixel = engine.pixel(i);
        if (!pixel) {
            return fail("pixel() failed unexpectedly for valid index");
        }
        if (isNonBlack(pixel.value())) {
            ++lit_pixels;
        }
    }
    if (lit_pixels == 0) {
        return fail("Engine produced no visible pixels");
    }

    const auto out_of_range = engine.pixel(engine.pixelCount());
    if (out_of_range.ok() || out_of_range.status().code() != lightpath::ErrorCode::OutOfRange) {
        return fail("Out-of-range pixel access did not return ErrorCode::OutOfRange");
    }

    engine.stopAll();
    for (int frame = 0; frame < 24; ++frame) {
        engine.tick(16);
    }

    return 0;
}
