#include "Weight.h"
#include "../core/Platform.h"

void Weight::add(const Port *incoming, uint8_t w) {
    if (incoming == nullptr) {
        return;
    }
    if (conditional.size() >= MAX_CONDITIONAL_WEIGHTS &&
        conditional.find(incoming->id) == conditional.end()) {
        LG_LOGLN("Weight conditional map overflow");
        return;
    }
    conditional[incoming->id] = w;
}

uint8_t Weight::get(const Port *incoming) const {
    if (incoming != NULL) {
        const auto it = conditional.find(incoming->id);
        if (it != conditional.end()) {
            return it->second;
        }
    }
    return w;
}

void Weight::remove(const Port *incoming) {
    if (incoming != NULL) {
        conditional.erase(incoming->id);
    }
}
