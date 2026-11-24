// =============================================================================
// 05_ThisSystem.cpp — I/O Setup, Input Debounce, Physical Control & HTTP Routes
// =============================================================================
// Implements hardware I/O initialisation, the 50 ms input debounce algorithm,
// the physical control logic (button handling, remote pump watchdog — all
// driven from loop(), see main.cpp), the WebSocket page-value maintenance
// dispatcher, and the central HTTP route registration function.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// SetupSystemIOs
// Configures every entry in OutPort[] as a digital output (initially LOW) and
// every entry in InPort[] as a digital input.  Called once from InitHardware().
// OUTStat is initialised to false to keep software state in sync with the pins.
// -----------------------------------------------------------------------------
void SetupSystemIOs()
{
	for (int i = 0; i < OutPorts; i++){
		OutPort[i].OUTStat = false;
		pinMode(OutPort[i].Port, OUTPUT);
		digitalWrite(OutPort[i].Port, LOW);
	}

	for (int i = 0; i < InputPorts; i++){
		pinMode(InPort[i].PinIO, INPUT);
	}
}

// -----------------------------------------------------------------------------
// PinDebounce (called every 10 ms from loop())
// Implements 50 ms steady-state debounce on each InPort[] entry using millis().
// Uses two-phase state machine per pin:
// - Rising edge: MemoryH=true starts timer; Input=true after continuous HIGH >= DEBOUNCE_STEADY_MS.
// - Falling edge: MemoryL=true starts timer; Input=false after continuous LOW >= DEBOUNCE_STEADY_MS.
// Steady states reset memories. Overflow-safe via unsigned subtraction.
// Thread-safe for same-core tasks (atomic bool reads, no critical sections needed).
// -----------------------------------------------------------------------------
void PinDebounce() {
    unsigned long now = millis();  // Local, precise
    for (int i = 0; i < InputPorts; i++) {
        bool reading = digitalRead(InPort[i].PinIO);

        if (reading && !InPort[i].Input) {
            // Rising edge phase
            InPort[i].MemoryL = false;
            if (!InPort[i].MemoryH) {
                InPort[i].Time = now;
                InPort[i].MemoryH = true;
            } else if ((now - InPort[i].Time) >= DEBOUNCE_STEADY_MS) {
                InPort[i].Input = true;
            }
        } else if (!reading && InPort[i].Input) {
            // Falling edge phase (only if was HIGH)
            InPort[i].MemoryH = false;
            if (!InPort[i].MemoryL) {
                InPort[i].Time = now;
                InPort[i].MemoryL = true;
            } else if ((now - InPort[i].Time) >= DEBOUNCE_STEADY_MS) {
                InPort[i].Input = false;
            }
        } else {
            // Steady state: reset memories
            InPort[i].MemoryH = InPort[i].MemoryL = false;
        }
    }
}

// -----------------------------------------------------------------------------
// MaintainWebPageValues  (called every 10 ms from MaintainWebPage)
// Scans every active PageVar list and pushes any changed values to connected
// WebSocket clients.  Also triggers the valve-configuration maintenance cycle.
// Only calls Send*PageValues() functions for pages whose objects are allocated.
// -----------------------------------------------------------------------------
void MaintainWebPageValues(){
	SendSettingsPageValues();                          // Always present (12_Settings.cpp)
	if (TankPageObject)        SendTankPageValues();   // 09_Tank.cpp
	if (MainPageObject)        SendHomePageValues();   // 08_Home.cpp
	if (SuspendObject)         SendSuspendPageValues();// 09_Suspend.cpp
	if (ElectricValvesObject)  SendWaterValvesPageValues(); // 09_WaterValves.cpp
	if (ElectricValvesObject)  SendConfigPageValues(); // 09_WaterValves.cpp
	if (IrrigSetupObject)      SendIrrigSetupPageValues();  // 09_Irrigation.cpp
	if (IrriObject)            SendIrrigationOverviewValues(); // 09_Irrigation.cpp
	if (NtfyPageObject)        SendNtfyPageValues();           // 16_Ntfy.cpp
	SendFwValues();                                            // ABOUT firmware update status (22_Update.cpp)

	ElecValves->MaintainValveConfig();   // Apply any pending valve switching (09_WaterValves.cpp)
}

// -----------------------------------------------------------------------------
// MaintainButtonStatus  (called every 10 ms from loop())
// Handles edge-triggered toggle logic for the two physical momentary buttons:
//
//   IIDX_SYSTEM_DISABLE_ENABLE — Each press toggles NORMAL_OPERATION.
//     When false, all outputs are inhibited and the disabled LED is lit.
//
//   IIDX_MAN_START_STOP — Each press toggles MANUAL_STARTED (borehole pump).
//     Records the press time in ManualStartMs for elapsed-time display.
//
// The .Status flag prevents re-triggering while the button is held down;
// it is cleared only when the button is released (Input returns false).
// -----------------------------------------------------------------------------
void MaintainButtonStatus(){

	// ── System Enable / Disable Toggle ───────────────────────────────────────
	if (InPort[IIDX_SYSTEM_DISABLE_ENABLE].Input && !InPort[IIDX_SYSTEM_DISABLE_ENABLE].Status){
		// Only allow disabling when nothing is actively running
		bool busyNow = (IrriActivityLine >= 0) || BoreholeStartedByTank || MANUAL_STARTED;
		if (NORMAL_OPERATION && busyNow) {
			// Busy — ignore the press, do not disable
		} else {
			NORMAL_OPERATION = !NORMAL_OPERATION;
		}
		InPort[IIDX_SYSTEM_DISABLE_ENABLE].Status = true;
	}
	else if (!InPort[IIDX_SYSTEM_DISABLE_ENABLE].Input){
		InPort[IIDX_SYSTEM_DISABLE_ENABLE].Status = false;
	}
	// LED is ON when the system is disabled (NORMAL_OPERATION = false)
	digitalWrite(OutPort[OIDX_SYST_DISABLED_LED].Port, !NORMAL_OPERATION ? HIGH : LOW);
	OutPort[OIDX_SYST_DISABLED_LED].OUTStat = !NORMAL_OPERATION;

	// ── Manual Borehole Start / Stop Toggle ──────────────────────────────────
	if (InPort[IIDX_MAN_START_STOP].Input && !InPort[IIDX_MAN_START_STOP].Status){
		MANUAL_STARTED = !MANUAL_STARTED;
		if (MANUAL_STARTED) ManualStartMs = millis();   // Record start time for elapsed display
		InPort[IIDX_MAN_START_STOP].Status = true;
	}
	else if (!InPort[IIDX_MAN_START_STOP].Input){
		InPort[IIDX_MAN_START_STOP].Status = false;
	}

	// ── Pump indicator LEDs ───────────────────────────────────────────────────
	digitalWrite(OutPort[OIDX_PUMP_MANUAL_LED].Port, MANUAL_STARTED ? HIGH : LOW);
	OutPort[OIDX_PUMP_MANUAL_LED].OUTStat = MANUAL_STARTED;

	digitalWrite(OutPort[OIDX_MY_PUMP_LED].Port, InPort[IIDX_ME_PUMP_BH].Input ? HIGH : LOW);
	OutPort[OIDX_MY_PUMP_LED].OUTStat = InPort[IIDX_ME_PUMP_BH].Input;

	digitalWrite(OutPort[OIDX_OTHER_PUMP_LED].Port, InPort[IIDX_OTHER_PUMP_HB].Input ? HIGH : LOW);
	OutPort[OIDX_OTHER_PUMP_LED].OUTStat = InPort[IIDX_OTHER_PUMP_HB].Input;
}

// -----------------------------------------------------------------------------
// MaintainRemotePumpWatchdog  (called every 10 ms from loop())
// Remote connectivity watchdog: if the remote controller started the pump but
// hasn't sent a /pumpstatus heartbeat in 60 s, stop the pump.
// -----------------------------------------------------------------------------
void MaintainRemotePumpWatchdog(){
	if (RemoteControlActive && MANUAL_STARTED &&
	    RemoteHeartbeatMs > 0 &&
	    (millis() - RemoteHeartbeatMs > 60000UL))
	{
		MANUAL_STARTED                     = false;
		InPort[IIDX_MAN_START_STOP].Input  = false;
		InPort[IIDX_MAN_START_STOP].Status = false;
		RemoteControlActive                = false;
		RemoteHeartbeatMs                  = 0;
		SaveGenReport("Pump: Stopped - Remote heartbeat lost");
	}
}

// -----------------------------------------------------------------------------
// AddRoutes
// Registers all HTTP GET/POST route handlers with the AsyncWebServer.
// Called once from SetupWebPage() before server.begin().
// Routes are grouped by functional area for readability.
// -----------------------------------------------------------------------------
void AddRoutes(){

	// ── Main Pages ────────────────────────────────────────────────────────────
	server.on("/",                   handleRoot);             // Home page         (08_Home.cpp)
	server.on("/report",             handleOthers);           // Reports / other   (10_Reports.cpp)
	server.on("/settings",           handleSettings);         // System settings   (12_Settings.cpp)
	server.on("/about",              handleAbout);            // About / OTA       (15_About.cpp)
	server.on("/fwcheck",    HTTP_POST, handleFwCheck);          // ABOUT "CHECK FOR UPDATE" (22_Update.cpp)
	server.on("/fwinstall",  HTTP_POST, handleFwInstall);        // ABOUT "INSTALL FROM GITHUB"
	server.on("/update",     HTTP_POST, handleFwUploadDone, handleFwUploadData);  // ABOUT ".bin upload"

	// ── Irrigation ────────────────────────────────────────────────────────────
	server.on("/irrigation",         handleIrrigation);       // Zone list         (09_Irrigation.cpp)
	server.on("/resumeirrigateion",  handleResumeIrrig);      // Resume irrigation (09_Irrigation.cpp)
	server.on("/suspendirrigateion", handleSuspendIrrig);     // Suspend irrigation(09_Irrigation.cpp)
	server.on("/managesuspend",      handleManageSuspend);    // Suspension calendar(09_Suspend.cpp)
	server.on("/zonesnext",          handleIrrigSetupNext);   // Next zone setup   (09_Irrigation.cpp)
	server.on("/zonesprev",          handleIrrigSetupPrev);   // Previous zone setup(09_Irrigation.cpp)

	// ── Valve Management ──────────────────────────────────────────────────────
	server.on("/valvenames",         handleWaterValves);      // Valve names/delays(09_WaterValves.cpp)
	server.on("/valveconfigs",       handleValvesConfigs);    // Valve configs     (09_WaterValves.cpp)
	server.on("/testvalves",         handleTestValves);       // Manual valve test (13_TestValves.cpp)
	server.on("/togglevalve",        handleToggleValve);      // Toggle one valve  (13_TestValves.cpp)

	// ── Tank & Borehole ───────────────────────────────────────────────────────
	server.on("/tank",               handleTank);             // Tank settings     (09_Tank.cpp)
	server.on("/tankstartfilter",    handleTankStartFilter);  // Start filter pump (09_Tank.cpp)
	server.on("/tankstopfilter",     handleTankStopFilter);   // Stop filter pump  (09_Tank.cpp)
	server.on("/disablesystem",      handleDisableSystem);    // Disable system    (12_Settings.cpp)
	server.on("/enablesystem",       handleEnableSystem);     // Enable system     (12_Settings.cpp)
	server.on("/startfilter",        handleStartFilter);      // Start filter pump (12_Settings.cpp)
	server.on("/stopfilter",         handleStopFilter);       // Stop filter pump  (12_Settings.cpp)
	server.on("/startborehole",      handleStartBorehole);    // Start borehole    (12_Settings.cpp)
	server.on("/stopborehole",       handleStopBorehole);     // Stop borehole     (12_Settings.cpp)
	server.on("/pumpstart",          handlePumpStart);        // Remote start API  (12_Settings.cpp)
	server.on("/pumpstop",           handlePumpStop);         // Remote stop API   (12_Settings.cpp)
	server.on("/pumpstatus",         handlePumpStatus);       // Remote status API (12_Settings.cpp)
	server.on("/resetasp",           handleResetASP);         // Reset WiFi        (12_Settings.cpp)
	server.on("/wifi",               handleWifiNets);         // WiFi Networks page (05_WiFi.cpp)
	server.on("/wifinet/add",    HTTP_POST, handleWifiNetAdd);    // Add a network    (05_WiFi.cpp)
	server.on("/wifinet/delete", HTTP_POST, handleWifiNetDelete); // Delete a network (05_WiFi.cpp)
	server.on("/removewifi",         handleRemoveWifi);       // Remove WiFi & reboot(12_Settings.cpp)

	// ── NTFY Push Notifications ──────────────────────────────────────────────
	server.on("/ntfy",               handleNtfy);             // NTFY settings     (16_Ntfy.cpp)
	server.on("/testntfy",           handleTestNtfy);         // Send test NTFY    (16_Ntfy.cpp)
	server.on("/testntfy2",          handleTestNtfy2);        // Send test NTFY 2  (16_Ntfy.cpp)

	// ── Reports & Actions ─────────────────────────────────────────────────────
	server.on("/actions",            handleActions);          // Actions (stub)    (11_Actions.cpp)
	server.on("/reportgen",          handleGenReport);          // General log       (10_Reports.cpp)
	server.on("/reportirrig",        handleIrrigationReport);   // Irrigation log    (10_Reports.cpp)
	server.on("/reportpump",         handleIrrigReport);        // Water used log    (10_Reports.cpp)
	server.on("/reportfilter",       handleTankFilterReport);   // Filter log        (10_Reports.cpp)

	// ── File Manager ──────────────────────────────────────────────────────────
	// IMPORTANT: specific /files/* routes must be registered BEFORE /files.
	// ESPAsyncWebServer 3.x uses startsWith() prefix matching — /files would
	// otherwise capture every /files/* request before the specific handler fires.
	server.on("/files/view",   handleFileView);               // File viewer       (03_Files.cpp)
	server.on("/files/delete", handleFileDeleteJson);         // AJAX delete       (03_Files.cpp)
	server.on("/api/files",    handleApiFiles);               // JSON file list    (03_Files.cpp)
	server.on("/files",        FilesPage);                    // Directory listing (03_Files.cpp)
	server.on("/download",     File_Download);                // File download     (03_Files.cpp)
	server.on("/upload",   File_Upload);                      // Upload form       (03_Files.cpp)
	server.on("/delete",   File_Delete);                      // File delete       (03_Files.cpp)
	server.on("/fupload", HTTP_POST,
		[](AsyncWebServerRequest *request){ request->redirect("/files"); },
		handleFileUpload                                      // Multipart upload handler (03_Files.cpp)
	);
}
