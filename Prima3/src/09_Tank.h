#pragma once
#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// 09_Tank.h — Tank Level, Filter Pressure & Borehole Management Definitions
// =============================================================================
// Declares the PresureSensor and TankLevel classes with their averaging
// algorithms, configuration fields, and JSON persistence methods.  Also
// declares all global state variables used by MaintainTankFilling() and the
// elapsed-time display in MaintainHomePage().
// =============================================================================

// ─── Sensor Averaging Window ──────────────────────────────────────────────────
#define TANK_AVE_OF  15   // Number of ADC samples averaged for both level and pressure sensors

// =============================================================================
// PresureSensor
// Reads an ADC pin on a configurable tick interval (TIM_FILTER_SENSOR) and
// maintains a 15-point rolling average.  Returns the current pressure in kPa.
// Used for the filter backwash pressure sensor (ADC pin 03).
// =============================================================================
class PresureSensor {
public:
	PresureSensor(int Pin);    // Constructor — assigns the ADC pin and initialises the buffer
	~PresureSensor();          // Destructor
	virtual void Maintain();   // Sample ADC and update the rolling average (call on TIM_FILTER_SENSOR)
	virtual int  getValue();   // Return current pressure in kPa (0–103.4 kPa, 0–15 PSI range)
	void WriteData();          // Serialise calibration settings to /FilterSensorData.json
	void ReadData();           // Deserialise calibration settings from /FilterSensorData.json

	int      IOPin        = 0;      // ADC GPIO pin number
	int      TimerIdx     = 0;      // Timer index used for the averaging tick
	long int RawValue     = 0;      // Most recent raw ADC reading (0–8191, ESP32-S2 13-bit ADC)
	int      RawValueMax  = 8191;   // ADC value at full-scale (400 kPa) — ESP32-S2 13-bit ADC

	long int Idx     = -1;               // Current buffer index; -1 = not yet initialised
	long int AveList[TANK_AVE_OF];       // Circular averaging buffer

	String StrFilterKPa    = "0";  // Formatted kPa string for display (updated by Maintain())
	String StrFilterRawValue = "0"; // Raw ADC average for display (updated by Maintain())
	int    BackwashWarning        = 80;    // Alert threshold in kPa — home page turns red above this value
	bool   FilterPressureExceeded = false; // Latching flag — set true when BackwashWarning is exceeded; must be manually cleared
};

// ─── Tank Fill Mode Strings ───────────────────────────────────────────────────
#define TANK_FILL_SENSE_FLOAT   "Float Switch"    // Use float switch to detect full/empty
#define TANK_FILL_SENSE_LEVEL   "Level Sensor"    // Use level sensor percentage thresholds

#define TANK_FILL_OPER_NORMAL   "Any Time"         // Fill whenever level drops below threshold
#define TANK_FILL_OPER_SHEDULE  "Scheduled Time"   // Only fill during a configured time window

#define TANK_BORHOLE_ENABLE   "Enabled"    // Borehole pump is authorised to run
#define TANK_BORHOLE_DISABLE  "Disabled"   // Borehole pump is blocked regardless of other conditions

// ─── Tank Page Row Index Constants ───────────────────────────────────────────
// Each constant is an index into TankList[] and determines the row order on
// the Tank settings web page.
#define TANK_USER1              0   // User 1 name
#define TANK_USER2              1   // User 2 name
#define TANK_RAW_VALUE          2   // Display only — live ADC raw value from level sensor
#define TANK_SENSOR_100         3   // ADC calibration value for 100% full
#define TANK_SENSOR_100_FILTER  4   // ADC calibration value for 100% (filtered)
#define TANK_SENSOR_0           5   // ADC calibration value for 0% (empty)
#define TANK_FILL_SENSE         6   // Fill detection method: Float Switch or Level Sensor
#define TANK_LEVEL_START_FILL   7   // Level % at which filling starts
#define TANK_LEVEL_STOP_FILL    8   // Level % at which filling stops
#define TANK_FILL_OPER          9   // Fill operation mode: Any Time or Scheduled Time
#define TANK_SHED_START         10  // Scheduled fill window start time (HH:MM)
#define TANK_SHED_STOP          11  // Scheduled fill window stop time (HH:MM)
#define TANK_FILTPUMP           12  // Filter pump start delay after tank full (minutes)
#define TANK_BORE_HOLE_OPER     13  // Borehole enable/disable
#define TANK_BORE_TOUT          14  // Borehole start confirmation timeout (seconds)
#define TANK_BORE_RETRY         15  // Borehole retry interval after failure (minutes)
#define TANK_KPA_VALUE          16  // Display only — live filter pressure in kPa
#define TANK_SENSOR_MAX         17  // Calibration — ADC value at full-scale pressure
#define TANK_FILTER_RAW_VALUE   18  // Display only — live ADC raw value from filter sensor

#define PAGE_VAR_TANK  19   // Total number of tank page PageVar entries
// Note: Warning to Backwash (kPa) and Filter Presure Exceeded moved to /ntfy page

// =============================================================================
// TankLevel
// Manages the ultrasonic/pressure tank level sensor via a 15-point rolling ADC
// average.  Provides level as a percentage (0–100 %) and all fill-control
// parameters used by MaintainTankFilling().
// =============================================================================
class TankLevel
{
public:
	TankLevel(int Pin);       // Constructor — assigns ADC pin and initialises the buffer
	~TankLevel();             // Destructor
	void WriteData();         // Serialise settings to /TankLevelData.json
	void ReadData();          // Deserialise settings from /TankLevelData.json
	int  getValue();          // Return current tank level as a percentage (0–100 %)
	int  getValueNotFilter(); // Return unfiltered (raw last sample) percentage
	void Maintain();          // Sample ADC and advance the rolling average (call on TIM_TANK_SENSOR)

	// ── ADC / Calibration ─────────────────────────────────────────────────────
	int      IOPin          = 0;     // ADC GPIO pin number
	int      TimerIdx       = 0;     // Timer index for the averaging tick
	long int RawValue       = 0;     // Most recent raw ADC reading (0–8191, ESP32-S2 13-bit ADC)
	long int Idx            = -1;    // Averaging buffer index; -1 = not yet initialised
	long int AveList[TANK_AVE_OF];   // Circular averaging buffer

	int RawValueMax    = 7000;  // ADC value corresponding to 100 % (full tank)
	int RawValueMaxFil = 6000;  // Alternative max for filtered reading path
	int RawValueMin    = 500;   // ADC value corresponding to 0 % (empty tank)

	// ── Fill Control Thresholds ────────────────────────────────────────────────
	int StartFillLevel = 70;    // Level % below which filling is triggered
	int StopFillLevel  = 100;   // Level % at which filling stops

	// ── Fill Configuration ────────────────────────────────────────────────────
	String TankFillingSense  = TANK_FILL_SENSE_FLOAT;  // Detection method
	String TankFillingTime   = TANK_FILL_OPER_NORMAL;  // Timing mode
	String TankFillStartTime = "08:30";                // Scheduled start time (HH:MM)
	String TankFillStopTime  = "16:00";                // Scheduled stop time (HH:MM)
	int    FilterPumpDelay   = 10;                     // Minutes before filter pump starts after full

	// ── Borehole Configuration ────────────────────────────────────────────────
	String BoreholOperation  = TANK_BORHOLE_ENABLE;  // Enable or disable borehole use
	int    BoreholeTimeout   = 3;   // Seconds to wait for borehole response after starting
	int    BoreholeRetry     = 3;   // Minutes before retrying a failed borehole start

	// ── User Names ────────────────────────────────────────────────────────────
	String User_1 = DEFAULT_USER_1;    // Display name for User 1 (borehole detector input IIDX_OTHER_PUMP_HB)
	String User_2 = DEFAULT_USER_2;   // Display name for User 2 (borehole detector input IIDX_ME_PUMP_BH)

	String StrTankRawValue = " ";   // Formatted raw ADC value for display on the Tank page
};

// ─── Global Sensor Objects ────────────────────────────────────────────────────
inline PresureSensor *FilterSensor;   // Filter backwash pressure sensor
inline TankLevel     *TankWaterLevel; // Tank level sensor and fill-control object
inline PageVar*       TankList;       // PageVar array for the Tank settings web page

inline String StrTankRawValueAtt   = ATT_GREEN;  // CSS style for the tank raw-value display cell (fixed green)
inline String StrFilterRawValueAtt = ATT_GREEN;  // CSS style for the filter raw-value display cell (fixed green)

// ─── Tank Fill State Flags ────────────────────────────────────────────────────
inline bool TankFillAllowed       = true;   // Overall gate — false when conditions prevent filling
inline bool TankFilterOnOffButton = true;   // Web/button filter state
inline bool TankFillChange        = false;  // true = a fill-state change was detected this cycle
inline bool FloatHigh             = false;  // true = tank full float switch is active
inline bool LevelHigh             = false;  // true = level sensor reads above StopFillLevel
inline bool FloatLow              = false;  // true = tank low float switch is active
inline bool AllowFillingAtTime    = true;   // true = current time is within the scheduled fill window
inline bool TankIsFull            = true;   // true = tank reached stop level; filter delay is active

// ─── Borehole State Tracking ──────────────────────────────────────────────────
// Tracks which subsystem last requested the borehole pump and whether the
// start confirmation is still pending.
inline bool BoreholeStartedByTank  = false;  // Borehole running for tank fill
inline bool BoreholeStartedByIrri  = false;  // Borehole running for irrigation
inline bool BoreholeCheckPending   = false;  // Waiting for borehole input confirmation

// ─── Elapsed-Time Tracking (millis() snapshots) ───────────────────────────────
// Used by MaintainHomePage() to display HH:MM:SS elapsed time for each activity.
inline unsigned long IrriActivityStartMs = 0;    // millis() when the current irrigation zone started
inline int           IrriActivityLine    = -1;   // Active zone index (-1 = no irrigation running)
inline unsigned long TankFillStartMs     = 0;    // millis() when the current tank fill started
inline unsigned long ManualStartMs       = 0;    // millis() when manual borehole was started
inline unsigned long FilterOnStartMs     = 0;    // millis() when the filter pump relay was last turned ON
inline unsigned long User1StartMs        = 0;    // millis() when User_1 started pumping (IIDX_OTHER_PUMP_HB)
inline unsigned long User2StartMs        = 0;    // millis() when User_2 started pumping (IIDX_ME_PUMP_BH)
inline bool          PrevUser1Pumping    = false; // previous state of IIDX_OTHER_PUMP_HB — edge detection
inline bool          PrevUser2Pumping    = false; // previous state of IIDX_ME_PUMP_BH  — edge detection
