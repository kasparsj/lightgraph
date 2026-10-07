#include "Triangle.h"

#include "../geometry/BuiltinGeometry.h"

std::unique_ptr<lightgraph::geometry::GeometryProvider> Triangle::createGeometry() {
    lightgraph::geometry::GeometryResult result;
    lightgraph::geometry::BuiltinGeometryRoles roles;
    roles.intersectionIds = {geometryVertex1Id_, geometryVertex2Id_, geometryVertex3Id_};
    roles.count = 3;
    auto geometry = lightgraph::geometry::createBuiltinGeometry(
        *this, lightgraph::geometry::BuiltinGeometryKind::Triangle, geometryPixelCount_, roles, result);
    setGeometryCreationResult(result);
    return geometry;
}

void Triangle::setup() {
    Model::maxWeights = 18;  // One for each physical connection and each bridge

    // Add models
    Model* defaultModel = addModel(new Model(T_DEFAULT, 10, GROUP1));
    Model* clockwiseModel = addModel(new Model(T_CLOCKWISE, 10, GROUP1));
    Model* counterClockwiseModel = addModel(new Model(T_COUNTER_CLOCKWISE, 10, GROUP1));
    
    // Add main vertices of the triangle
    Intersection* vertex1 = addIntersection(new Intersection(2, segment1Start, segment3End, GROUP1));
    Intersection* vertex2 = addIntersection(new Intersection(2, segment1End, segment2Start, GROUP1));
    Intersection* vertex3 = addIntersection(new Intersection(2, segment2End, segment3Start, GROUP1));
    geometryVertex1Id_ = vertex1->id;
    geometryVertex2Id_ = vertex2->id;
    geometryVertex3Id_ = vertex3->id;
    
    // Add midpoints for side 1 (vertex1 to vertex2)
    Intersection* side1_mid1 = addIntersection(new Intersection(2, segment1Start + subSegmentSize, 0, GROUP1));
    Intersection* side1_mid2 = addIntersection(new Intersection(2, segment1Start + 2*subSegmentSize, 0, GROUP1));
    Intersection* side1_mid3 = addIntersection(new Intersection(2, segment1Start + 3*subSegmentSize, 0, GROUP1));
    Intersection* side1_mid4 = addIntersection(new Intersection(2, segment1Start + 4*subSegmentSize, 0, GROUP1));
    
    // Add midpoints for side 2 (vertex2 to vertex3)
    Intersection* side2_mid1 = addIntersection(new Intersection(2, segment2Start + subSegmentSize, 0, GROUP1));
    Intersection* side2_mid2 = addIntersection(new Intersection(2, segment2Start + 2*subSegmentSize, 0, GROUP1));
    Intersection* side2_mid3 = addIntersection(new Intersection(2, segment2Start + 3*subSegmentSize, 0, GROUP1));
    Intersection* side2_mid4 = addIntersection(new Intersection(2, segment2Start + 4*subSegmentSize, 0, GROUP1));
    
    // Add midpoints for side 3 (vertex3 to vertex1)
    Intersection* side3_mid1 = addIntersection(new Intersection(2, segment3Start + subSegmentSize, 0, GROUP1));
    Intersection* side3_mid2 = addIntersection(new Intersection(2, segment3Start + 2*subSegmentSize, 0, GROUP1));
    Intersection* side3_mid3 = addIntersection(new Intersection(2, segment3Start + 3*subSegmentSize, 0, GROUP1));
    Intersection* side3_mid4 = addIntersection(new Intersection(2, segment3Start + 4*subSegmentSize, 0, GROUP1));
    
    // Add bridges between segments
    Connection* bridge1 = addBridge(segment1End, segment2Start, GROUP1, 2);
    Connection* bridge2 = addBridge(segment2End, segment3Start, GROUP1, 2);
    Connection* bridge3 = addBridge(segment3End, segment1Start, GROUP1, 2);
    
    // Add physical connections for Side 1 (5 sub-segments)
    Connection* side1_conn1 = addConnection(new Connection(vertex1, side1_mid1, GROUP1, subSegmentSize));
    Connection* side1_conn2 = addConnection(new Connection(side1_mid1, side1_mid2, GROUP1, subSegmentSize));
    Connection* side1_conn3 = addConnection(new Connection(side1_mid2, side1_mid3, GROUP1, subSegmentSize));
    Connection* side1_conn4 = addConnection(new Connection(side1_mid3, side1_mid4, GROUP1, subSegmentSize));
    Connection* side1_conn5 = addConnection(new Connection(side1_mid4, vertex2, GROUP1, subSegmentSize));
    
    // Add physical connections for Side 2 (5 sub-segments)
    Connection* side2_conn1 = addConnection(new Connection(vertex2, side2_mid1, GROUP1, subSegmentSize));
    Connection* side2_conn2 = addConnection(new Connection(side2_mid1, side2_mid2, GROUP1, subSegmentSize));
    Connection* side2_conn3 = addConnection(new Connection(side2_mid2, side2_mid3, GROUP1, subSegmentSize));
    Connection* side2_conn4 = addConnection(new Connection(side2_mid3, side2_mid4, GROUP1, subSegmentSize));
    Connection* side2_conn5 = addConnection(new Connection(side2_mid4, vertex3, GROUP1, subSegmentSize));
    
    // Add physical connections for Side 3 (5 sub-segments)
    Connection* side3_conn1 = addConnection(new Connection(vertex3, side3_mid1, GROUP1, subSegmentSize));
    Connection* side3_conn2 = addConnection(new Connection(side3_mid1, side3_mid2, GROUP1, subSegmentSize));
    Connection* side3_conn3 = addConnection(new Connection(side3_mid2, side3_mid3, GROUP1, subSegmentSize));
    Connection* side3_conn4 = addConnection(new Connection(side3_mid3, side3_mid4, GROUP1, subSegmentSize));
    Connection* side3_conn5 = addConnection(new Connection(side3_mid4, vertex1, GROUP1, subSegmentSize));
    
    // Set up default model - equal weights for all sides
    defaultModel->put(side1_conn1, 10);
    defaultModel->put(side1_conn2, 10);
    defaultModel->put(side1_conn3, 10);
    defaultModel->put(side1_conn4, 10);
    defaultModel->put(side1_conn5, 10);
    defaultModel->put(side2_conn1, 10);
    defaultModel->put(side2_conn2, 10);
    defaultModel->put(side2_conn3, 10);
    defaultModel->put(side2_conn4, 10);
    defaultModel->put(side2_conn5, 10);
    defaultModel->put(side3_conn1, 10);
    defaultModel->put(side3_conn2, 10);
    defaultModel->put(side3_conn3, 10);
    defaultModel->put(side3_conn4, 10);
    defaultModel->put(side3_conn5, 10);
    defaultModel->put(bridge1, 0);
    defaultModel->put(bridge2, 0);
    defaultModel->put(bridge3, 0);
    
    // Set up clockwise model
    clockwiseModel->put(side1_conn1, 10);
    clockwiseModel->put(side1_conn2, 10);
    clockwiseModel->put(side1_conn3, 10);
    clockwiseModel->put(side1_conn4, 10);
    clockwiseModel->put(side1_conn5, 10);
    clockwiseModel->put(side2_conn1, 10);
    clockwiseModel->put(side2_conn2, 10);
    clockwiseModel->put(side2_conn3, 10);
    clockwiseModel->put(side2_conn4, 10);
    clockwiseModel->put(side2_conn5, 10);
    clockwiseModel->put(side3_conn1, 10);
    clockwiseModel->put(side3_conn2, 10);
    clockwiseModel->put(side3_conn3, 10);
    clockwiseModel->put(side3_conn4, 10);
    clockwiseModel->put(side3_conn5, 10);
    clockwiseModel->put(bridge1, 10);
    clockwiseModel->put(bridge2, 10);
    clockwiseModel->put(bridge3, 10);
    
    // Set up counter-clockwise model (reverse direction)
    counterClockwiseModel->put(side1_conn1, 0);
    counterClockwiseModel->put(side1_conn2, 0);
    counterClockwiseModel->put(side1_conn3, 0);
    counterClockwiseModel->put(side1_conn4, 0);
    counterClockwiseModel->put(side1_conn5, 0);
    counterClockwiseModel->put(side2_conn1, 0);
    counterClockwiseModel->put(side2_conn2, 0);
    counterClockwiseModel->put(side2_conn3, 0);
    counterClockwiseModel->put(side2_conn4, 0);
    counterClockwiseModel->put(side2_conn5, 0);
    counterClockwiseModel->put(side3_conn1, 0);
    counterClockwiseModel->put(side3_conn2, 0);
    counterClockwiseModel->put(side3_conn3, 0);
    counterClockwiseModel->put(side3_conn4, 0);
    counterClockwiseModel->put(side3_conn5, 0);
    counterClockwiseModel->put(bridge1, 10);
    counterClockwiseModel->put(bridge2, 10);
    counterClockwiseModel->put(bridge3, 10);
}

uint8_t Triangle::getSegmentForPixel(uint16_t pixel) const {
    if (pixel >= segment1Start && pixel <= segment1End) {
        return 1;
    } else if (pixel >= segment2Start && pixel <= segment2End) {
        return 2;
    } else {
        return 3;
    }
}

float Triangle::getProgressOnSegment(uint16_t pixel, uint8_t segment) const {
    switch (segment) {
        case 1:
            return (float)(pixel - segment1Start) / (segment1End - segment1Start);
        case 2:
            return (float)(pixel - segment2Start) / (segment2End - segment2Start);
        case 3:
            return (float)(pixel - segment3Start) / (segment3End - segment3Start);
        default:
            return 0;
    }
}

uint16_t Triangle::getPixelOnSegment(float perc, uint8_t segment) const {
    switch (segment) {
        case 1:
            return segment1Start + round(perc * (segment1End - segment1Start));
        case 2:
            return segment2Start + round(perc * (segment2End - segment2Start));
        case 3:
            return segment3Start + round(perc * (segment3End - segment3Start));
        default:
            return 0;
    }
}

uint16_t* Triangle::getMirroredPixels(uint16_t pixel, Owner* /*mirrorFlipEmitter*/, bool mirrorRotate) {
    mirrorPixels[0] = 0;
    
    if (mirrorRotate) {
        // Determine which segment the pixel is on
        uint8_t segment = getSegmentForPixel(pixel);
        
        // Get the progress along that segment
        float progress = getProgressOnSegment(pixel, segment);
        
        // Mirror by reflecting across the segment
        uint8_t mirroredSegment;
        if (segment == 1) {
            mirroredSegment = 2;
        } else if (segment == 2) {
            mirroredSegment = 3;
        } else {
            mirroredSegment = 1;
        }
        
        mirrorPixels[1] = getPixelOnSegment(1.0 - progress, mirroredSegment);
        mirrorPixels[0] = 1;
    }
    
    return mirrorPixels;
}
