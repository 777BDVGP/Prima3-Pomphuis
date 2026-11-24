#pragma once

// =============================================================================
// 09_Irrigation.h — Irrigation Zone Definitions, Data Classes & Schedule Engine
// =============================================================================
// Defines the 9-zone irrigation configuration (IrrigateSelection), the
// persistence class (Irrigation), and the schedule state machine (SheduleIrri /
// SheduleLines) that drives automated and manual zone activation.
// =============================================================================

	#define IRRIGATE_SELECTIONS   9   // Total number of configurable irrigation zones
	#define IRRIGATE_SEL_1        0
	#define IRRIGATE_SEL_2			1
	#define IRRIGATE_SEL_3			2
	#define IRRIGATE_SEL_4			3
	#define IRRIGATE_SEL_5			4
	#define IRRIGATE_SEL_6			5
	#define IRRIGATE_SEL_7			6
	#define IRRIGATE_SEL_8			7
	#define IRRIGATE_SEL_9			8

	// Zone active state strings (stored in IrrigateSelection.Status)
	#define IRRI_STATUS_ACTIVE    "Active"      // Zone is enabled for scheduling
	#define IRRI_STATUS_INACTIVE  "Not Active"  // Zone is disabled — will not run

	// Operating mode strings (stored in IrrigateSelection.OperMode)
	#define IRRI_MODE_MANUAL   "Manual"     // Zone must be started manually via the web UI
	#define IRRI_MODE_SHEDULE  "Sheduled"   // Zone runs automatically on its schedule

	// Rain gate strings (stored in IrrigateSelection.RespectRainfail)
	#define IRRI_RESP_RAIN_YES  "Yes"  // Skip this zone if rain has been detected
	#define IRRI_RESP_RAIN_NO   "No"   // Ignore rain state — always irrigate on schedule

	// Water source strings (stored in IrrigateSelection.WaterSource)
	#define IRRI_SOURCE_TANK      "Tank"      // Use tank water (valve config REL_TANK)
	#define IRRI_SOURCE_BOLEHOLE  "Borehole"  // Use borehole water (valve config REL_IRRILINE)

	// Irrigation web button state strings (returned by IrriObject->GetOption())
	#define IRRI_STATUS_START  "START"       // Zone is idle — button starts it
	#define IRRI_STATUS_STOP   "BUSY - STOP" // Zone is active — button stops it
	#define IRRI_STATUS_SHED   "SCHEDULED"   // Zone is queued — button cancels it

	// Per-zone per-day suspension strings (stored in IrrigateSelection.SuspendMon … SuspendALL)
	// Note: SUSPEND_YES/NO in 09_Suspend.h are bool; these are the String equivalents.
	#define IRRI_SUSP_YES  "Yes"  // This zone is suspended on the given day
	#define IRRI_SUSP_NO   "No"   // This zone runs normally on the given day

	// WeekDay match values (1-based; matches WeekDay global set by CompileDateTime)
	#define DAY_SUNDAY   1
	#define DAY_MONDAY   2
	#define DAY_TUEDAY   3
	#define DAY_WEDDAY   4
	#define DAY_THUDAY   5
	#define DAY_FRIDAY   6
	#define DAY_SATDAY   7

	inline int  CurrentIrriSetup        = 0;
	inline bool IrrigSetupObject        = false;
	inline bool IrrigationOverviewSync  = false;   // Set true on WS connect to force button resync

	#define IRRIG_SETUP_FIELDS 18

	inline PageVar* IrrigSetupList;

// =============================================================================
// IrrigateSelection
// Stores all configuration and scheduling state for a single irrigation zone.
// One instance per zone; 9 instances are held in Irrigation::Selection[].
// =============================================================================
class IrrigateSelection
{
public:
	String GetPeriodToIrrigateSeconds();

	String Status              = IRRI_STATUS_INACTIVE;
	String SelectDescription   = "Not Used";
	String ConfigLine          = "Zone 1";
	String OperMode            = IRRI_MODE_MANUAL;
	String NextDateToIrrigate  = "2024-01-01";
	String NextTimeToIrrigate  = "06:00";
	String PeriodToIrrigate    = "00:10";
	int    IrrigationFreq      = 1;  // In Days
	String WaterSource         = IRRI_SOURCE_TANK;
	String RespectRainfail     = IRRI_RESP_RAIN_NO;
	String SuspendMon          = IRRI_SUSP_NO;
	String SuspendTue          = IRRI_SUSP_NO;
	String SuspendWed          = IRRI_SUSP_NO;
	String SuspendThu          = IRRI_SUSP_NO;
	String SuspendFri          = IRRI_SUSP_NO;
	String SuspendSat          = IRRI_SUSP_NO;
	String SuspendSun          = IRRI_SUSP_NO;
	String SuspendALL          = IRRI_SUSP_NO;

	int SelectDescriptionID;
	int StatusID;
	int ConfigLineID;
	int OperModeID;
	int NextDateToIrrigateID;
	int NextTimeToIrrigateID;
	int PeriodToIrrigateID;
	int IrrigationFreqID;
	int WaterSourceID;
	int RespectRainfailID;
	int SuspendMonID;
	int SuspendTueID;
	int SuspendWedID;
	int SuspendThuID;
	int SuspendFriID;
	int SuspendSatID;
	int SuspendSunID;
	int SuspendALLID;
};

//  String dateString = String(now.year()) + "-" + String(now.month()) + "-" + String(now.day());
//  String timeString = String(now.hour()) + ":" + String(now.minute()) + ":" + String(now.second());

// =============================================================================
// Irrigation
// Manages all 9 IrrigateSelection zones, handles JSON persistence to
// /Irrigation.json, and provides helper methods used by the web UI.
// =============================================================================
class Irrigation
{
public:
	Irrigation();                      // Constructor — allocates and initialises Selection[]
	~Irrigation();                     // Destructor
	void   Maintain();
	void   Init();
	String GetDescription(int Line);   // Returns formatted zone description for the web list button
	String GetOption(int Line);        // Returns the button action label (START / BUSY-STOP / SCHEDULED)
	void   WriteData();                // Serialise all zone configs to /Irrigation.json
	void   ReadData();                 // Deserialise zone configs from /Irrigation.json

	int BusyPumpingConfig = -1;        // Index of the valve config currently active (-1 = none)

	IrrigateSelection Selection[IRRIGATE_SELECTIONS];  // All 9 zone configurations
};

	inline Irrigation *IrriObject;

// =============================================================================
// Irrigation Schedule State Machine
// =============================================================================
// SheduleIrri holds one SheduleLines entry per zone.  IrrigationShedule()
// (called every 10 ms) checks each line and transitions it through three states:
//   "S" (Scheduled)  — Waiting for the NextDateToIrrigate / NextTimeToIrrigate
//   "A" (Active)     — Valve config is open; counting down to StopDateTime
//   "D" (Deactive)   — Completed or cancelled; date advanced to next cycle
// =============================================================================

	#define Lines  IRRIGATE_SELECTIONS   // Alias used within the schedule classes

	// Schedule activation type (passed to SetupIrrigateShedule)
	#define SHED_TYPE_AUTO    "A"   // Automated: started by the scheduler on schedule
	#define SHED_TYPE_MANUAL  "M"   // Manual: started by the user via the web UI

	// Schedule status values stored in SheduleLines.ShedStat
	#define SHED_STATUS_SCHEDULED  "S"   // Waiting for its scheduled start time
	#define SHED_STATUS_ACTIVE     "A"   // Currently irrigating
	#define SHED_STATUS_DEACTIVE   "D"   // Idle / completed

// =============================================================================
// SheduleLines
// Runtime state for one zone's current schedule slot.
// =============================================================================
class SheduleLines
{
public:
	String ShedStat;      // Current state: SHED_STATUS_SCHEDULED / ACTIVE / DEACTIVE
	String StopDateTime;  // "YYYY-MM-DD HH:MM" when this irrigation run should end
	bool   ManualStop;    // true = user has requested an early stop via the web button
};

// =============================================================================
// SheduleIrri
// Container for all 9 zone schedule entries.
// =============================================================================
class SheduleIrri
{
public:
	SheduleIrri();   // Constructor — initialises all SheduleLines to DEACTIVE
	~SheduleIrri();  // Destructor
	void Init();

	SheduleLines IrrigateShedule[Lines];  // One entry per irrigation zone
};

	inline SheduleIrri *SheduleObject; 
