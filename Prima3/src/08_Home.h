#pragma once

// =============================================================================
// 08_Home.h — Home Page Globals, Status Strings & Display Constants
// =============================================================================
// Declares all variables that drive the real-time home-page display, the
// background-colour attribute strings used to colour each status row, and
// the process-state machine constants reserved for future pump-control logic.
// =============================================================================

// ─── User Identifiers ─────────────────────────────────────────────────────────
	#define NoUser   0   // No user currently active
	#define User1    1   // User 1 (name: TankWaterLevel->User_1)
	#define User2    2   // User 2 (name: TankWaterLevel->User_2)

// ─── Process State Machine Constants ─────────────────────────────────────────
// Reserved for a future pump-sequencing state machine.
	#define NoStat          0
	#define Stat1           1
	#define Stat2           2
	#define Stat3           3
	#define Stat4           4
	#define Stat5           5
	#define Stat6           6
	#define Stat7           7
	#define Stat8           8

// ─── Home Page Row Index Constants ───────────────────────────────────────────
// Each index maps to one row in the HomeList[] PageVar array and one row in
// the WT table on the home page.  Order determines the display sequence.
	#define HOME_CONTROLLERST   0   // Controller status (Active / Not Active)
	#define HOME_TANKFILLIRRG   1   // Current activity with elapsed time
	#define HOME_TANKLEVEL      2   // Tank water level percentage
	#define HOME_TANKFLOATSTA   3   // Tank float switch status
	#define HOME_BOREHOLESTA    4   // Borehole pump enable / disable state
	#define HOME_BOREHOLEOPER   5   // Who is currently pumping from the borehole
	#define HOME_FILTERPUMPAC   6   // Filter pump activity and countdown
	#define HOME_FILTERPRESSU   7   // Filter pressure in kPa
	#define HOME_BOREHOLERETR   8   // Borehole retry countdown
	#define HOME_SUSPENDSTAT    9   // Global irrigation suspend status

	#define PAGE_VAR_HOME      10   // Total number of home-page PageVar entries

	inline bool     PumpFlash = true;    // Flash state for pump-active LED animation
	inline PageVar* HomeList;            // Dynamically allocated home-page PageVar array

// ─── Background Colour Attribute Strings ─────────────────────────────────────
// Used as the 'ATT' argument to webParamUpdate() to set the CSS background
// colour of each value cell on the home page.
	#define ATT_RED    "color:white;background-color:red;padding:4px 10px;border-radius:6px"
	#define ATT_WHITE  "color:black;background-color:white"
	#define ATT_ORANGE "color:black;background-color:orange;padding:4px 10px;border-radius:6px"
	#define ATT_YELLOW "color:black;background-color:yellow;padding:4px 10px;border-radius:6px"
	#define ATT_GREEN  "color:white;background-color:green;padding:4px 10px;border-radius:6px"

// ─── Controller Status Text Constants ────────────────────────────────────────
	#define HOME_CONTROL_ACTIVE    "Active"      // Shown when NORMAL_OPERATION = true
	#define HOME_CONTROL_NACTIVE   "Not Active"  // Shown when NORMAL_OPERATION = false

// ─── Tank Fill Status Text Constants ─────────────────────────────────────────
	#define HOME_TANKFILL_NO_BUSY  "Off"           // No fill or irrigation in progress
	#define HOME_TANKFILL_BUSY     "Busy Filling"  // Tank fill cycle active

// ─── Live Status Strings (updated every 10 ms by MaintainHomePage) ───────────
// Each pair: StrXxx holds the display text, StrXxxAtt holds the cell CSS style.

	inline String StrControllerStatus       = "ACTIVE";
	inline String StrControllerStatusAtt    = ATT_GREEN;

	inline String StrTankFillIrrigation     = " ";
	inline String StrTankFillIrrigationAtt  = ATT_WHITE;

	inline String StrTankLevel              = " ";
	inline String StrTankLevelAtt           = ATT_WHITE;

	inline String StrTankFloatStatus        = " ";
	inline String StrTankFloatStatusAtt     = ATT_WHITE;

	inline String StrBoreholeStatus         = " ";
	inline String StrBoreholeStatusAtt      = ATT_WHITE;

	inline String StrBoreholeOperation      = " ";
	inline String StrBoreholeOperationAtt   = ATT_WHITE;

	inline String StrFilterPumpActivity     = " ";
	inline String StrFilterPumpActivityAtt  = ATT_WHITE;

	inline String StrFilterPressure         = " ";
	inline String StrFilterPressureAtt      = ATT_WHITE;

	inline String StrBoreholeRetry          = " ";
	inline String StrBoreholeRetryAtt       = ATT_WHITE;

	inline String StrSuspendStatus          = " ";
	inline String StrSuspendStatusAtt       = ATT_WHITE;

	inline String StrRaininlastdays         = " ";    // Reserved — no rain sensor fitted
	inline String StrRaininlastdaysAtt      = ATT_WHITE;

	inline bool   HomePageReady             = false;  // Set true after boot stabilisation delay

// ─── Button Attribute Strings ─────────────────────────────────────────────────
// Used to colour action buttons (ON = red, OFF = green) on the home page.
	inline String ONATT  = F("color:white;background-color:red;padding:4px 10px;border-radius:6px");
	inline String OFFATT = F("color:white;background-color:green;padding:4px 10px;border-radius:6px");

// ─── Pump Session Tracking ────────────────────────────────────────────────────
	inline int   UserNr0to2  = 0;   // Currently serving: NoUser, User1, or User2
	inline float CurrentNow  = 0;   // Most recent current reading (A) — current sensing not wired up

	// Simple elapsed-time structure for pump session duration display
	struct Time
	{
		unsigned int Sec;
		unsigned int Min;
		unsigned int Hour;
	};
	inline Time PumpTime;   // Session elapsed time for active pump run

// ─── Pump Process State Machine ───────────────────────────────────────────────
// Reserved for a future automated pump-sequencing feature.
	#define INPUT_REQ       1
	#define STARTING        2
	#define STABILIZE       3
	#define PUMPING         4
	#define STOP_PUMP       5
	#define ZERO_CURRENT    6
	#define LOW_CURRENT     7
	#define OVERCURRENT     9
	#define POWERFAIL      10
	#define MANUAL_TIMEOUT 11

	inline int  PROCESS        = INPUT_REQ;  // Current state-machine state
	inline int  ServingUser    = User1;      // Which user the pump is serving
	inline bool ManualSelection = false;     // True when manual mode was selected

	inline bool ManualOnUser1  = false;      // Manual pump request pending for User 1
	inline bool ManualOnUser2  = false;      // Manual pump request pending for User 2

	inline bool ManualButtonUser1 = true;    // Manual start button enabled for User 1
	inline bool ManualButtonUser2 = true;    // Manual start button enabled for User 2

	inline int PumpTimeSec  = 0;   // Pump session duration accumulator (seconds)
	inline int InternetSec  = 0;   // Current seconds value from SNTP (updated by CompileDateTime)

	inline bool ManUser1Active = false;   // User 1 manual pump session currently running
	inline bool ManUser2Active = false;   // User 2 manual pump session currently running
