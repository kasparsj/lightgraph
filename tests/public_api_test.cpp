#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

#include <lightgraph/lightgraph.hpp>

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const lightgraph::Color& color) {
    return color.r > 0 || color.g > 0 || color.b > 0;
}

} // namespace

int main() {
    std::srand(11);

    if (lightgraph::kVersionMajor < 1) {
        return fail("Version major must be >= 1");
    }
    if (std::string(lightgraph::kVersionString).empty()) {
        return fail("Version string must not be empty");
    }

    // Existing aggregate initialization remains source-compatible: new centered
    // controls are appended after all legacy fields.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
    lightgraph::EmitCommand legacy_aggregate{0,
                                             1.0f,
                                             std::optional<uint16_t>(3),
                                             0,
                                             std::optional<uint32_t>(0xFFFFFF),
                                             42,
                                             0,
                                             255,
                                             0,
                                             0,
                                             0,
                                             1000,
                                             -1,
                                             true};
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
    if (legacy_aggregate.trail != 0 || legacy_aggregate.note_id != 42 ||
        legacy_aggregate.length_mode != lightgraph::LengthMode::Legacy) {
        return fail("legacy EmitCommand aggregate initialization changed meaning");
    }

    lightgraph::EngineConfig config;
    config.object_type = lightgraph::ObjectType::Line;
    config.pixel_count = 64;
    lightgraph::Engine engine(config);

    if (!engine.isOn()) {
        return fail("Engine should start with runtime output enabled");
    }

    if (engine.pixelCount() != 64) {
        return fail("Engine did not respect configured pixel count");
    }

    engine.tick(16);
    for (uint16_t i = 0; i < engine.pixelCount(); ++i) {
        const auto pixel = engine.pixel(i);
        if (!pixel) {
            return fail("pixel() failed unexpectedly before any emit");
        }
        if (isNonBlack(pixel.value())) {
            return fail("Fresh engine should not render visible pixels before emit");
        }
    }

    lightgraph::EmitCommand invalid;
    invalid.model = 99;
    const auto invalid_result = engine.emit(invalid);
    if (invalid_result.ok() ||
        invalid_result.status().code() != lightgraph::ErrorCode::InvalidModel) {
        return fail("Invalid model emit did not return ErrorCode::InvalidModel");
    }
    if (invalid_result.status().message().empty()) {
        return fail("Invalid model emit did not return an explanatory message");
    }

    lightgraph::EmitCommand invalid_brightness;
    invalid_brightness.model = 0;
    invalid_brightness.min_brightness = 220;
    invalid_brightness.max_brightness = 120;
    const auto invalid_brightness_result = engine.emit(invalid_brightness);
    if (invalid_brightness_result.ok() ||
        invalid_brightness_result.status().code() != lightgraph::ErrorCode::InvalidArgument) {
        return fail("Invalid brightness bounds did not return ErrorCode::InvalidArgument");
    }
    if (invalid_brightness_result.status().message().empty()) {
        return fail("Invalid brightness emit did not return an explanatory message");
    }

    lightgraph::EmitCommand invalid_visible_length;
    invalid_visible_length.model = 0;
    invalid_visible_length.visible_length = std::numeric_limits<float>::quiet_NaN();
    const auto invalid_visible_length_result = engine.emit(invalid_visible_length);
    if (invalid_visible_length_result.ok() ||
        invalid_visible_length_result.status().code() != lightgraph::ErrorCode::InvalidArgument) {
        return fail("Non-finite legacy visible length did not return ErrorCode::InvalidArgument");
    }

    lightgraph::EmitCommand command;
    command.model = 0;
    command.speed = 1.0f;
    command.length = 5;
    command.color = 0x22AA44;
    command.note_id = 7;

    const auto emit_result = engine.emit(command);
    if (!emit_result) {
        return fail("Valid emit command failed");
    }

    lightgraph::EmitCommand centered;
    centered.model = 0;
    centered.speed = 0.5f;
    centered.length = 8;
    centered.length_mode = lightgraph::LengthMode::Centered;
    centered.visible_length = 4.0f;
    centered.color = 0xFFFFFF;
    centered.note_id = 8;
    const auto centered_result = engine.emit(centered);
    if (!centered_result) {
        return fail("Valid centered emit command failed");
    }
    const lightgraph::ListLengthUpdate resize[] = {{8, 2.5f}, {60000, 1.0f}};
    if (!engine.setListLengths(resize, 2)) {
        return fail("Valid centered list-length batch failed");
    }

    {
        lightgraph::Engine capacity_engine(config);
        lightgraph::EmitCommand full;
        full.model = 0;
        full.speed = 0.5f;
        full.length = 1500;
        full.note_id = 77;
        full.length_mode = lightgraph::LengthMode::Centered;
        if (!capacity_engine.emit(full) || !capacity_engine.emit(full)) {
            return fail("same-note replacement at MAX_TOTAL_LIGHTS should succeed");
        }
    }
    const lightgraph::ListLengthUpdate invalid_resize[] = {{8, 1.0f}, {8, 2.0f}};
    if (engine.setListLengths(invalid_resize, 2).ok()) {
        return fail("Duplicate centered list IDs should reject the whole batch");
    }
    lightgraph::ListLengthUpdate too_many[20]{};
    for (uint16_t i = 0; i < 20; ++i) {
        too_many[i].note_id = static_cast<uint16_t>(100 + i);
        too_many[i].visible_length = 1.0f;
    }
    if (engine.setListLengths(too_many, 20).ok()) {
        return fail("List-length batches larger than 19 should be rejected");
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

    engine.setOn(false);
    if (engine.isOn()) {
        return fail("Engine should report output disabled after setOn(false)");
    }
    for (uint16_t i = 0; i < engine.pixelCount(); ++i) {
        const auto pixel = engine.pixel(i);
        if (!pixel) {
            return fail("pixel() failed unexpectedly while output was disabled");
        }
        if (isNonBlack(pixel.value())) {
            return fail("Disabled engine should not report visible pixels");
        }
    }
    engine.setOn(true);
    if (!engine.isOn()) {
        return fail("Engine should report output enabled after setOn(true)");
    }
    int resumed_lit_pixels = 0;
    for (uint16_t i = 0; i < engine.pixelCount(); ++i) {
        const auto pixel = engine.pixel(i);
        if (!pixel) {
            return fail("pixel() failed unexpectedly after re-enabling output");
        }
        if (isNonBlack(pixel.value())) {
            ++resumed_lit_pixels;
        }
    }
    if (resumed_lit_pixels == 0) {
        return fail("Re-enabled engine should expose the current rendered frame");
    }

    lightgraph::EngineConfig auto_config;
    auto_config.object_type = lightgraph::ObjectType::Line;
    auto_config.pixel_count = 32;
    auto_config.auto_emit = true;
    lightgraph::Engine auto_engine(auto_config);
    if (!auto_engine.autoEmitEnabled()) {
        return fail("Engine should honor initial auto_emit configuration");
    }
    auto_engine.setOn(false);
    if (!auto_engine.autoEmitEnabled()) {
        return fail("setOn(false) should not change auto-emit state");
    }
    auto_engine.setOn(true);
    if (!auto_engine.autoEmitEnabled()) {
        return fail("setOn(true) should preserve auto-emit state");
    }

    const auto out_of_range = engine.pixel(engine.pixelCount());
    if (out_of_range.ok() || out_of_range.status().code() != lightgraph::ErrorCode::OutOfRange) {
        return fail("Out-of-range pixel access did not return ErrorCode::OutOfRange");
    }

    engine.stopAll();
    for (int frame = 0; frame < 24; ++frame) {
        engine.tick(16);
    }

    return 0;
}
