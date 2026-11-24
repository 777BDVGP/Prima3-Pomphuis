#pragma once

// =============================================================================
// 13_TestValves.h — Manual Per-Valve Test Page
// =============================================================================
// Lets a technician open/close each of the 7 solenoid valves individually,
// bypassing the named-configuration system — ElectricValves::SwitchValveConfig()
// always drives all 7 valves at once from a stored config pattern, there is no
// per-valve API. Only reachable while NORMAL_OPERATION is false (system
// disabled): IrrigationShedule() and MaintainTankFilling() both no-op entirely
// while disabled, so nothing else can call SwitchValveConfig() and fight a
// manual toggle made here.
// =============================================================================

	#define PS_TESTVALVES  15   // Navigation index constant for the Test Valves page

	// Valve labels in the fixed top-to-bottom display order, indexed 0..VALVE_COUNT-1
	// — same physical order as OIDX_VALVE_SOL_* / VALVE_1..VALVE_7.
	inline const char* TestValveLabel[VALVE_COUNT] = { "V3", "V8", "V10", "V7", "V2", "V9", "V17" };
