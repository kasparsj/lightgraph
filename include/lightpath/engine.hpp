#pragma once

#include <cstdint>
#include <memory>

#include "status.hpp"
#include "types.hpp"

namespace lightpath {

/**
 * @file engine.hpp
 * @brief High-level runtime facade for the stable Lightpath API.
 */

/**
 * @brief Thread-safe engine facade for built-in topologies and runtime state.
 *
 * `Engine` wraps the legacy topology/runtime implementation behind value-based
 * commands and typed status/error returns.
 */
class Engine {
  public:
    /**
     * @brief Construct an engine instance from configuration.
     */
    explicit Engine(const EngineConfig& config = EngineConfig{});
    ~Engine();

    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;
    Engine(Engine&&) noexcept;
    Engine& operator=(Engine&&) noexcept;

    /**
     * @brief Emit a new light list using a value command.
     * @return list index on success, otherwise an error code/message.
     */
    Result<int8_t> emit(const EmitCommand& command);

    /**
     * @brief Advance runtime to an absolute timestamp (milliseconds).
     */
    void update(uint64_t millis);

    /**
     * @brief Advance runtime by a delta in milliseconds.
     */
    void tick(uint64_t delta_millis);

    /**
     * @brief Stop all active lights/lists.
     */
    void stopAll();

    /**
     * @brief Return whether runtime output is enabled.
     */
    bool isOn() const;
    /**
     * @brief Enable or disable runtime output.
     */
    void setOn(bool on);

    /**
     * @brief Return auto-emit state.
     */
    bool autoEmitEnabled() const;
    /**
     * @brief Enable or disable auto-emit.
     */
    void setAutoEmitEnabled(bool enabled);

    /**
     * @brief Return total pixel count for the active object.
     */
    uint16_t pixelCount() const;
    /**
     * @brief Fetch a rendered pixel color by index.
     */
    Result<Color> pixel(uint16_t index, uint8_t max_brightness = 255) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace lightpath
