#include "LPOwner.h"

#include "../runtime/LPLight.h"

void LPOwner::add(LPLight* const light) const {
    light->owner = this;
    light->owner->update(light);
}
