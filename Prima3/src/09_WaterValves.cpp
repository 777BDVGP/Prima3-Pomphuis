// =============================================================================
// 09_WaterValves.cpp — Solenoid Valve Names, Configs & Web Pages
// =============================================================================
// Implements:
//   - Valve names page (/valvenames):  7 valve labels + switch delays.
//   - Valve configs page (/valveconfigs): 9 configurations × 7 valve states.
//   - ElectricValves class: Init, MaintainValveConfig, SwitchValveConfig,
//     WriteData, ReadData.
//   - CreateElectricValves() and CreateValvesConfigMaps() object factories.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// Valve Names Page  (GET /valvenames)
// =============================================================================

// -----------------------------------------------------------------------------
// handleWaterValves  (GET /valvenames)
// Lazy-initialises ElectricValves PageVar maps if not yet built, then serves
// the valve names and switch-delay configuration page.
// -----------------------------------------------------------------------------
void handleWaterValves(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleWaterValves " + String(ESP.getFreeHeap()));
	if(!ElectricValvesObject) CreateValvesConfigMaps();
	CompilePage_WaterValves(request);
}

// -----------------------------------------------------------------------------
// CompilePage_WaterValves
// Builds and serves the valve names page using the static PageBuf.
// Navigation shows the irrigation submenu with "Valve Delays" active.
// -----------------------------------------------------------------------------
void CompilePage_WaterValves(AsyncWebServerRequest *request)
{
	DEBUG_PRINTLN("CW 01");
	WT_SETUP_WaterValves();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);            
	IrrigatePageSel(PS_IRRIGATION_VALVES);    
	appendHtmlStr(webSocketScript);           
	appendHtmlBuf(PageBuf, getPageBufLen());  
	appendHtmlStr(webUpateButton);            
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
	DEBUG_PRINTLN("CompilePage_WaterValves done");
}

// -----------------------------------------------------------------------------
// MapWaterValvesPage
// Allocates the ElecValvesList PageVar array (VALVE_COUNT × 2 entries):
//   Entries [0 .. VALVE_COUNT-1]            → valve name strings.
//   Entries [VALVE_COUNT .. 2×VALVE_COUNT-1] → valve switch delays (ms).
// The PCB terminal label (Terminal field) is used as the display description
// for each valve so the user can identify which physical connection to label.
// -----------------------------------------------------------------------------
void MapWaterValvesPage(){
	ElecValvesList = new PageVar[VALVE_COUNT*2];
	for (int i=0; i < VALVE_COUNT; i++)
	{
		ElecValvesList[i].Var             = new PageElement(&ElecValves->WaterValve[i].Name,        ElecValves->WaterValve[i].Terminal, "", &SetStyle, i+ID_VALVES);
		ElecValvesList[i+VALVE_COUNT].Var = new PageElement(&ElecValves->WaterValve[i].SwitchDelay, "",                                "", &SetStyle, i+VALVE_COUNT+ID_VALVES);
	}
}

// -----------------------------------------------------------------------------
// WT_SETUP_WaterValves
// Populates PageBuf with the valve names table.  Each row shows:
//   Column 1 (label)  — PCB terminal identifier (e.g. "AC1").
//   Column 2 (input)  — User-editable valve name (e.g. "V3").
//   Column 3 (input)  — Switch delay in milliseconds.
// -----------------------------------------------------------------------------
void WT_SETUP_WaterValves(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("PCB Terminal", "Valve Name", "MS Delay");

	for (int i=0; i < VALVE_COUNT; i++)
	{
		WT_StartLine();
		WT_AutoColumn(WT_LABEL, WT_FIRST, WT_LAST, &ElecValvesList[i], "L");
		WT_AutoColumn(WT_INPUT, WT_FIRST, WT_MORE, &ElecValvesList[i]);
		WT_AutoColumn(WT_INPUT, WT_NEXT,  WT_LAST, &ElecValvesList[i+VALVE_COUNT]);
		WT_EndLine();
	}
	WT_EndPage();
}

// -----------------------------------------------------------------------------
// SendWaterValvesPageValues
// Pushes changed valve name / delay values to connected WebSocket clients.
// Only entries that differ from their last-sent baseline are transmitted.
// -----------------------------------------------------------------------------
void SendWaterValvesPageValues(){
	for (int i=0; i < VALVE_COUNT*2; i++)
	{
		if(ElecValvesList[i].Var->HasChanged())
		{
			webParamUpdate(ElecValvesList[i].Var->GetName(),
			               ElecValvesList[i].Var->GetID(""),
			               ElecValvesList[i].Var->GetVarStyle());
			ElecValvesList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateWaterValvesPageValues
// Called by UnpackJasonPayload() when the browser sends an UPDATE payload from
// the valve names page.  Applies all JSON key-value pairs to the matching
// ElecValvesList entry by WebSocket ID, then persists to /WaterValves.json.
// -----------------------------------------------------------------------------
void UpdateWaterValvesPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		for (int i=0; i < VALVE_COUNT*2; i++)
		{
			if(ElecValvesList[i].Var->GetID("") == kv.key().c_str())
			{
				ElecValvesList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	WebMessUpdate(clientId, "Valves Updated!", "color: red", true);
	ElecValves->WriteData();
}

// =============================================================================
// Valve Configurations Page  (GET /valveconfigs)
// =============================================================================

// -----------------------------------------------------------------------------
// handleValvesConfigs  (GET /valveconfigs)
// Lazy-initialises ElectricValves PageVar maps if not yet built, then serves
// the 9-configuration × 7-valve matrix page.
// -----------------------------------------------------------------------------
void handleValvesConfigs(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleValvesConfigs " + String(ESP.getFreeHeap()));
	if(!ElectricValvesObject) CreateValvesConfigMaps();
	CompilePage_ValvesConfigs(request);
}

// -----------------------------------------------------------------------------
// CompilePage_ValvesConfigs
// Builds the valve configs 9×7 checkbox matrix and serves it.
// snprintf writes directly into a 160-byte stack buf; appendHtmlStr writes
// each fragment straight into the response stream — no malloc, no realloc.
// -----------------------------------------------------------------------------
void CompilePage_ValvesConfigs(AsyncWebServerRequest *request)
{
	char buf[160];
	DEBUG_PRINTLN("CV 01");
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);               
	IrrigatePageSel(PS_IRRIGATION_CONFIG);       
	appendHtmlStr(webSocketScript);              

	appendHtmlStr("<script>const parameters = ["); 
	appendHtmlStr("{name:\"Configuration\",labels:[],inputs:[],},");

	for (int i = 0; i < VALVE_COUNT; i++) {
		snprintf(buf, sizeof(buf), "{name:\"%s\",labels:[],inputs:[],},",
		         ElecValves->WaterValve[i].Name.c_str());
		appendHtmlStr(buf);
	}

	int I = 0;
	for (int i = 0; i < VALVE_CONFIGS; i++) {
		snprintf(buf, sizeof(buf),
		         "{labels:[{type:\"text\",text:\"%s\",id:\"ID%d\","
		         "attributes:[{n:\"style\",v:\"\"},],},],",
		         ElecValves->ValveConfig[i].ConfigName.c_str(), i);
		appendHtmlStr(buf);

		for (int j = 0; j < VALVE_COUNT; j++) {
			int         id  = ID_VALCONF + I;
			const char* val = ElecValves->ValveConfig[i].ValveStatus[j].State ? "true" : "false";
			if (j == 0) {
				snprintf(buf, sizeof(buf),
				         "inputs:[{type:\"checkbox\",value:\"%s\",id:\"%d\","
				         "attributes:[{n:\"style\",v:\"\"},],},", val, id);
			} else if (j < VALVE_COUNT - 1) {
				snprintf(buf, sizeof(buf),
				         "{type:\"checkbox\",value:\"%s\",id:\"%d\","
				         "attributes:[{n:\"style\",v:\"\"},],},", val, id);
			} else {
				snprintf(buf, sizeof(buf),
				         "{type:\"checkbox\",value:\"%s\",id:\"%d\","
				         "attributes:[{n:\"style\",v:\"\"},],},],", val, id);
			}
			appendHtmlStr(buf);
			I++;
		}
		appendHtmlStr("},");
	}

	appendHtmlStr("]; createLabelInputTable(parameters, 'grid'); </script>");
	appendHtmlStr(webUpateButton);                
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
	DEBUG_PRINTLN("CompilePage_ValvesConfigs done");
}

// -----------------------------------------------------------------------------
// MapWaterConfigPage
// Allocates the ConfigList PageVar array (VALVE_CONFIGS × VALVE_COUNT entries)
// and binds each entry to the corresponding ValveConfig[i].ValveStatus[j].State
// field, using a flat linear index I = i*VALVE_COUNT + j.
// The ElecValvesList array (valve names) must already exist from MapWaterValvesPage().
// -----------------------------------------------------------------------------
void MapWaterConfigPage(){
	// ElecValvesList already allocated by MapWaterValvesPage() — do not reallocate
	ConfigList = new PageVar[VALVE_CONFIGS*VALVE_COUNT];

	int I = 0;
	for (int i=0; i < VALVE_CONFIGS; i++)
	{
		ValveConfigurations* currentConfig = &ElecValves->ValveConfig[i];
		for (int j=0; j < VALVE_COUNT; j++)
		{
			ValveActiveState* currentStatus = &currentConfig->ValveStatus[j];
			ConfigList[I].Var = new PageElement(&currentStatus->State, "", "", &SetStyle, I+ID_VALCONF);
			I++;
		}
	}
}

// -----------------------------------------------------------------------------
// WT_SETUP_Config
// Populates PageBuf with the valve configuration matrix HTML.
// The header row lists the configuration name label followed by each valve's
// user-defined name (from ElecValvesList).  Each data row shows the config
// name as a non-editable label, then a checkbox / select input per valve.
// The WT_FIRST / WT_NEXT / WT_LAST parameters control cell borders.
// -----------------------------------------------------------------------------
void WT_SETUP_Config(){
	resetPageBuf();
	WT_StartPage();

	// ── Column headers: "Configuration" + one per valve name ─────────────────
	WT_Header("Configuration");
	for (int i=0; i < VALVE_COUNT; i++)
	{
		WT_Header(ElecValvesList[i].Var->GetName().c_str());
	}

	int I = 0;
	for (int i=0; i < VALVE_CONFIGS; i++)
	{
		WT_StartLine();
		String ID = F("ID"); ID += i;
		// Config name label (non-editable — uses a synthetic ID not in the update list)
		WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text", ElecValves->ValveConfig[i].ConfigName.c_str(), ID.c_str(), "", &ConfigList[0]);
		for (int j=0; j < VALVE_COUNT; j++)
		{
			int Par2 = (j == 0)             ? WT_FIRST : WT_NEXT;
			int Par3 = (j == VALVE_COUNT-1) ? WT_LAST  : WT_MORE;
			WT_AutoColumn(WT_INPUT, Par2, Par3, &ConfigList[I]);
			I++;
		}
		WT_EndLine();
	}
	WT_EndPage();
}

// -----------------------------------------------------------------------------
// SendConfigPageValues
// Pushes changed valve configuration state values to connected WebSocket clients.
// Iterates the full VALVE_CONFIGS × VALVE_COUNT flat index.
// -----------------------------------------------------------------------------
void SendConfigPageValues(){
	int I = 0;
	for (int i=0; i < VALVE_CONFIGS; i++)
	{
		for (int j=0; j < VALVE_COUNT; j++)
		{
			if(ConfigList[I].Var->HasChanged())
			{
				webParamUpdate(ConfigList[I].Var->GetName(),
				               ConfigList[I].Var->GetID(""),
				               ConfigList[I].Var->GetVarStyle());
				ConfigList[I].Var->UpdateLast();
			}
			I++;
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateConfigPageValues
// Called by UnpackJasonPayload() when the browser sends an UPDATE payload from
// the valve configs page.  Matches each JSON key to a ConfigList entry by
// WebSocket ID and applies the new state.  After persisting to /WaterValves.json,
// re-applies the default config to physical outputs so changes are reflected
// immediately — but only when no irrigation, tank fill, or manual borehole run
// is currently controlling the valves.
// -----------------------------------------------------------------------------
void UpdateConfigPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		int I = 0;
		for (int i=0; i < VALVE_CONFIGS; i++)
		{
			for (int j=0; j < VALVE_COUNT; j++)
			{
				if(ConfigList[I].Var->GetID("") == kv.key().c_str())
				{
					ConfigList[I].Var->SetName(String(kv.value().as<const char*>()));
				}
				I++;
			}
		}
	}
	WebMessUpdate(clientId, "Configuration Updated!", "color: red", true);
	ElecValves->WriteData();
	// Re-apply default config so output pins immediately reflect any changes,
	// but only when the valves are not in use by another process
	if (IrriActivityLine < 0 && !BoreholeStartedByTank && !MANUAL_STARTED) {
		ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);
	}
}

// =============================================================================
// ElectricValves Class — Constructor, Factory, Init, Control, Persistence
// =============================================================================

// -----------------------------------------------------------------------------
// CreateElectricValves
// Allocates the global ElecValves object and calls Init() to set PCB terminal
// labels, default valve names, switch delays, configuration names, and load
// stored settings from /WaterValves.json.
// Called once from InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreateElectricValves() {
	ElecValves = new ElectricValves;
	ElecValves->Init();
}

// -----------------------------------------------------------------------------
// CreateValvesConfigMaps
// Allocates the ElecValvesList and ConfigList PageVar arrays for the valve
// names and valve configs web pages.  Sets ElectricValvesObject = true to
// suppress lazy re-initialisation on subsequent page requests.
// Called lazily from handleWaterValves() / handleValvesConfigs() on first visit.
// -----------------------------------------------------------------------------
void CreateValvesConfigMaps() {
	ElectricValvesObject = true;
	MapWaterValvesPage();
	MapWaterConfigPage();
}

ElectricValves::ElectricValves() {}   // No dynamic members to initialise
ElectricValves::~ElectricValves() {}  // No heap members to release

// -----------------------------------------------------------------------------
// ElectricValves::Init
// Sets compile-time defaults for PCB terminal labels, valve names, switch
// delays, configuration names, and relevance categories.  Calls ReadData()
// at the end to overwrite defaults with any values saved in /WaterValves.json.
// All defaults can be overridden by the user via the web UI.
// -----------------------------------------------------------------------------
void ElectricValves::Init() {
	// ── PCB terminal labels (fixed wiring, not user-editable) ────────────────
	WaterValve[VALVE_1].Terminal = "AC6";
	WaterValve[VALVE_2].Terminal = "AC5";
	WaterValve[VALVE_3].Terminal = "AC4";
	WaterValve[VALVE_4].Terminal = "AC2";
	WaterValve[VALVE_5].Terminal = "AC1";
	WaterValve[VALVE_6].Terminal = "AC3";
	WaterValve[VALVE_7].Terminal = "AC7";

	// ── Default valve names (matches physical valve labels on the manifold) ──
	WaterValve[VALVE_1].Name = "V3";
	WaterValve[VALVE_2].Name = "V8";
	WaterValve[VALVE_3].Name = "V10";
	WaterValve[VALVE_4].Name = "V7";
	WaterValve[VALVE_5].Name = "V2";
	WaterValve[VALVE_6].Name = "V9";
	WaterValve[VALVE_7].Name = "V17";

	// ── Switch delays — VALVE_5 uses the longer delay (solenoid type) ────────
	WaterValve[VALVE_1].SwitchDelay = VALVE_SWITCH_DELAY_1;   // 500 ms
	WaterValve[VALVE_2].SwitchDelay = VALVE_SWITCH_DELAY_1;
	WaterValve[VALVE_3].SwitchDelay = VALVE_SWITCH_DELAY_1;
	WaterValve[VALVE_4].SwitchDelay = VALVE_SWITCH_DELAY_1;
	WaterValve[VALVE_5].SwitchDelay = VALVE_SWITCH_DELAY_2;   // 15 000 ms
	WaterValve[VALVE_6].SwitchDelay = VALVE_SWITCH_DELAY_1;
	WaterValve[VALVE_7].SwitchDelay = VALVE_SWITCH_DELAY_1;

	// ── Configuration names and relevance categories ─────────────────────────
	ValveConfig[CONFIG_1].ConfigName  = "Tank Zone 1"; ValveConfig[CONFIG_1].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_2].ConfigName  = "Tank Zone 2"; ValveConfig[CONFIG_2].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_3].ConfigName  = "Tank Zone 3"; ValveConfig[CONFIG_3].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_4].ConfigName  = "BH Zone 1";   ValveConfig[CONFIG_4].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_5].ConfigName  = "BH Zone 2";   ValveConfig[CONFIG_5].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_6].ConfigName  = "BH Zone 3";   ValveConfig[CONFIG_6].Relevance  = REL_IRRILINE;
	ValveConfig[CONFIG_MAN].ConfigName = "Manual";      ValveConfig[CONFIG_MAN].Relevance = REL_MANUAL;
	ValveConfig[CONFIG_TANK].ConfigName = "Tank";       ValveConfig[CONFIG_TANK].Relevance = REL_TANK;
	ValveConfig[CONFIG_DEF].ConfigName  = "Default";    ValveConfig[CONFIG_DEF].Relevance  = REL_DEFAULT;

	ElecValves->ReadData();   // Overwrite defaults with stored values
}

// -----------------------------------------------------------------------------
// ElectricValves::MaintainValveConfig
// Called every 10 ms from loop() via CheckSystemStatus().
// After SwitchValveConfig() sets RequiredState and UpdateTime for each valve,
// this function drives the GPIO pin once UpdateTime is reached.  This staged
// timing prevents energising multiple solenoids simultaneously, which would
// cause an inrush current spike on the 24 VAC supply.
// -----------------------------------------------------------------------------
void ElectricValves::MaintainValveConfig() {
	for (int k=0; k < VALVE_COUNT; k++){
		if( WaterValve[k].UpdateTime >= millis()) {
			if(digitalRead(OutPort[k].Port) != WaterValve[k].RequiredState)
				digitalWrite(OutPort[k].Port, WaterValve[k].RequiredState);
		}
	}
}

// -----------------------------------------------------------------------------
// ElectricValves::SwitchValveConfig
// Schedules all 7 valve outputs to transition to the state defined by
// configuration ConfigNr.
//   ValveActiveState == VALVE_OPEN  → apply ConfigNr's ACTIVE/INACTIVE pattern.
//   ValveActiveState == VALVE_CLOSE → apply CONFIG_DEF (all off / safe state).
// For each valve, RequiredState is set and UpdateTime is set to
// millis() + SwitchDelay so MaintainValveConfig() staggers the GPIO writes.
// -----------------------------------------------------------------------------
void ElectricValves::SwitchValveConfig(int ConfigNr, bool ValveActiveState) {
	int ConfNumber = ConfigNr;
	if(ValveActiveState == VALVE_CLOSE) ConfNumber = CONFIG_DEF;   // Force default config on close

	for (int k=0; k < VALVE_COUNT; k++){
		WaterValve[k].RequiredState = (ValveConfig[ConfNumber].ValveStatus[k].State == VALVE_ACTIVE)
		                               ? VALVE_OPEN : VALVE_CLOSE;
		WaterValve[k].UpdateTime = millis() + WaterValve[k].SwitchDelay;
	}
}

// -----------------------------------------------------------------------------
// ElectricValves::WriteData
// Serialises valve names, switch delays, configuration relevance categories,
// and all VALVE_CONFIGS × VALVE_COUNT active-state booleans to /WaterValves.json.
// JSON keys use a compact generated format (e.g. "VC_0C0V0") to keep the
// document small enough for the 6 KB DynamicJsonDocument.
// -----------------------------------------------------------------------------
void ElectricValves::WriteData() {
	File ChannelDataFile = SPIFFS.open(StrElectricValves, "w");
	DynamicJsonDocument doc(6000);

	String docDesc;
	for (int i=0; i < VALVE_COUNT; i++){
		docDesc = F("ValName_"); docDesc += i; doc[docDesc] = WaterValve[i].Name;
		docDesc = F("ValDel_");  docDesc += i; doc[docDesc] = WaterValve[i].SwitchDelay;
	}

	int J = 0;
	for (int i=0; i < VALVE_CONFIGS; i++){
		docDesc = F("ConfRel_"); docDesc += i; doc[docDesc] = ValveConfig[i].Relevance;
		for (int j=0; j < VALVE_COUNT; j++){
			docDesc = F("VC_"); docDesc += J; docDesc += F("C"); docDesc += i; docDesc += F("V"); docDesc += j;
			doc[docDesc] = ValveConfig[i].ValveStatus[j].State;
			J++;
		}
	}

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

// -----------------------------------------------------------------------------
// ElectricValves::ReadData
// Deserialises /WaterValves.json into the WaterValve[] and ValveConfig[] arrays.
// Missing or "null" keys leave the Init() defaults in place.
// Switch delay key: "ValDel_N" (matches WriteData).
// -----------------------------------------------------------------------------
void ElectricValves::ReadData() {
	File ChannelDataFile = SPIFFS.open(StrElectricValves, "r");
	if (ChannelDataFile)
	{
		size_t size = ChannelDataFile.size();
		if (size <= 6000)
		{
			std::unique_ptr<char[]> buf(new char[size]); // Stack-safe heap buffer for file contents
			ChannelDataFile.readBytes(buf.get(), size);
			DynamicJsonDocument doc(6000);
			auto error = deserializeJson(doc, buf.get());
			if (!error)
			{
				String docDesc;
				for (int i=0; i < VALVE_COUNT; i++){
					docDesc = F("ValName_");   docDesc += i;
					if ( doc[docDesc].as<String>() != "null" ) WaterValve[i].Name = doc[docDesc].as<String>();
					docDesc = F("ValDel_"); docDesc += i;
					if ( doc[docDesc].as<String>() != "null" ) WaterValve[i].SwitchDelay = doc[docDesc];
				}

				int J = 0;
				for (int i=0; i < VALVE_CONFIGS; i++){
					docDesc = F("ConfRel_"); docDesc += i;
					if ( doc[docDesc].as<String>() != "null" ) ValveConfig[i].Relevance = doc[docDesc];

					for (int j=0; j < VALVE_COUNT; j++){
						docDesc = F("VC_"); docDesc += J; docDesc += F("C"); docDesc += i; docDesc += F("V"); docDesc += j;
						if ( doc[docDesc].as<String>() != "null" ) ValveConfig[i].ValveStatus[j].State = doc[docDesc];
						J++;
					}
				}
			}
			else
			{
				DEBUG_PRINT("Error deserialize "); DEBUG_PRINTLN(StrElectricValves);
			}
		}
		else
		{
			DEBUG_PRINT(StrElectricValves); DEBUG_PRINTLN(" File size is > 6000 ");
		}
		ChannelDataFile.close();
	}
}
