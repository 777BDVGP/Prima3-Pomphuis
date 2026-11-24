#pragma once

// Firmware version — shown on the ABOUT page.  Bump it with every change you upload.
#define FW_VERSION "1.1.1"

// Private defaults (git-ignored wifi_secrets.h, never in the public copy):
// WiFi networks, and the default names of the two borehole users.
#if __has_include("wifi_secrets.h")
	#include "wifi_secrets.h"
#endif
#ifndef DEFAULT_USER_1
	#define DEFAULT_USER_1 "User 1"   // name shown for borehole user 1 (SETTINGS / Tank page)
#endif
#ifndef DEFAULT_USER_2
	#define DEFAULT_USER_2 "User 2"
#endif

// =============================================================================
// 01_Main.h — Global Definitions, System Libraries & Core State Variables
// =============================================================================
// Declares all global flags, hardware pin aliases, library includes, and
// watchdog / WiFi infrastructure shared across the entire firmware.
// =============================================================================

// ─── Runtime Object Presence Flags ───────────────────────────────────────────
// Set to true once the corresponding heap object has been allocated.
// Guards against calling page-build functions before initialisation completes.
	inline bool MainPageObject       = false;  // Home page PageVar list allocated
	inline bool TankPageObject       = false;  // Tank page PageVar list allocated
	inline bool SuspendObject        = false;  // Suspend page PageVar list allocated
	inline bool ElectricValvesObject = false;  // Valve/config page PageVar lists allocated

// ─── Board Revision ──────────────────────────────────────────────────────────
	#define BRD_VERSION 1

// ─── Debug / Production Switch ───────────────────────────────────────────────
// Set DEBUG 1 for development (Serial output enabled).
// Set DEBUG 0 for production  (all DEBUG_PRINT* calls compile to nothing).
	#define DEBUG 0  // 1 = http://192.168.1.120/ and 0 = http://192.168.1.118/

	#if DEBUG
		#define DEBUG_PRINT(x)          Serial.print(x)
		#define DEBUG_PRINTLN(x)        Serial.println(x)
		#define DEBUG_PRINTF(x, ...)    Serial.printf(x, ##__VA_ARGS__)
	#else
		#define DEBUG_PRINT(x)
		#define DEBUG_PRINTLN(x)
		#define DEBUG_PRINTF(x, ...)
	#endif

// ─── Network Configuration (debug vs production IP) ──────────────────────────
	#if DEBUG
		#define NAV_IP   IPAddress(192,168,1,120)
		#define NAV_GW   IPAddress(192,168,1,  1)
		#define NAV_SN   IPAddress(255,255,255,0)
		#define NAV_DNS  IPAddress(192,168,1,  1)
	#else
		#define NAV_IP   IPAddress(192,168,1,118)
		#define NAV_GW   IPAddress(192,168,1,  1)
		#define NAV_SN   IPAddress(255,255,255,0)
		#define NAV_DNS  IPAddress(192,168,1,  1)
	#endif

// ─── Third-Party Library Includes ────────────────────────────────────────────
	#include <WiFi.h>
	#include <WiFiUdp.h>
	#include <HTTPClient.h>           // HTTP client for NTFY push notifications
	#include <SPIFFS.h>               // SPI Flash File System
	#include <ESPAsyncWebServer.h>    // Non-blocking HTTP + WebSocket server
	#include <ArduinoJson.h>          // JSON serialisation / deserialisation
	#include <Ticker.h>               // Software timer used for the hardware watchdog

// ─── Board-Specific Pin Aliases ──────────────────────────────────────────────
	#if defined(ARDUINO_ARCH_ESP8266)
		#define ASPRESET D3    // GPIO 0  — WiFiManager reset button (D3)
		#define LEDOUT   D4    // GPIO 2  — PCB activity LED          (D4)
	#elif defined(ARDUINO_ARCH_ESP32)
		#define ASPRESET 00    // GPIO 0  — Boot button / WiFiManager reset (ESP32-S3 BOOT key)
		#define LEDOUT   15    // GPIO 15 — Blue PCB activity LED           (off-board LED if fitted)

		// On-board WS2812 RGB heartbeat (S3 Mini only — the S2 Mini had no
		// addressable LED here). Driven via the core's RMT-backed
		// rgbLedWrite(), which requires the pioarduino 3.x platform (see
		// platformio.ini). Colour communicates boot phase; brightness kept
		// dim since it's an always-on heartbeat.
		#define HB_RGB_PIN    47
		#define HB_RGB_LEVEL  20
	#endif

// ─── Heartbeat Mode (boot-phase colour for the on-board RGB LED) ─────────────
//   HB_MODE_BOOT    green — booting / attempting to join saved WiFi
//   HB_MODE_PORTAL  red   — no/failed WiFi credentials, captive portal up
//   HB_MODE_NORMAL  blue  — WiFi joined, boot successful, running normally
	enum HbMode : uint8_t { HB_MODE_BOOT, HB_MODE_PORTAL, HB_MODE_NORMAL };
	inline HbMode HeartbeatMode = HB_MODE_BOOT;

// ─── Heap Monitor Thresholds ─────────────────────────────────────────────────
	#define MIN_FREE_HEAP    10000  // Layer 2: background watchdog reboot threshold (10 KB)
	#define HEAP_FREE_MIN    18000  // Layer 1: browse-time guard — total free heap  (18 KB)
	#define HEAP_MAXBLK_MIN  10000  // Layer 1: browse-time guard — largest free block (10 KB)

// ─── Gateway Reachability Guard (MaintainGatewayWatch, 01_Main.cpp) ─────────
// WiFi.status() alone can keep reporting WL_CONNECTED while the device has
// silently dropped off the network (no ping, no ARP reply). Pinging the
// router proves the link actually carries traffic; reboot if it goes silent.
// The ping ALSO acts as a keepalive: regular outbound traffic stops the Omada
// AP/switch/router from ageing this client out (see main.cpp investigation
// note). Keep the interval well under the network's idle/ARP ageing times.
	#define GW_PING_INTERVAL_MS   30000UL    // One ping to the gateway every 30 s
	#define GW_PING_TIMEOUT_MS     2000UL    // Per-ping reply timeout
	#define GW_UNREACHABLE_MS    300000UL    // Reboot after 5 min with no reply

// ─── Watchdog Variables ───────────────────────────────────────────────────────
// The ISR increments watchdogCount every second; LEDToggle() resets it.
// If it exceeds 15 without a reset the firmware forces a reboot.
	inline Ticker secondTick;              // 1-second Ticker driving ISRwatchdog()
	inline volatile int watchdogCount = 0; // Incremented by ISR, reset by LEDToggle()
	inline String localIP;                 // Cached local IP address string

	inline long int cycles = 0;           // General-purpose cycle counter (debug use)

// ─── Pre-Boot WiFi Setup ──────────────────────────────────────────────────────
	inline Ticker MsTick;   // 150 ms Ticker for LED flash during captive-portal phase

// ─── Global System State Flags ────────────────────────────────────────────────
	inline bool NORMAL_OPERATION = true;   // true = system running; false = all outputs inhibited
	inline bool RAIN_OK_DO_IRRI  = true;   // Rain-sensor gate — always true (no physical sensor fitted)
	inline bool MANUAL_STARTED   = false;  // true = borehole pump started manually via button or web
	inline bool FILTER_ON        = false;  // true = filter/circulation pump relay energised