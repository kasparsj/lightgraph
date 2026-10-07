#include "Weight.h"
#include "../core/Platform.h"

void Weight::add(const Port *incoming, uint8_t weight) {
    if (incoming == nullptr) {
        return;
    }
    add(incoming->id, weight);
}

void Weight::add(uint16_t incomingPortId, uint8_t weight) {
    if (conditional.size() >= MAX_CONDITIONAL_WEIGHTS &&
        conditional.find(incomingPortId) == conditional.end()) {
        LG_LOGLN("Weight conditional map overflow");
        return;
    }
    conditional[incomingPortId] = weight;
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
