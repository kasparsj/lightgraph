#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "TopologyObject.h"

struct TopologySummaryPort {
    bool present = false;
    bool isExternal = false;
    uint8_t id = 0;
    bool direction = false;
    uint8_t group = 0;
    std::array<uint8_t, 6> device = {0, 0, 0, 0, 0, 0};
    uint8_t targetId = 0;
    int16_t targetIntersectionId = TOPOLOGY_TARGET_INTERSECTION_UNSET;
    bool hasTargetId = false;
};

struct TopologySummaryIntersection {
    uint8_t id = 0;
    uint8_t group = 0;
    uint8_t numPorts = 0;
    uint16_t topPixel = 0;
    int16_t bottomPixel = -1;
    bool allowEndOfLife = true;
    bool allowEmit = true;
    std::vector<TopologySummaryPort> ports;
};

struct TopologySummaryConnection {
    uint8_t group = 0;
    uint16_t fromPixel = 0;
    uint16_t toPixel = 0;
    uint16_t numLeds = 0;
    bool pixelDir = false;
    bool hasFromIntersectionId = false;
    uint8_t fromIntersectionId = 0;
    bool hasToIntersectionId = false;
    uint8_t toIntersectionId = 0;
    bool hasFromPortId = false;
    uint8_t fromPortId = 0;
    bool hasToPortId = false;
    uint8_t toPortId = 0;
};

struct TopologySummaryModel {
    bool present = false;
    uint8_t id = 0;
    uint8_t defaultWeight = 0;
    uint8_t emitGroups = 0;
    uint16_t maxLength = 0;
};

struct TopologySummary {
    uint8_t schemaVersion = 3;
    uint16_t pixelCount = 0;
    uint16_t realPixelCount = 0;
    uint16_t modelCount = 0;
    uint16_t gapCount = 0;
    std::vector<TopologySummaryIntersection> intersections;
    std::vector<TopologySummaryConnection> connections;
    std::vector<TopologySummaryModel> models;
    std::vector<PixelGap> gaps;
};

inline TopologySummary buildTopologySummary(const TopologyObject& object) {
    TopologySummary out;
    out.pixelCount = object.pixelCount;
    out.realPixelCount = object.realPixelCount;
    out.modelCount = static_cast<uint16_t>(object.models.size());
    out.gapCount = static_cast<uint16_t>(object.gaps.size());

    for (uint8_t group = 0; group < MAX_GROUPS; group++) {
        for (Intersection* intersection : object.inter[group]) {
            if (intersection == nullptr) {
                continue;
            }

            TopologySummaryIntersection entry;
            entry.id = intersection->id;
            entry.group = intersection->group;
            entry.numPorts = intersection->numPorts;
            entry.topPixel = intersection->topPixel;
            entry.bottomPixel = intersection->bottomPixel;
            entry.allowEndOfLife = intersection->allowEndOfLife;
            entry.allowEmit = intersection->allowEmit;
            entry.ports.reserve(intersection->numPorts);

            for (uint8_t slot = 0; slot < intersection->numPorts; slot++) {
                TopologySummaryPort portEntry;
                const Port* port = intersection->ports[slot];
                if (port != nullptr) {
                    portEntry.present = true;
                    portEntry.id = port->id;
                    portEntry.isExternal = port->isExternal();
                    portEntry.direction = port->direction;
                    portEntry.group = port->group;
                    if (portEntry.isExternal) {
                        const auto* externalPort = static_cast<const ExternalPort*>(port);
                        portEntry.device = externalPort->device;
                        portEntry.targetId = externalPort->targetId;
                        portEntry.targetIntersectionId = externalPort->targetIntersectionId;
                        portEntry.hasTargetId = externalPort->hasTargetId;
                    }
                }
                entry.ports.push_back(portEntry);
            }

            out.intersections.push_back(std::move(entry));
        }
    }

    for (uint8_t group = 0; group < MAX_GROUPS; group++) {
        for (Connection* connection : object.conn[group]) {
            if (connection == nullptr) {
                continue;
            }

            TopologySummaryConnection entry;
            entry.group = connection->group;
            entry.fromPixel = connection->fromPixel;
            entry.toPixel = connection->toPixel;
            entry.numLeds = connection->numLeds;
            entry.pixelDir = connection->pixelDir;
            if (connection->from != nullptr) {
                entry.hasFromIntersectionId = true;
                entry.fromIntersectionId = connection->from->id;
            }
            if (connection->to != nullptr) {
                entry.hasToIntersectionId = true;
                entry.toIntersectionId = connection->to->id;
            }
            if (connection->fromPort != nullptr) {
                entry.hasFromPortId = true;
                entry.fromPortId = connection->fromPort->id;
            }
            if (connection->toPort != nullptr) {
                entry.hasToPortId = true;
                entry.toPortId = connection->toPort->id;
            }
            out.connections.push_back(entry);
        }
    }

    out.models.reserve(object.models.size());
    for (Model* model : object.models) {
        TopologySummaryModel entry;
        if (model != nullptr) {
            entry.present = true;
            entry.id = model->id;
            entry.defaultWeight = model->defaultW;
            entry.emitGroups = model->emitGroups;
            entry.maxLength = model->maxLength;
        }
        out.models.push_back(entry);
    }

    out.gaps = object.gaps;
    return out;
}
