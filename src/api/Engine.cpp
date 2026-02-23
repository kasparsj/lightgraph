#include <algorithm>
#include <mutex>
#include <utility>

#include <lightpath/engine.hpp>

#include "../core/Limits.h"
#include "../Globals.h"
#include "../objects/Cross.h"
#include "../objects/Heptagon3024.h"
#include "../objects/Heptagon919.h"
#include "../objects/Line.h"
#include "../objects/Triangle.h"
#include "../runtime/EmitParams.h"
#include "../runtime/State.h"

namespace lightpath {

namespace {

uint16_t defaultPixelCountFor(ObjectType type) {
    switch (type) {
    case ObjectType::Heptagon919:
        return HEPTAGON919_PIXEL_COUNT;
    case ObjectType::Heptagon3024:
        return HEPTAGON3024_PIXEL_COUNT;
    case ObjectType::Line:
        return LINE_PIXEL_COUNT;
    case ObjectType::Cross:
        return CROSS_PIXEL_COUNT;
    case ObjectType::Triangle:
        return TRIANGLE_PIXEL_COUNT;
    }
    return LINE_PIXEL_COUNT;
}

std::unique_ptr<LPObject> makeObject(const EngineConfig& config) {
    const uint16_t pixel_count =
        config.pixel_count > 0 ? config.pixel_count : defaultPixelCountFor(config.object_type);

    switch (config.object_type) {
    case ObjectType::Heptagon919:
        return std::unique_ptr<LPObject>(new Heptagon919());
    case ObjectType::Heptagon3024:
        return std::unique_ptr<LPObject>(new Heptagon3024());
    case ObjectType::Line:
        return std::unique_ptr<LPObject>(new Line(pixel_count));
    case ObjectType::Cross:
        return std::unique_ptr<LPObject>(new Cross(pixel_count));
    case ObjectType::Triangle:
        return std::unique_ptr<LPObject>(new Triangle(pixel_count));
    }

    return std::unique_ptr<LPObject>(new Line(pixel_count));
}

} // namespace

struct Engine::Impl {
    explicit Impl(const EngineConfig& config)
        : object(makeObject(config)), state(*object), now_millis(0) {
        state.autoEnabled = config.auto_emit;
    }

    bool hasFreeListSlot(uint16_t note_id) const {
        if (note_id > 0 && state.findList(static_cast<uint8_t>(note_id)) >= 0) {
            return true;
        }
        for (uint8_t i = 0; i < MAX_LIGHT_LISTS; ++i) {
            if (state.lightLists[i] == nullptr) {
                return true;
            }
        }
        return false;
    }

    std::unique_ptr<LPObject> object;
    State state;
    uint64_t now_millis;
    mutable std::mutex mutex;
};

Engine::Engine(const EngineConfig& config) : impl_(new Impl(config)) {}

Engine::~Engine() = default;

Engine::Engine(Engine&&) noexcept = default;
Engine& Engine::operator=(Engine&&) noexcept = default;

Result<int8_t> Engine::emit(const EmitCommand& command) {
    std::lock_guard<std::mutex> lock(impl_->mutex);

    if (command.max_brightness < command.min_brightness) {
        return Result<int8_t>::error(ErrorCode::InvalidArgument,
                                     "max_brightness must be >= min_brightness");
    }

    const int8_t model_index = command.model;
    if (model_index < 0 || impl_->object->getModel(model_index) == nullptr) {
        return Result<int8_t>::error(ErrorCode::InvalidModel,
                                     "model index is invalid for the current object");
    }

    if (!impl_->hasFreeListSlot(command.note_id)) {
        return Result<int8_t>::error(ErrorCode::NoFreeLightList,
                                     "no free light-list slots are available");
    }

    if (command.length.has_value() &&
        impl_->state.totalLights + *command.length > MAX_TOTAL_LIGHTS) {
        return Result<int8_t>::error(ErrorCode::CapacityExceeded,
                                     "emit request exceeds MAX_TOTAL_LIGHTS");
    }

    EmitParams params(model_index, command.speed, command.color.value_or(RANDOM_COLOR));
    if (command.length.has_value()) {
        params.setLength(*command.length);
    }
    params.trail = command.trail;
    params.noteId = command.note_id;
    params.minBri = command.min_brightness;
    params.maxBri = command.max_brightness;
    params.behaviourFlags = command.behaviour_flags;
    params.emitGroups = command.emit_groups;
    params.emitOffset = command.emit_offset;
    params.duration = command.duration_ms;
    params.from = command.from;
    params.linked = command.linked;

    Model* const model = impl_->object->getModel(model_index);
    if (model != nullptr) {
        const uint8_t emit_groups = params.getEmitGroups(model->emitGroups);
        if ((params.behaviourFlags & B_EMIT_FROM_CONN) != 0) {
            if (impl_->object->countConnections(params.emitGroups) == 0) {
                return Result<int8_t>::error(ErrorCode::NoEmitterAvailable,
                                             "no matching connections are available for emit");
            }
        } else if (impl_->object->countIntersections(emit_groups) == 0) {
            return Result<int8_t>::error(ErrorCode::NoEmitterAvailable,
                                         "no matching intersections are available for emit");
        }
    }

    const int8_t list_index = impl_->state.emit(params);
    if (list_index < 0) {
        return Result<int8_t>::error(ErrorCode::InternalError, "emit failed unexpectedly");
    }
    return Result<int8_t>(list_index);
}

void Engine::update(uint64_t millis) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->now_millis = millis;
    gMillis = static_cast<unsigned long>(millis);
    impl_->state.autoEmit(gMillis);
    impl_->state.update();
}

void Engine::tick(uint64_t delta_millis) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->now_millis += delta_millis;
    gMillis = static_cast<unsigned long>(impl_->now_millis);
    impl_->state.autoEmit(gMillis);
    impl_->state.update();
}

void Engine::stopAll() {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.stopAll();
}

bool Engine::isOn() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.isOn();
}

void Engine::setOn(bool on) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.setOn(on);
}

bool Engine::autoEmitEnabled() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->state.autoEnabled;
}

void Engine::setAutoEmitEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->state.autoEnabled = enabled;
}

uint16_t Engine::pixelCount() const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->object->pixelCount;
}

Result<Color> Engine::pixel(uint16_t index, uint8_t max_brightness) const {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (index >= impl_->object->pixelCount) {
        return Result<Color>::error(ErrorCode::OutOfRange, "pixel index is out of range");
    }

    const ColorRGB value = impl_->state.getPixel(index, max_brightness);
    return Result<Color>(Color{value.R, value.G, value.B});
}

} // namespace lightpath
