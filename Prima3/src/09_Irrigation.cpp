// =============================================================================
// 09_Irrigation.cpp — Irrigation Zone Management, Scheduler & Web Pages
// =============================================================================
// Implements the 9-zone irrigation system:
//   - Zone overview page (/irrigation) with per-zone manual start/stop buttons.
//   - Zone setup page (/irrigation?L=n) with 18 parameters per zone.
//   - Global suspend/resume toggle (/suspendirrigateion / /resumeirrigateion).
//   - IrrigationShedule() state machine called every 10 ms from loop().
//   - Irrigation and SheduleIrri class constructors, WriteData/ReadData, helpers.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// Irrigation Overview Page  (GET /irrigation)
// =============================================================================

// -----------------------------------------------------------------------------
// handleIrrigation  (GET /irrigation)
// Entry point for all requests to /irrigation.  Delegates to IrrigationPage()
// which handles optional URL arguments for zone action buttons.
// -----------------------------------------------------------------------------
void handleIrrigation(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleIrrigation " + String(ESP.getFreeHeap())); //BDV TAKEOUT
	IrrigationPage(request);
}

// -----------------------------------------------------------------------------
// IrrigationPage
// Handles both the plain zone overview and the action-button URL arguments:
//   "A<n>" — action button for zone n:
//            SHED/STOP status → set ManualStop to cancel the current run.
//            START status     → call SetupIrrigateShedule() for a manual start.
//            After the action, re-serve the overview page.
//   "L<n>" — zone label button for zone n:
//            Navigate to the zone setup page (CompilePage_IrrigSetup).
// No arguments → serve the zone overview page directly.
// -----------------------------------------------------------------------------
void IrrigationPage(AsyncWebServerRequest *request) {

	int Args = request->args();
	if ( Args == 1 )
	{
		int Line = -1;
		if ( request->argName(0).substring(0,1) == "A" )
		{
			// Action button pressed — toggle the scheduled/running state
			Line = request->argName(0).substring(1,2).toInt();
			if(IrriObject->GetOption(Line) == IRRI_STATUS_SHED)
			{
				SheduleObject->IrrigateShedule[Line].ManualStop = true;   // Cancel pending schedule
			}
			else if (IrriObject->GetOption(Line) == IRRI_STATUS_STOP)
			{
				SheduleObject->IrrigateShedule[Line].ManualStop = true;   // Stop active run
			}
			else if (IrriObject->GetOption(Line) == IRRI_STATUS_START)
			{
				SetupIrrigateShedule(Line, SHED_TYPE_MANUAL);             // Queue a manual start
			}
			CompilePage_Irrigation(request);   // Reload overview to show updated state
		}
		else if ( request->argName(0).substring(0,1) == "L" )
		{
			// Zone label button pressed — open the zone setup page
			Line = request->argName(0).substring(1,2).toInt();
			CurrentIrriSetup = Line;
			CompilePage_IrrigSetup(request, Line);
		}
	}
	else
	{
		CompilePage_Irrigation(request);   // Plain GET — serve overview
	}
}

// -----------------------------------------------------------------------------
// CompilePage_Irrigation
// Builds and serves the irrigation zone overview page.
// Contains the navigation menu, zone action buttons, and WebSocket script.
// Note: this page has no WT_* table — the zone list is an HTML <form><table>
// built by append_Form_Irrigation().
// -----------------------------------------------------------------------------
void CompilePage_Irrigation(AsyncWebServerRequest *request)
{
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);    
	IrrigatePageSel(PS_IRRIGATION);   
	append_Form_Irrigation();         
	appendHtmlStr(webSocketScript);   
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// IrrigatePageSel
// Builds the irrigation navigation <ul> block.
//   - Active page (Index) is a non-clickable <lh> label.
//   - SUSPEND/RESUME IRRIGATION button toggles based on the SUSPEND_IRRI flag.
// -----------------------------------------------------------------------------
void IrrigatePageSel(int Index){

	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == PS_HOME               ? "<a class='act'>HOME</a>"           : "<a href='/'>HOME</a>");
	appendHtmlStr(Index == PS_IRRIGATION         ? "<a class='act'>IRRIGATION</a>"     : "<a href='/irrigation'>IRRIGATION</a>");
	appendHtmlStr(Index == PS_IRRIGATION_VALVES  ? "<a class='act'>Valve Delays</a>"   : "<a href='/valvenames'>Valve Delays</a>");
	appendHtmlStr(Index == PS_IRRIGATION_CONFIG  ? "<a class='act'>Valve Configs</a>"  : "<a href='/valveconfigs'>Valve Configs</a>");
	appendHtmlStr(Index == PS_IRRIGATION_SUSPEND ? "<a class='act'>Manage Suspend</a>" : "<a href='/managesuspend'>Manage Suspend</a>");
	if (SUSPEND_IRRI) appendHtmlStr("<a class='warn' href='/resumeirrigateion'>RESUME IRRIGATION</a>");
	else              appendHtmlStr("<a class='warn' href='/suspendirrigateion'>SUSPEND IRRIGATION</a>");
	appendHtmlStr("</div>");
}

// -----------------------------------------------------------------------------
// append_Form_Irrigation
// Builds the zone list HTML form.  Each active zone row contains:
//   - An "L<n>" label button (zone name + mode + period) — navigates to the
//     zone setup page when submitted.
//   - An "A<n>" action button (START / SHED / STOP) — visible only for active
//     zones when irrigation is not globally suspended.  The action button label
//     reflects the current scheduler state from GetOption().
// Inactive zones show only the label button (value = IRRI_STATUS_INACTIVE).
// -----------------------------------------------------------------------------
void append_Form_Irrigation()
{
	char buf[64];
	appendHtmlStr("<form><table>");
	for ( int x = 0 ; x < IRRIGATE_SELECTIONS; x++ )
	{
		snprintf(buf, sizeof(buf), "L%d", x);
		appendHtmlStr("<tr><th><p>");
		appendHtmlStr("<input type='submit' name='"); appendHtmlStr(buf);
		appendHtmlStr("' value='"); appendHtmlStr(IrriObject->GetDescription(x).c_str());
		appendHtmlStr("' style='font-size:14px;' />");
		appendHtmlStr("</p></th>");

		if(IrriObject->Selection[x].Status == IRRI_STATUS_ACTIVE && SUSPEND_IRRI == false)
		{
			char id[8];
			snprintf(buf, sizeof(buf), "A%d", x);
			snprintf(id,  sizeof(id),  "II%d", x);
			appendHtmlStr("<th><p>");
			appendHtmlStr("<input type='submit' name='"); appendHtmlStr(buf);
			appendHtmlStr("' value='");                  appendHtmlStr(IrriObject->GetOption(x).c_str());
			appendHtmlStr("' id='");                     appendHtmlStr(id);
			appendHtmlStr("' style='font-size:14px;' />");
			appendHtmlStr("</p></th>");
		}
		appendHtmlStr("</tr>");
	}
	appendHtmlStr("</table></form>");
}

// =============================================================================
// Irrigation Zone Setup Page  (GET /irrigation?L=n)
// =============================================================================

// -----------------------------------------------------------------------------
// handleIrrigSetupNext  (GET /zonesnext)
// Advances CurrentIrriSetup to the next zone, wrapping around after zone 9.
// Serves the setup page for the new zone index.
// -----------------------------------------------------------------------------
void handleIrrigSetupNext(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleIrrigSetupNext " + String(ESP.getFreeHeap())); //BDV TAKEOUT
	if(CurrentIrriSetup == IRRIGATE_SEL_9)
		CurrentIrriSetup = IRRIGATE_SEL_1;   // Wrap around to first zone
	else
		CurrentIrriSetup++;
	CompilePage_IrrigSetup(request, CurrentIrriSetup);
}

// -----------------------------------------------------------------------------
// handleIrrigSetupPrev  (GET /zonesprev)
// Moves CurrentIrriSetup to the previous zone, wrapping around before zone 1.
// Serves the setup page for the new zone index.
// -----------------------------------------------------------------------------
void handleIrrigSetupPrev(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	//SaveGenReport("handleIrrigSetupPrev " + String(ESP.getFreeHeap())); //BDV TAKEOUT
	if(CurrentIrriSetup == IRRIGATE_SEL_1)
		CurrentIrriSetup = IRRIGATE_SEL_9;   // Wrap around to last zone
	else
		CurrentIrriSetup--;
	CompilePage_IrrigSetup(request, CurrentIrriSetup);
}

// -----------------------------------------------------------------------------
// MapIrrigSetupPage
// Rebuilds the IrrigSetupList PageVar array to point at zone Line.
// Because ESPAsyncWebServer/AsyncTCP handlers run in their own task (this
// function can be called from an HTTP handler) while loop() reads
// IrrigSetupList concurrently, IrrigSetupObject is set to false before
// modifying the list and restored to true afterwards, preventing a race.
// The existing list is deleted and reallocated so that the 18 PageElements
// point at the correct IrriObject->Selection[Line] fields.
// Each dropdown field is configured with SetStringToSelect() + AddOption().
// -----------------------------------------------------------------------------
void MapIrrigSetupPage(int Line){
	IrrigSetupObject = false;   // Guard: disable WebSocket access during rebuild
	if(IrrigSetupList) {
		for(int i = 0; i < IRRIG_SETUP_FIELDS; i++) { delete IrrigSetupList[i].Var; }
		delete[] IrrigSetupList;
		IrrigSetupList = nullptr;
	}
	IrrigSetupList = new PageVar[IRRIG_SETUP_FIELDS];
	int i = 0;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendALL,          "Suspended",          "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendALLID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SelectDescription,   "Description",        "", &SetStyle, i+ID_IRRISETUP);
	IrriObject->Selection[Line].SelectDescriptionID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].Status,              "Status",             "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_STATUS_ACTIVE); IrrigSetupList[i].Var->AddOption(IRRI_STATUS_INACTIVE);
	IrriObject->Selection[Line].StatusID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].ConfigLine,          "Valve Config",       "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(6);
	IrrigSetupList[i].Var->AddOption("Tank Zone 1");
	IrrigSetupList[i].Var->AddOption("Tank Zone 2");
	IrrigSetupList[i].Var->AddOption("Tank Zone 3");
	IrrigSetupList[i].Var->AddOption("BH Zone 1");
	IrrigSetupList[i].Var->AddOption("BH Zone 2");
	IrrigSetupList[i].Var->AddOption("BH Zone 3");
	IrriObject->Selection[Line].ConfigLineID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].OperMode,            "Operation Mode",     "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_MODE_MANUAL); IrrigSetupList[i].Var->AddOption(IRRI_MODE_SHEDULE);
	IrriObject->Selection[Line].OperModeID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].NextDateToIrrigate,  "Next Date",          "", &SetStyle, i+ID_IRRISETUP);
	IrriObject->Selection[Line].NextDateToIrrigateID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].NextTimeToIrrigate,  "Next Time",          "", &SetStyle, i+ID_IRRISETUP);
	IrriObject->Selection[Line].NextTimeToIrrigateID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].PeriodToIrrigate,    "Irrigation Period",  "", &SetStyle, i+ID_IRRISETUP);
	IrriObject->Selection[Line].PeriodToIrrigateID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].IrrigationFreq,      "Frequency (days)",   "", &SetStyle, i+ID_IRRISETUP);
	IrriObject->Selection[Line].IrrigationFreqID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].WaterSource,         "Water Source",       "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SOURCE_TANK); IrrigSetupList[i].Var->AddOption(IRRI_SOURCE_BOLEHOLE);
	IrriObject->Selection[Line].WaterSourceID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].RespectRainfail,     "Respect Rain",       "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_RESP_RAIN_NO); IrrigSetupList[i].Var->AddOption(IRRI_RESP_RAIN_YES);
	IrriObject->Selection[Line].RespectRainfailID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendMon,          "Susp Monday",        "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendMonID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendTue,          "Susp Tuesday",       "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendTueID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendWed,          "Susp Wednesday",     "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendWedID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendThu,          "Susp Thursday",      "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendThuID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendFri,          "Susp Friday",        "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendFriID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendSat,          "Susp Saturday",      "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendSatID = i + ID_IRRISETUP; i++;

	IrrigSetupList[i].Var = new PageElement(&IrriObject->Selection[Line].SuspendSun,          "Susp Sunday",        "", &SetStyle, i+ID_IRRISETUP);
	IrrigSetupList[i].Var->SetStringToSelect(2); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_NO); IrrigSetupList[i].Var->AddOption(IRRI_SUSP_YES);
	IrriObject->Selection[Line].SuspendSunID = i + ID_IRRISETUP; i++;

	IrrigSetupObject = true;   // Rebuild complete — re-enable WebSocket access
}

// -----------------------------------------------------------------------------
// WT_SETUP_IrrigSetup
// Populates PageBuf with the zone setup parameter table for zone Line.
// Most columns use the type returned by GetTypeStr() (text, number, select).
// A type-override array is applied for the date and time fields, which require
// HTML input type="date" or type="time" instead of the generic "text".
// Output is written to the static PageBuf to avoid heap fragmentation.
// -----------------------------------------------------------------------------
void WT_SETUP_IrrigSetup(int Line){
	resetPageBuf();
	String Val = "Value Line "; Val += String(Line + 1);
	WT_StartPage();
	WT_StartTable("Description", Val.c_str(), "");

	// Type overrides for fields that need HTML-specific input types
	static const char* typeOverride[IRRIG_SETUP_FIELDS] = {
		"",      // 0:  SuspendALL          - select
		"",      // 1:  SelectDescription   - text
		"",      // 2:  Status              - select
		"",      // 3:  ConfigLine          - select
		"",      // 4:  OperMode            - select
		"date",  // 5:  NextDateToIrrigate
		"time",  // 6:  NextTimeToIrrigate
		"time",  // 7:  PeriodToIrrigate
		"",      // 8:  IrrigationFreq      - number
		"",      // 9:  WaterSource         - select
		"",      // 10: RespectRainfail     - select
		"",      // 11: SuspendMon          - select
		"",      // 12: SuspendTue          - select
		"",      // 13: SuspendWed          - select
		"",      // 14: SuspendThu          - select
		"",      // 15: SuspendFri          - select
		"",      // 16: SuspendSat          - select
		"",      // 17: SuspendSun          - select
	};

	for (int i = 0; i < IRRIG_SETUP_FIELDS; i++)
	{
		WT_EditRow(&IrrigSetupList[i], typeOverride[i]);
	}
	WT_EndPage();
}

// -----------------------------------------------------------------------------
// SendIrrigSetupPageValues
// Called periodically by MaintainWebPage() to push changed zone-parameter
// values to connected WebSocket clients.  Only entries whose current value
// differs from the last-sent baseline are transmitted; UpdateLast() records
// the new baseline to suppress duplicate transmissions.
// -----------------------------------------------------------------------------
void SendIrrigSetupPageValues(){
	for (int i = 0; i < IRRIG_SETUP_FIELDS; i++)
	{
		if(IrrigSetupList[i].Var->HasChanged())
		{
			webParamUpdate(IrrigSetupList[i].Var->GetName(),
			               IrrigSetupList[i].Var->GetID(""),
			               IrrigSetupList[i].Var->GetVarStyle());
			IrrigSetupList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// SendIrrigationOverviewValues
// Called periodically by MaintainWebPageValues() to push the action-button
// label for each active zone to connected WebSocket clients whenever
// IrrigationOverviewSync is true (set when a new client connects).
// Button IDs are "II0"–"II8"; values come from GetOption() (START/SHED/STOP).
// Clears IrrigationOverviewSync after sending.
// -----------------------------------------------------------------------------
void SendIrrigationOverviewValues(){
	static String lastOption[IRRIGATE_SELECTIONS];

	// On WS connect: force full resync by clearing the last-sent cache
	if (IrrigationOverviewSync) {
		IrrigationOverviewSync = false;
		for (int x = 0; x < IRRIGATE_SELECTIONS; x++) lastOption[x] = "";
	}

	for (int x = 0; x < IRRIGATE_SELECTIONS; x++)
	{
		if (IrriObject->Selection[x].Status == IRRI_STATUS_ACTIVE && !SUSPEND_IRRI)
		{
			String opt = IrriObject->GetOption(x);
			if (opt != lastOption[x]) {
				String id = "II"; id += String(x);
				webParamUpdate(opt, id, "");
				lastOption[x] = opt;
			}
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateIrrigSetupPageValues
// Called by UnpackJasonPayload() when the browser sends an UPDATE payload from
// the zone setup page.  Matches each JSON key to an IrrigSetupList entry by
// WebSocket ID and applies the new value via SetName().
// After all values are applied, persists all zone data to /Irrigation.json.
// -----------------------------------------------------------------------------
void UpdateIrrigSetupPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		for (int i = 0; i < IRRIG_SETUP_FIELDS; i++)
		{
			if(IrrigSetupList[i].Var->GetID("") == kv.key().c_str())
			{
				IrrigSetupList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	WebMessUpdate(clientId, "Irrigation Setup Updated!", "color: red", true);
	IrriObject->WriteData();
}

// -----------------------------------------------------------------------------
// CompilePage_IrrigSetup
// Builds and serves the zone parameter setup page for zone Line.
// Calls MapIrrigSetupPage() to bind PageElements to the correct zone fields,
// then WT_SETUP_IrrigSetup() to render the parameter table into PageBuf.
// The PREV/NEXT navigation allows stepping between zones without returning to
// the overview page.
// -----------------------------------------------------------------------------
void CompilePage_IrrigSetup(AsyncWebServerRequest *request, int Line){
	IrrigSetupObject = true;                  
	MapIrrigSetupPage(Line);                  
	WT_SETUP_IrrigSetup(Line);
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	Irrigate_Next_Prev();                     
	appendHtmlStr(webSocketScript);           
	appendHtmlBuf(PageBuf, getPageBufLen());  
	appendHtmlStr(webUpateButton);            
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// Irrigate_Next_Prev
// Returns the navigation <ul> for the zone setup page:
// HOME, IRRIGATION overview, PREV zone, NEXT zone.
// -----------------------------------------------------------------------------
void Irrigate_Next_Prev(){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr("<a href='/'>HOME</a>");
	appendHtmlStr("<a href='/irrigation'>IRRIGATION</a>");
	appendHtmlStr("<a href='/zonesprev'>PREV</a>");
	appendHtmlStr("<a href='/zonesnext'>NEXT</a>");
	appendHtmlStr("</div>");
}

// =============================================================================
// Irrigation Suspend / Resume Handlers
// =============================================================================

// -----------------------------------------------------------------------------
// handleResumeIrrig  (GET /resumeirrigateion)
// Clears the global SUSPEND_IRRI flag, allowing the scheduler to resume
// activating zones normally.  Re-serves the irrigation overview page.
// -----------------------------------------------------------------------------
void handleResumeIrrig(AsyncWebServerRequest *request){
	//SaveGenReport("handleResumeIrrig " + String(ESP.getFreeHeap())); //BDV TAKEOUT
	SetMess               = FA_MESS_GRN;
	SpesialStr            = "Irrigation has been Resumed";
	SUSPEND_IRRI          = false;
	CalendarSuspendActive = false;   // Allow manual override to hold for the rest of today
	IrrigationPage(request);
}

// -----------------------------------------------------------------------------
// handleSuspendIrrig  (GET /suspendirrigateion)
// Sets the global SUSPEND_IRRI flag.  IrrigationShedule() will call
// SuspendAllIrrigation() on its next cycle to close any active valves and
// cancel all queued zones.  Re-serves the irrigation overview page.
// -----------------------------------------------------------------------------
void handleSuspendIrrig(AsyncWebServerRequest *request){
	// SaveGenReport("handleSuspendIrrig " + String(ESP.getFreeHeap())); //BDV TAKEOUT
	SetMess      = FA_MESS_RED;
	SpesialStr   = "Irrigation has been Suspended";
	SUSPEND_IRRI = true;
	IrrigationPage(request);
}

// =============================================================================
// Object Factory Functions
// =============================================================================

// -----------------------------------------------------------------------------
// CreateIrrigation
// Allocates the global IrriObject and loads the 9-zone configuration from
// /Irrigation.json.  Called once from InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreateIrrigation() {
	IrriObject = new Irrigation;
	IrriObject->ReadData();
}

// -----------------------------------------------------------------------------
// SetupIrrigateShedule
// Queues zone Line for irrigation if the zone is Active and no schedule is
// already pending for it.  Type selects the triggering mode:
//   SHED_TYPE_MANUAL — user-initiated; NextDateToIrrigate is not advanced.
//   SHED_TYPE_AUTO   — scheduler-initiated; NextDateToIrrigate is advanced
//                      when irrigation completes (not when queued).
// -----------------------------------------------------------------------------
void SetupIrrigateShedule(int Line, String Type) {
	if ( IrriObject->Selection[Line].Status == IRRI_STATUS_ACTIVE &&
	     SheduleObject->IrrigateShedule[Line].ShedStat != SHED_STATUS_SCHEDULED )
	{
		SheduleObject->IrrigateShedule[Line].ShedStat = SHED_STATUS_SCHEDULED;
		// Note: UpdateToNextDate is now called in Phase 3 when irrigation completes
		// to ensure the date is only advanced after a successful run
	}
}

// -----------------------------------------------------------------------------
// CreateSheduleObj
// Allocates the global SheduleObject and initialises all SheduleLines entries
// to the DEACTIVE state.  Called once from InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreateSheduleObj() {
	SheduleObject = new SheduleIrri;
	SheduleObject->Init();
}

// =============================================================================
// Irrigation Class — Constructor, Init, Maintain, Accessors, Persistence
// =============================================================================

Irrigation::Irrigation() {}   // No dynamic members to initialise beyond field defaults
Irrigation::~Irrigation() {}  // No heap members to release

// -----------------------------------------------------------------------------
// Irrigation::Init
// Writes the current field values to /Irrigation.json, then immediately reads
// them back.  Used to ensure the file is present and coherent after first boot.
// -----------------------------------------------------------------------------
void Irrigation::Init() {
	IrriObject->WriteData();
	IrriObject->ReadData();
}

void Irrigation::Maintain() {}   // Reserved for future periodic housekeeping

// -----------------------------------------------------------------------------
// Irrigation::GetDescription
// Returns the button label for zone Line on the irrigation overview page:
//   - Inactive zone    → IRRI_STATUS_INACTIVE ("Not Active")
//   - Suspended zone   → "SUSP. <name>"
//   - Active zone      → "M <period> <name>" (Manual) or "S <period> <name>" (Scheduled)
// -----------------------------------------------------------------------------
String Irrigation::GetDescription(int Line) {
	String Desc = "";
	if(this->Selection[Line].Status != IRRI_STATUS_ACTIVE)
	{
		return IRRI_STATUS_INACTIVE;
	}
	else if (IrriObject->Selection[Line].SuspendALL == IRRI_SUSP_YES)
	{
		return String(Desc + "SUSP. " + this->Selection[Line].SelectDescription);
	}
	else
	{
		Desc = (this->Selection[Line].OperMode == IRRI_MODE_MANUAL) ? "M " : "S ";
		return String(Desc + this->Selection[Line].PeriodToIrrigate + " " + this->Selection[Line].SelectDescription);
	}
}

// -----------------------------------------------------------------------------
// Irrigation::GetOption
// Returns the action-button label for zone Line:
//   IRRI_STATUS_START — zone is idle or ManualStop was just cleared.
//   IRRI_STATUS_SHED  — zone is queued (SCHEDULED) and awaiting activation.
//   IRRI_STATUS_STOP  — zone is the currently pumping line (BusyPumpingConfig).
// Returns IRRI_STATUS_START if SheduleObject is not yet initialised.
// -----------------------------------------------------------------------------
String Irrigation::GetOption(int Line) {
	if(!SheduleObject) return IRRI_STATUS_START;

	if(SheduleObject->IrrigateShedule[Line].ManualStop)
	{
		return IRRI_STATUS_START;   // ManualStop pending — show START so it can be re-queued
	}
	else if(SheduleObject->IrrigateShedule[Line].ShedStat == SHED_STATUS_SCHEDULED)
	{
		return IRRI_STATUS_SHED;    // Queued and waiting to start
	}
	else if(IrriObject->BusyPumpingConfig == Line)
	{
		return IRRI_STATUS_STOP;    // Currently irrigating this line
	}
	else
	{
		return IRRI_STATUS_START;   // Idle
	}
}

// -----------------------------------------------------------------------------
// Irrigation::WriteData
// Serialises all 9 zones to /Irrigation.json as a JSON array.
// The 8 192-byte DynamicJsonDocument is large enough to hold the full 9-zone
// dataset with the 18 string/int fields per zone.
// -----------------------------------------------------------------------------
void Irrigation::WriteData()
{
	String StrFileName = StrIrrigation;
	File ChannelDataFile = SPIFFS.open(StrFileName, "w");

	if (!ChannelDataFile) {
		DEBUG_PRINTLN("Failed to open file for writing");
		return;
	}

	DynamicJsonDocument doc(8192);
	JsonArray irrigationArray = doc.createNestedArray("irrigationSelections");

	for (int i = 0; i < IRRIGATE_SELECTIONS; i++) {
		JsonObject selectionObj = irrigationArray.createNestedObject();
		selectionObj["Status"]              = Selection[i].Status;
		selectionObj["SelectDescription"]   = Selection[i].SelectDescription;
		selectionObj["ConfigLine"]          = Selection[i].ConfigLine;
		selectionObj["OperMode"]            = Selection[i].OperMode;
		selectionObj["NextDateToIrrigate"]  = Selection[i].NextDateToIrrigate;
		selectionObj["NextTimeToIrrigate"]  = Selection[i].NextTimeToIrrigate;
		selectionObj["PeriodToIrrigate"]    = Selection[i].PeriodToIrrigate;
		selectionObj["IrrigationFreq"]      = Selection[i].IrrigationFreq;
		selectionObj["WaterSource"]         = Selection[i].WaterSource;
		selectionObj["RespectRainfail"]     = Selection[i].RespectRainfail;
		selectionObj["SuspendMon"]          = Selection[i].SuspendMon;
		selectionObj["SuspendTue"]          = Selection[i].SuspendTue;
		selectionObj["SuspendWed"]          = Selection[i].SuspendWed;
		selectionObj["SuspendThu"]          = Selection[i].SuspendThu;
		selectionObj["SuspendFri"]          = Selection[i].SuspendFri;
		selectionObj["SuspendSat"]          = Selection[i].SuspendSat;
		selectionObj["SuspendSun"]          = Selection[i].SuspendSun;
		selectionObj["SuspendALL"]          = Selection[i].SuspendALL;
	}

	String output;
	serializeJson(doc, output);
	ChannelDataFile.print(output);
	ChannelDataFile.close();
}

// -----------------------------------------------------------------------------
// Irrigation::ReadData
// Deserialises /Irrigation.json into the Selection[] array.
// A helper lambda S() returns the stored string or a safe default when the
// key is absent or "null" (possible after a firmware upgrade that adds new
// fields).  Enumeration fields are validated against the known string
// constants so that unexpected legacy values do not corrupt the state machine.
// Boolean-to-string migration: older SPIFFS files stored booleans (false/true);
// the validation logic coerces these to the correct IRRI_STATUS_* strings.
// -----------------------------------------------------------------------------
void Irrigation::ReadData()
{
	String StrFileName = StrIrrigation;
	File ChannelDataFile = SPIFFS.open(StrFileName, "r");

	if (!ChannelDataFile) {
		DEBUG_PRINTLN("Failed to open file for reading");
		return;
	}

	String fileContents = "";
	while (ChannelDataFile.available()) {
		char c = ChannelDataFile.read();
		fileContents += c;
	}
	ChannelDataFile.close();

	DynamicJsonDocument doc(8192);
	DeserializationError error = deserializeJson(doc, fileContents);

	if (error) {
		DEBUG_PRINTLN("Failed to parse JSON file");
		return;
	}

	// Helper lambda: return default if JSON value is missing ("null") or empty
	auto S = [](JsonObject& obj, const char* key, String def) -> String {
		String v = obj[key].as<String>();
		return (v == "null" || v == "") ? def : v;
	};

	JsonArray irrigationArray = doc["irrigationSelections"];
	for (int i = 0; i < (int)irrigationArray.size(); i++) {
		JsonObject selectionObj = irrigationArray[i];
		// Validate select fields — old SPIFFS stored bools (false/true), coerce to known string values
		String vStatus = S(selectionObj, "Status", IRRI_STATUS_INACTIVE);
		Selection[i].Status = (vStatus == IRRI_STATUS_ACTIVE) ? IRRI_STATUS_ACTIVE : IRRI_STATUS_INACTIVE;

		Selection[i].SelectDescription  = S(selectionObj, "SelectDescription",  "Not Used");
		Selection[i].ConfigLine         = S(selectionObj, "ConfigLine",         "Zone 1");

		String vMode = S(selectionObj, "OperMode", IRRI_MODE_MANUAL);
		Selection[i].OperMode = (vMode == IRRI_MODE_SHEDULE) ? IRRI_MODE_SHEDULE : IRRI_MODE_MANUAL;

		Selection[i].NextDateToIrrigate = S(selectionObj, "NextDateToIrrigate", "2024-01-01");
		Selection[i].NextTimeToIrrigate = S(selectionObj, "NextTimeToIrrigate", "06:00");
		Selection[i].PeriodToIrrigate   = S(selectionObj, "PeriodToIrrigate",   "00:10");
		if(selectionObj["IrrigationFreq"].is<int>()) Selection[i].IrrigationFreq = selectionObj["IrrigationFreq"];

		String vSource = S(selectionObj, "WaterSource", IRRI_SOURCE_TANK);
		Selection[i].WaterSource = (vSource == IRRI_SOURCE_BOLEHOLE) ? IRRI_SOURCE_BOLEHOLE : IRRI_SOURCE_TANK;

		String vRain = S(selectionObj, "RespectRainfail", IRRI_RESP_RAIN_NO);
		Selection[i].RespectRainfail = (vRain == IRRI_RESP_RAIN_YES) ? IRRI_RESP_RAIN_YES : IRRI_RESP_RAIN_NO;

		// Suspend day fields — any value other than IRRI_SUSP_YES defaults to IRRI_SUSP_NO
		auto YN = [](String v) -> String { return (v == IRRI_SUSP_YES) ? IRRI_SUSP_YES : IRRI_SUSP_NO; };
		Selection[i].SuspendMon = YN(S(selectionObj, "SuspendMon", IRRI_SUSP_NO));
		Selection[i].SuspendTue = YN(S(selectionObj, "SuspendTue", IRRI_SUSP_NO));
		Selection[i].SuspendWed = YN(S(selectionObj, "SuspendWed", IRRI_SUSP_NO));
		Selection[i].SuspendThu = YN(S(selectionObj, "SuspendThu", IRRI_SUSP_NO));
		Selection[i].SuspendFri = YN(S(selectionObj, "SuspendFri", IRRI_SUSP_NO));
		Selection[i].SuspendSat = YN(S(selectionObj, "SuspendSat", IRRI_SUSP_NO));
		Selection[i].SuspendSun = YN(S(selectionObj, "SuspendSun", IRRI_SUSP_NO));
		Selection[i].SuspendALL = YN(S(selectionObj, "SuspendALL", IRRI_SUSP_NO));
	}
}

// =============================================================================
// SheduleIrri Class — Constructor, Init
// =============================================================================

SheduleIrri::SheduleIrri() {}   // No dynamic members to initialise
SheduleIrri::~SheduleIrri() {}  // No heap members to release

// -----------------------------------------------------------------------------
// SheduleIrri::Init
// Resets all IrrigateShedule[] entries to the DEACTIVE state.
// Called once from CreateSheduleObj() during boot so the scheduler starts
// clean regardless of any previous run state.
// -----------------------------------------------------------------------------
void SheduleIrri::Init() {
	for ( int i = 0 ; i < Lines; i++ ) {
		IrrigateShedule[i].ShedStat     = SHED_STATUS_DEACTIVE;
		IrrigateShedule[i].StopDateTime = "";
		IrrigateShedule[i].ManualStop   = false;
	}
}

// =============================================================================
// IrrigateSelection — Helper Method
// =============================================================================

// -----------------------------------------------------------------------------
// IrrigateSelection::GetPeriodToIrrigateSeconds
// Returns PeriodToIrrigate with ":00" appended (HH:MM → HH:MM:00), producing
// an HH:MM:SS string for callers that require seconds precision.
// -----------------------------------------------------------------------------
String IrrigateSelection::GetPeriodToIrrigateSeconds()
{
	return PeriodToIrrigate + ":00";
}

// =============================================================================
// Scheduling Helper Functions
// =============================================================================

// -----------------------------------------------------------------------------
// GetScheduledDateAndTime
// Returns the next scheduled start timestamp for zone Entry as a combined
// "YYYY-MM-DD HH:MM" string, suitable for direct comparison with
// GetCurrentDateAndTime().
// -----------------------------------------------------------------------------
String GetScheduledDateAndTime(int Entry) {
	return IrriObject->Selection[Entry].NextDateToIrrigate + " " +
	       IrriObject->Selection[Entry].NextTimeToIrrigate;
}

// -----------------------------------------------------------------------------
// UpdateToNextDate
// Advances NextDateToIrrigate for zone Entry forward by IrrigationFreq days
// using IncrementDateWithOneDay() in a loop.  Persists to /Irrigation.json
// immediately so the updated date survives a reboot between runs.
// -----------------------------------------------------------------------------
void UpdateToNextDate(int Entry) {
	String NewDate = IrriObject->Selection[Entry].NextDateToIrrigate;
	int freq = max(1, IrriObject->Selection[Entry].IrrigationFreq);   // Minimum 1 day — prevents immediate re-queue
	for ( int i = 0; i < freq; i++ ) {
		NewDate = IncrementDateWithOneDay(NewDate);
	}
	IrriObject->Selection[Entry].NextDateToIrrigate = NewDate;
	IrriObject->WriteData();
}

// -----------------------------------------------------------------------------
// ConfigLineToIndex  (file-local)
// Maps a ConfigLine string to its zero-based index in the ValveConfigurations[]
// array:
//   "Tank Zone 1" → 0   "Tank Zone 2" → 1   "Tank Zone 3" → 2
//   "BH Zone 1"   → 3   "BH Zone 2"   → 4   "BH Zone 3"   → 5
// Zone number is extracted from the last character of the string.
// -----------------------------------------------------------------------------
static int ConfigLineToIndex(const String& configLine) {
	int zone = configLine.charAt(configLine.length() - 1) - '0';   // Last char: '1', '2', or '3'
	return configLine.startsWith("BH") ? zone + 2 : zone - 1;
}

// -----------------------------------------------------------------------------
// ConfigLineIsBorehole  (file-local)
// Returns true if ConfigLine refers to a borehole zone ("BH Zone n").
// Used to decide whether the borehole pump must be started before opening
// the irrigation valves.
// -----------------------------------------------------------------------------
static bool ConfigLineIsBorehole(const String& configLine) {
	return configLine.startsWith("BH");
}

// -----------------------------------------------------------------------------
// SuspendAllIrrigation
// Emergency-stops all irrigation:
//   1. Clears ManualStop flags and deactivates all SCHEDULED zones.
//   2. Closes valves for any ACTIVE zone via SwitchValveConfig().
//   3. Stops the borehole pump if it was started by the irrigation system.
//   4. Resets IrriActivityLine and re-allows tank filling.
// Called from IrrigationShedule() when SUSPEND_IRRI is set, and from the
// borehole-failure guards (Note 2, Note 4) inside IrrigationShedule().
// -----------------------------------------------------------------------------
void SuspendAllIrrigation() {
	for ( int x = 0; x < IRRIGATE_SELECTIONS; x++ ) {
		SheduleObject->IrrigateShedule[x].ManualStop = false;
		if (SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_SCHEDULED) {
			SheduleObject->IrrigateShedule[x].ShedStat = SHED_STATUS_DEACTIVE;
		}
		if (SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_ACTIVE) {
			SheduleObject->IrrigateShedule[x].ShedStat = SHED_STATUS_DEACTIVE;
			int configIdx = ConfigLineToIndex(IrriObject->Selection[x].ConfigLine);
			ElecValves->SwitchValveConfig(configIdx, VALVE_CLOSE);
		}
	}
	if (BoreholeStartedByIrri) {
		StopBorehole();
		BoreholeStartedByIrri = false;
	}
	IrriActivityLine = -1;
	TankFillAllowed  = true;
}

// =============================================================================
// IrrigationInit
// =============================================================================

// -----------------------------------------------------------------------------
// IrrigationInit
// Called once during boot (after NTP sync and IrriObject is loaded) to repair
// any stale schedule dates.  For every Active, Scheduled zone whose next run
// datetime is already in the past, the date is advanced to today + IrrigationFreq
// days via GetCurrentDate() so the zone does not fire immediately at boot
// but instead picks up its normal cadence from the current date.
// Saves to /Irrigation.json if any date was corrected.
// -----------------------------------------------------------------------------
void IrrigationInit() {
	String CurrentDateTime = GetCurrentDateAndTime();
	for ( int x = 0; x < IRRIGATE_SELECTIONS; x++ ) {
		if (IrriObject->Selection[x].Status == IRRI_STATUS_ACTIVE &&
		    IrriObject->Selection[x].OperMode == IRRI_MODE_SHEDULE)
		{
			if (CurrentDateTime > GetScheduledDateAndTime(x)) {
				// Next run date is in the past — advance to current date + frequency
				IrriObject->Selection[x].NextDateToIrrigate = GetCurrentDate(max(1, IrriObject->Selection[x].IrrigationFreq));
				IrriObject->WriteData();
			}
		}
	}
}

// =============================================================================
// IrrigationShedule — Main Scheduler State Machine
// =============================================================================

// -----------------------------------------------------------------------------
// IrrigationShedule
// Called every 10 ms from loop().  Drives the full irrigation lifecycle:
//
//  Phase 1 — Global suspend check:
//    If SUSPEND_IRRI is set, call SuspendAllIrrigation() to close all active
//    valves and cancel queued zones, then return.
//
//  Phase 2 — Queue eligible scheduled zones (DEACTIVE → SCHEDULED):
//    For each Active, Scheduled zone that is currently DEACTIVE and whose
//    scheduled time has arrived, check rain/day-suspend guards.  If all guards
//    pass, call SetupIrrigateShedule(SHED_TYPE_AUTO) to queue the zone.
//    If a guard blocks the run, advance the date without irrigating.
//
//  Phase 3 — Stop completed or manually cancelled zones (ACTIVE → DEACTIVE):
//    For each ACTIVE zone whose stop time has passed or ManualStop is set,
//    close the valve, stop the borehole if it was started by irrigation,
//    log the session duration, and reset IrriActivityLine.
//    ReadyToShedule remains true only if no zone is still ACTIVE after this pass.
//
//  Phase 4 — Borehole health guards:
//    Note 2: after borehole start, TIM_BOREHOLE_TOUT confirms the pump responded
//            (InPort[IIDX_ME_PUMP_BH].Input goes HIGH).  If not, suspend all.
//    Note 4: if BoreholOperation is disabled mid-run, suspend all.
//    Note 5: if IIDX_ME_PUMP_BH drops mid-run after initial confirmation, cancel
//            the zone (advance to next date), stop borehole, allow tank fill.
//
//  Phase 5 — Tank time-window guard:
//    If the tank is scheduled for filling during the current time window and
//    the tank is not yet full (FloatHigh), hold ReadyToShedule = false to
//    prevent starting any irrigation while the tank may need filling.
//
//  Phase 6 — Activate queued zones (SCHEDULED → ACTIVE):
//    If ReadyToShedule is true, start the first SCHEDULED zone found:
//    open its valve config, start the borehole if needed, compute StopDateTime,
//    record IrriActivityStartMs, and break so only one zone runs at a time.
// -----------------------------------------------------------------------------
void IrrigationShedule() {
	if (!NORMAL_OPERATION) return;

	String CurrentDateTime = GetCurrentDateAndTime();

	// ── Phase 0: Daily suspension calendar (/managesuspend) ──────────────────
	// Fires once per boot (bootProcessed guard) AND on every runtime day change.
	//
	// isMidnight = true  (LastProcessedDay != WeekDay):
	//   Midnight rollover or reboot that crossed a day boundary.
	//   Apply step a (Suspend) for the NEW day, then step b (Once clearing) for
	//   the day that just ENDED (LastProcessedDay).  Clearing the previous day's
	//   flags keeps today's Suspend=true in SPIFFS so an intra-day reboot can
	//   still re-apply the suspension via step a.
	//
	// isMidnight = false (LastProcessedDay == WeekDay):
	//   Intra-day reboot on the same calendar day.
	//   Apply step a (Suspend) only — today's flags are still in SPIFFS and will
	//   correctly re-raise SUSPEND_IRRI if needed.  Never touch Once on reboot.
	{
		static bool bootProcessed = false;
		int calIdx = WeekDay - 1;   // WeekDay 1-based (1=Sun…7=Sat) → array 0-based

		if (!bootProcessed || DaysSuspend->LastProcessedDay != WeekDay) {
			bool isMidnight = (DaysSuspend->LastProcessedDay != WeekDay);
			bootProcessed = true;

			// ── Step a (always — reboot AND midnight): apply today's Suspend ──
			if (DaysSuspend->SuspendingDay[calIdx].Suspend) {
				if (!SUSPEND_IRRI) {
					SaveGenReport("Suspend: " + DaysSuspend->SuspendingDay[calIdx].Day +
					              (isMidnight ? " - Calendar" : " - Calendar (reboot)"));
				}
				SUSPEND_IRRI          = true;
				CalendarSuspendActive = true;
			} else if (isMidnight && CalendarSuspendActive) {
				// Runtime midnight: previous day was calendar-suspended, new day is not → release
				SUSPEND_IRRI          = false;
				CalendarSuspendActive = false;
				SaveGenReport("Suspend released: " + DaysSuspend->SuspendingDay[calIdx].Day);
			}

			// ── Step b (midnight only): clear Once + paired Suspend for the day
			//    that just ENDED (LastProcessedDay), not the day starting now.
			//    This keeps today's Suspend=true in SPIFFS so an intra-day reboot
			//    can re-apply the suspension via step a above.
			if (isMidnight && DaysSuspend->LastProcessedDay != 0) {
				int prevCalIdx = DaysSuspend->LastProcessedDay - 1;   // 1-based → 0-based
				if (DaysSuspend->SuspendingDay[prevCalIdx].Once) {
					DaysSuspend->SuspendingDay[prevCalIdx].Once    = false;
					DaysSuspend->SuspendingDay[prevCalIdx].Suspend = false;
					SaveGenReport("Suspend Once cleared: " + DaysSuspend->SuspendingDay[prevCalIdx].Day);
				}
			}

			// Persist processed day (one WriteData covers LastProcessedDay + any cleared flags)
			DaysSuspend->LastProcessedDay = WeekDay;
			DaysSuspend->WriteData();
		}
	}

	// ── Phase 1: Global suspend — close all active lines ─────────────────────
	if (SUSPEND_IRRI) {
		SuspendAllIrrigation();
	}

	// ── Phase 2: Queue eligible scheduled zones ───────────────────────────────
	for ( int x = 0; x < IRRIGATE_SELECTIONS; x++ ) {
		if (IrriObject->Selection[x].Status == IRRI_STATUS_ACTIVE &&
		    IrriObject->Selection[x].OperMode == IRRI_MODE_SHEDULE &&
		    SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_DEACTIVE)
		{
			if (CurrentDateTime >= GetScheduledDateAndTime(x)) {
				if (SUSPEND_IRRI) {
					// Suspended globally — skip this occurrence
					UpdateToNextDate(x);
				} else {
					if (!RAIN_OK_DO_IRRI && IrriObject->Selection[x].RespectRainfail == IRRI_RESP_RAIN_YES) {
						UpdateToNextDate(x);
					} else {
						String Suspend = IRRI_SUSP_NO;
						switch (WeekDay) {
							case DAY_SUNDAY:  Suspend = IrriObject->Selection[x].SuspendSun; break;
							case DAY_MONDAY:  Suspend = IrriObject->Selection[x].SuspendMon; break;
							case DAY_TUEDAY:  Suspend = IrriObject->Selection[x].SuspendTue; break;
							case DAY_WEDDAY:  Suspend = IrriObject->Selection[x].SuspendWed; break;
							case DAY_THUDAY:  Suspend = IrriObject->Selection[x].SuspendThu; break;
							case DAY_FRIDAY:  Suspend = IrriObject->Selection[x].SuspendFri; break;
							case DAY_SATDAY:  Suspend = IrriObject->Selection[x].SuspendSat; break;
						}
						if (Suspend == IRRI_SUSP_YES || IrriObject->Selection[x].SuspendALL == IRRI_SUSP_YES) {
							UpdateToNextDate(x);
						} else {
							SetupIrrigateShedule(x, SHED_TYPE_AUTO);
						}
					}
				}
			}
		}
	}

	// ── Phase 3: Stop completed or manually cancelled zones ──────────────────
	bool ReadyToShedule = true;
	for ( int x = 0; x < IRRIGATE_SELECTIONS; x++ ) {
		// Cancel a SCHEDULED zone if ManualStop was set before it started
		if (SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_SCHEDULED &&
		    SheduleObject->IrrigateShedule[x].ManualStop)
		{
			SheduleObject->IrrigateShedule[x].ManualStop = false;
			SheduleObject->IrrigateShedule[x].ShedStat   = SHED_STATUS_DEACTIVE;
		}
		// Stop an ACTIVE zone when its duration expires or ManualStop is set
		if (SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_ACTIVE) {
			if (CurrentDateTime >= SheduleObject->IrrigateShedule[x].StopDateTime ||
			    SheduleObject->IrrigateShedule[x].ManualStop)
			{
				SheduleObject->IrrigateShedule[x].ManualStop = false;
				SheduleObject->IrrigateShedule[x].ShedStat   = SHED_STATUS_DEACTIVE;
				int configIdx = ConfigLineToIndex(IrriObject->Selection[x].ConfigLine);
				ElecValves->SwitchValveConfig(configIdx, VALVE_CLOSE);
				if (BoreholeStartedByIrri) {
					StopBorehole();
					BoreholeStartedByIrri = false;
				}
				IrriObject->BusyPumpingConfig = -1;
				IrriActivityLine = -1;
				TankFillAllowed  = true;
				// Advance to next scheduled date now that irrigation completed successfully
				UpdateToNextDate(x);
				unsigned long irriDurSec = (millis() - IrriActivityStartMs) / 1000;
				SaveIrrigationReport("Irrigation: " + IrriObject->Selection[x].SelectDescription +
				                " Dur: " + fmtDur(irriDurSec));
			} else {
				ReadyToShedule = false;   // A zone is still running — do not start another
			}
		}
	}

	// ── Phase 4: Borehole health guards ──────────────────────────────────────
	// Note 2: confirm borehole responded within TIM_BOREHOLE_TOUT after start
	if (BoreholeStartedByIrri && BoreholeCheckPending && TimerExpired(TIM_BOREHOLE_TOUT)) {
		BoreholeCheckPending = false;
		if (!InPort[IIDX_ME_PUMP_BH].Input) {
			// Pump did not respond — cancel this zone and advance to its next date
			if (IrriActivityLine >= 0) {
				int configIdx = ConfigLineToIndex(IrriObject->Selection[IrriActivityLine].ConfigLine);
				ElecValves->SwitchValveConfig(configIdx, VALVE_CLOSE);
				SaveIrrigationReport("Irrigation: " + IrriObject->Selection[IrriActivityLine].SelectDescription +
				                " BH No Response");
				SheduleObject->IrrigateShedule[IrriActivityLine].ShedStat = SHED_STATUS_DEACTIVE;
				UpdateToNextDate(IrriActivityLine);
				IrriObject->BusyPumpingConfig = -1;
				IrriActivityLine = -1;
				TankFillAllowed  = true;
			}
			StopBorehole();
			BoreholeStartedByIrri = false;
		}
	}
	// Note 4: borehole disabled while an irrigation run is in progress
	if (BoreholeStartedByIrri && TankWaterLevel->BoreholOperation == TANK_BORHOLE_DISABLE) {
		SuspendAllIrrigation();
	}
	// Note 5 (irrigation): IIDX_ME_PUMP_BH fell away mid-run after initial confirmation.
	// Cancel the schedule completely — advance to the next occurrence so this zone
	// does not immediately re-queue.  No retry timer: the run is treated as done.
	if (BoreholeStartedByIrri && !BoreholeCheckPending && !InPort[IIDX_ME_PUMP_BH].Input) {
		if (IrriActivityLine >= 0) {
			int configIdx = ConfigLineToIndex(IrriObject->Selection[IrriActivityLine].ConfigLine);
			ElecValves->SwitchValveConfig(configIdx, VALVE_CLOSE);
			unsigned long irriDurSec = (millis() - IrriActivityStartMs) / 1000;
			SaveIrrigationReport("Irrigation: " + IrriObject->Selection[IrriActivityLine].SelectDescription +
			                " BH Lost Dur: " + fmtDur(irriDurSec));
			SheduleObject->IrrigateShedule[IrriActivityLine].ShedStat = SHED_STATUS_DEACTIVE;
			UpdateToNextDate(IrriActivityLine);
			IrriActivityLine = -1;
			TankFillAllowed  = true;
		}
		StopBorehole();
		BoreholeStartedByIrri = false;
	}

	// ── Phase 5: Tank time-window guard ──────────────────────────────────────
	// Scheduled fill mode: during the configured fill window, hold ReadyToShedule
	// false until the tank is full — the borehole is reserved for tank filling.
	// Any Time fill mode: block irrigation only when the tank is confirmed low/empty,
	// not just when it is not completely full (a partial tank is still usable).
	if (TankWaterLevel->TankFillingTime == TANK_FILL_OPER_SHEDULE) {
		if (GetCurrentTime() > TankWaterLevel->TankFillStartTime &&
		    GetCurrentTime() < TankWaterLevel->TankFillStopTime)
		{
			if (TankWaterLevel->TankFillingSense == TANK_FILL_SENSE_FLOAT) {
				if (!FloatHigh) ReadyToShedule = false;
			} else {
				if (TankWaterLevel->getValue() < TankWaterLevel->StopFillLevel) ReadyToShedule = false;
			}
		}
	} else {
		// "Any Time" fill mode — block only if tank is confirmed low/empty
		if (TankWaterLevel->TankFillingSense == TANK_FILL_SENSE_FLOAT) {
			if (FloatLow) ReadyToShedule = false;
		} else {
			if (TankWaterLevel->getValue() < TankWaterLevel->StartFillLevel) ReadyToShedule = false;
		}
	}

	// ── Phase 6: Activate the first queued zone ───────────────────────────────
	if (ReadyToShedule) {
		for ( int x = 0; x < IRRIGATE_SELECTIONS; x++ ) {
			if (SheduleObject->IrrigateShedule[x].ShedStat == SHED_STATUS_SCHEDULED) {
				bool isBH = (IrriObject->Selection[x].WaterSource == IRRI_SOURCE_BOLEHOLE);
				if (isBH && !StartBorehole()) {
					break;   // Borehole unavailable — leave zone SCHEDULED, retry next cycle
				}
				if (isBH) BoreholeStartedByIrri = true;
				IrriObject->BusyPumpingConfig = x;
				// Calculate the absolute stop time from current time + zone period
				SheduleObject->IrrigateShedule[x].StopDateTime =
					AddTimeToCurrentDateTime(CurrentDateTime, IrriObject->Selection[x].PeriodToIrrigate);
				SheduleObject->IrrigateShedule[x].ShedStat = SHED_STATUS_ACTIVE;
				int configIdx = ConfigLineToIndex(IrriObject->Selection[x].ConfigLine);
				ElecValves->SwitchValveConfig(configIdx, VALVE_OPEN);
				IrriActivityStartMs = millis();
				IrriActivityLine    = x;
				TankFillAllowed     = false;
				break;   // Start only one zone at a time
			}
		}
	}
}
