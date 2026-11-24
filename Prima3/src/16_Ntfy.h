// =============================================================================
// 16_Ntfy.h — NTFY.sh Push Notification Parameters & Page Declarations
// =============================================================================
#pragma once

// ─── NtfyMsg — FreeRTOS queue message struct ──────────────────────────────────
struct NtfyMsg { char message[256]; char code[64]; };

// =============================================================================
// NtfyParams
// =============================================================================
class NtfyParams
{
public:
	NtfyParams();
	~NtfyParams();
	void WriteData();
	void ReadData();

	String NtfyCode            = " ";        // NTFY topic for User 2 (User 2)
	String NtfyCode2           = " ";        // NTFY topic for User 1 (User 1)
	String SysNameUsr1         = "Prima3A";  // System name used in User 1 NTFY messages
	int    BoreholeMaxTime     = 100;        // Alert when User 2 pump exceeds this many minutes
	int    BoreholeMaxTimeUsr1 = 100;        // Alert when User 1 pump exceeds this many minutes
	int    TankFillMaxHrsUsr2  = 24;         // Alert when User 2 pump inactive for this many hours
	int    TankFillMaxHrsUsr1  = 24;         // Alert when User 1 pump inactive for this many hours
	bool   NtfyOnBoreholeRetry        = true;  // Send NTFY on borehole retry events
	bool   EnableMaxPumpNotifyUsr2    = true;  // Enable max-pump-time alert for User 2
	bool   EnableTankFillNotifyUsr2   = true;  // Enable tank-fill inactivity alert for User 2
	bool   EnableBackwashNotify       = true;  // Enable filter backwash warning NTFY
	bool   EnableMaxPumpNotifyUsr1    = true;  // Enable max-pump-time alert for User 1
	bool   EnableTankFillNotifyUsr1   = true;  // Enable tank-fill inactivity alert for User 1
	bool   RebootOnMaxPumpUsr2        = false; // Reboot system after max-pump-time alert for User 2
};

// ─── NTFY Page Row Index Constants ───────────────────────────────────────────
#define NTFY_CODE            0   // NTFY topic for User 2 (User 2)
#define NTFY_CODE2           1   // NTFY topic for User 1 (User 1)
#define NTFY_MAX_PUMP_TIME   2   // Max pumping time (min) — alert threshold
#define NTFY_TANKFILL_HOURS  3   // Tank fill inactivity alert (hours) — User 2
#define NTFY_BACKWASH        4   // Warning to Backwash (kPa) — from FilterSensor
#define NTFY_FILTER_EXCEEDED 5   // Filter Pressure Exceeded — from FilterSensor
#define NTFY_BH_RETRY            6   // Enable NTFY on borehole retry events
#define NTFY_MAX_PUMP_TIME_USR1  7   // Max pumping time (min) — User 1 alert threshold
#define NTFY_TANKFILL_HOURS2     8   // Tank fill inactivity alert (hours) — User 1
#define NTFY_EN_MAX_PUMP_USR2    9   // Enable checkbox — max-pump-time alert (User 2)
#define NTFY_EN_TANKFILL_USR2   10   // Enable checkbox — tank-fill inactivity alert (User 2)
#define NTFY_EN_BACKWASH        11   // Enable checkbox — backwash warning
#define NTFY_EN_MAX_PUMP_USR1   12   // Enable checkbox — max-pump-time alert (User 1)
#define NTFY_EN_TANKFILL_USR1   13   // Enable checkbox — tank-fill inactivity alert (User 1)
#define NTFY_SYSNAME_USR1       14   // System name used in User 1 NTFY messages
#define NTFY_REBOOT_MAX_PUMP_USR2 15 // Enable checkbox — reboot after max-pump-time alert (User 2)
#define PAGE_VAR_NTFY           16

// ─── Inline Globals ───────────────────────────────────────────────────────────
inline NtfyParams   *NtfyData             = nullptr;
inline PageVar      *NtfyList             = nullptr;
inline String        NtfyStyle            = "";
inline bool          NtfyPageObject       = false;
inline QueueHandle_t NtfyQueue            = NULL;
inline TaskHandle_t  NtfyTaskHandle       = NULL;
inline bool          NtfyBoreholeMaxSent        = false;  // send-once flag for max-pump-time alert (User 2)
inline bool          NtfyBoreholeMaxSentUsr1   = false;  // send-once flag for max-pump-time alert (User 1)
inline bool          NtfyTankFillOverdueSentUsr2 = false;  // send-once flag for tank-fill inactivity alert (User 2)
inline uint32_t      LastUser2PumpMs             = 0;       // millis() when User 2 pump last active (0 = uninitialised)
inline bool          NtfyTankFillOverdueSentUsr1 = false;  // send-once flag for tank-fill inactivity alert (User 1)
inline uint32_t      LastUser1PumpMs             = 0;       // millis() when User 1 pump last active (0 = uninitialised)
