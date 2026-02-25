#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>

#include <lightgraph/lightgraph.hpp>

namespace {

using clock_type = std::chrono::high_resolution_clock;

struct BenchmarkScenario {
    const char* name;
    lightgraph::ObjectType object_type;
    uint16_t pixel_count;
    int frames;
    int emits_per_frame;
};

double runScenario(const BenchmarkScenario& scenario) {
    lightgraph::EngineConfig config;
    config.object_type = scenario.object_type;
    config.pixel_count = scenario.pixel_count;

    lightgraph::Engine engine(config);
    int emit_success = 0;

    const auto start = clock_type::now();
    for (int frame = 0; frame < scenario.frames; ++frame) {
        for (int emit_idx = 0; emit_idx < scenario.emits_per_frame; ++emit_idx) {
            lightgraph::EmitCommand command;
            command.model = 0;
            command.speed = 1.0f + static_cast<float>((frame + emit_idx) % 8);
            command.length = static_cast<uint16_t>(4 + ((frame + emit_idx) % 12));
            command.note_id = static_cast<uint16_t>((frame + emit_idx) % 64);
            command.color = static_cast<uint32_t>(0x102030 + ((frame + emit_idx) % 0x00FFFF));

            const auto emit_result = engine.emit(command);
            if (emit_result) {
                ++emit_success;
            }
        }

        engine.tick(16);
        static_cast<void>(engine.pixel(static_cast<uint16_t>(frame % engine.pixelCount())));
    }
    const auto end = clock_type::now();

    const auto elapsed_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    const double fps = elapsed_ms > 0
                           ? static_cast<double>(scenario.frames) /
                                 (static_cast<double>(elapsed_ms) / 1000.0)
                           : 0.0;

    std::cout << "Benchmark scenario: " << scenario.name << "\n";
    std::cout << "Benchmark frames: " << scenario.frames << "\n";
    std::cout << "Benchmark emits/frame: " << scenario.emits_per_frame << "\n";
    std::cout << "Benchmark successful emits: " << emit_success << "\n";
    std::cout << "Benchmark elapsed (ms): " << elapsed_ms << "\n";
    std::cout << "Benchmark approx frames/sec: " << fps << "\n";

    return fps;
}

} // namespace

int main() {
    const std::array<BenchmarkScenario, 3> scenarios = {
        BenchmarkScenario{"line-180-single-emit", lightgraph::ObjectType::Line, 180, 5000, 1},
        BenchmarkScenario{"triangle-512-double-emit", lightgraph::ObjectType::Triangle, 512, 3500, 2},
        BenchmarkScenario{"heptagon919-single-emit", lightgraph::ObjectType::Heptagon919, 0, 2500, 1},
    };

    double min_fps = std::numeric_limits<double>::max();
    for (const BenchmarkScenario& scenario : scenarios) {
        const double fps = runScenario(scenario);
        min_fps = std::min(min_fps, fps);
    }

    if (min_fps == std::numeric_limits<double>::max()) {
        min_fps = 0.0;
    }

    std::cout << "Benchmark minimum frames/sec: " << min_fps << "\n";
    return 0;
}
