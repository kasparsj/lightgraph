#include "Model.h"
#include "LPObject.h"

// override from your Sculpture Object
uint8_t Model::maxWeights = 1;

uint16_t Model::getMaxLength() const {
    if (maxLength > 0) {
        return maxLength;
    }
    return LPObject::instance->pixelCount;
}
