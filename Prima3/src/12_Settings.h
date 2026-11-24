#pragma once
#include <memory>

// =============================================================================
// 12_Settings.h — System Parameters Class, Page Buffer & Settings Page Globals
// =============================================================================
// Declares the AllParams class (system name, logging flags, boot delay) with
// JSON persistence, and the 8 KB static PageBuf used by all WT_* page builders
// to avoid repeated heap fragmentation from String += operations.
// =============================================================================

// =============================================================================
// AllParams
// Holds global system configuration that is stored in /AllParams.json.
// =============================================================================
class AllParams
{
public:
	AllParams();    // Constructor
	~AllParams();   // Destructor
	void Maintain();  // Reserved for future periodic maintenance logic
	void WriteData(); // Serialise settings to /AllParams.json
	void ReadData();  // Deserialise settings from /AllParams.json

	// ── System Settings ───────────────────────────────────────────────────────
	String SysName         = "New Water Controller"; // Displayed in the H01 page header
	bool   LogGeneralEvents    = true;   // Append timestamped entries to GeneralLog.txt
	bool   LogIrrigationEvents = true;   // Append timestamped entries to IrrigationLog.txt
	bool   LogBoreholeUsedEvents = true;  // Append timestamped entries to BoreholeUsedLog.txt
	bool   LogTankFilterEvents = true;   // Append timestamped entries to TankFilterLog.txt
	int    BootupWait       = 5000;   // Delay (ms) after InitFileSystem() before WiFi connect

	// ── Rain gauge (17_RainGauge.cpp) — sets RAIN_OK_DO_IRRI hourly ─────────
	bool   RainGaugeEnable  = false;                     // false -> RAIN_OK_DO_IRRI always true
	String RainGaugeURL     = "http://192.168.1.124/";   // RainGauge_PIO unit

	// ── ABOUT page ──────────────────────────────────────────────────────────
	bool   ShowOtaInfo      = false;   // ABOUT: show the PC-upload (ArduinoOTA) rows — upload works either way

};

	inline AllParams *Params;   // Global system parameters object

// ─── Settings Page Row Index Constants ───────────────────────────────────────
	#define SET_SYSNAME           0   // System name text input
	#define SET_LOG_GENERAL       1   // Log general events checkbox
	#define SET_LOG_IRRIGATION    2   // Log irrigation events checkbox
	#define SET_LOG_WATERUSED     3   // Log water-used events checkbox
	#define SET_LOG_TANKFILTER    4   // Log tank-filter events checkbox
	#define SET_BOOTUP_DELAY      5   // Boot delay number input (ms)

	#define PAGE_VAR_SETTINGS_BASE 6  // Rows rendered by the generic per-row loop

	#define SET_RAINGAUGE_EN      6   // Rain gauge — enable checkbox
	#define SET_RAINGAUGE_URL     7   // Rain gauge — IP / URL
	#define SET_RAINGAUGE_STAT    8   // Rain gauge — live status (display only, id "V108")
	#define SET_RAINGAUGE_BADGE   9   // Rain gauge — green IRRIGATE / red NO IRRIGATION badge (display only, id "V109")

	#define SET_SHOW_OTA         10   // ABOUT: show the PC-upload (OTA) rows checkbox

	#define PAGE_VAR_SETTINGS 11  // Total number of settings page PageVar entries

// =============================================================================
// Static Page Build Buffer  (WT_* table content only)
// =============================================================================
// WT_* table builder functions write into PageBuf.  8 KB is large enough for
// the biggest WT table (~5.9 KB for Valve Names / Suspend / Tank).
// =============================================================================
	inline char   PageBuf[8000];
	inline size_t PageBufPos = 0;

	inline void   resetPageBuf()                   { PageBufPos = 0; }
	inline void   appendToPageBuf(const char *s)   { while (*s && PageBufPos + 1 < sizeof(PageBuf)) PageBuf[PageBufPos++] = *s++; }
	inline void   appendToPageBuf(const String &s) { for (size_t i = 0; i < s.length() && PageBufPos + 1 < sizeof(PageBuf); ++i) PageBuf[PageBufPos++] = s[i]; }
	inline size_t getPageBufLen()                  { PageBuf[PageBufPos] = '\0'; return PageBufPos; }

// =============================================================================
// HTML Assembly Buffer  (full page — used by every page handler)
// =============================================================================
// HtmlBuf is allocated from the 2 MB PSRAM once in InitFileSystem() via
// ps_malloc — costs only 4 bytes of DRAM for the pointer.  Putting 22 KB in
// DRAM .bss starved the WiFi heap and caused boot crashes.
// Every CompilePage_* handler calls resetHtmlBuf(), fills via appendHtml*(),
// then sends with String(HtmlBuf, getHtmlBufLen()).
// =============================================================================
	inline constexpr size_t HTML_BUF_SIZE = 22000;
	inline char*   HtmlBuf    = nullptr;   // points into PSRAM after InitFileSystem()
	inline size_t  HtmlBufPos = 0;

	inline void   resetHtmlBuf()  { HtmlBufPos = 0; if (HtmlBuf) HtmlBuf[0] = '\0'; }
	inline size_t getHtmlBufLen() { if (HtmlBuf) HtmlBuf[HtmlBufPos] = '\0'; return HtmlBufPos; }

	// Append a null-terminated C-string into HtmlBuf (memcpy — fast)
	inline void appendHtmlStr(const char *s) {
		if (!HtmlBuf) return;
		size_t n = strlen(s);
		if (HtmlBufPos + n < HTML_BUF_SIZE - 1) {
			memcpy(HtmlBuf + HtmlBufPos, s, n);
			HtmlBufPos += n;
			HtmlBuf[HtmlBufPos] = '\0';
		}
	}

	// Append exactly n bytes (for PageBuf content which has no null at position n)
	inline void appendHtmlBuf(const char *s, size_t n) {
		if (!HtmlBuf || !n) return;
		if (HtmlBufPos + n < HTML_BUF_SIZE - 1) {
			memcpy(HtmlBuf + HtmlBufPos, s, n);
			HtmlBufPos += n;
			HtmlBuf[HtmlBufPos] = '\0';
		}
	}

	inline PageVar* SettingsList;                          // PageVar array for the Settings web page
	inline String   SetStyle = "color: black; font-size:14px"; // Default label style for settings rows

// =============================================================================
// Remote Pump API State  (used by 12_Settings.cpp handlers + 05_ThisSystem.cpp watchdog)
// =============================================================================
	inline unsigned long RemoteHeartbeatMs   = 0;     // millis() of last /pumpstatus call; 0 = never
	inline bool          RemoteControlActive = false; // true while pump is under remote control

// =============================================================================
// sendHtmlPage — Stream page from PSRAM snapshot; zero DRAM copy.
// =============================================================================
// ps_malloc() copies HtmlBuf into a fresh PSRAM buffer for this response.
// The filler callback feeds it to ESPAsyncWebServer in TCP-sized chunks with
// Content-Length set so the browser renders immediately (no chunked encoding).
// std::shared_ptr frees the PSRAM snapshot when the response is delivered or
// cancelled — no DRAM String copy, no heap fragmentation from page delivery.
// =============================================================================
	inline void sendHtmlPage(AsyncWebServerRequest *request) {
		size_t len = getHtmlBufLen();
		if (!HtmlBuf || !len) { request->send(500); return; }
		char* raw = (char*)ps_malloc(len);
		if (!raw)  { request->send(500); return; }
		memcpy(raw, HtmlBuf, len);
		auto snap = std::shared_ptr<char>(raw, [](char* p){ free(p); });
		request->send("text/html", len,
			[snap, len](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
				size_t remaining = len - index;
				if (!remaining) return 0;
				size_t n = remaining < maxLen ? remaining : maxLen;
				memcpy(buf, snap.get() + index, n);
				return n;
			}
		);
	}
