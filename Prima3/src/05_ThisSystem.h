#pragma once

// =============================================================================
// 05_ThisSystem.h — Hardware I/O Map, Debounce Engine & Navigation Constants
// =============================================================================
// Defines all GPIO pin assignments (OPIN_* / IPIN_*), the array index aliases
// (OIDX_* / IIDX_*), the input debounce structure and InPort[] array, the
// output state structure and OutPort[] array, and navigation/page-builder
// constants shared across the firmware.
// =============================================================================

// ─── WiFiManager Access Point Name ───────────────────────────────────────────
// Used during first-boot captive portal provisioning and when credentials are
// erased via the ASPRESET button.
	#define ACCPOINTNAME  "V5_WATER_DEV_SOCK"

// =============================================================================
// Output Pin Numbers  (OPIN_*)
// Physical GPIO numbers written to OutPort[].Port.
// Solenoid valves MUST occupy the first VALVE_COUNT entries of OutPort[] —
// the valve-switching logic indexes directly from 0.
// =============================================================================
	#if defined(ARDUINO_ARCH_ESP32)

	// ── Solenoid Valve Outputs ────────────────────────────────────────────────
	// GPIO numbers below are S3 Mini values. The S2 Mini and S3 Mini are NOT
	// electrically pin-compatible despite sharing the same physical socket —
	// most physical positions carry a different GPIO number on the two
	// boards. All 17 outputs below were confirmed correct by direct
	// output-cycle testing against the real hardware rig on 2026-09-14.
	#define OPIN_VALVE_SOL_03         4   // Solenoid valve 03
	#define OPIN_VALVE_SOL_08         6   // Solenoid valve 08
	#define OPIN_VALVE_SOL_10        12   // Solenoid valve 10
	#define OPIN_VALVE_SOL_07         7   // Solenoid valve 07
	#define OPIN_VALVE_SOL_02        13   // Solenoid valve 02
	#define OPIN_VALVE_SOL_09         8   // Solenoid valve 09
	#define OPIN_VALVE_SOL_17        11   // Solenoid valve 17

	// ── Indicator LEDs ────────────────────────────────────────────────────────
	#define OPIN_PUMP_MANUAL_LED     10   // Manual pump mode active indicator
	#define OPIN_MY_PUMP_LED          9   // User 2 currently pumping indicator
	#define OPIN_OTHER_PUMP_LED      14   // User 1 currently pumping indicator
	#define OPIN_FLOAT_UP_LED        16   // Tank full float switch indicator
	#define OPIN_FLOAT_LOW_LED       18   // Tank low float switch indicator
	#define OPIN_SYST_DISABLED_LED   17   // System disabled (NORMAL_OPERATION = false) indicator

	// ── Control Outputs ───────────────────────────────────────────────────────
	#define OPIN_BOREHOLE             2   // Borehole pump relay — active HIGH (HIGH=START, LOW=STOP)
	#define OPIN_FILTER_PUMP_RELAY   21   // Filter/circulation pump relay

	// ── Spare Outputs ─────────────────────────────────────────────────────────
	// OPIN_EXTRA_1 landed on GPIO43 (the S3's UART0 TX pin) during testing
	// and was moved to GPIO33 to avoid it; native USB mode
	// (ARDUINO_USB_CDC_ON_BOOT=1) routes Serial over USB-Serial-JTAG rather
	// than UART0, so GPIO43 would likely have been free too, but untested.
	#define OPIN_EXTRA_1             33   // Spare output 1
	#define OPIN_EXTRA_2              5   // Spare output 2

// =============================================================================
// Input Pin Numbers  (IPIN_*)
// Physical GPIO numbers read by PinDebounce() into InPort[].Input.
// GPIO numbers below are S3 Mini values, confirmed 2026-09-14 (see note above).
// =============================================================================
	#define IPIN_TANK_FILTER_SENCE      1  // Tank filter presence sensor (analog-capable)
	#define IPIN_TANK_LEVEL_SENCE       3  // Tank level sensor (analog-capable)
	#define IPIN_SYSTEM_DISABLE_ENABLE 35  // System enable / disable push-button
	#define IPIN_ME_PUMP_BH            34  // User 2 borehole pump flow detector
	#define IPIN_OTHER_PUMP_HB         36  // User 1 borehole pump flow detector
	#define IPIN_TANK_FULL             38  // Tank full float switch
	#define IPIN_TANK_LOW              44  // Tank low float switch (GPIO44, S3's UART0 RX —
	                                        // should be free under native USB mode)
	#define IPIN_MAN_START_STOP        37  // Manual borehole start / stop button
	#define IPIN_TANK_FILTER_ON_OFF    43  // Filter pump on / off button (GPIO43, S3's UART0 TX —
	                                        // should be free under native USB mode)

// =============================================================================
// Output Array Index Aliases  (OIDX_*)
// Index into OutPort[].  Solenoid valves occupy indices 0–6 (VALVE_COUNT).
// Always update OutPort[OIDX_x].OUTStat when calling digitalWrite() so that
// software state mirrors the physical output.
// =============================================================================
	#define OIDX_VALVE_SOL_03         0   // Pin  4  — Solenoid valve 03
	#define OIDX_VALVE_SOL_08         1   // Pin  6  — Solenoid valve 08
	#define OIDX_VALVE_SOL_10         2   // Pin 12  — Solenoid valve 10
	#define OIDX_VALVE_SOL_07         3   // Pin  7  — Solenoid valve 07
	#define OIDX_VALVE_SOL_02         4   // Pin 13  — Solenoid valve 02
	#define OIDX_VALVE_SOL_09         5   // Pin  8  — Solenoid valve 09
	#define OIDX_VALVE_SOL_17         6   // Pin 11  — Solenoid valve 17
	#define OIDX_PUMP_MANUAL_LED      7   // Pin 10  — Manual pump LED
	#define OIDX_BOREHOLE             8   // Pin  2  — Borehole pump relay (active HIGH)
	#define OIDX_MY_PUMP_LED          9   // Pin  9  — User 2 pumping LED
	#define OIDX_OTHER_PUMP_LED      10   // Pin 14  — User 1 pumping LED
	#define OIDX_FLOAT_UP_LED        11   // Pin 16  — Tank full float LED
	#define OIDX_FLOAT_LOW_LED       12   // Pin 18  — Tank low float LED
	#define OIDX_SYST_DISABLED_LED   13   // Pin 17  — System disabled LED
	#define OIDX_FILTER_PUMP_RELAY   14   // Pin 21  — Filter pump relay

// =============================================================================
// Input Array Index Aliases  (IIDX_*)
// Index into InPort[].  Use InPort[IIDX_x].Input for the debounced state.
// InPort[IIDX_x].Status is used as an edge-already-processed flag in
// MaintainButtonStatus() to prevent repeat triggers on a held button.
// =============================================================================
	#define IIDX_SYSTEM_DISABLE_ENABLE  0   // Pin 35 — System enable / disable button
	#define IIDX_ME_PUMP_BH             1   // Pin 34 — User 2 borehole detector
	#define IIDX_OTHER_PUMP_HB          2   // Pin 36 — User 1 borehole detector
	#define IIDX_TANK_FULL              3   // Pin 38 — Tank full float switch
	#define IIDX_TANK_LOW               4   // Pin 44 — Tank low float switch
	#define IIDX_MAN_START_STOP         5   // Pin 37 — Manual borehole button
	#define IIDX_TANK_FILTER_ON_OFF     6   // Pin 43 — Filter pump button

	#endif   // ARDUINO_ARCH_ESP32

// =============================================================================
// Input Debounce Engine
// =============================================================================
// PinDebounce() (05_ThisSystem.cpp) is called every 10 ms from loop().
// It requires DEBOUNCE_STEADY_MS continuous ms of stable reading before
// updating InPort[i].Input, preventing false triggers from contact bounce.
// =============================================================================

	inline unsigned long DebouceTimeRead;         // millis() snapshot taken during debounce scan
	#define DEBOUNCE_STEADY_MS   50               // Input must be stable for this long (ms)

	// DebounceStruct — state for one debounced digital input
	struct DebounceStruct
	{
		const int    PinIO;     // GPIO pin number
		bool         MemoryH;   // true once the rising-edge timing window has started
		bool         MemoryL;   // true once the falling-edge timing window has started
		bool         Input;     // Debounced logical state (true = HIGH / active)
		unsigned long Time;     // millis() when the timing window opened
		bool         Status;    // Edge-processed flag (set by button handler to prevent repeat)
		String       Desc;      // Human-readable label for debugging
	};

	#define InputPorts   7   // Total number of digital inputs in InPort[]

	// Input array — order must match IIDX_* index constants above
	// Columns: PinIO                        MemH   MemL   Input  Time  Status  Description
	inline DebounceStruct InPort[InputPorts] =
	{
		{ IPIN_SYSTEM_DISABLE_ENABLE,  false, false, false, 0, false, "SYSTEM DIS/EN-ABLE"   },
		{ IPIN_ME_PUMP_BH,             false, false, false, 0, false, "USER 2 — FROM BH"     },
		{ IPIN_OTHER_PUMP_HB,          false, false, false, 0, false, "USER 1 — FROM BH"     },
		{ IPIN_TANK_FULL,              false, false, false, 0, false, "TANK FULL FLOAT"       },
		{ IPIN_TANK_LOW,               false, false, false, 0, false, "TANK LOW FLOAT"        },
		{ IPIN_MAN_START_STOP,         false, false, false, 0, false, "MANUAL BH START/STOP"  },
		{ IPIN_TANK_FILTER_ON_OFF,     false, false, false, 0, false, "TANK FILTER START/STOP"},
	};

// =============================================================================
// Output Port Structure
// =============================================================================
// Always write through both digitalWrite() AND update OUTStat to keep the
// software state in sync with the physical pin for home-page status reporting.
// =============================================================================

	// OutPortStruct — state for one digital output
	struct OutPortStruct
	{
		int    Port;     // GPIO pin number
		String Desc;     // Human-readable label for debugging and web display
		bool   OUTStat;  // Current logical output state (mirrors the physical pin)
	};

	#define OutPorts   17   // Total number of digital outputs in OutPort[]
	// Note: solenoid valve entries MUST occupy indices 0–6 (OIDX_VALVE_SOL_*)
	// so that the valve-switching logic can iterate directly from index 0.

	// Output array — order must match OIDX_* index constants above
	// Columns: GPIO pin               Description          Initial state
	inline OutPortStruct OutPort[OutPorts] =
	{
		{ OPIN_VALVE_SOL_03,      "VALVE_SOL_03",      false },  // Pin  4
		{ OPIN_VALVE_SOL_08,      "VALVE_SOL_08",      false },  // Pin  6
		{ OPIN_VALVE_SOL_10,      "VALVE_SOL_10",      false },  // Pin 12
		{ OPIN_VALVE_SOL_07,      "VALVE_SOL_07",      false },  // Pin  7
		{ OPIN_VALVE_SOL_02,      "VALVE_SOL_02",      false },  // Pin 13
		{ OPIN_VALVE_SOL_09,      "VALVE_SOL_09",      false },  // Pin  8
		{ OPIN_VALVE_SOL_17,      "VALVE_SOL_17",      false },  // Pin 11
		{ OPIN_PUMP_MANUAL_LED,   "PUMP_MANUAL_LED",   false },  // Pin 10
		{ OPIN_BOREHOLE,          "BOREHOLE",          false },  // Pin  2  (active HIGH)
		{ OPIN_MY_PUMP_LED,       "MY_PUMP_LED",       false },  // Pin  9
		{ OPIN_OTHER_PUMP_LED,    "OTHER_PUMP_LED",    false },  // Pin 14
		{ OPIN_FLOAT_UP_LED,      "FLOAT_UP_LED",      false },  // Pin 16
		{ OPIN_FLOAT_LOW_LED,     "FLOAT_LOW_LED",     false },  // Pin 18
		{ OPIN_SYST_DISABLED_LED, "SYST_DISABLED_LED", false },  // Pin 17
		{ OPIN_FILTER_PUMP_RELAY, "FILTER_PUMP_RELAY", false },  // Pin 21
		{ OPIN_EXTRA_1,           "O_EXTRA_1",         false },  // Pin 33
		{ OPIN_EXTRA_2,           "O_EXTRA_2",         false },  // Pin  5
	};

// =============================================================================
// Navigation Page Index Constants  (PS_*)
// Passed to appendMainPageSel() and page-specific selector functions to
// determine which menu item is shown as the active (non-clickable) label.
// =============================================================================

	// ── Main navigation tabs ──────────────────────────────────────────────────
	#define PS_HOME          1   // Home / real-time status
	#define PS_REPORTS       2   // Other / reports
	#define PS_IRRIGATION    3   // Irrigation zone management
	#define PS_TANK          4   // Tank and borehole settings
	#define PS_POOL          5   // Reserved
	#define PS_PLUGS         6   // Reserved
	#define PS_OTHER         7   // Other/reports alias
	#define PS_SETTINGS      8   // System settings
	#define PS_MANIRRIG      9   // Manual irrigation (reserved)

	#define PS_NTFY          11   // NTFY push notification settings

	// ── Irrigation sub-pages ──────────────────────────────────────────────────
	#define PS_IRRIGATION_MANUAL    12   // Manual irrigation control
	#define PS_IRRIGATION_SUSPEND   12   // Manage suspension calendar
	#define PS_IRRIGATION_VALVES    13   // Valve names and delays
	#define PS_IRRIGATION_CONFIG    14   // Valve configuration sets

// =============================================================================
// WT_* Table-Builder Type Constants
// =============================================================================
// Passed to WT_Column() to control the type and position of each cell emitted
// into the JavaScript parameters array that createLabelInputTable() consumes.
// =============================================================================
	#define WT_LABEL   0   // Emit a label (read-only text) cell
	#define WT_INPUT   1   // Emit an input (editable) cell
	#define WT_FIRST   2   // This is the first cell — open the labels/inputs array
	#define WT_NEXT    3   // Continuing — array already open, add next entry
	#define WT_MORE    5   // More cells follow in this array — keep it open
	#define WT_LAST    4   // This is the final cell — close the array
