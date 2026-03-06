#pragma once

#include <array>
#include <cstdint>
#include <limits>

class Connection;
class Intersection;
class RuntimeLight;
class TopologyObject;

class Port {

  public:
    enum class Type : uint8_t {
        Internal = 0,
        External = 1,
    };

    static constexpr uint16_t INVALID_ID = std::numeric_limits<uint16_t>::max();

    uint16_t id = INVALID_ID;
    Connection* connection;
    Intersection* intersection;
    TopologyObject* object = nullptr;
    bool direction;
    uint8_t group;
  
    Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group, int16_t slotIndex = -1);
    virtual ~Port();
    virtual void sendOut(RuntimeLight* const light, bool sendList = false) = 0;
    virtual bool isExternal() const { return false; }
    virtual Type portType() const { return Type::Internal; }
    static uint16_t poolCount();
    static void setNextId(uint16_t id);
    static uint16_t nextId();

  protected:
    void handleColorChange(RuntimeLight* const light) const;
};

class InternalPort : public Port {
    
    public:
        InternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group, int16_t slotIndex = -1);
        virtual void sendOut(RuntimeLight* const light, bool sendList = false) override;
};

class ExternalPort : public Port {

  public:
    std::array<uint8_t, 6> device{};
    uint8_t targetId = 0;
    bool hasTargetId = true;
    int16_t targetIntersectionId = -1;
    
    ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                 const uint8_t device[6], uint8_t targetId, int16_t slotIndex = -1,
                 int16_t targetIntersectionId = -1, bool hasTargetId = true);
    virtual void sendOut(RuntimeLight* const light, bool sendList = false) override;
    virtual bool isExternal() const override { return true; }
    virtual Type portType() const override { return Type::External; }
};

// Legacy global fallback for source integrations that have not yet migrated to
// TopologyObject::setExternalSendHook().
extern bool (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList);
