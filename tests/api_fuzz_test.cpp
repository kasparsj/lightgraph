#include <cstdlib>
#include <iostream>
#include <string>

#include <lightpath/lightpath.hpp>

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

} // namespace

int main() {
    std::srand(1234);

    lightpath::EngineConfig config;
    config.object_type = lightpath::ObjectType::Line;
    config.pixel_count = 180;

    lightpath::Engine engine(config);

    for (int i = 0; i < 600; ++i) {
        lightpath::EmitCommand command;
        command.model = (std::rand() % 20 == 0) ? 42 : 0;
        command.speed = 0.5f + (std::rand() % 80) / 10.0f;
        command.length = static_cast<uint16_t>(1 + (std::rand() % 24));
        command.color = static_cast<uint32_t>(std::rand() & 0xFFFFFF);
        command.note_id = static_cast<uint16_t>(std::rand() % 16);
        command.duration_ms = static_cast<uint32_t>(200 + (std::rand() % 2000));
        command.trail = static_cast<uint16_t>(std::rand() % 8);

        const auto emit_result = engine.emit(command);
        if (command.model == 42) {
            if (emit_result.ok() ||
                emit_result.status().code() != lightpath::ErrorCode::InvalidModel) {
                return fail("Invalid model in fuzz loop did not return ErrorCode::InvalidModel");
            }
        }

        engine.tick(16);

        const uint16_t sample_pixel = static_cast<uint16_t>(std::rand() % engine.pixelCount());
        const auto pixel = engine.pixel(sample_pixel);
        if (!pixel) {
            return fail("pixel() failed for in-range index during fuzz loop");
        }
    }

    return 0;
}
