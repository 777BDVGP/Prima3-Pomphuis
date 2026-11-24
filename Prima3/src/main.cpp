// =============================================================================
// main.cpp — Boot Sequence & Cooperative Main Loop
// =============================================================================
//
// Platform  : WEMOS LOLIN S3 Mini (ESP32-S3, dual-core LX7, 240 MHz,
//             4 MB Flash, 2 MB PSRAM) — pin-compatible drop-in for the
//             S2 Mini this project started on.
// Framework : Arduino  (PlatformIO)
//
// Description:
//   Automated irrigation and borehole pump controller for a residential water
//   system.  Manages nine irrigation zones, borehole pump scheduling, tank-level
//   monitoring, filter-pressure sensing, and solenoid-valve configurations.
//   A real-time web interface (HTTP + WebSocket on port 80) provides live
//   status and full configuration without requiring a mobile app or cloud
//   connection.
//
// Key Features:
//   - 9-zone irrigation scheduler with per-zone frequency, duration, and
//     water-source selection (tank or borehole)
//   - Automatic tank filling via float switches or pressure sensor, with a
//     configurable start/stop level and optional scheduled fill window
//   - Borehole pump management with response-confirmation timeout and retry
//   - 7 solenoid valves across 9 named configurations (tank, borehole, manual)
//   - Filter pressure monitoring with backwash alert threshold
//   - 7-day irrigation suspension calendar with per-zone day masking
//   - SPIFFS-backed JSON persistent storage for all settings and schedules
//   - General and irrigation event logs (auto-trimmed to 200 lines each)
//   - WiFiManager captive portal for first-time WiFi setup;
//     static IP defaults to 192.168.1.120, persisted and reloaded via About.json
//   - ArduinoOTA support (OTA partition; upload via PlatformIO -e ota target)
//   - Hardware watchdog with automatic reboot on stall (> 15 s)
//
// -----------------------------------------------------------------------------
// WHY THIS PROJECT DROPPED FreeRTOS TASKS
// -----------------------------------------------------------------------------
// This firmware originally ran three tasks — SystemTask, WebSocketTask and
// TimeCheckTask — all created with xTaskCreatePinnedToCore(..., core 0), all
// at low priority, each body a `for(;;) { work(); vTaskDelay(10ms); }` loop.
//
// That layout bought nothing and cost real memory: all three were pinned to
// core 0 and yielded every 10 ms, so they already ran as cooperative
// round-robin — exactly what a single loop() does, just with three stacks
// and a scheduler in the way (~24 KB of heap gone to task stacks), and left
// OutPort[]/InPort[]/Params/PageVar lists as a shared-state race surface
// touched from more than one task with no mutex. Folding the three task
// bodies into loop() (this file) reclaims that RAM, collapses everything to
// one execution context, and lets the software watchdog catch a real stall.
// (NtfyTask, 16_Ntfy.cpp, stays a task — it does a blocking HTTPClient POST,
// which would stall irrigation/pump control for up to 5 s if run from loop().)
//
// loop() schedule:
//   every  10 ms : LEDToggle, MaintainGatewayWatch, MaintainTimers, PinDebounce,
//                  MaintainMidnightReboot, MaintainHeapGuard,
//                  MaintainButtonStatus, MaintainTankFilling,
//                  IrrigationShedule, MaintainRemotePumpWatchdog
//   every pass   : MaintainWebService (self-timed: OTA 200 ms /
//                  web page 50 ms / home page 500 ms internally), delay(1)
// -----------------------------------------------------------------------------
//
// Boot sequence (setup()):
//   1. Serial init (115 200 baud)
//   2. InitHardware()          — GPIO setup
//   3. InitFileSystem()        — SPIFFS, JSON object creation
//   4. delay(BootupWait)       — configurable startup settle time
//   5. WiFiManager connection  — load IP from About.json (default 192.168.1.120),
//                                captive portal on failure, save actual IP back
//   6. SetupTime()             — NTP sync (za.pool.ntp.org, UTC+2)
//   7. SetupAbout()            — update build stamp in About.json
//   8. SetupOTA()              — arm ArduinoOTA listener
//   9. SetupWebPage()          — HTTP routes + WebSocket
//  10. SetupNTFY()             — create NTFY queue, launch NtfyTask
//  11. IrrigationInit()        — repair stale schedule dates
//  12. UpdateTimerTimes()      — apply loaded timer durations
//  13. MaintainStatusOfValves()— safe initial valve state
//  14. WatchdogSetup()         — arm 1-second Ticker ISR
//
// -----------------------------------------------------------------------------
// INVESTIGATION — "UNREACHABLE AFTER ~24 H" (started 2026-09-26)
// -----------------------------------------------------------------------------
// Symptom: production unit (192.168.1.118) stops responding in the browser
// after roughly a day. The blue heartbeat LED keeps flashing and irrigation /
// pump control carries on, but the unit answers neither ping nor ARP — the
// whole WiFi link is dead, not just the web server. Only a manual reboot
// brought it back. All other ESP devices in the house (.119, .125,
// .112–.114, all on older ESP32/ESP8266 platforms) stay reachable; this is
// the only one on the S3 / Arduino-ESP32 3.x (IDF 5) platform.
//
// Why nothing recovered it: the only existing guard (LEDToggle, 01_Main.cpp)
// checks WiFi.status(), which kept reporting WL_CONNECTED, and loop() never
// stalled, so the software watchdog never fired either.
//
// Mitigation + diagnostics added (commit a19ffac, MaintainGatewayWatch() in
// 01_Main.cpp): ping the router every 30 s; after 5 min with no reply, log and
// reboot. GeneralLog lines to look for when the user brings back a log:
//   "REBOOT - Gateway unreachable 5 min (missed N, status S, RSSI R,
//    heap H, min heap M)"
//       status 3 = WL_CONNECTED → confirms the "zombie connected" state.
//       Low heap / min heap     → memory leak or exhaustion starving lwIP
//                                 (compare to a fresh-boot heap figure).
//       Weak RSSI (< -80 dBm)   → radio / signal problem.
//   "WiFi disconnected, reason N (drops since boot: K)"
//       N is an IDF wifi_err_reason_t (e.g. 8 ASSOC_LEAVE, 15 4WAY_HANDSHAKE_
//       TIMEOUT, 200 BEACON_TIMEOUT, 201 NO_AP_FOUND) — tells whether the
//       router or the ESP dropped the association.
//   Also note the time of these entries relative to the 00:13 scheduled
//   midnight reboot (MaintainMidnightReboot, 02_Time.cpp) — the unit already
//   reboots daily, so uptime never exceeds ~24 h.
//
// Update 2026-09-27 — the gateway ping is most likely a KEEPALIVE, not just a
// detector. Since a19ffac went in the unit has stayed reachable. Working
// theory: the house network is TP-Link Omada, and before the ping the ESP was
// almost purely passive (it waited for inbound requests and rarely sent
// anything itself). Omada APs/switches age silent clients out of their client
// / MAC tables, and Omada's ARP/broadcast handling relies on those tables — so
// ARP requests for .118 stopped reaching the ESP while it still believed it
// was associated (WL_CONNECTED). Regular outbound traffic (one ping every
// 30 s) keeps the AP table, the switch MAC table and the router's ARP entry
// fresh. DO NOT remove MaintainGatewayWatch() as "just diagnostics" — it is
// both the keepalive and the reboot safety net.
// Still to confirm: (1) a week+ of clean running, and (2) no
// "REBOOT - Gateway unreachable" lines in GeneralLog — if those appear, the
// fault is still happening and the watchdog is quietly recovering from it.
// If the exact cause is wanted, check the Omada SSID advanced settings: Group
// Key Update Period, PMF, 802.11r fast roaming, band steering,
// multicast-to-unicast.
// -----------------------------------------------------------------------------
//
// PCB Design (EAGLE):
//   Board file    : WEMOS_ESP32_2S_V5.brd
//   Schematic file: WEMOS_ESP32_2S_V5.sch
//
// Build Settings (PlatformIO / Arduino IDE):
//   Partition Scheme  : OTA-capable (see platformio.ini -e ota env)
//   USB CDC on Boot   : Enabled
//   Core Debug Level  : None
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// loop() runs all maintenance that used to be spread across 3 tasks — give it
// a little more stack than the 8 KB default.
SET_LOOP_TASK_STACK_SIZE(12288);

// ─── File-scope State ────────────────────────────────────────────────────────
static uint32_t lastPrint = 0;   // Timestamp of last PrintTestMsg output (ms)
String PrevMessage = "";          // Last message printed by PrintTestMsg (dedup)
String FSInitLog   = "";          // Collects log lines from InitFileSystem() so
                                  // they can be printed after the boot delay

// ─── FreeRTOS Task Handles ───────────────────────────────────────────────────
// NtfyTask (16_Ntfy.cpp) is the only remaining application task — it does a
// blocking HTTP POST and must not run from loop().
TaskHandle_t TestingTaskHandle   = NULL;   // main.cpp (I/O test, normally disabled)

// =============================================================================
// setup
// =============================================================================

// -----------------------------------------------------------------------------
// setup
// Arduino entry point — runs once on power-on or reset.
// Performs the full boot sequence and launches the three FreeRTOS tasks.
// If WiFi connection fails, the device restarts after 500 ms so the watchdog
// does not need to intervene during early boot.
// Hold ASPRESET (GPIO 0) LOW at power-on to open the WiFi setup hotspot
// "Prima3-Setup" (http://192.168.4.1/) and add / change a network.
// -----------------------------------------------------------------------------
void setup(){
	// ── 1. Serial ────────────────────────────────────────────────────────────
	Serial.begin(115200);
	#if DEBUG
		delay(5000);   // Allow USB CDC to enumerate before first Serial output
	#endif

	// ── 2. GPIO initialisation ───────────────────────────────────────────────
	InitHardware(); 

	// ── 3. SPIFFS + JSON object creation ─────────────────────────────────────
	InitFileSystem();

	// ── 4. Configurable settle delay (default 5 000 ms) ──────────────────────
	delay(Params->BootupWait);
	DEBUG_PRINTLN("\nBootUp New Water System");

	// Print any messages buffered by InitFileSystem() during the delay
	if (FSInitLog.length() > 0) DEBUG_PRINT(FSInitLog);

	// ── 5. WiFiManager connection ─────────────────────────────────────────────
	// Hold IPIN_SYSTEM_DISABLE_ENABLE HIGH at boot to trigger a settings reset
	// SetupWiFi() tries the saved (/WiFiConfig.json) + built-in (wifi_secrets.h)
	// networks, strongest in range first, joining the strongest AP of that SSID.
	// Nothing joined after WIFI_ROUNDS rounds → setup hotspot "Prima3-Setup" for
	// 5 min, then reboot.  Hold ASPRESET (GPIO 0) LOW at power-on = hotspot now.
	if (!SetupWiFi()) {
		SaveGenReport F("REBOOT - WiFi connect failed");
		delay(500);
		ESP.restart();
	}
	else {
		DEBUG_PRINTLN("Server has been Connected!");
		SetupTime();               // Start NTP sync (za.pool.ntp.org, UTC+2)
		SetupAbout();              // Record USB-flash download date if buildStamp changed
		SetupOTA();                // Arm ArduinoOTA — records download date on every OTA upload
		SetupWebPage();            // Register HTTP routes and open WebSocket
		SetupNTFY();               // Create NTFY queue and launch NtfyTask (16_Ntfy.cpp)
		SetupRainGauge();          // Create rain-gauge queues and launch RainGaugeTask (17_RainGauge.cpp)
		IrrigationInit();          // Repair any stale schedule dates after reboot
		// CreatePresureSensors() moved to InitFileSystem() — must run before CreateNtfyParams()
		UpdateTimerTimes();        // Apply loaded timer durations to Timer[] array
		MaintainStatusOfValves();  // Ensure borehole stopped and valves at default

		SetTimerValue(1000, TIM_REMOTE_LED_FLASHING);   // Remote LED flash period
		SaveGenReport F("- Up and Running after Reboot");

		IPAddress locIP = WiFi.localIP();
		localIP = locIP.toString();

		activateTimer(TIM_RESTART_DELAY);     // Used by the web reboot button

		// ── 12. Arm watchdog ──────────────────────────────────────────────────
		WatchdogSetup();   // 1-second Ticker ISR; reboots if not reset within 15 s

		// Start the home-page stabilisation timer — MaintainHomePage() returns
		// early until this expires, preventing transient wrong values on first load.
		activateTimer(TIM_STABILIZE_DELAY);

		// TestingTask — I/O self-test (normally commented out in production)
		//xTaskCreatePinnedToCore(TestingTask, "TestingTask", 4096, NULL, 1, &TestingTaskHandle, 0);

		DEBUG_PRINTLN("Setup complete - entering main loop");
	}
}

// -----------------------------------------------------------------------------
// loop
// Runs all maintenance that used to be spread across SystemTask, WebSocketTask
// and TimeCheckTask (see the header comment at the top of this file for why).
// -----------------------------------------------------------------------------
void loop(){
	// During an OTA flash, do nothing but pump the update stream as fast as
	// possible — anything else (page work, the heap guard's ESP.restart(),
	// a scheduled reboot) can stall or kill the transfer.
	if (OTAInProgress){
		ArduinoOTA.handle();
		delay(1);                     // ~1 kHz — drains the flash stream fast, still yields to WiFi/IDLE
		return;
	}

	const uint32_t now = millis();

	static uint32_t t10 = 0;
	if (now - t10 >= 10){
		t10 = now;
		LEDToggle();                    // LED heartbeat, watchdog reset, ASPRESET, WiFi-loss reboot (01_Main.cpp)
		MaintainGatewayWatch();          // Reboot if the router stops answering pings for 5 min  (01_Main.cpp)
		MaintainTimers();                // Expire any software timers that have elapsed        (02_Time.cpp)
		PinDebounce();                   // Debounce all digital inputs                          (05_ThisSystem.cpp)
		MaintainMidnightReboot();        // Scheduled nightly reboot at 00:13                     (02_Time.cpp)
		MaintainHeapGuard();             // Reboot if free heap drops below MIN_FREE_HEAP         (02_Time.cpp)
		MaintainButtonStatus();          // Physical button toggle processing                     (05_ThisSystem.cpp)
		MaintainTankFilling();           // Tank level and borehole pump management                (09_Tank.cpp)
		IrrigationShedule();             // Irrigation zone scheduler                              (09_Irrigation.cpp)
		MaintainRemotePumpWatchdog();    // Remote /pumpstatus heartbeat timeout                   (05_ThisSystem.cpp)
		MaintainRainGauge();             // RAIN_OK_DO_IRRI from the rain gauge, polled hourly     (17_RainGauge.cpp)
		MaintainFwUpdate();              // ABOUT firmware CHECK / INSTALL (blocking, by hand) + restart after upload (22_Update.cpp)
		MaintainWifiNets();              // Apply WiFi network ADD / DELETE from the /wifi page    (05_WiFi.cpp)
	}

	MaintainWebService();               // ArduinoOTA + web/WebSocket upkeep, self-timed (04_WebPages.cpp)

	delay(1);                            // Yield to AsyncTCP / WiFi / IDLE task
}

// =============================================================================
// Hardware Test Utilities
// =============================================================================
// The following functions are used during development and I/O bring-up only.
// In production, TestingTask is commented out in setup() and these functions
// are never called.  They are retained in the build so they can be re-enabled
// quickly if I/O diagnostics are needed.
// =============================================================================

// -----------------------------------------------------------------------------
// TestingTask
// FreeRTOS task body for the I/O self-test.  Calls TestInPins() every 10 ms.
// To switch to output cycling, replace with TestOutPins().
// Activate by un-commenting the xTaskCreatePinnedToCore call in setup().
// -----------------------------------------------------------------------------
void TestingTask(void *pvParameters) {
	for (;;) {
		// TestOutPins();    // Cycle outputs one by one (destructive — use with care)
		TestInPins();
		vTaskDelay(10 / portTICK_PERIOD_MS);
	}
}

// -----------------------------------------------------------------------------
// PrintTestMsg
// Prints a diagnostic message to Serial with built-in deduplication:
//   - Always prints immediately when the message text changes.
//   - Repeats the same message at most once every <seconds> seconds.
// Prevents Serial flooding when the same condition persists over many task cycles.
// -----------------------------------------------------------------------------
void PrintTestMsg(String Message, uint32_t seconds){
	if (Message != PrevMessage) {
		PrevMessage = Message;
		lastPrint   = millis();
		DEBUG_PRINTLN(Message);
	} else if (millis() - lastPrint >= seconds * 1000UL) {
		lastPrint = millis();
		DEBUG_PRINTLN(Message);
	}
}

// -----------------------------------------------------------------------------
// TestOutPins
// Cycles every output pin HIGH for 2 seconds then LOW, printing the pin
// description to Serial.  The last two outputs (OutPorts-2 limit) are excluded
// to avoid energising safety-critical relays during test.
// WARNING: this function uses blocking delay() — do not call from a task that
// must remain responsive.  Only use when the device is in a safe test state.
// -----------------------------------------------------------------------------
void TestOutPins()
{
	delay(500);
	DEBUG_PRINTLN(" "); DEBUG_PRINTLN("New"); DEBUG_PRINTLN(" ");
	for ( int i = 0 ; i < OutPorts - 2; i++ )
	{
		digitalWrite(OutPort[i].Port, HIGH);
		DEBUG_PRINT("ON: "); DEBUG_PRINTLN(OutPort[i].Desc);
		delay(2000);
		digitalWrite(OutPort[i].Port, LOW);
	}
}

// -----------------------------------------------------------------------------
// TestInPins
// Reads all relevant debounced digital inputs and mirrors their states to the
// corresponding indicator LEDs / relay output.  Used to verify wiring without
// running the full system logic.  Called from TestingTask every 10 ms.
// -----------------------------------------------------------------------------
void TestInPins()
{
	PinDebounce();   // Update InPort[].Input debounce state

	// System enable/disable button → system-disabled LED
	digitalWrite(OutPort[OIDX_SYST_DISABLED_LED].Port,
	             InPort[IIDX_SYSTEM_DISABLE_ENABLE].Input ? HIGH : LOW);

	// User 2 borehole pump sense → User 2 pump LED
	digitalWrite(OutPort[OIDX_MY_PUMP_LED].Port,
	             InPort[IIDX_ME_PUMP_BH].Input ? HIGH : LOW);

	// User 1 borehole pump sense → User 1 pump LED
	digitalWrite(OutPort[OIDX_OTHER_PUMP_LED].Port,
	             InPort[IIDX_OTHER_PUMP_HB].Input ? HIGH : LOW);

	// Tank full float switch → tank high-float LED
	digitalWrite(OutPort[OIDX_FLOAT_UP_LED].Port,
	             InPort[IIDX_TANK_FULL].Input ? HIGH : LOW);

	// Tank low float switch → tank low-float LED
	digitalWrite(OutPort[OIDX_FLOAT_LOW_LED].Port,
	             InPort[IIDX_TANK_LOW].Input ? HIGH : LOW);

	// Manual borehole start/stop switch → manual pump LED
	digitalWrite(OutPort[OIDX_PUMP_MANUAL_LED].Port,
	             InPort[IIDX_MAN_START_STOP].Input ? HIGH : LOW);

	// Tank filter on/off switch → filter pump relay
	digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port,
	             InPort[IIDX_TANK_FILTER_ON_OFF].Input ? HIGH : LOW);
}

