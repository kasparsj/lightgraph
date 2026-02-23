#include <chrono>
#include <cstdint>
#include <iostream>

#include <lightpath/lightpath.hpp>

namespace {

using clock_type = std::chrono::high_resolution_clock;

void runEmitTickBenchmark() {
    lightpath::EngineConfig config;
    config.object_type = lightpath::ObjectType::Line;
    config.pixel_count = 180;

    lightpath::Engine engine(config);

    constexpr int kFrames = 5000;
    int emit_success = 0;

    const auto start = clock_type::now();
    for (int frame = 0; frame < kFrames; ++frame) {
        lightpath::EmitCommand command;
        command.model = 0;
        command.speed = 1.0f + static_cast<float>(frame % 8);
        command.length = static_cast<uint16_t>(4 + (frame % 12));
        command.note_id = static_cast<uint16_t>(frame % 16);
        command.color = static_cast<uint32_t>(0x102030 + (frame % 0x00FFFF));

        const auto emit_result = engine.emit(command);
        if (emit_result) {
            ++emit_success;
        }

        engine.tick(16);
        static_cast<void>(engine.pixel(static_cast<uint16_t>(frame % engine.pixelCount())));
    }
    const auto end = clock_type::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    const double fps = elapsed_ms > 0
                           ? static_cast<double>(kFrames) / (static_cast<double>(elapsed_ms) / 1000.0)
                           : 0.0;

    std::cout << "Benchmark emit+tick frames: " << kFrames << "\n";
    std::cout << "Benchmark successful emits: " << emit_success << "\n";
    std::cout << "Benchmark elapsed (ms): " << elapsed_ms << "\n";
    std::cout << "Benchmark approx frames/sec: " << fps << "\n";
}

}  // namespace

int main() {
    runEmitTickBenchmark();
    return 0;
}
