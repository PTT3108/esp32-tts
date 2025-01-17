#ifndef DEBUG_H
#define DEBUG_H

#include "VA_OPT.h"
#include "Arduino.h"
/**
 * Debug logging macros.
 * 
 * - Define DEBUG_LOG to enable basic logging.
 * - Define DEBUG_LOG_VERBOSE to enable verbose logging (implies DEBUG_LOG).
 * 
 * Macro Definitions:
 * - ERRLN(msg, ...): Print error messages.
 * - DBGCR: Print a newline.
 * - DBGW(c): Write a single byte.
 * - DBG(msg, ...): Print formatted debug message.
 * - DBGLN(msg, ...): Print formatted debug message with a newline.
 * - DBGVCR: Verbose version of DBGCR.
 * - DBGVW(c): Verbose version of DBGW(c).
 * - DBGV(...): Verbose version of DBG(...).
 * - DBGVLN(...): Verbose version of DBGLN(...).
 * 
 * Set LOGGING_UART to specify the UART instance if not using Serial.
 **/

// UART Configuration for TARGET_TX (default)
extern Stream *NodeBackpack;

#if defined(PLATFORM_ESP32_S3)
  #define LOGGING_UART (Serial)
#else
  #define LOGGING_UART (*NodeBackpack)
#endif

// Function declarations
void debugPrintf(const char *fmt, ...);

#if defined(LOG_INIT)
  void debugCreateInitLogger();
  void debugFreeInitLogger();
#else
  #define debugCreateInitLogger()
  #define debugFreeInitLogger()
#endif

// Error logging macro
#define ERRLN(msg, ...) IFNE(__VA_ARGS__)({ \
      LOGGING_UART.print("ERROR: "); \
      debugPrintf(msg, ##__VA_ARGS__); \
      LOGGING_UART.println(); }, LOGGING_UART.println("ERROR: " msg))

// Basic debug macros
#define DBGCR LOGGING_UART.println()
#define DBGW(c) LOGGING_UART.write(c)

#ifndef LOG_USE_PROGMEM
  #define DBG(msg, ...) debugPrintf(msg, ##__VA_ARGS__)
  #define DBGLN(msg, ...)              \
    do {                                \
      debugPrintf(msg, ##__VA_ARGS__);  \
      LOGGING_UART.println();           \
    } while (0)
#else
  #define DBG(msg, ...) debugPrintf(PSTR(msg), ##__VA_ARGS__)
  #define DBGLN(msg, ...)                \
    do {                                  \
      debugPrintf(PSTR(msg), ##__VA_ARGS__); \
      LOGGING_UART.println();             \
    } while (0)
#endif

// Verbose debug macros
#if defined(DEBUG_LOG_VERBOSE)
  #define DBGVCR DBGCR
  #define DBGVW(c) DBGW(c)
  #define DBGV(...) DBG(__VA_ARGS__)
  #define DBGVLN(...) DBGLN(__VA_ARGS__)
#else
  #define DBGVCR
  #define DBGVW(c)
  #define DBGV(...)
  #define DBGVLN(...)
#endif

#endif // DEBUG_H
