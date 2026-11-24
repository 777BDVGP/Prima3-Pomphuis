#pragma once

// =============================================================================
// 17_RainGauge.h — RAIN_OK_DO_IRRI from the RainGauge_PIO unit
// =============================================================================
// Settings page: "Rain Gauge IP" (e.g. http://192.168.1.124/) + checkbox.
//
//   Checkbox OFF -> RAIN_OK_DO_IRRI = true (irrigate as normal, gauge ignored).
//   Checkbox ON  -> every RG_POLL_MS (1 hour), and right after the Settings
//                   page is saved, GET <Rain Gauge IP>api/irrigate and copy
//                   its "irrigate" true/false into RAIN_OK_DO_IRRI.
//
// The scheduler (IrrigationShedule, 09_Irrigation.cpp) is unchanged: when a
// zone with "Respect Rainfall = Yes" is due, RAIN_OK_DO_IRRI false skips it.
//
// Gauge not reachable: the last good answer is kept for RG_STALE_MS (3 h);
// after that RAIN_OK_DO_IRRI falls back to true, so the garden is never left
// dry because of a network or gauge fault.
//
// The HTTP GET blocks (up to ~6 s), so — like NtfyTask — it runs in its own
// small FreeRTOS task (RainGaugeTask).  The task only does the GET; loop()
// (MaintainRainGauge) owns RAIN_OK_DO_IRRI, the status text and the logging.
// =============================================================================

	#define RG_POLL_MS        (60UL * 60UL * 1000UL)        // poll every hour
	#define RG_STALE_MS       (3UL * 60UL * 60UL * 1000UL)  // keep last answer this long on failure
	#define RG_HTTP_TIMEOUT   3000                          // ms, connect and read

	inline String RainGaugeStatus = "Not active";   // shown on the Settings page
	// Green / red badge under the IP row: what RAIN_OK_DO_IRRI is right now.
	inline String RainGaugeBadge      = "";
	inline String RainGaugeBadgeStyle = "";
	// The status text is long: let it wrap (the page CSS sets nowrap on a row's
	// last cell, which would squeeze the description column to one letter).
	inline String RainGaugeStatStyle = "color: black; font-size:13px; white-space:normal; display:inline-block; text-align:left";

	// loop() -> task: URL to fetch
	struct RainGaugeReq {
		char url[96];
	};

	// task -> loop(): result of one fetch
	struct RainGaugeRes {
		bool ok;               // HTTP 200 and valid JSON with "irrigate"
		bool irrigate;
		bool sim;              // gauge is in simulation mode
		int  httpCode;         // <0 = connection error (HTTPClient codes)
		char reason[120];
	};

	inline QueueHandle_t RainGaugeReqQueue   = NULL;
	inline QueueHandle_t RainGaugeResQueue   = NULL;
	inline TaskHandle_t  RainGaugeTaskHandle = NULL;
	inline volatile bool RainGaugePollNow    = true;   // set by Settings save -> poll at once
