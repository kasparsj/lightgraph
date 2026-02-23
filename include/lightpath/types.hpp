#pragma once

#include <cstdint>
#include <optional>

namespace lightpath {

/**
 * @file types.hpp
 * @brief Public value types and commands for the stable Lightpath API.
 */

/**
 * @brief 8-bit RGB color.
 */
struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
};

/**
 * @brief Built-in object topologies supported by the high-level engine API.
 */
enum class ObjectType {
    Heptagon919,
    Heptagon3024,
    Line,
    Cross,
    Triangle,
};

/**
 * @brief Engine construction configuration.
 */
struct EngineConfig {
    /// Built-in topology implementation to instantiate.
    ObjectType object_type = ObjectType::Line;
    /// Optional pixel-count override for configurable objects (line/cross/triangle).
    /// A value of `0` uses the object's default pixel count.
    uint16_t pixel_count = 0;
    /// Enable or disable internal automatic emission on each update/tick.
    bool auto_emit = false;
};

/**
 * @brief One emit request.
 */
struct EmitCommand {
    /// Topology model index.
    int8_t model = 0;
    /// Light speed scalar.
    float speed = 1.0f;
    /// Optional explicit list length.
    std::optional<uint16_t> length;
    /// Trail length.
    uint16_t trail = 0;
    /// Optional packed RGB color (`0xRRGGBB`); omitted means random color.
    std::optional<uint32_t> color;
    /// Optional note identifier for list reuse.
    uint16_t note_id = 0;
    /// Minimum brightness bound.
    uint8_t min_brightness = 0;
    /// Maximum brightness bound.
    uint8_t max_brightness = 255;
    /// Legacy behavior-flag bitmask.
    uint16_t behaviour_flags = 0;
    /// Optional emit-group mask override.
    uint8_t emit_groups = 0;
    /// Initial emit position offset.
    uint8_t emit_offset = 0;
    /// Optional list duration in milliseconds (`0` means library default behavior).
    uint32_t duration_ms = 0;
    /// Optional source index override (`-1` means automatic).
    int8_t from = -1;
    /// Whether new lights should be linked.
    bool linked = true;
};

} // namespace lightpath
