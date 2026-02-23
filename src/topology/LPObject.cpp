#include <algorithm>
#include "LPObject.h"
#include "Port.h"

LPObject* LPObject::instance = 0;

LPObject::LPObject(uint16_t pixelCount) : pixelCount(pixelCount), realPixelCount(pixelCount) {
    instance = this;
}

LPObject::~LPObject() {
    // Clean up connections first (owns Port instances).
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        for (Connection* connection : conn[i]) {
            delete connection;
        }
        conn[i].clear();
    }

    // Then clean up intersections.
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        for (Intersection* intersection : inter[i]) {
            delete intersection;
        }
        inter[i].clear();
    }
    
    // Clean up models
    for (Model* model : models) {
        delete model;
    }
    models.clear();
    
    // Gaps don't need deletion as they're stored by value
    gaps.clear();
}

// Initialization methods removed - vectors handle dynamic sizing

Model* LPObject::addModel(Model *model) {
    // Ensure vector is large enough
    while (models.size() <= model->id) {
        models.push_back(nullptr);
    }
    models[model->id] = model;
    return model;
}

Intersection* LPObject::addIntersection(Intersection *intersection) {
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        if (intersection->group & groupMaskForIndex(i)) {
            inter[i].push_back(intersection);
            break;
        }
    }
    return intersection;
}

Connection* LPObject::addConnection(Connection *connection) {
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        if (connection->group & groupMaskForIndex(i)) {
            conn[i].push_back(connection);
            break;
        }
    }
    return connection;
}

bool LPObject::removeConnection(uint8_t groupIndex, size_t index) {
    if (groupIndex >= MAX_GROUPS || index >= conn[groupIndex].size()) {
        return false;
    }
    Connection* connection = conn[groupIndex][index];
    conn[groupIndex].erase(conn[groupIndex].begin() + static_cast<std::ptrdiff_t>(index));
    delete connection;
    return true;
}

bool LPObject::removeConnection(Connection* connection) {
    if (connection == nullptr) {
        return false;
    }
    for (uint8_t i = 0; i < MAX_GROUPS; i++) {
        auto it = std::find(conn[i].begin(), conn[i].end(), connection);
        if (it != conn[i].end()) {
            conn[i].erase(it);
            delete connection;
            return true;
        }
    }
    return false;
}

void LPObject::addGap(uint16_t fromPixel, uint16_t toPixel) {
    gaps.push_back({fromPixel, toPixel});
    
    // Recalculate real pixel count
    uint16_t gapPixels = 0;
    for (const PixelGap& gap : gaps) {
        gapPixels += (gap.toPixel - gap.fromPixel + 1);
    }
    realPixelCount = pixelCount - gapPixels;
}

Connection* LPObject::addBridge(uint16_t fromPixel, uint16_t toPixel, uint8_t group, uint8_t numPorts) {
    Intersection *from = new Intersection(numPorts, fromPixel, -1, group);
    Intersection *to = new Intersection(numPorts, toPixel, -1, group);
    addIntersection(from);
    addIntersection(to);
    return addConnection(new Connection(from, to, group));
}

Intersection* LPObject::getIntersection(uint8_t i, uint8_t groups) {
    for (uint8_t j = 0; j < MAX_GROUPS; j++) {
        if (groups == 0 || (groups & groupMaskForIndex(j))) {
            if (i < inter[j].size()) {
                return inter[j][i];
            }
            i -= inter[j].size();
        }
    }
    return nullptr;
}

Connection* LPObject::getConnection(uint8_t i, uint8_t groups) {
    for (uint8_t j = 0; j < MAX_GROUPS; j++) {
        if (groups == 0 || (groups & groupMaskForIndex(j))) {
            if (i < conn[j].size()) {
                return conn[j][i];
            }
            i -= conn[j].size();
        }
    }
    return nullptr;
}

// Gap initialization removed - vectors handle dynamic sizing

bool LPObject::isPixelInGap(uint16_t logicalPixel) {
    for (const PixelGap& gap : gaps) {
        if (logicalPixel >= gap.fromPixel && logicalPixel <= gap.toPixel) {
            return true;
        }
    }
    return false;
}

int16_t LPObject::translateToRealPixel(uint16_t logicalPixel) {
    if (gaps.empty()) {
        return logicalPixel;
    }
    
    uint16_t realPixel = logicalPixel;
    for (const PixelGap& gap : gaps) {
        if (logicalPixel > gap.toPixel) {
            // Subtract the gap size from the real pixel position
            realPixel -= (gap.toPixel - gap.fromPixel + 1);
        } else if (logicalPixel >= gap.fromPixel) {
            // Pixel is in a gap, return invalid position
            return -1;
        }
    }
    return realPixel;
}

uint16_t LPObject::translateToLogicalPixel(uint16_t realPixel) {
    if (gaps.empty()) {
        return realPixel;
    }
    
    uint16_t logicalPixel = realPixel;
    for (const PixelGap& gap : gaps) {
        if (logicalPixel >= gap.fromPixel) {
            // Add the gap size to the logical pixel position
            logicalPixel += (gap.toPixel - gap.fromPixel + 1);
        }
    }
    return logicalPixel;
}
