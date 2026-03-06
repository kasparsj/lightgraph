#pragma once

#if defined(ARDUINO)

#include "Arduino.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using LgLogMirrorCallback = void (*)(const char* data, size_t len);
inline LgLogMirrorCallback gLgLogMirrorCallback = nullptr;

inline void lgSetLogMirrorCallback(LgLogMirrorCallback callback) {
  gLgLogMirrorCallback = callback;
}

inline void lgLogWriteBytes(const char* data, size_t len) {
  if (data == nullptr || len == 0) {
    return;
  }
  Serial.write(reinterpret_cast<const uint8_t*>(data), len);
#if defined(DEV_ENABLED)
  if (gLgLogMirrorCallback != nullptr) {
    gLgLogMirrorCallback(data, len);
  }
#else
  (void)gLgLogMirrorCallback;
#endif
}

inline void lgLogPrint(const String& value) {
  lgLogWriteBytes(value.c_str(), value.length());
}

inline void lgLogPrint(const char* value) {
  if (value == nullptr) {
    return;
  }
  lgLogWriteBytes(value, strlen(value));
}

inline void lgLogPrint(char value) {
  lgLogWriteBytes(&value, 1);
}

inline void lgLogPrint(signed char value) {
  const String rendered(static_cast<int>(value));
  lgLogWriteBytes(rendered.c_str(), rendered.length());
}

inline void lgLogPrint(unsigned char value) {
  const String rendered(static_cast<unsigned int>(value));
  lgLogWriteBytes(rendered.c_str(), rendered.length());
}

inline void lgLogPrint(const __FlashStringHelper* value) {
  if (value == nullptr) {
    return;
  }
  const String rendered(value);
  lgLogWriteBytes(rendered.c_str(), rendered.length());
}

template <typename T>
inline void lgLogPrint(const T& value) {
  const String rendered(value);
  lgLogWriteBytes(rendered.c_str(), rendered.length());
}

template <typename T>
inline void lgLogPrintln(const T& value) {
  lgLogPrint(value);
  lgLogWriteBytes("\n", 1);
}

inline void lgLogVPrintf(const char* format, va_list args) {
  if (format == nullptr) {
    return;
  }

  va_list argsCopy;
  va_copy(argsCopy, args);
  const int required = vsnprintf(nullptr, 0, format, argsCopy);
  va_end(argsCopy);
  if (required <= 0) {
    return;
  }

  char stackBuffer[256];
  const size_t requiredLen = static_cast<size_t>(required);
  if (requiredLen < sizeof(stackBuffer)) {
    vsnprintf(stackBuffer, sizeof(stackBuffer), format, args);
    lgLogWriteBytes(stackBuffer, requiredLen);
    return;
  }

  char* heapBuffer = static_cast<char*>(malloc(requiredLen + 1));
  if (heapBuffer == nullptr) {
    static const char kLogOom[] = "[log] printf OOM\n";
    lgLogWriteBytes(kLogOom, sizeof(kLogOom) - 1);
    return;
  }
  vsnprintf(heapBuffer, requiredLen + 1, format, args);
  lgLogWriteBytes(heapBuffer, requiredLen);
  free(heapBuffer);
}

inline void lgLogPrintf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  lgLogVPrintf(format, args);
  va_end(args);
}

#define LG_LOG(...) lgLogPrint(__VA_ARGS__)
#define LG_LOGF(...) lgLogPrintf(__VA_ARGS__)
#define LG_LOGLN(...) lgLogPrintln(__VA_ARGS__)

#define LG_RANDOM(...) random(__VA_ARGS__)
#define LG_STRING String

#ifndef MIN
#define MIN(x,y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x,y) (((x) > (y)) ? (x) : (y))
#endif

#else

#include "ofMain.h"

#define LG_LOG(...) ofLog(OF_LOG_WARNING, __VA_ARGS__)
#define LG_LOGF(...) ofLog(OF_LOG_WARNING, __VA_ARGS__)
#define LG_LOGLN ofLogWarning
#define LG_RANDOM ofRandom
#define LG_STRING std::string

#endif
