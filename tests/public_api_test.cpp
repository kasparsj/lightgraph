#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include <lightpath/lightpath.hpp>

namespace {

int fail(const std::string& message) {
    std::cerr << "FAIL: " << message << std::endl;
    return 1;
}

bool isNonBlack(const lightpath::Color& color) {
    return color.R > 0 || color.G > 0 || color.B > 0;
}

}  // namespace

int main() {
    std::srand(11);

    // Factory defaults should be deterministic for built-in objects.
    {
        auto line = lightpath::makeObject(lightpath::BuiltinObjectType::Line);
        if (line->pixelCount != lightpath::kLinePixelCount) {
            return fail("makeObject(Line) did not use default line pixel count");
        }
    }
    {
        auto triangle = lightpath::makeObject(lightpath::BuiltinObjectType::Triangle);
        if (triangle->pixelCount != lightpath::kTrianglePixelCount) {
            return fail("makeObject(Triangle) did not use default triangle pixel count");
        }
    }

    // Engine facade should drive the existing runtime behavior.
    auto object = lightpath::makeObject(lightpath::BuiltinObjectType::Line, 64);
    lightpath::Engine engine(std::move(object));

    lightpath::EmitParams params(0, 1.0f, 0x22AA44);
    params.setLength(5);
    params.noteId = 7;

    const int8_t listIndex = engine.state().emit(params);
    if (listIndex < 0) {
        return fail("Engine state emit failed");
    }

    lightpath::millis() = 0;
    for (int frame = 0; frame < 32; ++frame) {
        lightpath::millis() += 16;
        engine.update(lightpath::millis());
    }

    int litPixels = 0;
    for (uint16_t i = 0; i < engine.object().pixelCount; ++i) {
        if (isNonBlack(engine.state().getPixel(i))) {
            ++litPixels;
        }
    }
    if (litPixels == 0) {
        return fail("Engine update produced no visible pixels");
    }

    // Public palette helpers should remain wired to built-in palette catalog.
    if (lightpath::paletteCount() == 0) {
        return fail("paletteCount returned 0");
    }

    lightpath::Palette palette({0xFF0000, 0x00FF00}, {0.0f, 1.0f});
    std::vector<lightpath::Color> rgb = palette.interpolate(6);
    if (rgb.size() != 6) {
        return fail("Palette interpolation returned unexpected size");
    }

    const lightpath::Color& wrapped = lightpath::Palette::wrapColors(7, 6, rgb, lightpath::kWrapRepeat);
    if (!isNonBlack(wrapped)) {
        return fail("Palette wrapColors returned invalid wrapped color");
    }

    return 0;
}
