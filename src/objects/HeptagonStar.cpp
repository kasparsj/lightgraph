#include "HeptagonStar.h"

void HeptagonStar::setupLayout(const LayoutDescriptor& descriptor) {
    Model::maxWeights = kSegmentCount * 4 * 2;

    addModel(new Model(M_DEFAULT, 10, GROUP1));
    addModel(new Model(M_STAR, 0, GROUP1));
    addModel(new Model(M_OUTER_SUN, 10, GROUP1, 602));
    addModel(new Model(M_INNER_TRIS, 0, GROUP2));
    addModel(new Model(M_SMALL_STAR, 0, GROUP2));
    addModel(new Model(M_INNER_SUN, 0, GROUP2));
    addModel(new Model(M_SPLATTER, 10, GROUP2 | GROUP3));

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        const BridgePlacement& bridge = descriptor.outerBridges[i];
        addBridge(bridge.fromPixel, bridge.toPixel, GROUP1);
    }

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        const IntersectionPlacement& placement = descriptor.middleIntersections[i];
        addIntersection(new Intersection(4, placement.topPixel, placement.bottomPixel, GROUP2));
    }

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        Intersection* outerNeuron1 = inter[0][i * 2 + 1];
        Intersection* outerNeuron2 = inter[0][((i + 1) * 2) % (kSegmentCount * 2)];
        addConnection(new Connection(outerNeuron1, inter[1][i], GROUP2));
        addConnection(new Connection(outerNeuron2, inter[1][i], GROUP2));
    }

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        const IntersectionPlacement& placement = descriptor.innerIntersections[i];
        addIntersection(new Intersection(4, placement.topPixel, placement.bottomPixel, GROUP3));
    }

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        Intersection* middleNeuron1 = inter[1][i];
        Intersection* middleNeuron2 = inter[1][(i - 1 + kSegmentCount) % kSegmentCount];
        Connection* middleConnection1 = addConnection(new Connection(middleNeuron1, inter[2][i], GROUP3));
        Connection* middleConnection2 = addConnection(new Connection(middleNeuron2, inter[2][i], GROUP3));
        models[M_STAR]->put(middleConnection1->fromPort, middleNeuron1->ports[1], 10);
        models[M_STAR]->put(middleConnection2->fromPort, middleNeuron2->ports[0], 10);
        models[M_OUTER_SUN]->put(middleConnection1, 0);
        models[M_OUTER_SUN]->put(middleConnection2, 0);
        models[M_INNER_TRIS]->put(middleConnection1, 10);
        models[M_INNER_TRIS]->put(middleConnection2, 10);
        models[M_SMALL_STAR]->put(middleConnection1->fromPort, 10);
        models[M_SMALL_STAR]->put(middleConnection2->fromPort, 10);
        models[M_INNER_SUN]->put(middleConnection1->fromPort, 10);
        models[M_INNER_SUN]->put(middleConnection2->fromPort, 10);
        models[M_INNER_SUN]->put(middleConnection1->toPort, 10);
        models[M_INNER_SUN]->put(middleConnection2->toPort, 10);
    }

    for (uint8_t i = 0; i < kSegmentCount; i++) {
        Intersection* innerNeuron1 = inter[2][i];
        Intersection* innerNeuron2 = inter[2][(i + 1) % kSegmentCount];
        Connection* innerConnection = addConnection(new Connection(innerNeuron1, innerNeuron2, GROUP4));
        models[M_STAR]->put(innerConnection->fromPort, innerNeuron1->ports[1], 10);
        models[M_STAR]->put(innerConnection->toPort, innerNeuron2->ports[0], 10);
        models[M_SMALL_STAR]->put(innerConnection->fromPort, innerNeuron1->ports[1], 10);
        models[M_SMALL_STAR]->put(innerConnection->toPort, innerNeuron2->ports[0], 10);
        models[M_OUTER_SUN]->put(innerConnection, 0);
        models[M_INNER_TRIS]->put(innerConnection, 10);
    }

    for (std::size_t i = 0; i < descriptor.gapCount; i++) {
        addGap(descriptor.gaps[i].fromPixel, descriptor.gaps[i].toPixel);
    }
}

uint16_t* HeptagonStar::getMirroredPixels(uint16_t pixel, Owner* mirrorFlipEmitter, bool mirrorRotate) {
    uint8_t pathIndex = getStarSegmentIndex(pixel);
    float progress = getProgressOnStarSegment(pathIndex, pixel);
    uint8_t i = 1;
    mirrorPixels[0] = 0;
    if (mirrorFlipEmitter != NULL) {
        if (mirrorFlipEmitter->getType() == Owner::TYPE_INTERSECTION) {
            uint8_t emitterIndex = static_cast<Intersection*>(mirrorFlipEmitter)->id / 2;
            if (emitterIndex < 7) {
                uint8_t mirrorIndex = ((emitterIndex + (emitterIndex - pathIndex) + 11) % 7);
                mirrorPixels[i++] = getPixelOnStarSegment(mirrorIndex, 1.0 - progress);
                mirrorPixels[0] += 1;
            }
        }
    }
    if (mirrorRotate) {
        uint8_t mirrorIndex = (pathIndex + 4) % 7;
        mirrorPixels[i++] = getPixelOnStarSegment(mirrorIndex, progress);
        mirrorPixels[0] += 1;
    }
    return mirrorPixels;
}

uint8_t HeptagonStar::getStarSegmentIndex(uint16_t pixel) const {
    uint8_t stripIndex = 0;
    for (size_t i = 0; i < conn[0].size(); i++) {
        if (pixel >= conn[0][i]->to->topPixel && pixel <= conn[0][(i + 3) % 7]->from->topPixel) {
            stripIndex = static_cast<uint8_t>(i);
        }
    }
    return stripIndex;
}

float HeptagonStar::getProgressOnStarSegment(uint8_t pathIndex, uint16_t pixel) const {
    Intersection *from = conn[0][pathIndex]->to;
    uint8_t toIndex = (pathIndex + 3) % 7;
    Intersection *to = conn[0][toIndex]->from;
    return (float) (pixel - from->topPixel) / (to->topPixel - from->topPixel);
}

uint16_t HeptagonStar::getPixelOnStarSegment(uint8_t pathIndex, float perc) const {
    Intersection *from = conn[0][pathIndex]->to;
    uint8_t toIndex = (pathIndex + 3) % 7;
    Intersection *to = conn[0][toIndex]->from;
    return from->topPixel + round((to->topPixel - from->topPixel) * perc);
}
