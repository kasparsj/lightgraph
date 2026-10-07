#include "Line.h"

#include "../geometry/BuiltinGeometry.h"

std::unique_ptr<lightgraph::geometry::GeometryProvider> Line::createGeometry() {
    lightgraph::geometry::GeometryResult result;
    lightgraph::geometry::BuiltinGeometryRoles roles;
    roles.intersectionIds = {geometryStartId_, geometryEndId_};
    roles.count = 2;
    auto geometry = lightgraph::geometry::createBuiltinGeometry(
        *this, lightgraph::geometry::BuiltinGeometryKind::Line, geometryPixelCount_, roles, result);
    setGeometryCreationResult(result);
    return geometry;
}

void Line::setup() {
    Model::maxWeights = 2;  // One for bridge, one for physical connection

    // Add a default model
    addModel(new Model(L_DEFAULT, 10, GROUP1));
    addModel(new Model(L_BOUNCE, 10, GROUP1));

    // Add a bridge from last pixel to first
    Connection* bridge = addBridge(pixelCount-1, 0, GROUP1);
    geometryStartId_ = bridge->to->id;
    geometryEndId_ = bridge->from->id;
    
    // Add a physical connection with explicit numLeds to cover the full line
    Connection* lineConn = addConnection(new Connection(inter[0][1], inter[0][0], GROUP1, pixelCount-3));
    
    // Add weights to the model for both connections
    models[L_BOUNCE]->put(bridge, 0);
    models[L_BOUNCE]->put(lineConn, 10);
}

float Line::getProgressOnLine(uint16_t pixel) const {
    // Assuming a sequential range of pixels from 0 to pixelCount-1
    return (float)pixel / (pixelCount - 1);
}

uint16_t Line::getPixelOnLine(float perc) const {
    return round(perc * (pixelCount - 1));
}

uint16_t* Line::getMirroredPixels(uint16_t pixel, Owner* /*mirrorFlipEmitter*/, bool mirrorRotate) {
    float progress = getProgressOnLine(pixel);
    mirrorPixels[0] = 0;
    
    // Mirror by reflecting across the middle of the line
    if (mirrorRotate) {
        mirrorPixels[1] = getPixelOnLine(1.0 - progress);
        mirrorPixels[0] = 1;
    }
    
    return mirrorPixels;
}
