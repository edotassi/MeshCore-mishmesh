#pragma once

#include <stdint.h>
#include "Mesh.h"


class LocationProvider {
protected:
    bool _time_sync_needed = true;
    // [mishmesh]
    bool _time_sync_enabled = true;
    // [/mishmesh]

public:
    virtual void syncTime() { _time_sync_needed = true; }
    virtual bool waitingTimeSync() { return _time_sync_needed; }
    // [mishmesh] gate automatic clock writes without powering GPS on/off.
    virtual void setTimeSyncEnabled(bool en) { _time_sync_enabled = en; }
    // [/mishmesh]
    virtual long getLatitude() = 0;
    virtual long getLongitude() = 0;
    virtual long getAltitude() = 0;
    virtual long satellitesCount() = 0;
    virtual bool isValid() = 0;
    virtual long getTimestamp() = 0;
    // [mishmesh] speed over ground (thousandths of a knot), course clockwise
    // from North (thousandths of a degree), and horizontal dilution of
    // precision (tenths) - not every backend parses these, so they default
    // to 0 (unknown) rather than being pure virtual.
    virtual long getSpeed() { return 0; }
    virtual long getCourse() { return 0; }
    virtual uint8_t getHDOP() { return 0; }
    // [/mishmesh]
    virtual void sendSentence(const char * sentence);
    virtual void reset() = 0;
    virtual void begin() = 0;
    virtual void stop() = 0;
    virtual void loop() = 0;
    virtual bool isEnabled() = 0;
};
