#pragma once

#include "../Config.h"

class Connection;
class Intersection;
class LPLight;

class Port {

  public:
    uint8_t id;
    Connection* connection;
    Intersection* intersection;
    bool direction;
    uint8_t group;
  
    Port(Connection* connection, Intersection* intersection, bool direction, uint8_t group);
    virtual ~Port();
    virtual void sendOut(LPLight* const light, bool sendList = false) = 0;
    virtual bool isExternal() const { return false; }
    
    // Static pool management
    static Port* findById(uint8_t id);
    static void addToPool(Port* port);
    static void removeFromPool(Port* port);
    static uint8_t poolCount() { return poolSize; }
    
  protected:
    
    uint8_t getNextId() {
        static uint8_t lastId = 0;
        return lastId++;
    }
    void handleColorChange(LPLight* const light) const;
    
  private:
    static const uint8_t MAX_PORTS = 200;
    static Port* portPool[MAX_PORTS];
    static uint8_t poolSize;
  
};

class InternalPort : public Port {
    
    public:
        InternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group);
        virtual void sendOut(LPLight* const light, bool sendList = false) override;
};

class ExternalPort : public Port {

  public:
    uint8_t device[6];
    uint8_t targetId;
    
    ExternalPort(Connection* connection, Intersection* intersection, bool direction, uint8_t group, uint8_t device[6]);
    virtual void sendOut(LPLight* const light, bool sendList = false) override;
    virtual bool isExternal() const override { return true; }
};

// Function pointer for optional ESP-NOW functionality
extern void (*sendLightViaESPNow)(const uint8_t* mac, uint8_t id, LPLight* const light, bool sendList);
