#pragma once

// =============================================================================
// 11_Actions.h — Actions Page Globals
// =============================================================================
// Declares the shared action-instruction string and the pump-session start-time
// variables populated by SetStartPumpTime() in 10_Reports.cpp.
// The Actions page itself is currently a stub pending future implementation.
// =============================================================================

	// Status message and colour displayed on the Actions/Reports header
	inline String ACTION_INST = "";             // Current action instruction text
	inline String COLOR       = COLOR_DEFAULT;  // CSS colour for the instruction text

	// Pump session start-time snapshot (set by SetStartPumpTime in 10_Reports.cpp)
	inline int StartYear;     // Calendar year    (e.g. 2026)
	inline int StartMonth;    // Month 0-based    (0 = January)
	inline int StartDay;      // Day of month     (1–31)
	inline int StartHours;    // Hour of day      (0–23)
	inline int StartMinutes;  // Minute           (0–59)
	inline int StartSeconds;  // Second           (0–59)
