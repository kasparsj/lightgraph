#pragma once

#include <array>
#include <cstdint>

class Connection;
class Intersection;
class RuntimeLight;

class Port {

  public:
    enum class Type : uint8_t {
        Internal = 0,
        External = 1,
    };

    uint8_t id;
    Connection* connection;
    Intersection* intersection;
    bool direction;
    uint8_t group;
  
    Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group, int16_t slotIndex = -1);
    virtual ~Port();
    virtual void sendOut(RuntimeLight* const light, bool sendList = false) = 0;
    virtual bool isExternal() const { return false; }
    virtual Type portType() const { return Type::Internal; }
    
    // Static pool management
    static Port* findById(uint8_t id);
    static void addToPool(Port* port);
    static void removeFromPool(Port* port);
    static uint8_t poolCount() { return poolSize; }
    static uint8_t allocateId();
    static void setNextId(uint8_t id);
    static uint8_t nextId() { return nextPortId; }
    
  protected:
    void handleColorChange(RuntimeLight* const light) const;
    
  private:
    static const uint8_t MAX_PORTS = 200;
    static Port* portPool[MAX_PORTS];
    static uint8_t poolSize;
    static uint8_t nextPortId;
  
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
    
    ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group,
                 const uint8_t device[6], uint8_t targetId, int16_t slotIndex = -1);
    virtual void sendOut(RuntimeLight* const light, bool sendList = false) override;
    virtual bool isExternal() const override { return true; }
    virtual Type portType() const override { return Type::External; }
};

// Function pointer for optional ESP-NOW functionality
extern void (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, RuntimeLight* const light, bool sendList);
