#include <lightgraph/lightgraph.hpp>

int main() {
    lightgraph::Engine engine;

    lightgraph::EmitCommand emit;
    emit.model = 0;
    emit.length = 1;

    const auto emit_result = engine.emit(emit);
    if (!emit_result) {
        return 1;
    }

    engine.tick(16);
    const auto pixel = engine.pixel(0);

    return pixel ? 0 : 1;
}
