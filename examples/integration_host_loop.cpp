#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>

#include <lightpath/integration.hpp>

namespace lp = lightpath::integration;

namespace {

lp::EmitParams makeGradientParams(int8_t model, uint16_t note_id, float speed) {
    lp::EmitParams params(model, speed);
    params.noteId = note_id;
    params.setLength(12);
    params.duration = lp::EmitParams::frameMs() * 180;
    params.linked = true;
    params.trail = 2;

    std::vector<int64_t> colors = {0x0044FF, 0x22CC66, 0xFFAA00};
    std::vector<float> positions = {0.0f, 0.45f, 1.0f};
    params.setColors(colors);
    params.setColorPositions(positions);
    return params;
}

int runLoop(lp::Object& object, lp::RuntimeState& state, lp::EmitParams& params, uint16_t stride) {
    const int8_t emit_index = state.emit(params);
    if (emit_index < 0) {
        return 0;
    }

    int non_black_samples = 0;
    const uint16_t pixel_count = object.pixelCount;

    for (uint16_t frame = 0; frame < 180; ++frame) {
        state.update();
        const uint16_t sample_index = static_cast<uint16_t>((frame * stride) % pixel_count);
        const ColorRGB color = state.getPixel(sample_index, 255);
        if (color.R > 0 || color.G > 0 || color.B > 0) {
            ++non_black_samples;
        }
    }

    return non_black_samples;
}

} // namespace

int main() {
    auto line = lp::makeObject(lp::BuiltinObjectType::Line, 144);
    auto cross = lp::makeObject(lp::BuiltinObjectType::Cross, 180);

    lp::RuntimeState line_state(*line);
    lp::RuntimeState cross_state(*cross);

    lp::EmitParams line_params = makeGradientParams(0, 1, 1.5f);
    lp::EmitParams cross_params = makeGradientParams(0, 2, 0.8f);
    cross_params.trail = 4;
    cross_params.emitOffset = 1;

    const int line_samples = runLoop(*line, line_state, line_params, 3);
    const int cross_samples = runLoop(*cross, cross_state, cross_params, 5);

    std::cout << "line non-black samples: " << line_samples << "\n";
    std::cout << "cross non-black samples: " << cross_samples << "\n";

    return (line_samples > 0 && cross_samples > 0) ? 0 : 1;
}
