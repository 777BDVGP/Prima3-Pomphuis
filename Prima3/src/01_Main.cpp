// =============================================================================
// 01_Main.cpp — Hardware Initialisation, Watchdog & LED Heartbeat
// =============================================================================
// Implements board-level startup, the hardware watchdog ISR, the WiFi-loss
// reboot guard, and the periodic LED heartbeat called from loop() (main.cpp).
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"
#include "ping/ping_sock.h"

// ── On-board WS2812 RGB heartbeat (GPIO 47, S3 Mini only) ───────────────────
// Driven through the RMT peripheral via the core helper rgbLedWrite() — exact
// hardware timing, no bit-bang. Requires the pioarduino arduino-esp32 3.x
// platform (see platformio.ini); the old 2.0.14 core's RMT driver rejected
// GPIO 47.
static bool hbRgbReady = false;

// Colour per HeartbeatMode (01_Main.h) — indexed by the enum value.
static const uint8_t HB_MODE_COLOR[][3] = {
	{ 0, HB_RGB_LEVEL, 0 },   // HB_MODE_BOOT    green
	{ HB_RGB_LEVEL, 0, 0 },   // HB_MODE_PORTAL  red
	{ 0, 0, HB_RGB_LEVEL },   // HB_MODE_NORMAL  blue
};

static void hbRgbInit(){
	rgbLedWrite(HB_RGB_PIN, 0, 0, 0);   // first call inits the RMT channel
	hbRgbReady = true;
}

// Lights HeartbeatMode's colour when turning on; always off (0,0,0) when
// turning off, so the LED reads as a clean on/off blink in that colour.
static void hbRgbSend(bool on){
	if (!hbRgbReady) return;
	if (!on) { rgbLedWrite(HB_RGB_PIN, 0, 0, 0); return; }
	const uint8_t* c = HB_MODE_COLOR[HeartbeatMode];
	rgbLedWrite(HB_RGB_PIN, c[0], c[1], c[2]);
}

// Sets the heartbeat colour for the current boot/connection phase (see
// HbMode in 01_Main.h). Called from 05_WiFi.cpp as the boot sequence
// progresses; takes effect on the next LED toggle.
void HeartbeatSetMode(HbMode mode){
	HeartbeatMode = mode;
}

// -----------------------------------------------------------------------------
// InitHardware
// Configures the on-board LED and the boot/reset button, then delegates all
// system I/O pin setup to SetupSystemIOs() in 05_ThisSystem.cpp.
// -----------------------------------------------------------------------------
void InitHardware(){
	pinMode(LEDOUT, OUTPUT);
	digitalWrite(LEDOUT, HIGH);    // Turn LED on immediately as a power-on indicator
	hbRgbInit();
	pinMode(ASPRESET, INPUT_PULLUP);

	SetupSystemIOs();              // Configure all application input / output pins
}

// -----------------------------------------------------------------------------
// WatchdogSetup
// Attaches a 1-second Ticker to ISRwatchdog().  Must be called once during
// setup() after WiFi connects and all tasks are about to start.
// -----------------------------------------------------------------------------
void WatchdogSetup() {
	secondTick.attach(1, ISRwatchdog);
}

// -----------------------------------------------------------------------------
// ISRwatchdog  (ISR — called every 1 second by Ticker)
// Increments watchdogCount.  If LEDToggle() has not reset the counter within
// 15 seconds the firmware concludes a task has stalled and forces a reboot.
// -----------------------------------------------------------------------------
void ISRwatchdog() {
	watchdogCount++;
	if (watchdogCount > 15)   // > 15 seconds without a reset = task stall
	{
		SaveGenReport F("REBOOT - Watchdog");
		delay(500);
		ESP.restart();
	}
}

// -----------------------------------------------------------------------------
// LEDToggle  (called every 10 ms from loop())
// Performs three duties on each call:
//   1. Resets the watchdog counter so ISRwatchdog() knows tasks are alive.
//   2. Monitors the ASPRESET button; holds for TIM_ASP_RESET (5 s) to erase
//      saved WiFi networks (/WiFiConfig.json) and reboot onto the built-in one.
//   3. Reboots the controller if WiFi has been absent for TIM_WIFI_RECONNECT
//      (60 s), ensuring recovery if the router drops offline.
//   4. Toggles the activity LED at TIM_LED_FLASHING_SPEED (600 ms) as a
//      visible heartbeat confirming the firmware is running.
// -----------------------------------------------------------------------------
void LEDToggle(){

	// Reset watchdog — proves loop() is still running
	watchdogCount = 0;

	// ── WiFiManager Reset ─────────────────────────────────────────────────────
	// Hold ASPRESET LOW for 5 seconds to erase saved WiFi credentials and
	// reboot into Access Point mode for reconfiguration.
	if (!digitalRead(ASPRESET)){
		if(TimerExpired(TIM_ASP_RESET)){
			SaveGenReport F("RESET TO ASP");
			delay(1000);
			ClearWifiSettings();
			SaveGenReport F("WiFi reset triggered by hardware button");
			delay(500);
			ESP.restart();
		}
	}
	else{
		activateTimer(TIM_ASP_RESET);   // Keep timer alive while button is released
	}

	// ── WiFi-Loss Reboot Guard ────────────────────────────────────────────────
	// Start the reconnect timer on first disconnect; reboot if it expires.
	// Deactivate the timer whenever WiFi is healthy.
	if (WiFi.status() != WL_CONNECTED) {
		if (!Timer[TIM_WIFI_RECONNECT].Status) {
			activateTimer(TIM_WIFI_RECONNECT);
		} else if (TimerExpired(TIM_WIFI_RECONNECT)) {
			SaveGenReport F("REBOOT - WiFi Lost");
			delay(500);
			ESP.restart();
		}
	} else {
		deactivateTimer(TIM_WIFI_RECONNECT);
	}

	// ── Activity LED Heartbeat ────────────────────────────────────────────────
	// Toggle the LED at the configured flash rate as a visual execution indicator.
	if(TimerExpired(TIM_LED_FLASHING_SPEED)){
		activateTimer(TIM_LED_FLASHING_SPEED);
		bool on = !digitalRead(LEDOUT);
		digitalWrite(LEDOUT, on);
		hbRgbSend(on);
	}
}

// ── Gateway reachability guard state ────────────────────────────────────────
// Written from the ping task / WiFi event task, read from loop().
static esp_ping_handle_t  gwPing          = nullptr;
static volatile uint32_t  gwLastReplyMs   = 0;
static volatile uint32_t  gwMissed        = 0;   // Consecutive ping timeouts
static volatile uint32_t  wifiDropCount   = 0;   // STA_DISCONNECTED events seen
static volatile uint8_t   wifiDropReason  = 0;   // wifi_err_reason_t of the last one

static void gwPingReply(esp_ping_handle_t, void*)   { gwLastReplyMs = millis(); gwMissed = 0; }
static void gwPingTimeout(esp_ping_handle_t, void*) { gwMissed++; }

// -----------------------------------------------------------------------------
// MaintainGatewayWatch  (called every 10 ms from loop())
// WiFi.status() can keep saying WL_CONNECTED after the device has silently
// dropped off the network — it then answers neither ping nor ARP, while
// loop() keeps running (heartbeat LED flashing) so neither the software
// watchdog nor the WiFi-loss guard in LEDToggle() fires. This pings the
// router every GW_PING_INTERVAL_MS from the IDF ping task (non-blocking) and
// reboots if no reply arrives for GW_UNREACHABLE_MS.
// The ping doubles as a KEEPALIVE: before it existed the unit was almost
// purely passive and the Omada network aged it out (no ARP reply to anyone
// while still WL_CONNECTED). Since the ping went in it has stayed reachable,
// so this function must stay even once diagnostics are no longer needed.
// Also logs every WiFi disconnect event with its reason code, so the
// GeneralLog shows what the link was doing before a reboot.
// -----------------------------------------------------------------------------
void MaintainGatewayWatch(){
	static uint32_t loggedDrops = 0;
	if (wifiDropCount != loggedDrops){
		loggedDrops = wifiDropCount;
		SaveGenReport("WiFi disconnected, reason " + String(wifiDropReason) +
		              " (drops since boot: " + String(loggedDrops) + ")");
	}

	if (!gwPing){
		if (WiFi.status() != WL_CONNECTED) return;

		WiFi.onEvent([](WiFiEvent_t, WiFiEventInfo_t info){
			wifiDropReason = info.wifi_sta_disconnected.reason;
			wifiDropCount++;
		}, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

		esp_ping_config_t cfg = ESP_PING_DEFAULT_CONFIG();
		cfg.target_addr.type             = IPADDR_TYPE_V4;
		cfg.target_addr.u_addr.ip4.addr  = static_cast<uint32_t>(WiFi.gatewayIP());
		cfg.count                        = ESP_PING_COUNT_INFINITE;
		cfg.interval_ms                  = GW_PING_INTERVAL_MS;
		cfg.timeout_ms                   = GW_PING_TIMEOUT_MS;

		esp_ping_callbacks_t cbs = {};
		cbs.on_ping_success = gwPingReply;
		cbs.on_ping_timeout = gwPingTimeout;

		gwLastReplyMs = millis();
		if (esp_ping_new_session(&cfg, &cbs, &gwPing) != ESP_OK){
			gwPing = nullptr;
			return;
		}
		esp_ping_start(gwPing);
		return;
	}

	// Read the reply stamp before millis() so a reply landing in between can
	// never make the difference wrap negative.
	uint32_t lastReply = gwLastReplyMs;
	if (millis() - lastReply > GW_UNREACHABLE_MS){
		SaveGenReport("REBOOT - Gateway unreachable " + String(GW_UNREACHABLE_MS / 60000UL) +
		              " min (missed " + String(gwMissed) + ", status " + String(WiFi.status()) +
		              ", RSSI " + String(WiFi.RSSI()) + ", heap " + String(ESP.getFreeHeap()) +
		              ", min heap " + String(ESP.getMinFreeHeap()) + ")");
		delay(500);
		ESP.restart();
	}
}

// -----------------------------------------------------------------------------
// ISR_LED_Setup  (ISR — called every 150 ms by MsTick during captive portal)
// Flashes the LED while the captive portal is open so the user knows the
// device is waiting for WiFi credentials. rgbLedWrite() is RMT-backed (not a
// bit-bang), so it's safe to call from here — mirrors GPIO 15 and the
// on-board LED together (red, HB_MODE_PORTAL, set by runCaptivePortal() in
// 05_WiFi.cpp).
// -----------------------------------------------------------------------------
void ISR_LED_Setup() {
	bool on = !digitalRead(LEDOUT);
	digitalWrite(LEDOUT, on);
	hbRgbSend(on);
}

// -----------------------------------------------------------------------------
// UpdateTimerTimes
// Propagates user-configurable borehole and filter-pump delay values from the
// TankLevel object into the software timer table.  Call this after loading or
// saving TankLevel settings so runtime timeouts stay in sync with stored values.
// -----------------------------------------------------------------------------
void UpdateTimerTimes(){
	SetTimerValue(TankWaterLevel->BoreholeTimeout, TIM_BOREHOLE_TOUT );
	SetTimerValue(TankWaterLevel->BoreholeRetry,   TIM_BOREHOLE_RETRY);
	SetTimerValue(TankWaterLevel->FilterPumpDelay, TIM_TANK_ON_DELAY );
}
