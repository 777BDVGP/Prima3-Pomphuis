#pragma once

// =============================================================================
// 09_Suspend.h — 7-Day Irrigation Suspension Calendar
// =============================================================================
// Defines the SuspendDays and SuspendAttributes classes that allow irrigation
// to be globally suppressed on specific days of the week.  Each day can be
// configured for a permanent suspend or a one-shot (Once) override.
// SUSPEND_IRRI is the global master gate checked by IrrigationShedule().
// =============================================================================

	// Boolean suspend state constants
	#define SUSPEND_YES  true    // Day is suspended — no irrigation will run
	#define SUSPEND_NO   false   // Day is not suspended — irrigation runs normally

	// Array size and index constants for the 7-day suspension calendar
	#define SUSPEND_DAYS  7   // Days in a week
	#define SUSPEND_1     0   // Sunday
	#define SUSPEND_2     1   // Monday
	#define SUSPEND_3     2   // Tuesday
	#define SUSPEND_4     3   // Wednesday
	#define SUSPEND_5     4   // Thursday
	#define SUSPEND_6     5   // Friday
	#define SUSPEND_7     6   // Saturday

	// Global irrigation suspend flag — checked every 10 ms by IrrigationShedule()
	inline bool SUSPEND_IRRI = false;

	// Set true when SUSPEND_IRRI was raised by the /managesuspend calendar (not by manual button).
	// Allows the scheduler to auto-release the suspension when the day rolls over.
	inline bool CalendarSuspendActive = false;

// =============================================================================
// SuspendAttributes
// Holds the suspension state for a single day of the week.
// =============================================================================
class SuspendAttributes
{
public:
	String Day     = "";          // Day name displayed on the web page (e.g. "Monday")
	int    Weekday = 0;           // 1-based weekday number matching WeekDay global (1=Sun … 7=Sat)
	bool   Suspend = SUSPEND_NO;  // true = all irrigation suspended on this day (permanent)
	bool   Once    = SUSPEND_NO;  // true = skip irrigation once then auto-clear (single-use)
};

// =============================================================================
// SuspendDays
// Manages the 7-day suspension calendar: initialisation, JSON persistence,
// and the SuspendingDay[] array.
// =============================================================================
class SuspendDays
{
public:
	SuspendDays();   // Constructor
	~SuspendDays();  // Destructor
	void Init();      // Assign day names/numbers and load data from SPIFFS
	void WriteData(); // Serialise suspension calendar to /SuspendDays.json
	void ReadData();  // Deserialise suspension calendar from /SuspendDays.json

	SuspendAttributes SuspendingDay[SUSPEND_DAYS];  // One entry per day of the week
	int LastProcessedDay = 0;  // Persisted: WeekDay last fully processed by Phase 0 (0 = never)
};

	inline SuspendDays *DaysSuspend;   // Global suspension calendar object
	inline PageVar*     SuspendList;   // PageVar array for the Manage Suspend web page
