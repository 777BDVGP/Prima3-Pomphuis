// =============================================================================
// 09_Tank.cpp — Tank Level, Filter Pressure, Borehole Control & Web Page
// =============================================================================
// Implements:
//   - PresureSensor class: 15-point ADC averaging, kPa conversion, persistence.
//   - TankLevel class:     15-point ADC averaging, % calculation, persistence.
//   - Tank web page (/tank): MapTankPage(), WT_SETUP_Tank(), Send/Update handlers.
//   - MaintainTankFilling(): float-switch / level-sensor fill logic, borehole
//     start/stop with pre-condition guards, filter pump control, manual pump.
//   - StartBorehole() / StopBorehole() / MaintainStatusOfValves() utilities.
//   - CreatePresureSensors(): object factory called from InitFileSystem().
//
// Borehole note numbering used throughout this file:
//   Note 1 — Pre-condition checks before starting the borehole.
//   Note 2 — Response-confirmation timeout after start (TIM_BOREHOLE_TOUT).
//   Note 3 — Filter pump is started with the borehole and stopped after a
//             cooldown delay (TIM_TANK_ON_DELAY) once the borehole stops.
//   Note 4 — Borehole disabled mid-run: shut down immediately.
//   Note 5 — Pump signal absent or lost mid-run (IIDX_ME_PUMP_BH goes LOW after
//             initial confirmation): close valve, stop borehole, turn filter off
//             immediately, start TIM_BOREHOLE_RETRY.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// PresureSensor Class — ADC Averager & Filter Pressure Sensor
// =============================================================================

// -----------------------------------------------------------------------------
// PresureSensor::PresureSensor  (constructor)
// Stores the ADC pin number.  All other fields use their inline defaults.
// -----------------------------------------------------------------------------
PresureSensor::PresureSensor(int Pin) {
	IOPin = Pin;
}
PresureSensor::~PresureSensor() {}   // No heap members to release

// -----------------------------------------------------------------------------
// PresureSensor::Maintain
// Called every TIM_PRESURE_SENSOR (300 ms) by MaintainTankFilling().
// Maintains a 15-point circular buffer of raw ADC readings:
//   - On first call (Idx == -1): pre-fills the entire buffer so the average
//     is valid from the very first reading, avoiding a ramp-up period.
//   - On subsequent calls: writes one new ADC sample into the next buffer slot,
//     computes the rolling average, and updates StrFilterKPa for display.
// Sensor range: 0–RawValueMax → 0–400 kPa (gauge range).
// When the filter pump is not running, StrFilterKPa is forced to "0kPa"
// regardless of the raw ADC reading (tank back-pressure is ignored).
// -----------------------------------------------------------------------------
void PresureSensor::Maintain(){
	if(TimerExpired(TIM_PRESURE_SENSOR)){
		activateTimer(TIM_PRESURE_SENSOR);

		// Pre-fill buffer on first call so average is valid immediately
		if(Idx == -1){
			Idx = 0;
			for(int i = 0; i < TANK_AVE_OF; i++){
				AveList[i] = analogRead(IOPin);
			}
		}
		// Update one slot in the circular buffer
		AveList[Idx] = analogRead(IOPin);
		Idx++; if(Idx >= TANK_AVE_OF) Idx = 0;

		// Compute rolling average across all buffer entries
		RawValue = 0;
		for(int i = 0; i < TANK_AVE_OF; i++){
			RawValue += AveList[i];
		}
		RawValue = RawValue / TANK_AVE_OF;
		StrFilterRawValue = String(RawValue);
		if(!OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat) {
			StrFilterKPa = "Filter Off";
		} else if(RawValueMax > 0) {
			StrFilterKPa = String((int)((float)RawValue / (float)RawValueMax * 103.4f)) + "kPa";
		}

		// Latch FilterPressureExceeded on rising edge — persist immediately to JSON and log
		if (getValue() > BackwashWarning && !FilterPressureExceeded) {
			FilterPressureExceeded = true;
			WriteData();
			SaveTankFilterReport("Filter Pressure Exceeded: " + String(getValue()) + " kPa (Limit: " + String(BackwashWarning) + " kPa)");
			if (NtfyData->EnableBackwashNotify)
				MessageNTFY("Filter Pressure " + String(getValue()) + "kPa > " +
				            String(BackwashWarning) + "kPa on " + Params->SysName);
		}
	}
}

// -----------------------------------------------------------------------------
// PresureSensor::getValue
// Returns 0 when the filter pump is not running (tank back-pressure ignored).
// When running, maps RawValue linearly from 0–RawValueMax to 0–400 kPa.
// Returns 0 if RawValueMax is not calibrated (avoids divide-by-zero).
// -----------------------------------------------------------------------------
int PresureSensor::getValue(){
	if(!OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat) return 0;
	if(RawValueMax <= 0) return 0;
	return (int)((float)RawValue / (float)RawValueMax * 103.4f);
}

// -----------------------------------------------------------------------------
// PresureSensor::WriteData
// Serialises calibration parameters to /FilterSensorData.json.
// -----------------------------------------------------------------------------
void PresureSensor::WriteData(){
	String StrFileName = "/FilterSensorData.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "w");
	DynamicJsonDocument doc(256);

	doc["RawValueMax"]              = RawValueMax;
	doc["BackwashWarning"]          = BackwashWarning;
	doc["FilterPressureExceeded"]   = FilterPressureExceeded;

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

// -----------------------------------------------------------------------------
// PresureSensor::ReadData
// Deserialises /FilterSensorData.json.  Missing or "null" keys leave the
// inline defaults in place.
// -----------------------------------------------------------------------------
void PresureSensor::ReadData(){
	String StrFileName = "/FilterSensorData.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "r");
	if (ChannelDataFile)
	{
		size_t size = ChannelDataFile.size();
		if (size <= 256)
		{
			std::unique_ptr<char[]> buf(new char[size]);
			ChannelDataFile.readBytes(buf.get(), size);
			DynamicJsonDocument doc(256);
			auto error = deserializeJson(doc, buf.get());
			if (!error)
			{
				if ( doc["RawValueMax"].as<String>()             != "null" ) RawValueMax             = doc["RawValueMax"];
				if ( doc["BackwashWarning"].as<String>()         != "null" ) BackwashWarning         = doc["BackwashWarning"];
				if ( doc["FilterPressureExceeded"].as<String>()  != "null" ) FilterPressureExceeded  = doc["FilterPressureExceeded"].as<bool>();
			}
			else
			{
				DEBUG_PRINT("Error deserialize "); DEBUG_PRINTLN(StrFileName);
			}
		}
		else
		{
			DEBUG_PRINT(StrFileName);  DEBUG_PRINTLN(" File size is > 256 ");
		}
		ChannelDataFile.close();
	}
}

// =============================================================================
// TankLevel Class — ADC Averager, % Calculator & Tank Settings Persistence
// =============================================================================

// -----------------------------------------------------------------------------
// TankLevel::TankLevel  (constructor)
// Stores the ADC pin number.  All other fields use their inline defaults.
// -----------------------------------------------------------------------------
TankLevel::TankLevel(int Pin) {
	IOPin = Pin;
}
TankLevel::~TankLevel() {}   // No heap members to release

// -----------------------------------------------------------------------------
// TankLevel::WriteData
// Serialises all TankLevel configuration fields to /TankLevelData.json.
// Called after every UpdateTankPageValues() to persist user changes.
// -----------------------------------------------------------------------------
void TankLevel::WriteData(){
	String StrFileName = "/TankLevelData.json";
	File ChannelDataFile = SPIFFS.open(StrFileName, "w");
	DynamicJsonDocument doc(1024);

	doc["RawValueMax"] = RawValueMax;
	doc["RawValueMaxFil"] = RawValueMaxFil;
	doc["RawValueMin"] = RawValueMin;
	doc["StartFillLevel"] = StartFillLevel;
	doc["StopFillLevel"] = StopFillLevel;
	doc["FilterPumpDelay"] = FilterPumpDelay;
	doc["BoreholOperation"] = BoreholOperation;
	doc["BoreholeTimeout"]  = BoreholeTimeout;
	doc["BoreholeRetry"]    = BoreholeRetry;
	doc["User_1"] = User_1;
	doc["User_2"] = User_2;
	doc["TankFillingSense"] = TankFillingSense;
	doc["TankFillingTime"] = TankFillingTime;
	doc["TankFillStartTime"] = TankFillStartTime;
	doc["TankFillStopTime"] = TankFillStopTime;

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

// -----------------------------------------------------------------------------
// TankLevel::ReadData
// Deserialises /TankLevelData.json into TankLevel fields.  Missing or "null"
// keys leave the inline defaults in place — safe on a fresh device.
// -----------------------------------------------------------------------------
void TankLevel::ReadData(){
	String StrFileName = "/TankLevelData.json";
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
				if ( doc["RawValueMax"].as<String>() != "null" ) RawValueMax = doc["RawValueMax"];
				if ( doc["RawValueMaxFil"].as<String>() != "null" ) RawValueMaxFil = doc["RawValueMaxFil"];
				if ( doc["RawValueMin"].as<String>() != "null" ) RawValueMin = doc["RawValueMin"];
				if ( doc["StartFillLevel"].as<String>() != "null" ) StartFillLevel = doc["StartFillLevel"];
				if ( doc["StopFillLevel"].as<String>() != "null" ) StopFillLevel = doc["StopFillLevel"];
				if ( doc["FilterPumpDelay"].as<String>() != "null" ) { FilterPumpDelay = doc["FilterPumpDelay"]; }
				if ( doc["BoreholOperation"].as<String>() != "null" ){ BoreholOperation = doc["BoreholOperation"].as<String>(); }
				if ( doc["BoreholeTimeout"].as<String>()  != "null" ) { BoreholeTimeout  = doc["BoreholeTimeout"]; }
				if ( doc["BoreholeRetry"].as<String>()    != "null" ) { BoreholeRetry    = doc["BoreholeRetry"]; }
				if ( doc["User_1"].as<String>() != "null" ){ User_1 = doc["User_1"].as<String>(); }
				if ( doc["User_2"].as<String>() != "null" ){ User_2 = doc["User_2"].as<String>(); }
				if ( doc["TankFillingSense"].as<String>() != "null" ){ TankFillingSense = doc["TankFillingSense"].as<String>();}
				if ( doc["TankFillingTime"].as<String>() != "null" ){ TankFillingTime = doc["TankFillingTime"].as<String>();}
				if ( doc["TankFillStartTime"].as<String>() != "null" ){ TankFillStartTime = doc["TankFillStartTime"].as<String>();}
				if ( doc["TankFillStopTime"].as<String>() != "null" ){ TankFillStopTime = doc["TankFillStopTime"].as<String>();}
			}
			else
			{
				DEBUG_PRINT("Error deserialize "); DEBUG_PRINTLN(StrFileName);
			}
		}
		else
		{
			DEBUG_PRINT(StrFileName);  DEBUG_PRINTLN(" File size is > 1024 ");
		}
		ChannelDataFile.close();
	}
}

// -----------------------------------------------------------------------------
// TankLevel::getValue
// Returns the tank level as a percentage (0–100).
// Uses two calibration ranges depending on whether the filter pump is running:
//   Filter OFF (relay HIGH) → use RawValueMax (higher ceiling when clean water)
//   Filter ON  (relay LOW)  → use RawValueMaxFil (lower ceiling under filter load)
// This dual-range approach compensates for the slight pressure head change
// caused by the running filter pump.
// -----------------------------------------------------------------------------
int TankLevel::getValue() {
	if(!digitalRead(OutPort[OIDX_FILTER_PUMP_RELAY].Port)){
		return int(float(RawValue-RawValueMin))/((float(RawValueMax-RawValueMin))/100.0);    // Filter OFF
	} else {
		return int(float(RawValue-RawValueMin))/((float(RawValueMaxFil-RawValueMin))/100.0); // Filter ON
	}
}

// -----------------------------------------------------------------------------
// TankLevel::getValueNotFilter
// Returns the tank level percentage using only the RawValueMax calibration,
// regardless of filter pump state.  Used where an uncompensated reading is
// needed (e.g. the home page display without filter correction).
// -----------------------------------------------------------------------------
int TankLevel::getValueNotFilter() {
	return int(float(RawValue-RawValueMin))/((float(RawValueMax-RawValueMin))/100.0);
}

// -----------------------------------------------------------------------------
// TankLevel::Maintain
// Called every TIM_TANK_SENSOR (1 s) by MaintainTankFilling().
// Operates identically to PresureSensor::Maintain():
//   - Pre-fills the 15-point circular buffer on first call.
//   - Updates one slot per tick and recomputes the rolling average.
//   - Stores the raw ADC average in StrTankRawValue for the tank web page display.
// -----------------------------------------------------------------------------
void TankLevel::Maintain() {
	if(TimerExpired(TIM_TANK_SENSOR)){
		activateTimer(TIM_TANK_SENSOR);

		// Pre-fill buffer on first call so average is valid immediately
		if(Idx == -1){
			Idx = 0;
			for(int i = 0; i < TANK_AVE_OF; i++){
				AveList[i] = analogRead(IOPin);
			}
		}
		// Update one slot in the circular buffer
		AveList[Idx] = analogRead(IOPin);
		Idx++; if(Idx >= TANK_AVE_OF) Idx = 0;

		// Compute rolling average across all buffer entries
		RawValue = 0;
		for(int i = 0; i < TANK_AVE_OF; i++){
			RawValue += AveList[i];
		}
		RawValue        = RawValue / TANK_AVE_OF;
		StrTankRawValue = String(RawValue);
	}
}

// =============================================================================
// Tank Web Page — Route Handlers & PageVar Mapping
// =============================================================================

// -----------------------------------------------------------------------------
// handleTank  (GET /tank)
// Lazy-initialises the home and tank PageVar lists if not yet built, then
// serves the tank setup page.  The home list must exist because WT_SETUP_Tank()
// borrows the HOME_TANKLEVEL row for the live tank-level display.
// -----------------------------------------------------------------------------
void handleTank(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleTank " + String(ESP.getFreeHeap()));
	if(!MainPageObject) MapHomePage();
	if(!TankPageObject) MapTankPage();
	CompilePage_Tank(request);
}

// -----------------------------------------------------------------------------
// CompilePage_Tank
// Builds and serves the tank parameters page.  WT_SETUP_Tank() writes the
// parameter table into PageBuf to avoid heap fragmentation.
// -----------------------------------------------------------------------------
void CompilePage_Tank(AsyncWebServerRequest *request)
{
	WT_SETUP_Tank();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	TankPageSel();                            
	appendHtmlStr(webSocketScript);           
	appendHtmlBuf(PageBuf, getPageBufLen());  
	appendHtmlStr(webUpateButton);            
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// TankPageSel
// Builds the Tank page navigation <ul> block.  Identical tabs to the main
// menu, plus a START FILTER / STOP FILTER toggle button styled the same way
// as SUSPEND/RESUME IRRIGATION on the irrigation page.
// -----------------------------------------------------------------------------
void TankPageSel(){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr("<a href='/'>HOME</a>");
	appendHtmlStr("<a class='act'>TANK</a>");
	if (OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat)
		appendHtmlStr("<a class='warn' href='/tankstopfilter'>STOP FILTER</a>");
	else
		appendHtmlStr("<a class='warn' href='/tankstartfilter'>START FILTER</a>");
	appendHtmlStr("</div>");
}

// -----------------------------------------------------------------------------
// handleTankStartFilter  (GET /tankstartfilter)
// Starts the filter pump and re-serves the Tank page.
// -----------------------------------------------------------------------------
void handleTankStartFilter(AsyncWebServerRequest *request) {
	FILTER_ON = true;
	activateTimer(TIM_TANK_ON_DELAY);
	FilterOnStartMs = millis();
	SaveTankFilterReport("Filter ON - Web");
	digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, HIGH);
	OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = HIGH;
	CompilePage_Tank(request);
}

// -----------------------------------------------------------------------------
// handleTankStopFilter  (GET /tankstopfilter)
// Stops the filter pump (blocked if borehole is running) and re-serves the
// Tank page.
// -----------------------------------------------------------------------------
void handleTankStopFilter(AsyncWebServerRequest *request) {
	if (!OutPort[OIDX_BOREHOLE].OUTStat) {   // Only stop if borehole is not running
		FILTER_ON = false;
		deactivateTimer(TIM_TANK_ON_DELAY);
		{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - Web Dur: "+fmtDur(_s)); }
		digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
		OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
	}
	CompilePage_Tank(request);
}

// -----------------------------------------------------------------------------
// MapTankPage
// Allocates the TankList PageVar array and binds each entry to the
// corresponding TankLevel or FilterSensor field.  Dropdown entries are
// configured with SetStringToSelect() + AddOption().
// Called lazily from handleTank() the first time the page is requested.
// -----------------------------------------------------------------------------
void MapTankPage(){
	TankPageObject = true;

	TankList = new PageVar[PAGE_VAR_TANK];
	for (int i=0; i < PAGE_VAR_TANK; i++)
	{
		switch(i)
		{
			case TANK_USER1:
				TankList[i].Var = new PageElement(&TankWaterLevel->User_1, F("Username for User at Prima 3a"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_USER2:
				TankList[i].Var = new PageElement(&TankWaterLevel->User_2, F("Username for User at Prima 3"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_RAW_VALUE:  // Display only
				TankList[i].Var = new PageElement(&TankWaterLevel->StrTankRawValue, F("Tank Raw Value:"), "", &StrTankRawValueAtt, i+ID_TANK);
				break;
			case TANK_SENSOR_100:
				TankList[i].Var = new PageElement(&TankWaterLevel->RawValueMax, F("Tank Sensor 100% calibration Value"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_SENSOR_100_FILTER:
				TankList[i].Var = new PageElement(&TankWaterLevel->RawValueMaxFil, F("Tank Sensor 100% calibration Filter On"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_SENSOR_0:
				TankList[i].Var = new PageElement(&TankWaterLevel->RawValueMin, F("Tank Sensor 0% calibration Value"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_FILL_SENSE:
				TankList[i].Var = new PageElement(&TankWaterLevel->TankFillingSense, F("Tank Filling Sensor type"), "", &SetStyle, i+ID_TANK);
				TankList[i].Var->SetStringToSelect(2);
				TankList[i].Var->AddOption(TANK_FILL_SENSE_FLOAT);
				TankList[i].Var->AddOption(TANK_FILL_SENSE_LEVEL);
				break;
			case TANK_LEVEL_START_FILL:
				TankList[i].Var = new PageElement(&TankWaterLevel->StartFillLevel, F("Start % for Level Sensor"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_LEVEL_STOP_FILL:
				TankList[i].Var = new PageElement(&TankWaterLevel->StopFillLevel, F("Stop % for Level Sensor"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_FILL_OPER:
				TankList[i].Var = new PageElement(&TankWaterLevel->TankFillingTime, F("Tank Filling Time of the day"), "", &SetStyle, i+ID_TANK);
				TankList[i].Var->SetStringToSelect(2);
				TankList[i].Var->AddOption(TANK_FILL_OPER_NORMAL);
				TankList[i].Var->AddOption(TANK_FILL_OPER_SHEDULE);
				break;
			case TANK_SHED_START:
				TankList[i].Var = new PageElement(&TankWaterLevel->TankFillStartTime, F("Start for Scheduled Time"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_SHED_STOP:
				TankList[i].Var = new PageElement(&TankWaterLevel->TankFillStopTime, F("Stop for Scheduled Time"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_FILTPUMP:
				TankList[i].Var = new PageElement(&TankWaterLevel->FilterPumpDelay, F("Filter Pump extra time (Min)"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_BORE_HOLE_OPER:
				TankList[i].Var = new PageElement(&TankWaterLevel->BoreholOperation, F("Borehole Operation Permission"), "", &SetStyle, i+ID_TANK);
				TankList[i].Var->SetStringToSelect(2);
				TankList[i].Var->AddOption(TANK_BORHOLE_ENABLE);
				TankList[i].Var->AddOption(TANK_BORHOLE_DISABLE);
				break;
			case TANK_BORE_TOUT:
				TankList[i].Var = new PageElement(&TankWaterLevel->BoreholeTimeout, F("Borehole Pumping Timeout (sec)"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_BORE_RETRY:
				TankList[i].Var = new PageElement(&TankWaterLevel->BoreholeRetry, F("Borehole Retry after Timeout (min)"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_KPA_VALUE:  // Display only
				TankList[i].Var = new PageElement(&FilterSensor->StrFilterKPa, F("Filter Pressure:"), "", &StrFilterPressureAtt, i+ID_TANK);
				break;
			case TANK_SENSOR_MAX:
				TankList[i].Var = new PageElement(&FilterSensor->RawValueMax, F("Filter Sensor Calibration:"), "", &SetStyle, i+ID_TANK);
				break;
			case TANK_FILTER_RAW_VALUE:  // Display only
				TankList[i].Var = new PageElement(&FilterSensor->StrFilterRawValue, F("Filter Raw Value:"), "", &StrFilterRawValueAtt, i+ID_TANK);
				break;
		}
	}
}

// -----------------------------------------------------------------------------
// WT_SETUP_Tank
// Populates PageBuf with the tank parameter table HTML.
// Layout notes:
//   - User 1 and User 2 names are rendered first as standard input rows.
//   - The live tank level display is borrowed from HomeList[HOME_TANKLEVEL] so
//     MaintainHomePage() keeps it updated via the home-page WebSocket IDs.
//   - TANK_RAW_VALUE and TANK_KPA_VALUE are read-only display rows rendered with
//     the "V" ID suffix and a plain-label WT_Column (not an input field).
//   - TANK_SHED_START / TANK_SHED_STOP use HTML input type="time".
//   - All other parameters use the type returned by GetTypeStr().
// -----------------------------------------------------------------------------
void WT_SETUP_Tank(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("Tank Parameters", "Value", "");

	// ── User names ───────────────────────────────────────────────────────────
	WT_EditRow(&TankList[TANK_USER1]);
	WT_EditRow(&TankList[TANK_USER2]);

	// ── Live tank level (display only — updated by MaintainHomePage via HomeList) ─
	WT_DisplayRow(&HomeList[HOME_TANKLEVEL]);

	// ── Raw ADC value (display only — uses "V" ID suffix) ──────────────────
	WT_DisplayRow(&TankList[TANK_RAW_VALUE]);

	for (int i=TANK_SENSOR_100; i <= TANK_BORE_RETRY; i++)
	{
		const char* tov = (i == TANK_SHED_START || i == TANK_SHED_STOP) ? "time" : "";
		WT_EditRow(&TankList[i], tov);
	}

	// ── Filter pressure live display (display only) ──────────────────────────
	WT_DisplayRow(&TankList[TANK_KPA_VALUE]);

	// ── Filter raw ADC value (display only) ─────────────────────────────────
	WT_DisplayRow(&TankList[TANK_FILTER_RAW_VALUE]);

	// ── Filter sensor calibration ────────────────────────────────────────────────
	// Note: Backwash warning and filter-exceeded flag moved to /ntfy page
	WT_EditRow(&TankList[TANK_SENSOR_MAX]);

	WT_EndPageClass("compact");
}

// -----------------------------------------------------------------------------
// SendTankPageValues
// Called periodically by MaintainWebPage() to push changed tank parameter
// values to connected WebSocket clients.
// Display-only entries (TANK_RAW_VALUE, TANK_KPA_VALUE) use the "V" element ID
// suffix because they are rendered as label columns, not input fields.
// All other entries use the standard "" suffix (input element ID).
// -----------------------------------------------------------------------------
void SendTankPageValues(){
	for (int i=0; i < PAGE_VAR_TANK; i++)
	{
		if(TankList[i].Var->HasChanged())
		{
			// Display-only rows use the "V" suffix (label element ID)
			if(i == TANK_RAW_VALUE || i == TANK_KPA_VALUE || i == TANK_FILTER_RAW_VALUE)
				webParamUpdate(TankList[i].Var->GetName(), TankList[i].Var->GetID("V"), TankList[i].Var->GetVarStyle());
			else
				webParamUpdate(TankList[i].Var->GetName(), TankList[i].Var->GetID(""), TankList[i].Var->GetVarStyle());
			TankList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateTankPageValues
// Called by UnpackJasonPayload() when the browser sends an UPDATE payload from
// the tank page.  Applies all JSON key-value pairs to the matching TankList
// entry by WebSocket ID, then persists all three affected data stores and
// refreshes software timer durations that depend on the tank settings.
// -----------------------------------------------------------------------------
void UpdateTankPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		for (int i=0; i < PAGE_VAR_TANK; i++)
		{
			if(TankList[i].Var->GetID("") == kv.key().c_str())
			{
				TankList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	WebMessUpdate(clientId, "Tank Parameters Updated!", "color: red", true);
	TankWaterLevel->WriteData();   // Persist tank calibration and fill settings
	FilterSensor->WriteData();     // Persist filter sensor calibration
	Params->WriteData();           // Persist any AllParams fields visible on this page
	UpdateTimerTimes();            // Apply any timer-duration changes (e.g. borehole timeout)
}

// =============================================================================
// Object Factory & Borehole Control Utilities
// =============================================================================

// -----------------------------------------------------------------------------
// CreatePresureSensors
// Allocates TankWaterLevel and FilterSensor objects and loads their stored
// calibration and configuration from SPIFFS.  Called once from
// InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreatePresureSensors() {
	TankWaterLevel = new TankLevel(IPIN_TANK_LEVEL_SENCE);
	TankWaterLevel->ReadData();

	FilterSensor = new PresureSensor(IPIN_TANK_FILTER_SENCE);
	FilterSensor->ReadData();
}

// -----------------------------------------------------------------------------
// StartBorehole
// Attempts to start the borehole pump (Note 1 — pre-condition check):
//   - Returns false if User 2 or User 1 is already pumping from the borehole.
//   - Returns false if BoreholOperation is disabled in the tank settings.
//   - Returns false if TIM_BOREHOLE_TOUT has not expired (enforces minimum
//     off-time between start attempts).
// On success:
//   - Drives OIDX_BOREHOLE LOW (active-low = START).
//   - Starts the filter pump relay (Note 3).
//   - Arms TIM_BOREHOLE_TOUT to gate the response-confirmation check (Note 2).
// Returns true on success, false if any precondition fails.
// -----------------------------------------------------------------------------
bool StartBorehole() {
	if (InPort[IIDX_ME_PUMP_BH].Input || InPort[IIDX_OTHER_PUMP_HB].Input) return false;  // Another user pumping
	if (TankWaterLevel->BoreholOperation != TANK_BORHOLE_ENABLE)             return false;  // Borehole disabled
	if (!TimerExpired(TIM_BOREHOLE_TOUT))                                    return false;  // Retry wait not elapsed
	digitalWrite(OutPort[OIDX_BOREHOLE].Port, HIGH);            // HIGH = START (active-high relay)
	OutPort[OIDX_BOREHOLE].OUTStat = HIGH;
	FilterOnStartMs = millis();
	SaveTankFilterReport("Filter ON - Borehole");
	digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, HIGH);   // Start filter pump (Note 3)
	OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = HIGH;
	activateTimer(TIM_BOREHOLE_TOUT);                           // Arm response-confirmation timeout (Note 2)
	BoreholeCheckPending = true;
	return true;
}

// -----------------------------------------------------------------------------
// StopBorehole
// Stops the borehole pump (HIGH = STOP on active-low relay) and arms the
// filter pump cooldown timer (TIM_TANK_ON_DELAY).  The filter relay stays on
// until TIM_TANK_ON_DELAY expires so the filter continues circulating water
// briefly after the borehole stops (Note 3).
// -----------------------------------------------------------------------------
void StopBorehole() {
	digitalWrite(OutPort[OIDX_BOREHOLE].Port, LOW);   // LOW = STOP (active-high relay)
	OutPort[OIDX_BOREHOLE].OUTStat = LOW;
	BoreholeCheckPending = false;
	activateTimer(TIM_TANK_ON_DELAY);                 // Start filter cooldown countdown (Note 3)
}

// =============================================================================
// MaintainTankFilling
// =============================================================================

// -----------------------------------------------------------------------------
// MaintainTankFilling
// Called every 10 ms from loop().  Drives all tank and borehole activity:
//
//  1. Tick ADC averagers for tank level and filter pressure sensors.
//  2. Update float-switch indicator LEDs (OIDX_FLOAT_UP_LED, OIDX_FLOAT_LOW_LED).
//  3. Evaluate scheduled fill window:
//       - "Any Time" → AllowFillingAtTime = true always.
//       - "Scheduled" → AllowFillingAtTime = false outside the configured window,
//         but only when the borehole is already running — stops any active fill
//         that has run past the window boundary.  If the borehole is not running,
//         AllowFillingAtTime stays true so float-edge detection can still start a
//         fill at the opening of the next window.
//  4. Note 4: if BoreholOperation is disabled while a tank fill is in progress,
//     close the tank valve and stop the borehole immediately.
//  5. Note 2: after borehole start (tank fill), confirm InPort[IIDX_ME_PUMP_BH]
//     went HIGH within TIM_BOREHOLE_TOUT.  On failure: close valve, stop
//     borehole, turn filter off immediately, and start TIM_BOREHOLE_RETRY.
//  6. Retry gate: set TankFillChange = true after TIM_BOREHOLE_RETRY expires
//     so the fill logic retries starting the borehole.
//  7. Outside-window shutdown: if AllowFillingAtTime became false and the
//     borehole is still running a tank fill, stop it now.
//  8. Float Switch fill mode:
//       - Rising edge on IIDX_TANK_FULL  → stop filling, log duration.
//       - Rising edge on IIDX_TANK_LOW   → start filling via StartBorehole().
//       - Keep TIM_TANK_ON_DELAY alive while tank is low so the filter stays on.
//  9. Level Sensor fill mode:
//       - Level ≥ StopFillLevel  → stop filling, log level + duration.
//       - Level < StartFillLevel → start filling via StartBorehole().
// 10. Manual filter ON/OFF button (IIDX_TANK_FILTER_ON_OFF):
//       Button press toggles the filter relay.  Stopping the filter while the
//       borehole is running is blocked (Note 4) to protect the pump.
// 11. Auto-stop filter after cooldown: TIM_TANK_ON_DELAY expiry + borehole
//     stopped → de-energise filter relay.
// 12. Manual borehole control (MANUAL_STARTED flag):
//       - Start: only if no irrigation is active and TIM_BOREHOLE_RETRY elapsed.
//       - Note 2 (manual): same response-confirmation check as tank fill.
//       - Stop: log duration, stop borehole, restore default valve config.
// -----------------------------------------------------------------------------
void MaintainTankFilling() {

	TankWaterLevel->Maintain();
	FilterSensor->Maintain();

	if (NORMAL_OPERATION)
	{
		// ── Float-switch indicator LEDs ───────────────────────────────────────
		if( InPort[IIDX_TANK_FULL].Input )
		{
			digitalWrite(OutPort[OIDX_FLOAT_LOW_LED].Port, LOW);   // FLOAT_LOW_LED ON
			digitalWrite(OutPort[OIDX_FLOAT_UP_LED].Port, HIGH);   // FLOAT_UP_LED OFF
		}
		if( InPort[IIDX_TANK_LOW].Input )
		{
			digitalWrite(OutPort[OIDX_FLOAT_LOW_LED].Port, HIGH);  // FLOAT_LOW_LED OFF
			digitalWrite(OutPort[OIDX_FLOAT_UP_LED].Port, LOW);    // FLOAT_UP_LED ON
		}

		// ── Scheduled fill window check ───────────────────────────────────────
		// AllowFillingAtTime gates all fill start/stop logic below.
		// If borehole is already running, the window boundaries are not applied
		// mid-run to avoid prematurely stopping an active fill cycle.
		if(TankWaterLevel->TankFillingTime == TANK_FILL_OPER_SHEDULE)
		{
			AllowFillingAtTime = true;
			if(GetCurrentTime() < TankWaterLevel->TankFillStartTime){
				if(!digitalRead(OutPort[OIDX_BOREHOLE].Port)) AllowFillingAtTime = false;
			}
			if(GetCurrentTime() > TankWaterLevel->TankFillStopTime){
				if(!digitalRead(OutPort[OIDX_BOREHOLE].Port)) AllowFillingAtTime = false;
			}
		}
		else
		{
			AllowFillingAtTime = true;   // "Any Time" mode — no window restriction
		}

		// Note 4: borehole operation disabled mid-run — stop immediately
		if (BoreholeStartedByTank && TankWaterLevel->BoreholOperation == TANK_BORHOLE_DISABLE) {
			ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
			StopBorehole();
			BoreholeStartedByTank = false;
		}

		// Note 2: verify borehole responded after start (tank fill)
		if (BoreholeStartedByTank && BoreholeCheckPending && TimerExpired(TIM_BOREHOLE_TOUT)) {
			BoreholeCheckPending = false;
			if (!InPort[IIDX_ME_PUMP_BH].Input) {
				// No response — close valves, stop borehole, wait before retry
				ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
				StopBorehole();
				// Borehole never pumped — no water flowing, turn filter off immediately
				deactivateTimer(TIM_TANK_ON_DELAY);
				{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - BH No Response Dur: "+fmtDur(_s)); }
				digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
				OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
				BoreholeStartedByTank = false;
				SaveGenReport("BH Retry: Tank Fill - No Response");
				activateTimer(TIM_BOREHOLE_RETRY);
				if (NtfyData->NtfyOnBoreholeRetry)
					MessageNTFY("BH Retry: " + Params->SysName + " - Tank Fill No Response");
			}
		}

		// Note 5 (tank): pump absent after timeout — covers both never-responded (6b)
		// and fell-away mid-fill (6c).  Uses TimerExpired(TIM_BOREHOLE_TOUT) so it
		// fires as soon as the 3-sec confirmation window closes if IIDX_ME_PUMP_BH
		// is still low, and also fires any time the pump drops during a running fill.
		if (BoreholeStartedByTank && TimerExpired(TIM_BOREHOLE_TOUT) && !InPort[IIDX_ME_PUMP_BH].Input) {
			ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
			StopBorehole();
			deactivateTimer(TIM_TANK_ON_DELAY);
			{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - BH Pump Lost Dur: "+fmtDur(_s)); }
			digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
			OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
			BoreholeStartedByTank = false;
			SaveGenReport("BH Retry: Tank Fill - Pump Lost");
			activateTimer(TIM_BOREHOLE_RETRY);
			if (NtfyData->NtfyOnBoreholeRetry)
				MessageNTFY("BH Retry: " + Params->SysName + " - Tank Fill Pump Lost");
		}

		// Retry fill after borehole failure (TIM_BOREHOLE_RETRY expired)
		if (!BoreholeStartedByTank && !TankIsFull && TankFillAllowed && !MANUAL_STARTED &&
		    TimerExpired(TIM_BOREHOLE_RETRY)) {
			TankFillChange = true;
		}

		// Outside scheduled fill window — shut down any active borehole fill
		if (!AllowFillingAtTime && BoreholeStartedByTank) {
			ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
			StopBorehole();
			BoreholeStartedByTank = false;
		}

		// ── Phase 8 & 9: Fill control (inside window) ────────────────────────
		if(AllowFillingAtTime)
		{
			if(TankWaterLevel->TankFillingSense == TANK_FILL_SENSE_FLOAT)  // "Float Switch" mode
			{
				// Detect float state changes to set TankFillChange edge flag
				if( FloatHigh != InPort[IIDX_TANK_FULL].Input )
				{
					FloatHigh      = InPort[IIDX_TANK_FULL].Input;
					TankFillChange = true;
					TankIsFull     = true;
				}
				if( FloatLow != InPort[IIDX_TANK_LOW].Input )
				{
					FloatLow       = InPort[IIDX_TANK_LOW].Input;
					TankFillChange = true;
					TankIsFull     = false;
				}

				if(TankFillAllowed && TankFillChange && !MANUAL_STARTED)
				{
					if(InPort[IIDX_TANK_FULL].Input)   // High-float triggered — tank is full
					{
						ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
						if (BoreholeStartedByTank) {
							StopBorehole();
							unsigned long durSec = (millis() - TankFillStartMs) / 1000;
							SaveIrrigReport("Tank Fill: Dur: " + fmtDur(durSec));
							BoreholeStartedByTank = false;
						}
					}
					if(InPort[IIDX_TANK_LOW].Input)    // Low-float triggered — start fill
					{
						if (InPort[IIDX_ME_PUMP_BH].Input) {
							// Borehole already running — open valve and proceed with fill
							BoreholeStartedByTank = true;
							TankFillStartMs = millis();
							ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_OPEN);
						} else if (StartBorehole()) {
							BoreholeStartedByTank = true;
							TankFillStartMs = millis();
							ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_OPEN);
						} else {
							if (!InPort[IIDX_OTHER_PUMP_HB].Input) {
								SaveGenReport("BH Retry: Tank Fill - Start Failed");
							}
							activateTimer(TIM_BOREHOLE_RETRY);   // Retry after cooldown
						}
					}
					TankFillChange = false;
				}

				// Keep TIM_TANK_ON_DELAY alive (filter stays on) while tank is still low
				if(InPort[IIDX_TANK_LOW].Input)
				{
					activateTimer(TIM_TANK_ON_DELAY);
				}
			}
			else   // "Level Sensor" mode — use ADC percentage thresholds
			{
				// Detect level threshold crossings to set TankFillChange edge flag
				if( TankWaterLevel->getValue() >= TankWaterLevel->StopFillLevel && LevelHigh)
				{
					LevelHigh      = false;
					TankFillChange = true;
					TankIsFull     = true;
				}
				if( TankWaterLevel->getValue() < TankWaterLevel->StartFillLevel && !LevelHigh)
				{
					LevelHigh      = true;
					TankFillChange = true;
					TankIsFull     = false;
				}

				if(TankFillAllowed && TankFillChange && !MANUAL_STARTED)
				{
					if( TankWaterLevel->getValue() >= TankWaterLevel->StopFillLevel )   // Full
					{
						ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_CLOSE);
						if (BoreholeStartedByTank) {
							StopBorehole();
							unsigned long durSec = (millis() - TankFillStartMs) / 1000;
							SaveIrrigReport("Tank Fill: Lvl " + String((int)TankWaterLevel->getValue()) + "% Dur: " +
							                fmtDur(durSec));
							BoreholeStartedByTank = false;
						}
					}
					if( TankWaterLevel->getValue() < TankWaterLevel->StartFillLevel )   // Low
					{
						if (InPort[IIDX_ME_PUMP_BH].Input) {
							// Borehole already running — open valve and proceed with fill
							BoreholeStartedByTank = true;
							TankFillStartMs = millis();
							ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_OPEN);
						} else if (StartBorehole()) {
							BoreholeStartedByTank = true;
							TankFillStartMs = millis();
							ElecValves->SwitchValveConfig(CONFIG_TANK, VALVE_OPEN);
						} else {
							if (!InPort[IIDX_OTHER_PUMP_HB].Input) {
								SaveGenReport("BH Retry: Tank Fill - Start Failed");
							}
							activateTimer(TIM_BOREHOLE_RETRY);   // Retry after cooldown
						}
					}
					TankFillChange = false;
				}

				// Keep TIM_TANK_ON_DELAY alive only while the borehole is actively filling
				// the tank. Without this guard the timer resets every 10 ms after a manual
				// stop (or mid-fill shutdown), so the filter can never auto-stop.
				if(!TankIsFull && BoreholeStartedByTank)
				{
					activateTimer(TIM_TANK_ON_DELAY);
				}
			}
		}
	}

	// ── Phase 10: Filter pump manual ON/OFF button ────────────────────────────
	// TankFilterOnOffButton is a debounce edge flag: true means the button was
	// released; false means a press has already been acted on.
	// Stopping the filter while the borehole is running is blocked (Note 4).
	if(InPort[IIDX_TANK_FILTER_ON_OFF].Input)
	{
		if(TankFilterOnOffButton){
			TankFilterOnOffButton = false;
			if(digitalRead(OutPort[OIDX_FILTER_PUMP_RELAY].Port))   // Filter currently ON
			{
				if (OutPort[OIDX_BOREHOLE].OUTStat) {
					// Borehole running (active-high HIGH) — do not stop filter (Note 4)
				} else {
					if(TankIsFull) deactivateTimer(TIM_TANK_ON_DELAY);
					{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - Button Dur: "+fmtDur(_s)); }
					digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
					OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
				}
			}
			else   // Filter currently OFF — start it and arm the cooldown timer
			{
				activateTimer(TIM_TANK_ON_DELAY);
				FilterOnStartMs = millis();
				SaveTankFilterReport("Filter ON - Button");
				digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, HIGH);
				OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = HIGH;
			}
		}
	}
	else
	{
		TankFilterOnOffButton = true;   // Button released — ready for next press
	}

	// ── Phase 11: Auto-stop filter after cooldown ─────────────────────────────
	// De-energise the filter relay once TIM_TANK_ON_DELAY expires, provided the
	// borehole is stopped (HIGH = stop on active-low relay).
	// TankIsFull is intentionally NOT checked — the filter must stop after
	// the cooldown regardless of fill-level state (tank between thresholds).
	if(TimerExpired(TIM_TANK_ON_DELAY) && !OutPort[OIDX_BOREHOLE].OUTStat && OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat) {
		FILTER_ON = false;
		{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - Cooldown Dur: "+fmtDur(_s)); }
		digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
		OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
	}

	// ── Phase 12: Manual borehole control ────────────────────────────────────
	// MANUAL_STARTED is set/cleared by handleStartBorehole() / handleStopBorehole()
	// or by the physical IIDX_MAN_START_STOP input via MaintainButtonStatus().
	static bool ManualBoreholeOn = false;

	// Note 2 (manual): confirm borehole responded after manual start
	if (MANUAL_STARTED && ManualBoreholeOn && BoreholeCheckPending && TimerExpired(TIM_BOREHOLE_TOUT)) {
		BoreholeCheckPending = false;
		if (!InPort[IIDX_ME_PUMP_BH].Input) {
			// Pump did not respond — restore default valve, stop borehole, arm retry
			ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);
			StopBorehole();
			deactivateTimer(TIM_TANK_ON_DELAY);
			{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - BH No Response Dur: "+fmtDur(_s)); }
			digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
			OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
			ManualBoreholeOn = false;
			SaveGenReport("BH Retry: Manual - No Response");
			activateTimer(TIM_BOREHOLE_RETRY);
			if (NtfyData->NtfyOnBoreholeRetry)
				MessageNTFY("BH Retry: " + Params->SysName + " - Manual No Response");
		}
	}

	// Note 5 (manual): IIDX_ME_PUMP_BH fell away mid-run after initial confirmation
	if (MANUAL_STARTED && ManualBoreholeOn && !BoreholeCheckPending && !InPort[IIDX_ME_PUMP_BH].Input) {
		ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);
		StopBorehole();
		deactivateTimer(TIM_TANK_ON_DELAY);
		{ unsigned long _s=(millis()-FilterOnStartMs)/1000; SaveTankFilterReport("Filter OFF - BH Pump Lost Dur: "+fmtDur(_s)); }
		digitalWrite(OutPort[OIDX_FILTER_PUMP_RELAY].Port, LOW);
		OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat = LOW;
		ManualBoreholeOn = false;
		SaveGenReport("BH Retry: Manual - Pump Lost");
		activateTimer(TIM_BOREHOLE_RETRY);
		if (NtfyData->NtfyOnBoreholeRetry)
			MessageNTFY("BH Retry: " + Params->SysName + " - Manual Pump Lost");
	}

	if (MANUAL_STARTED && !ManualBoreholeOn) {
		// Start manual borehole only if irrigation and tank fill are both idle
		// and the retry wait has elapsed since any previous failure
		if (IrriActivityLine < 0 && !BoreholeStartedByTank && TimerExpired(TIM_BOREHOLE_RETRY)) {
			if (StartBorehole()) {
				ManualBoreholeOn = true;
				ElecValves->SwitchValveConfig(CONFIG_MAN, VALVE_OPEN);
				ManualStartMs = millis();
			}
		}
	} else if (!MANUAL_STARTED && ManualBoreholeOn) {
		// Manual stop requested — log duration, stop borehole, restore default valve
		unsigned long durSec = (millis() - ManualStartMs) / 1000;
		SaveIrrigReport("Manual Pump: Dur: " + fmtDur(durSec));
		StopBorehole();
		ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);
		ManualBoreholeOn = false;
	}
}

// -----------------------------------------------------------------------------
// MaintainStatusOfValves
// Called once at the end of setup() to guarantee a known safe initial state:
// borehole stopped (HIGH = STOP on active-low relay) and valves set to the
// default configuration (CONFIG_DEF).
// -----------------------------------------------------------------------------
void MaintainStatusOfValves() {
	digitalWrite(OutPort[OIDX_BOREHOLE].Port, LOW);    // LOW = STOP (active-high relay)
	OutPort[OIDX_BOREHOLE].OUTStat = LOW;
	ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);
}