#include <iostream>
#include <memory>
#include "lightgraph/integration.hpp"
#include "lightgraph/integration/drawing_presentation.hpp"

using namespace lightgraph::integration;
namespace {
bool deny = false;
bool admission(lightgraph::memory::Operation operation, const lightgraph::memory::Estimate&,
               void*) noexcept {
    return !deny || (operation != lightgraph::memory::Operation::DrawingCache &&
                     operation != lightgraph::memory::Operation::Geometry);
}
#define CHECK(value)                                                                               \
    do {                                                                                           \
        if (!(value)) {                                                                            \
            std::cerr << "failed line " << __LINE__ << '\n';                                       \
            return 1;                                                                              \
        }                                                                                          \
    } while (false)
} // namespace
int main() {
    Heptagon919 object;
    object.runtimeContext().memoryAdmission = {admission, nullptr};
    auto state = std::make_unique<RuntimeState>(object);
    PendingDrawingPresentation pending;
    pending.bind(state->drawing());
    DrawingSession session(state->drawing());
    const std::uint8_t pixels[] = {0x11, 0x22, 0x33};
    DrawingHandle image, first, second;
    DrawingScene scene;
    scene.rgb1 = 0xabc123;
    CHECK(state->drawing().queueScene(scene, first) == DrawingResult::Applied);
    scene.rgb1 = 0x123abc;
    CHECK(state->drawing().queueScene(scene, second) == DrawingResult::Applied);
    CHECK(session.imageChunk(1, 0, 1, 1, 1, 0, pixels, sizeof(pixels), &image) ==
          DrawingResult::Applied);
    CHECK(session.imageCommit(1, 0, 1, &image) == DrawingResult::Applied);
    auto* anchor = object.inter[1][0];
    TopologyIntersectionUpdate update;
    update.numPorts = anchor->numPorts;
    update.topPixel = anchor->topPixel + 1;
    update.bottomPixel = anchor->bottomPixel;
    update.group = anchor->group;
    update.allowEndOfLife = anchor->allowEndOfLife;
    update.allowEmit = anchor->allowEmit;
    CHECK(object.updateIntersection(anchor, update));
    deny = true;
    CHECK(pending.enqueue(first));
    CHECK(pending.enqueue(first));
    CHECK(pending.size() == 1);
    CHECK(pending.enqueue(second));
    CHECK(pending.enqueue(image));
    CHECK(pending.size() == 3);
    CHECK(pending.poll() == DrawingResult::Busy);
    CHECK(pending.poll() == DrawingResult::Busy);
    CHECK(pending.size() == 3);
    CHECK(state->drawing().scene().rgb1 == 0);
    // A retired slot must not produce false queue-full rejection before the next poll.
    state->drawing().releaseQueued(second);
    deny = false;
    DrawingHandle replacement;
    CHECK(state->drawing().queueScene(scene, replacement) == DrawingResult::Applied);
    CHECK(pending.enqueue(replacement));
    CHECK(pending.size() == 3);
    CHECK(pending.poll() == DrawingResult::Applied);
    CHECK(pending.size() == 0);
    CHECK(state->drawing().scene().rgb1 == 0x123abc);
    CHECK(state->drawing().status().displayedFrameId == 1);
    CHECK(session.imageChunk(1, 10, 2, 1, 1, 0, pixels, sizeof(pixels), &image) ==
          DrawingResult::Applied);
    CHECK(session.imageCommit(1, 10, 2, &image) == DrawingResult::Applied);
    CHECK(pending.enqueue(image));
    CHECK(pending.poll() == DrawingResult::NoChange);
    CHECK(state->drawing().status().displayedFrameId == 2);
    CHECK(state->drawing().queueScene(scene, first) == DrawingResult::Applied);
    deny = true;
    CHECK(object.updateIntersection(anchor, update));
    CHECK(pending.enqueue(first));
    CHECK(state->setupBgChecked(0));
    CHECK(pending.poll() == DrawingResult::Stale);
    CHECK(pending.size() == 0);
    deny = false;
    pending.bind(state->drawing());
    CHECK(state->drawing().queueScene(scene, first) == DrawingResult::Applied);
    deny = true;
    CHECK(object.updateIntersection(anchor, update));
    CHECK(pending.enqueue(first));
    state.reset();
    CHECK(pending.poll() == DrawingResult::Stale);
    deny = false;
    state = std::make_unique<RuntimeState>(object);
    pending.bind(state->drawing());
    CHECK(state->drawing().queueScene(scene, first) == DrawingResult::Applied);
    CHECK(pending.enqueue(first));
    pending.reset();
    CHECK(state->drawing().queueScene(scene, first) == DrawingResult::Applied);
    state->drawing().releaseQueued(first);
    std::cout << "Pending presentation FIFO recovery/lifetime tests passed\n";
}
