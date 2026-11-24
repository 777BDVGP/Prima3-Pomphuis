// =============================================================================
// 12_Settings.cpp — System Parameters, Settings Page & System Control Handlers
// =============================================================================
// Implements the AllParams class (JSON persistence, Settings web page), the
// settings and system-control route handlers (disable/enable system, start/stop
// filter pump, start/stop borehole), and the WebSocket update dispatcher for
// the settings page.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

void appendSettingsPageSel(int Index);   // Local forward declaration

// =============================================================================
// AllParams — Object Factory & Persistence
// =============================================================================

// -----------------------------------------------------------------------------
// CreateAllParams
// Allocates the global Params object, loads settings from /AllParams.json,
// then immediately writes back to ensure the file is present and complete.
// Also maps the Settings web page PageVar list.
// Called once from InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreateAllParams(){
	Params = new AllParams;
	Params->ReadData();
	Params->WriteData();
	MapSettingsPage();
}

// -----------------------------------------------------------------------------
// AllParams — Constructor / Destructor / Maintain
// No dynamic initialisation is needed beyond the inline field defaults declared
// in AllParams.h.  Maintain() is reserved for future periodic housekeeping.
// -----------------------------------------------------------------------------
AllParams::AllParams()  {}        // Inline defaults in .h are sufficient
AllParams::~AllParams(){}         // No heap members to release
void AllParams::Maintain(){}      // Reserved — not currently used

// -----------------------------------------------------------------------------
// AllParams::WriteData
// Serialises all AllParams fields to /AllParams.json using ArduinoJson v6.
// Called once during boot (to ensure the file exists) and after every
// UpdateSettingsPageValues() invocation to persist user changes.
// -----------------------------------------------------------------------------
void AllParams::WriteData()
{
	String StrFileName = "/AllParams.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "w");
	DynamicJsonDocument doc(1024);

	doc["SysName"]             = SysName;
	doc["LogGeneralEvents"]    = LogGeneralEvents;
	doc["LogIrrigationEvents"] = LogIrrigationEvents;
	doc["LogBoreholeUsedEvents"] = LogBoreholeUsedEvents;
	doc["LogTankFilterEvents"] = LogTankFilterEvents;
	doc["BootupWait"]          = BootupWait;
	doc["RainGaugeEnable"]     = RainGaugeEnable;
	doc["RainGaugeURL"]        = RainGaugeURL;
	doc["ShowOtaInfo"]         = ShowOtaInfo;

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

// -----------------------------------------------------------------------------
// AllParams::ReadData
// Deserialises /AllParams.json into the AllParams fields.  Missing or "null"
// keys are silently ignored, leaving the inline defaults in place — this means
// a fresh device (no file yet) boots with sensible defaults automatically.
// Called once during boot before WriteData() to restore the last saved state.
// -----------------------------------------------------------------------------
void AllParams::ReadData()
{
	String StrFileName = "/AllParams.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "r");
	if (ChannelDataFile)
	{
		size_t size = ChannelDataFile.size();
		if (size <= 1024)
		{
			std::unique_ptr<char[]> buf(new char[size]); // Stack-safe heap buffer for file contents
			ChannelDataFile.readBytes(buf.get(), size);
			DynamicJsonDocument doc(1024);
			auto error = deserializeJson(doc, buf.get());
			if (!error)
			{
				// Only overwrite the default if the key is present and non-null
				if ( doc["SysName"].as<String>()            != "null" ){ SysName            = doc["SysName"].as<String>(); }
				if ( doc["LogGeneralEvents"].as<String>()   != "null" ){ LogGeneralEvents   = (doc["LogGeneralEvents"].as<String>()   == "true"); }
				if ( doc["LogIrrigationEvents"].as<String>() != "null" ){ LogIrrigationEvents = (doc["LogIrrigationEvents"].as<String>() == "true"); }
				if ( doc["LogBoreholeUsedEvents"].as<String>() != "null" ){ LogBoreholeUsedEvents = (doc["LogBoreholeUsedEvents"].as<String>() == "true"); }
				if ( doc["LogTankFilterEvents"].as<String>() != "null" ){ LogTankFilterEvents = (doc["LogTankFilterEvents"].as<String>() == "true"); }
				if ( doc["BootupWait"].as<String>()          != "null" ){ BootupWait          = doc["BootupWait"]; }
				if ( doc["RainGaugeEnable"].as<String>()     != "null" ){ RainGaugeEnable     = (doc["RainGaugeEnable"].as<String>() == "true"); }
				if ( doc["RainGaugeURL"].as<String>()        != "null" ){ RainGaugeURL        = doc["RainGaugeURL"].as<String>(); }
				if ( doc["ShowOtaInfo"].as<String>()         != "null" ){ ShowOtaInfo         = (doc["ShowOtaInfo"].as<String>() == "true"); }
			}
			else
			{
				SaveGenReport F(" File Error open File AllParams.json");
			}
		}
		else
		{
			SaveGenReport F(" File AllParams.json size is > 1024 ");
		}
		ChannelDataFile.close();
	}
}

// =============================================================================
// Settings Page — PageVar Mapping
// =============================================================================

// -----------------------------------------------------------------------------
// MapSettingsPage
// Allocates the SettingsList PageVar array (PAGE_VAR_SETTINGS entries) and
// binds each row to the corresponding AllParams field via a PageElement.
// The PageElement stores a pointer to the live C++ variable, the display label,
// an optional unit string, a style pointer, and the WebSocket ID used to push
// updates to the browser.
// Called once from CreateAllParams() during boot.
// -----------------------------------------------------------------------------
void MapSettingsPage(){
	SettingsList = new PageVar[PAGE_VAR_SETTINGS];
	for (int i=0; i < PAGE_VAR_SETTINGS; i++)
	{
		switch(i)
		{
			case SET_SYSNAME:
				SettingsList[i].Var = new PageElement(&Params->SysName, F("System Name"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_LOG_GENERAL:
				SettingsList[i].Var = new PageElement(&Params->LogGeneralEvents,    F("Log General Events"),     "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_LOG_IRRIGATION:
				SettingsList[i].Var = new PageElement(&Params->LogIrrigationEvents, F("Log Irrigation Events"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_LOG_WATERUSED:
				SettingsList[i].Var = new PageElement(&Params->LogBoreholeUsedEvents, F("Log BoreholeUsed Events"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_LOG_TANKFILTER:
				SettingsList[i].Var = new PageElement(&Params->LogTankFilterEvents, F("Log TankFilter Events"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_BOOTUP_DELAY:
				SettingsList[i].Var = new PageElement(&Params->BootupWait, F("Bootup Wait (Mills)"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_RAINGAUGE_EN:
				SettingsList[i].Var = new PageElement(&Params->RainGaugeEnable, F("Use Rain Gauge"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_RAINGAUGE_URL:
				SettingsList[i].Var = new PageElement(&Params->RainGaugeURL, F("Rain Gauge IP"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_RAINGAUGE_BADGE:
				SettingsList[i].Var = new PageElement(&RainGaugeBadge, F("Irrigation Today"), "", &RainGaugeBadgeStyle, i+ID_SETTINGS);
				break;
			case SET_SHOW_OTA:
				SettingsList[i].Var = new PageElement(&Params->ShowOtaInfo, F("Show PC Upload (OTA) on ABOUT"), "", &SetStyle, i+ID_SETTINGS);
				break;
			case SET_RAINGAUGE_STAT:
				SettingsList[i].Var = new PageElement(&RainGaugeStatus, F("Rain Gauge Status"), "", &RainGaugeStatStyle, i+ID_SETTINGS);
				break;
		}
	}
}

// =============================================================================
// Settings Web Page — Route Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// handleSettings  (GET /settings)
// Builds and serves the settings page.  Page construction order:
//   1. WT_SETUP_Settings() fills PageBuf with the parameter table (avoids
//      repeated heap allocation).
//   2. Standard head + action message + navigation menu are prepended.
//   3. WebSocket script is injected so the browser can receive live updates.
//   4. PageBuf content is appended as a single String copy.
//   5. UPDATE and REBOOT buttons close the page.
// Also called internally by all action handlers (disable/enable system, filter,
// borehole) so the updated navigation menu state is reflected immediately.
// -----------------------------------------------------------------------------
void handleSettings(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	WT_SETUP_Settings();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendSettingsPageSel(PS_SETTINGS);
	appendSettingsActionButtons();
	appendHtmlStr(webSocketScript);
	appendHtmlBuf(PageBuf, getPageBufLen());
	appendHtmlStr(webUpateButton);
	webWifiEditButton();
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}


// -----------------------------------------------------------------------------
// appendSettingsPageSel
// Builds the settings-page navigation <ul> block.
//   - The currently active page (Index) is rendered as a non-clickable <lh>
//     label; all other pages are rendered as clickable <li> links.
//   - The three action buttons (ENABLE/DISABLE SYSTEM, START/STOP FILTER,
//     START/STOP BOREHOLE) toggle their label and target URL based on the
//     current global state flags, giving the user a one-click toggle.
// -----------------------------------------------------------------------------
void appendSettingsPageSel(int Index){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == PS_HOME     ? "<a class='act'>HOME</a>"     : "<a href='/'>HOME</a>");
	appendHtmlStr(Index == PS_SETTINGS ? "<a class='act'>SETTINGS</a>" : "<a href='/settings'>SETTINGS</a>");
	appendHtmlStr("<a href='/about'>ABOUT</a>");
	appendHtmlStr("</div>");
}

void appendSettingsActionButtons(){
	appendHtmlStr("<div class='nav'>");
	if (!NORMAL_OPERATION) appendHtmlStr("<a class='warn' href='/enablesystem'>ENABLE SYSTEM</a>");
	else                   appendHtmlStr("<a class='warn' href='/disablesystem'>DISABLE SYSTEM</a>");
	if (FILTER_ON) appendHtmlStr("<a class='warn' href='/stopfilter'>STOP FILTER</a>");
	else           appendHtmlStr("<a class='warn' href='/startfilter'>START FILTER</a>");
	if (MANUAL_STARTED) appendHtmlStr("<a class='warn' href='/stopborehole'>STOP BOREHOLE</a>");
	else                appendHtmlStr("<a class='warn' href='/startborehole'>START BOREHOLE</a>");
	appendHtmlStr("</div>");
}
 

// =============================================================================
// Settings Page — WT_* Table Builder & WebSocket Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// WT_SETUP_Settings
// Populates PageBuf with the settings parameter table HTML.
// Each row contains a descriptive label column and an editable input column
// whose type (text / checkbox / number) is derived from the PageElement's
// underlying C++ variable type.
// Output is written to the static PageBuf to avoid heap fragmentation.
// -----------------------------------------------------------------------------
void WT_SETUP_Settings(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("Parameter Description", "Value", "");

	for (int i=0; i < PAGE_VAR_SETTINGS_BASE; i++)
	{
		WT_EditRow(&SettingsList[i]);
	}

	// Rain gauge: checkbox + IP on one row, then the live status (17_RainGauge.cpp)
	WT_SectionHeader("Rain Gauge  (sets Rain OK to Irrigate every hour)");
	WT_CheckEditRow(&SettingsList[SET_RAINGAUGE_EN], &SettingsList[SET_RAINGAUGE_URL]);
	WT_DisplayRow(&SettingsList[SET_RAINGAUGE_BADGE]);
	WT_DisplayRow(&SettingsList[SET_RAINGAUGE_STAT]);

	WT_SectionHeader("ABOUT page");
	WT_EditRow(&SettingsList[SET_SHOW_OTA]);
	WT_EndPage();
}

// -----------------------------------------------------------------------------
// SendSettingsPageValues
// Called periodically by MaintainWebPage() to push changed parameter values to
// all connected WebSocket clients.  Only entries whose current value differs
// from the last-sent value are transmitted, then UpdateLast() is called to
// record the new baseline — preventing unnecessary WebSocket traffic.
// -----------------------------------------------------------------------------
void SendSettingsPageValues(){
	for (int i=0; i < PAGE_VAR_SETTINGS; i++)
	{
		if(SettingsList[i].Var->HasChanged())
		{
			// The status and badge rows are display labels ("V108", "V109"), the rest are inputs.
			bool display = (i == SET_RAINGAUGE_STAT || i == SET_RAINGAUGE_BADGE);
			webParamUpdate(SettingsList[i].Var->GetName(),
			               SettingsList[i].Var->GetID(display ? "V" : ""),
			               SettingsList[i].Var->GetVarStyle());
			SettingsList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateSettingsPageValues
// Called by UnpackJasonPayload() when the browser sends an UPDATE button
// payload from the settings page.  Iterates over all JSON key-value pairs,
// matches each key to a SettingsList entry by WebSocket ID, and applies the
// new value via SetName().
// After all values are applied:
//   - Sends a confirmation message back to the requesting client.
//   - Persists all settings to /AllParams.json via WriteData().
//   - Refreshes any timer durations that depend on AllParams via UpdateTimerTimes().
// -----------------------------------------------------------------------------
void UpdateSettingsPageValues(uint32_t clientId, JsonObject *root){

	for (JsonPair kv : *root){
		for (int i=0; i < PAGE_VAR_SETTINGS; i++)
		{
			if(SettingsList[i].Var->GetID("") == kv.key().c_str())
			{
				SettingsList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}

	WebMessUpdate(clientId, "Parameters Updated!", "color: red", true);
	Params->WriteData();
	UpdateTimerTimes();
	RainGaugePollNow = true;   // ask the rain gauge at once with the new IP / checkbox (17_RainGauge.cpp)
}

// =============================================================================
// System Control Action Handlers
// =============================================================================
// All action handlers follow the same pattern:
//   1. Set SetMess / SpesialStr to display a one-shot feedback banner.
//   2. Update the relevant global state flag and/or InPort debounce input.
//   3. Redirect back to the settings page via handleSettings() so the
//      navigation menu immediately reflects the new state.
// =============================================================================

// -----------------------------------------------------------------------------
// handleDisableSystem  (GET /disablesystem)
// Refuses the request if irrigation, tank filling, or manual borehole is
// currently active — stopping mid-run could leave valves open or the borehole
// running unsupervised.  Only clears NORMAL_OPERATION when all are idle.
// -----------------------------------------------------------------------------
void handleDisableSystem(AsyncWebServerRequest *request) {
	SetMess = FA_MESS_RED;
	if (IrriActivityLine >= 0) {
		SpesialStr = "Cannot Disable: Irrigation Busy";
	} else if (BoreholeStartedByTank) {
		SpesialStr = "Cannot Disable: Tank Fill Busy";
	} else if (MANUAL_STARTED) {
		SpesialStr = "Cannot Disable: Manual Borehole Busy";
	} else {
		SetMess    = FA_MESS_GRN;
		SpesialStr = "Normal Operation Off";
		NORMAL_OPERATION = false;
	}
	handleSettings(request);
}

// -----------------------------------------------------------------------------
// handleEnableSystem  (GET /enablesystem)
// Restores NORMAL_OPERATION — outputs are released back to automatic control.
// -----------------------------------------------------------------------------
void handleEnableSystem(AsyncWebServerRequest *request) {
	SetMess    = FA_MESS_RED;
	SpesialStr = "Normal Operation On";
	NORMAL_OPERATION = true;
	ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);   // Force valves back to Default position
	handleSettings(request);
}

// -----------------------------------------------------------------------------
// handleStartFilter  (GET /startfilter)
// Sets FILTER_ON and forces the filter-pump debounce input high so
// MaintainTankFilling() picks up the change on its next cycle.
// -----------------------------------------------------------------------------
void handleStartFilter(AsyncWebServerRequest *request) {
	SetMess    = FA_MESS_GRN;
	SpesialStr = "Filter Started";
	FILTER_ON  = true;
	InPort[IIDX_TANK_FILTER_ON_OFF].Input = true;
	handleSettings(request);
}

// -----------------------------------------------------------------------------
// handleStopFilter  (GET /stopfilter)
// Stops the filter pump, but only if the borehole is not currently running.
// The borehole output pin is active-LOW: a LOW reading means the pump is on.
// Attempting to cut the filter while the borehole is active could starve the
// borehole pump of back-pressure, so the stop is refused in that case.
// -----------------------------------------------------------------------------
void handleStopFilter(AsyncWebServerRequest *request) {
	SetMess = FA_MESS_RED;
	if (OutPort[OIDX_BOREHOLE].OUTStat) {   // HIGH = borehole running (active-high)
		SpesialStr = "Filter Cannot Stop While Borehole Running";
	} else {
		SpesialStr = "Filter Stopped";
		FILTER_ON  = false;
		InPort[IIDX_TANK_FILTER_ON_OFF].Input = false;
		digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
		OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
	}
	handleSettings(request);
}

// -----------------------------------------------------------------------------
// handleStartBorehole  (GET /startborehole)
// Manually starts the borehole pump.  Mirrors the physical IIDX_MAN_START_STOP
// button path: sets MANUAL_STARTED, asserts the debounce Input, and marks the
// edge as already processed (.Status = true) so MaintainButtonStatus() does not
// immediately toggle MANUAL_STARTED back on its next 10 ms cycle.
// ManualStartMs records the start time for elapsed-duration display.
// -----------------------------------------------------------------------------
void handleStartBorehole(AsyncWebServerRequest *request) {
	SetMess        = FA_MESS_GRN;
	SpesialStr     = "Borehole Started";
	MANUAL_STARTED = true;
	InPort[IIDX_MAN_START_STOP].Input  = true;   // Reflect asserted state
	InPort[IIDX_MAN_START_STOP].Status = true;   // Edge already handled — suppress MaintainButtonStatus toggle
	ManualStartMs  = millis();
	handleSettings(request);
}

// -----------------------------------------------------------------------------
// handleStopBorehole  (GET /stopborehole)
// Manually stops the borehole pump.  Clears MANUAL_STARTED and de-asserts the
// debounce Input.  Status is explicitly cleared so the next physical button
// press is recognised as a fresh edge by MaintainButtonStatus().
// -----------------------------------------------------------------------------
void handleStopBorehole(AsyncWebServerRequest *request) {
	SetMess        = FA_MESS_RED;
	SpesialStr     = "Borehole Stopped";
	MANUAL_STARTED = false;
	InPort[IIDX_MAN_START_STOP].Input  = false;  // Reflect de-asserted state
	InPort[IIDX_MAN_START_STOP].Status = false;  // Ready for next physical press
	handleSettings(request);
}

// =============================================================================
// WiFiManager Reset
// =============================================================================

// -----------------------------------------------------------------------------
// webResetASPButton
// Returns an HTML button that navigates to /resetasp when clicked.
// Displayed on the Settings page alongside the UPDATE and REBOOT buttons.
// Styled to match webRebootButton() in 08_Home.cpp.
// -----------------------------------------------------------------------------
void webResetASPButton()
{
	appendHtmlStr("<div><br></div><div><b><a>");
	appendHtmlStr("<b><a><input type=\"button\" id=\"RESETASP\" onclick=\"window.location.href='/resetasp'\" style=\"font-size:14px;background-color:#dc2626;color:#fff;border:none;padding:6px 14px;border-radius:4px;cursor:pointer\" value=\"RESET ASP\"</a></b></div>");
}

// -----------------------------------------------------------------------------
// handleResetASP  (GET /resetasp)
// Deletes the saved (extra) WiFi networks (/WiFiConfig.json), logs the event,
// then restarts.  On reboot the controller joins the built-in network(s) from
// wifi_secrets.h; the Prima3-Setup hotspot opens only if none can be reached.
// -----------------------------------------------------------------------------
void handleResetASP(AsyncWebServerRequest *request) {
	SaveGenReport F("WiFi reset triggered by RESET ASP");
	request->send(200, "text/plain", "Resetting WiFi settings and restarting...");
	delay(500);
	ClearWifiSettings();
	ESP.restart();
}

// -----------------------------------------------------------------------------
// webWifiEditButton
// Dark-green "WiFi Networks" button — same colour as POWER_MONITOR.
// Navigates to /wifi (handleWifiNets, 05_WiFi.cpp): list / add / delete networks.
// -----------------------------------------------------------------------------
void webWifiEditButton()
{
	appendHtmlStr("<div><br></div><div><b><a>");
	appendHtmlStr("<b><a><input type=\"button\" id=\"WIFIEDIT\" onclick=\"window.location.href='/wifi'\" style=\"font-size:14px;background-color:#166534;color:#fff;border:none;padding:6px 14px;border-radius:4px;cursor:pointer\" value=\"&#9881; WiFi Networks\"</a></b></div>");
}

// =============================================================================
// Remote Pump API  (GET /pumpstart, /pumpstop, /pumpstatus)
// =============================================================================
// Plain-text REST endpoints called by the Alarm Support System (192.168.1.116).
// Responses match what RequestPump() in 11_Borehole.cpp expects.
// Every handler resets RemoteHeartbeatMs so MaintainRemotePumpWatchdog()
// (called from loop()) knows the remote controller is still online.
// =============================================================================

void handlePumpStart(AsyncWebServerRequest *request) {
	RemoteHeartbeatMs = millis();
	if (MANUAL_STARTED) {
		request->send(200, "text/plain", "Busy");
	} else if (!NORMAL_OPERATION) {
		request->send(200, "text/plain", "Not Ready");
	} else {
		MANUAL_STARTED = true;
		InPort[IIDX_MAN_START_STOP].Input  = true;
		InPort[IIDX_MAN_START_STOP].Status = true;
		ManualStartMs          = millis();
		RemoteControlActive    = true;
		request->send(200, "text/plain", "Started");
		SaveGenReport("Pump: Remote START");
	}
}

void handlePumpStop(AsyncWebServerRequest *request) {
	RemoteHeartbeatMs      = millis();
	MANUAL_STARTED         = false;
	InPort[IIDX_MAN_START_STOP].Input  = false;
	InPort[IIDX_MAN_START_STOP].Status = false;
	RemoteControlActive    = false;
	request->send(200, "text/plain", "Stopped");
	SaveGenReport("Pump: Remote STOP");
}

void handlePumpStatus(AsyncWebServerRequest *request) {
	RemoteHeartbeatMs = millis();
	bool pumpOn = MANUAL_STARTED || InPort[IIDX_ME_PUMP_BH].Input;
	request->send(200, "text/plain", pumpOn ? "Yes" : "Not");
}

// -----------------------------------------------------------------------------
// handleRemoveWifi  (GET /removewifi)
// Deletes the saved (extra) WiFi networks and reboots onto the built-in one(s).
// -----------------------------------------------------------------------------
void handleRemoveWifi(AsyncWebServerRequest *request) {
	ClearWifiSettings();
	SaveGenReport F("WiFi: saved networks removed via /removewifi");
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendHtmlStr("<div class='nav'><a href='/'>HOME</a><a class='act'>SETTINGS</a></div>");
	appendHtmlStr("<div style='padding:16px;font-size:14px'>"
	              "<b>Saved WiFi networks removed.</b><br>"
	              "Rebooting &mdash; the controller joins its built-in network again "
	              "(hotspot Prima3-Setup opens only if that cannot be reached).</div>");
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
	delay(1500);
	ESP.restart();
}
