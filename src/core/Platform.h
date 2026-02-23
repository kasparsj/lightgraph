#pragma once

#if defined(ARDUINO)

#include "Arduino.h"

#ifdef LOG_FILE
#define LP_LOG(...) do { Serial.print(__VA_ARGS__); logMessage(__VA_ARGS__); } while (0)
#define LP_LOGF(...) do { Serial.printf(__VA_ARGS__); logMessageF(__VA_ARGS__); } while (0)
#define LP_LOGLN(...) do { Serial.println(__VA_ARGS__); logMessage(__VA_ARGS__); } while (0)
#else
#define LP_LOG(...) Serial.print(__VA_ARGS__)
#define LP_LOGF(...) Serial.printf(__VA_ARGS__)
#define LP_LOGLN(...) Serial.println(__VA_ARGS__)
#endif

#define LP_RANDOM(...) random(__VA_ARGS__)
#define LP_STRING String

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

#else

#include "ofMain.h"

#define LP_LOG(...) ofLog(OF_LOG_WARNING, __VA_ARGS__)
#define LP_LOGF(...) ofLog(OF_LOG_WARNING, __VA_ARGS__)
#define LP_LOGLN ofLogWarning
#define LP_RANDOM ofRandom
#define LP_STRING std::string

#endif
