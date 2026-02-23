#include "../../vendor/ofxColorTheory/src/Rules/Analogous.h"
#include "../../vendor/ofxColorTheory/src/Rules/Complementary.h"
#include "../../vendor/ofxColorTheory/src/Rules/Compound.h"
#include "../../vendor/ofxColorTheory/src/Rules/FlippedCompound.h"
#include "../../vendor/ofxColorTheory/src/Rules/Monochrome.h"
#include "../../vendor/ofxColorTheory/src/Rules/SplitComplementary.h"
#include "../../vendor/ofxColorTheory/src/Rules/Tetrad.h"
#include "../../vendor/ofxColorTheory/src/Rules/Triad.h"
#include "../../vendor/ofxColorTheory/src/ColorWheelSchemes.h"

#include "../core/Types.h"

namespace ofxColorTheory {

template class Analogous_<ColorRGB>;
template class Complementary_<ColorRGB>;
template class Compound_<ColorRGB>;
template class FlippedCompound_<ColorRGB>;
template class Monochrome_<ColorRGB>;
template class SplitComplementary_<ColorRGB>;
template class Tetrad_<ColorRGB>;
template class Triad_<ColorRGB>;

template<>
const std::vector<std::shared_ptr<ColorWheelScheme_<ColorRGB>>> ColorWheelSchemes_<ColorRGB>::SCHEMES =
    ColorWheelSchemes_<ColorRGB>::createColorSchemes();

}  // namespace ofxColorTheory
