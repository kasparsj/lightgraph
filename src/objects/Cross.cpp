#include "Cross.h"

#include "../geometry/BuiltinGeometry.h"

std::unique_ptr<lightgraph::geometry::GeometryProvider> Cross::createGeometry() {
    lightgraph::geometry::GeometryResult result;
    lightgraph::geometry::BuiltinGeometryRoles roles;
    roles.intersectionIds = {geometryHorizontalStartId_, geometryHorizontalEndId_,
                             geometryVerticalStartId_, geometryVerticalEndId_, geometryCenterId_};
    roles.count = 5;
    auto geometry = lightgraph::geometry::createBuiltinGeometry(
        *this, lightgraph::geometry::BuiltinGeometryKind::Cross, geometryPixelCount_, roles, result);
    setGeometryCreationResult(result);
    return geometry;
}

void Cross::setup() {
    Model::maxWeights = 6;  // One for each connection

    // Add models
    Model* defaultModel = addModel(new Model(C_DEFAULT, 10, GROUP1));
    Model* horizontalModel = addModel(new Model(C_HORIZONTAL, 0, GROUP1));
    Model* verticalModel = addModel(new Model(C_VERTICAL, 0, GROUP1));
    Model* diagonalModel = addModel(new Model(C_DIAGONAL, 10, GROUP1));
    
    Connection* hBridge = addBridge(horizontalLineEnd, horizontalLineStart, GROUP1, 3); // inter[0,1]
    Connection* vBridge = addBridge(verticalLineEnd, verticalLineStart, GROUP1, 3); // inter[2,3]
    geometryHorizontalStartId_ = hBridge->to->id;
    geometryHorizontalEndId_ = hBridge->from->id;
    geometryVerticalStartId_ = vBridge->to->id;
    geometryVerticalEndId_ = vBridge->from->id;
    Connection* d1Bridge = addConnection(new Connection(vBridge->from, hBridge->from, GROUP1, 0));
    Connection* d2Bridge = addConnection(new Connection(hBridge->to, vBridge->to, GROUP1, 0));

    // Create intersection for the crossing point
    Intersection* center = addIntersection(new Intersection(4, horizontalCross, verticalCross, GROUP1)); // inter[4]
    geometryCenterId_ = center->id;
    
    // Create connections for horizontal line
    Connection* hConn1 = addConnection(new Connection(hBridge->to, center, GROUP1, pixelCount/4-3));
    Connection* hConn2 = addConnection(new Connection(center, hBridge->from, GROUP1, pixelCount/4-3));
    
    // Create connections for vertical line
    Connection* vConn1 = addConnection(new Connection(vBridge->to, center, GROUP1, pixelCount/4-3));
    Connection* vConn2 = addConnection(new Connection(center, vBridge->from, GROUP1, pixelCount/4-3));
    
    // Add weights to the default model for all connections
    defaultModel->put(d1Bridge, 0);
    defaultModel->put(d2Bridge, 0);
    
    // Add weights to the horizontal model
    horizontalModel->put(hBridge, 10);
    horizontalModel->put(hConn1, 10);
    horizontalModel->put(hConn2, 10);
    
    // Add weights to the vertical model
    verticalModel->put(vBridge, 10);
    verticalModel->put(vConn1, 10);
    verticalModel->put(vConn2, 10);
    
    diagonalModel->put(hBridge, 0);
    diagonalModel->put(vBridge, 0);
}

float Cross::getProgressOnLine(uint16_t pixel, bool isVertical) const {
    if (isVertical) {
        // For vertical line (288-575)
        return (float)(pixel - verticalLineStart) / (verticalLineEnd - verticalLineStart);
    } else {
        // For horizontal line (0-287)
        return (float)pixel / (horizontalLineEnd);
    }
}

uint16_t Cross::getPixelOnLine(float perc, bool isVertical) const {
    if (isVertical) {
        // For vertical line
        return verticalLineStart + round(perc * (verticalLineEnd - verticalLineStart));
    } else {
        // For horizontal line
        return round(perc * horizontalLineEnd);
    }
}

uint16_t* Cross::getMirroredPixels(uint16_t pixel, Owner* /*mirrorFlipEmitter*/, bool mirrorRotate) {
    mirrorPixels[0] = 0;
    
    // Determine if pixel is on vertical or horizontal line
    bool isVertical = pixel >= verticalLineStart && pixel <= verticalLineEnd;
    
    if (mirrorRotate) {
        float progress;
        
        if (isVertical) {
            // For vertical line, mirror horizontally
            progress = getProgressOnLine(pixel, true);
            mirrorPixels[1] = getPixelOnLine(1.0 - progress, true);
        } else {
            // For horizontal line, mirror vertically
            progress = getProgressOnLine(pixel, false);
            mirrorPixels[1] = getPixelOnLine(1.0 - progress, false);
        }
        
        mirrorPixels[0] = 1;
    }
    
    return mirrorPixels;
}
