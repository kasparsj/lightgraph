#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

#include "TopologyObject.h"

inline bool parseTopologyBoundedLong(JsonVariantConst value, long minValue, long maxValue, long& out) {
  const bool isNumber =
      value.is<int>() || value.is<long>() || value.is<unsigned int>() || value.is<unsigned long>();
  if (!isNumber) {
    return false;
  }
  const long parsed = value.as<long>();
  if (parsed < minValue || parsed > maxValue) {
    return false;
  }
  out = parsed;
  return true;
}

inline bool parseTopologyMacAddress(const String& input, uint8_t out[6]) {
  String normalized = input;
  normalized.replace(":", "");
  normalized.replace("-", "");
  normalized.trim();
  if (normalized.length() != 12) {
    return false;
  }

  for (int i = 0; i < 6; i++) {
    const int idx = i * 2;
    const char c1 = normalized.charAt(idx);
    const char c2 = normalized.charAt(idx + 1);
    auto hexValue = [](char c) -> int {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
      if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      return -1;
    };
    const int h1 = hexValue(c1);
    const int h2 = hexValue(c2);
    if (h1 < 0 || h2 < 0) {
      return false;
    }
    out[i] = static_cast<uint8_t>((h1 << 4) | h2);
  }
  return true;
}

inline size_t normalizeTopologySnapshotDuplicatePortIds(TopologySnapshot& snapshot) {
  bool usedIds[256] = {false};
  bool hasDuplicate = false;

  for (const TopologyPortSnapshot& port : snapshot.ports) {
    if (usedIds[port.id]) {
      hasDuplicate = true;
      break;
    }
    usedIds[port.id] = true;
  }

  if (!hasDuplicate) {
    return 0;
  }

  for (size_t i = 0; i < 256; i++) {
    usedIds[i] = false;
  }

  size_t normalizedCount = 0;
  for (TopologyPortSnapshot& port : snapshot.ports) {
    if (!usedIds[port.id]) {
      usedIds[port.id] = true;
      continue;
    }

    uint16_t replacementId = 0;
    while (replacementId < 256 && usedIds[replacementId]) {
      replacementId += 1;
    }
    if (replacementId >= 256) {
      break;
    }

    port.id = static_cast<uint8_t>(replacementId);
    usedIds[port.id] = true;
    normalizedCount += 1;
  }

  return normalizedCount;
}

inline bool parseTopologySnapshotFromJson(JsonObjectConst root, TopologySnapshot& snapshot, String& error) {
  long schemaVersion = 0;
  if (!parseTopologyBoundedLong(root["schemaVersion"], 0, 255, schemaVersion)) {
    error = "Missing schemaVersion";
    return false;
  }
  if (schemaVersion != 3) {
    error = "Unsupported schemaVersion; expected 3";
    return false;
  }
  snapshot.schemaVersion = static_cast<uint8_t>(schemaVersion);

  long pixelCount = 0;
  if (!parseTopologyBoundedLong(root["pixelCount"], 1, 65535, pixelCount)) {
    error = "Invalid or missing pixelCount";
    return false;
  }
  snapshot.pixelCount = static_cast<uint16_t>(pixelCount);

  JsonArrayConst intersections = root["intersections"];
  if (intersections.isNull()) {
    error = "Missing intersections array";
    return false;
  }
  for (JsonObjectConst intersectionJson : intersections) {
    long id = 0;
    long numPorts = 0;
    long topPixel = 0;
    long group = 0;
    if (!parseTopologyBoundedLong(intersectionJson["id"], 0, 255, id) ||
        !parseTopologyBoundedLong(intersectionJson["numPorts"], 2, 9, numPorts) ||
        !parseTopologyBoundedLong(intersectionJson["topPixel"], 0, 65535, topPixel) ||
        !parseTopologyBoundedLong(intersectionJson["group"], 1, 255, group)) {
      error = "Invalid intersection entry";
      return false;
    }
    long bottomPixel = -1;
    if (!intersectionJson["bottomPixel"].isNull()) {
      if (!parseTopologyBoundedLong(intersectionJson["bottomPixel"], -1, 32767, bottomPixel)) {
        error = "Invalid intersection bottomPixel";
        return false;
      }
    }
    snapshot.intersections.push_back({
        static_cast<uint8_t>(id),
        static_cast<uint8_t>(numPorts),
        static_cast<uint16_t>(topPixel),
        static_cast<int16_t>(bottomPixel),
        static_cast<uint8_t>(group),
    });
  }

  JsonArrayConst connections = root["connections"];
  if (connections.isNull()) {
    error = "Missing connections array";
    return false;
  }
  for (JsonObjectConst connectionJson : connections) {
    long fromIntersectionId = 0;
    long toIntersectionId = 0;
    long group = 0;
    long numLeds = 0;
    if (!parseTopologyBoundedLong(connectionJson["fromIntersectionId"], 0, 255, fromIntersectionId) ||
        !parseTopologyBoundedLong(connectionJson["toIntersectionId"], 0, 255, toIntersectionId) ||
        !parseTopologyBoundedLong(connectionJson["group"], 1, 255, group) ||
        !parseTopologyBoundedLong(connectionJson["numLeds"], 0, 65535, numLeds)) {
      error = "Invalid connection entry";
      return false;
    }
    snapshot.connections.push_back({
        static_cast<uint8_t>(fromIntersectionId),
        static_cast<uint8_t>(toIntersectionId),
        static_cast<uint8_t>(group),
        static_cast<uint16_t>(numLeds),
    });
  }

  JsonArrayConst ports = root["ports"];
  if (ports.isNull()) {
    error = "Missing ports array";
    return false;
  }
  for (JsonObjectConst portJson : ports) {
    long id = 0;
    long intersectionId = 0;
    long slotIndex = 0;
    long group = 0;
    if (!parseTopologyBoundedLong(portJson["id"], 0, 255, id) ||
        !parseTopologyBoundedLong(portJson["intersectionId"], 0, 255, intersectionId) ||
        !parseTopologyBoundedLong(portJson["slotIndex"], 0, 255, slotIndex) ||
        !parseTopologyBoundedLong(portJson["group"], 1, 255, group)) {
      error = "Invalid port entry";
      return false;
    }

    String type = String(portJson["type"] | "internal");
    type.toLowerCase();
    const TopologyPortType portType =
        (type == "external") ? TopologyPortType::External : TopologyPortType::Internal;
    bool direction = false;

    std::array<uint8_t, 6> deviceMac = {0, 0, 0, 0, 0, 0};
    long targetPortId = 0;
    if (portType == TopologyPortType::External) {
      String portRole = String(portJson["portRole"] | "");
      portRole.toLowerCase();
      if (portRole == "inbound" || portRole == "outbound") {
        direction = true;
      } else {
        error = "Invalid external port portRole";
        return false;
      }

      if (!portJson.containsKey("deviceMac")) {
        error = "External port missing deviceMac";
        return false;
      }
      uint8_t parsedMac[6] = {0};
      if (!parseTopologyMacAddress(String(portJson["deviceMac"].as<const char*>()), parsedMac)) {
        error = "Invalid external port deviceMac";
        return false;
      }
      for (uint8_t i = 0; i < 6; i++) {
        deviceMac[i] = parsedMac[i];
      }
      if (!parseTopologyBoundedLong(portJson["targetPortId"], 0, 255, targetPortId)) {
        error = "Invalid external port targetPortId";
        return false;
      }

      for (const TopologyPortSnapshot& existingPort : snapshot.ports) {
        if (existingPort.type != TopologyPortType::External ||
            existingPort.targetPortId != static_cast<uint8_t>(targetPortId) ||
            existingPort.direction != direction ||
            existingPort.group != static_cast<uint8_t>(group)) {
          continue;
        }

        bool sameDeviceMac = true;
        for (uint8_t i = 0; i < 6; i++) {
          if (existingPort.deviceMac[i] != deviceMac[i]) {
            sameDeviceMac = false;
            break;
          }
        }
        if (sameDeviceMac) {
          error = "Duplicate external port mapping";
          return false;
        }
      }
    } else {
      String endpointRole = String(portJson["endpointRole"] | "");
      endpointRole.toLowerCase();
      if (endpointRole == "from") {
        direction = false;
      } else if (endpointRole == "to") {
        direction = true;
      } else {
        error = "Invalid internal port endpointRole";
        return false;
      }
    }

    snapshot.ports.push_back({
        static_cast<uint8_t>(id),
        static_cast<uint8_t>(intersectionId),
        static_cast<uint8_t>(slotIndex),
        portType,
        direction,
        static_cast<uint8_t>(group),
        deviceMac,
        static_cast<uint8_t>(targetPortId),
    });
  }

  JsonArrayConst models = root["models"];
  if (!models.isNull()) {
    for (JsonObjectConst modelJson : models) {
      long id = 0;
      long defaultWeight = 0;
      long emitGroups = 0;
      long maxLength = 0;
      long routingStrategy = 0;
      if (!parseTopologyBoundedLong(modelJson["id"], 0, 255, id) ||
          !parseTopologyBoundedLong(modelJson["defaultWeight"], 0, 255, defaultWeight) ||
          !parseTopologyBoundedLong(modelJson["emitGroups"], 0, 255, emitGroups) ||
          !parseTopologyBoundedLong(modelJson["maxLength"], 0, 65535, maxLength)) {
        error = "Invalid model entry";
        return false;
      }
      if (!modelJson["routingStrategy"].isNull() &&
          !parseTopologyBoundedLong(modelJson["routingStrategy"], 0, 1, routingStrategy)) {
        error = "Invalid model routingStrategy";
        return false;
      }

      TopologyModelSnapshot modelSnapshot{
          static_cast<uint8_t>(id),
          static_cast<uint8_t>(defaultWeight),
          static_cast<uint8_t>(emitGroups),
          static_cast<uint16_t>(maxLength),
          static_cast<RoutingStrategy>(routingStrategy),
          {},
      };

      JsonArrayConst weights = modelJson["weights"];
      if (!weights.isNull()) {
        for (JsonObjectConst weightJson : weights) {
          long outgoingPortId = 0;
          long portDefaultWeight = 0;
          if (!parseTopologyBoundedLong(weightJson["outgoingPortId"], 0, 255, outgoingPortId) ||
              !parseTopologyBoundedLong(weightJson["defaultWeight"], 0, 255, portDefaultWeight)) {
            error = "Invalid model weight entry";
            return false;
          }
          TopologyPortWeightSnapshot weightSnapshot{
              static_cast<uint8_t>(outgoingPortId),
              static_cast<uint8_t>(portDefaultWeight),
              {},
          };

          JsonArrayConst conditionals = weightJson["conditionals"];
          if (!conditionals.isNull()) {
            for (JsonObjectConst conditionalJson : conditionals) {
              long incomingPortId = 0;
              long conditionalWeight = 0;
              if (!parseTopologyBoundedLong(conditionalJson["incomingPortId"], 0, 255, incomingPortId) ||
                  !parseTopologyBoundedLong(conditionalJson["weight"], 0, 255, conditionalWeight)) {
                error = "Invalid conditional model weight entry";
                return false;
              }
              weightSnapshot.conditionals.push_back({
                  static_cast<uint8_t>(incomingPortId),
                  static_cast<uint8_t>(conditionalWeight),
              });
            }
          }

          modelSnapshot.weights.push_back(weightSnapshot);
        }
      }

      snapshot.models.push_back(modelSnapshot);
    }
  }

  JsonArrayConst gaps = root["gaps"];
  if (!gaps.isNull()) {
    for (JsonObjectConst gapJson : gaps) {
      long fromPixel = 0;
      long toPixel = 0;
      if (!parseTopologyBoundedLong(gapJson["fromPixel"], 0, 65535, fromPixel) ||
          !parseTopologyBoundedLong(gapJson["toPixel"], 0, 65535, toPixel)) {
        error = "Invalid gap entry";
        return false;
      }
      snapshot.gaps.push_back({
          static_cast<uint16_t>(fromPixel),
          static_cast<uint16_t>(toPixel),
      });
    }
  }

  return true;
}

inline String serializeTopologySnapshotToJson(const TopologySnapshot& snapshot) {
  String payload;
  payload.reserve(16384);

  payload += "{\"schemaVersion\":";
  payload += String(snapshot.schemaVersion);
  payload += ",\"pixelCount\":";
  payload += String(snapshot.pixelCount);

  payload += ",\"intersections\":[";
  for (size_t i = 0; i < snapshot.intersections.size(); i++) {
    if (i > 0) payload += ",";
    const TopologyIntersectionSnapshot& intersection = snapshot.intersections[i];
    payload += "{\"id\":";
    payload += String(intersection.id);
    payload += ",\"numPorts\":";
    payload += String(intersection.numPorts);
    payload += ",\"topPixel\":";
    payload += String(intersection.topPixel);
    payload += ",\"bottomPixel\":";
    payload += String(intersection.bottomPixel);
    payload += ",\"group\":";
    payload += String(intersection.group);
    payload += "}";
  }
  payload += "]";

  payload += ",\"connections\":[";
  for (size_t i = 0; i < snapshot.connections.size(); i++) {
    if (i > 0) payload += ",";
    const TopologyConnectionSnapshot& connection = snapshot.connections[i];
    payload += "{\"fromIntersectionId\":";
    payload += String(connection.fromIntersectionId);
    payload += ",\"toIntersectionId\":";
    payload += String(connection.toIntersectionId);
    payload += ",\"group\":";
    payload += String(connection.group);
    payload += ",\"numLeds\":";
    payload += String(connection.numLeds);
    payload += "}";
  }
  payload += "]";

  payload += ",\"models\":[";
  for (size_t i = 0; i < snapshot.models.size(); i++) {
    if (i > 0) payload += ",";
    const TopologyModelSnapshot& model = snapshot.models[i];
    payload += "{\"id\":";
    payload += String(model.id);
    payload += ",\"defaultWeight\":";
    payload += String(model.defaultWeight);
    payload += ",\"emitGroups\":";
    payload += String(model.emitGroups);
    payload += ",\"maxLength\":";
    payload += String(model.maxLength);
    payload += ",\"routingStrategy\":";
    payload += String(static_cast<uint8_t>(model.routingStrategy));
    payload += ",\"weights\":[";

    for (size_t j = 0; j < model.weights.size(); j++) {
      if (j > 0) payload += ",";
      const TopologyPortWeightSnapshot& weight = model.weights[j];
      payload += "{\"outgoingPortId\":";
      payload += String(weight.outgoingPortId);
      payload += ",\"defaultWeight\":";
      payload += String(weight.defaultWeight);
      payload += ",\"conditionals\":[";

      for (size_t k = 0; k < weight.conditionals.size(); k++) {
        if (k > 0) payload += ",";
        const TopologyWeightConditionalSnapshot& conditional = weight.conditionals[k];
        payload += "{\"incomingPortId\":";
        payload += String(conditional.incomingPortId);
        payload += ",\"weight\":";
        payload += String(conditional.weight);
        payload += "}";
      }
      payload += "]}";
    }
    payload += "]}";
  }
  payload += "]";

  payload += ",\"ports\":[";
  for (size_t i = 0; i < snapshot.ports.size(); i++) {
    if (i > 0) payload += ",";
    const TopologyPortSnapshot& port = snapshot.ports[i];
    const bool isExternal = port.type == TopologyPortType::External;
    payload += "{\"id\":";
    payload += String(port.id);
    payload += ",\"intersectionId\":";
    payload += String(port.intersectionId);
    payload += ",\"slotIndex\":";
    payload += String(port.slotIndex);
    payload += ",\"type\":\"";
    payload += (isExternal ? "external" : "internal");
    if (isExternal) {
      payload += "\",\"portRole\":\"";
      payload += (port.direction ? "outbound" : "inbound");
    } else {
      payload += "\",\"endpointRole\":\"";
      payload += (port.direction ? "to" : "from");
    }
    payload += "\"";
    payload += ",\"group\":";
    payload += String(port.group);

    if (isExternal) {
      char macBuffer[18] = {0};
      snprintf(macBuffer,
               sizeof(macBuffer),
               "%02X:%02X:%02X:%02X:%02X:%02X",
               port.deviceMac[0],
               port.deviceMac[1],
               port.deviceMac[2],
               port.deviceMac[3],
               port.deviceMac[4],
               port.deviceMac[5]);
      payload += ",\"deviceMac\":\"";
      payload += macBuffer;
      payload += "\",\"targetPortId\":";
      payload += String(port.targetPortId);
    }
    payload += "}";
  }
  payload += "]";

  payload += ",\"gaps\":[";
  for (size_t i = 0; i < snapshot.gaps.size(); i++) {
    if (i > 0) payload += ",";
    const PixelGap& gap = snapshot.gaps[i];
    payload += "{\"fromPixel\":";
    payload += String(gap.fromPixel);
    payload += ",\"toPixel\":";
    payload += String(gap.toPixel);
    payload += "}";
  }
  payload += "]}";

  return payload;
}
