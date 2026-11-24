#pragma once

// =============================================================================
// 09_WaterValves.h — Solenoid Valve Definitions, Configurations & Management
// =============================================================================
// Defines the seven physical solenoid valves, nine named valve configurations,
// and the ElectricValves class that manages switching, sequencing, and JSON
// persistence for the complete valve subsystem.
// =============================================================================

// ─── Configuration Relevance Categories ──────────────────────────────────────
// Determines which system component activates a given configuration.
	#define REL_IRRILINE  1   // Used by the irrigation schedule (zone configs)
	#define REL_MANUAL    2   // Used for manual activation via the web UI
	#define REL_TANK      3   // Used during tank-fill borehole operation
	#define REL_DEFAULT   4   // Default/idle configuration applied when nothing is active

// ─── Valve State Constants ────────────────────────────────────────────────────
	#define VALVE_ACTIVE    true    // Valve is selected in a configuration
	#define VALVE_INACTIVE  false   // Valve is not selected in a configuration
	#define VALVE_OPEN      true    // Physical valve should be energised (open)
	#define VALVE_CLOSE     false   // Physical valve should be de-energised (closed)

// ─── Physical Valve Count & Index Aliases ────────────────────────────────────
	#define VALVE_COUNT  7   // Total number of physical solenoid valves
	#define VALVE_1      0
	#define VALVE_2      1
	#define VALVE_3      2
	#define VALVE_4      3
	#define VALVE_5      4
	#define VALVE_6      5
	#define VALVE_7      6

// ─── Configuration Count & Index Aliases ─────────────────────────────────────
	#define VALVE_CONFIGS   9   // Total named configurations stored in ValveConfig[]
	#define CONFIG_1        0   // Irrigation zone configurations 1–6
	#define CONFIG_2        1
	#define CONFIG_3        2
	#define CONFIG_4        3
	#define CONFIG_5        4
	#define CONFIG_6        5
	#define CONFIG_MAN      6   // Manual activation configuration
	#define CONFIG_TANK     7   // Tank-fill borehole configuration
	#define CONFIG_DEF      8   // Default/idle configuration

// ─── Valve Switch Timing ──────────────────────────────────────────────────────
// Two-phase delay allows solenoids to engage before flow begins.
	#define VALVE_SWITCH_DELAY_1   500    // 500 ms — short inter-valve switching gap
	#define VALVE_SWITCH_DELAY_2   15000  // 15 s   — solenoid engagement confirmation delay

// =============================================================================
// ValveActiveState
// Records whether a valve is selected (ACTIVE) in a specific configuration.
// One instance per valve slot within each ValveConfigurations entry.
// =============================================================================
class ValveActiveState
{
public:
	bool State = VALVE_INACTIVE;   // VALVE_ACTIVE = included; VALVE_INACTIVE = excluded
};

// =============================================================================
// ValveAtrributes
// Physical and timing properties of one solenoid valve.
// Name and SwitchDelay are user-configurable via the Valve Names web page.
// =============================================================================
class ValveAtrributes
{
public:
	String        Terminal     = "";                  // Internal reference terminal label
	bool          RequiredState = VALVE_CLOSE;        // Target state being applied during a switch
	unsigned long UpdateTime   = 0;                   // millis() snapshot used during switch sequencing

	String Name        = "";                          // User-assigned valve name (shown on web pages)
	int    SwitchDelay = VALVE_SWITCH_DELAY_1;        // Delay (ms) after opening this valve
};

// =============================================================================
// ValveConfigurations
// One named set of valve open/close states.  Nine configurations are stored in
// ElectricValves::ValveConfig[].  Relevance determines which subsystem uses it.
// =============================================================================
class ValveConfigurations
{
public:
	int    Relevance  = REL_IRRILINE;    // Category: irrigation / manual / tank / default
	String ConfigName = "";              // User-assigned configuration name

	ValveActiveState ValveStatus[VALVE_COUNT];  // Open/close state for each of the 7 valves
};

// =============================================================================
// ElectricValves
// Central valve management class.  Handles:
//   - Configuration activation/deactivation with switch sequencing
//   - MaintainValveConfig() drives the two-phase switch delay state machine
//   - JSON persistence to /WaterValves.json
// =============================================================================
class ElectricValves
{
public:
	ElectricValves();   // Constructor — calls Init() to set defaults
	~ElectricValves();  // Destructor

	void Init();                              // Set default valve names and configuration names
	void SwitchValveConfig(int Line, bool Status);  // Activate (true) or deactivate (false) a config
	void MaintainValveConfig();               // Drive the switch-delay state machine (call every 10 ms)
	void WriteData();                         // Serialise valve + config data to /WaterValves.json
	void ReadData();                          // Deserialise valve + config data from /WaterValves.json

	ValveAtrributes     WaterValve[VALVE_COUNT];    // Physical valve properties (7 entries)
	ValveConfigurations ValveConfig[VALVE_CONFIGS]; // Named configurations (9 entries)
};

	inline ElectricValves *ElecValves;    // Global valve management object
	inline PageVar*        ElecValvesList; // PageVar array for the Valve Names web page
	inline PageVar*        ConfigList;     // PageVar array for the Valve Configs web page
