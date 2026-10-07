#include "lightgraph/integration/drawing_codec.hpp"
#include <cstring>
namespace lightgraph::integration {
namespace {
constexpr std::uint32_t kSceneVersion = 1;
constexpr std::size_t kSceneHeaderBytes = 40;
constexpr std::size_t kSceneRecordBytes = 40;
std::uint32_t readU32(const std::uint8_t* value) {
    return (static_cast<std::uint32_t>(value[0]) << 24) |
           (static_cast<std::uint32_t>(value[1]) << 16) |
           (static_cast<std::uint32_t>(value[2]) << 8) | static_cast<std::uint32_t>(value[3]);
}

void writeU32(std::uint8_t* output, std::uint32_t value) {
    output[0] = static_cast<std::uint8_t>(value >> 24);
    output[1] = static_cast<std::uint8_t>(value >> 16);
    output[2] = static_cast<std::uint8_t>(value >> 8);
    output[3] = static_cast<std::uint8_t>(value);
}

float readFloat(const std::uint8_t* value) {
    const std::uint32_t bits = readU32(value);
    float result = 0.0f;
    std::memcpy(&result, &bits, sizeof(result));
    return result;
}

void writeFloat(std::uint8_t* output, float value) {
    std::uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    writeU32(output, bits);
}

} // namespace
bool DrawingSceneCodec::encode(const DrawingScene& scene, std::uint8_t* output,
                               std::size_t capacity, std::size_t& written) {
    written = 0;
    if (!output || !DrawingRuntime::validateScene(scene))
        return false;
    const std::size_t required = kSceneHeaderBytes + kSceneRecordBytes * scene.shapeCount;
    if (capacity < required)
        return false;
    std::uint8_t* cursor = output;
    const std::uint32_t headerInts[] = {kSceneVersion, scene.shapeCount,
                                        static_cast<std::uint32_t>(scene.baseKind), scene.rgb1,
                                        scene.rgb2};
    for (std::uint32_t word : headerInts) {
        writeU32(cursor, word);
        cursor += 4;
    }
    const float headerFloats[] = {scene.x1, scene.y1, scene.x2, scene.y2, scene.rotationDegrees};
    for (float word : headerFloats) {
        writeFloat(cursor, word);
        cursor += 4;
    }
    for (std::size_t index = 0; index < scene.shapeCount; ++index) {
        const DrawingShape& shape = scene.shapes[index];
        writeU32(cursor, static_cast<std::uint32_t>(shape.kind));
        cursor += 4;
        writeU32(cursor, shape.id);
        cursor += 4;
        writeU32(cursor, shape.rgb);
        cursor += 4;
        writeU32(cursor, shape.opacity);
        cursor += 4;
        for (float param : shape.params) {
            writeFloat(cursor, param);
            cursor += 4;
        }
    }
    written = required;
    return true;
}

bool DrawingSceneCodec::decode(const std::uint8_t* data, std::size_t size, DrawingScene& output) {
    if (!data || size < kSceneHeaderBytes || readU32(data) != kSceneVersion)
        return false;
    const std::uint32_t count = readU32(data + 4);
    if (count > DRAWING_MAX_SHAPES || size != kSceneHeaderBytes + kSceneRecordBytes * count)
        return false;
    DrawingScene candidate;
    candidate.shapeCount = static_cast<std::uint8_t>(count);
    const std::uint32_t baseKind = readU32(data + 8);
    if (baseKind > static_cast<std::uint32_t>(DrawingBaseKind::Gradient))
        return false;
    candidate.baseKind = static_cast<DrawingBaseKind>(baseKind);
    candidate.rgb1 = readU32(data + 12);
    candidate.rgb2 = readU32(data + 16);
    candidate.x1 = readFloat(data + 20);
    candidate.y1 = readFloat(data + 24);
    candidate.x2 = readFloat(data + 28);
    candidate.y2 = readFloat(data + 32);
    candidate.rotationDegrees = readFloat(data + 36);
    const std::uint8_t* cursor = data + kSceneHeaderBytes;
    for (std::size_t index = 0; index < count; ++index) {
        DrawingShape& shape = candidate.shapes[index];
        const std::uint32_t kind = readU32(cursor);
        cursor += 4;
        const std::uint32_t id = readU32(cursor);
        cursor += 4;
        if (kind < static_cast<std::uint32_t>(DrawingShapeKind::Band) ||
            kind > static_cast<std::uint32_t>(DrawingShapeKind::Rectangle))
            return false;
        shape.kind = static_cast<DrawingShapeKind>(kind);
        if (id > 255u)
            return false;
        shape.id = static_cast<std::uint8_t>(id);
        shape.rgb = readU32(cursor);
        cursor += 4;
        const std::uint32_t opacity = readU32(cursor);
        cursor += 4;
        if (opacity > 255u)
            return false;
        shape.opacity = static_cast<std::uint8_t>(opacity);
        for (float& param : shape.params) {
            param = readFloat(cursor);
            cursor += 4;
        }
    }
    if (!DrawingRuntime::validateScene(candidate))
        return false;
    output = candidate;
    return true;
}

} // namespace lightgraph::integration
