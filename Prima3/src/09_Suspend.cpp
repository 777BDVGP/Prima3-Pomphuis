// =============================================================================
// 09_Suspend.cpp — Irrigation Suspension Calendar Web Page & Data Management
// =============================================================================
// Implements the Manage Suspend web page, PageVar mapping, WebSocket update
// handlers, and the SuspendDays class constructor, initialiser, and JSON
// persistence methods.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// handleManageSuspend  (GET /managesuspend)
// Allocates the SuspendList PageVar array on first visit, then builds and
// serves the 7-day suspension calendar configuration page.
// -----------------------------------------------------------------------------
void handleManageSuspend(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleManageSuspend " + String(ESP.getFreeHeap()));
	if (!SuspendObject) CreateSuspendMaps();
	CompilePage_Suspend(request);
}

// -----------------------------------------------------------------------------
// CompilePage_Suspend
// Builds the complete suspension calendar page from PageBuf and sends it.
// PageBuf is used for the WT table to avoid heap fragmentation.
// -----------------------------------------------------------------------------
void CompilePage_Suspend(AsyncWebServerRequest *request)
{
	DEBUG_PRINTLN("CS 01");
	WT_SETUP_Suspend();                           
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	IrrigatePageSel(PS_IRRIGATION_SUSPEND);

	appendHtmlStr(webSocketScript);               
	appendHtmlBuf(PageBuf, getPageBufLen());      
	appendHtmlStr(webUpateButton);                
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
	DEBUG_PRINTLN("CompilePage_Suspend Done");
}

// -----------------------------------------------------------------------------
// MapSuspendPage
// Allocates SuspendList[] (SUSPEND_DAYS × 2 entries: Suspend + Once per day)
// and binds each entry to the corresponding SuspendDays field.
// Called once by CreateSuspendMaps() on first page visit.
// -----------------------------------------------------------------------------
void MapSuspendPage(){
	SuspendList = new PageVar[SUSPEND_DAYS * 2];
	for (int i = 0; i < SUSPEND_DAYS; i++){
		// First half: Suspend (permanent day block) — labelled with the day name
		SuspendList[i].Var = new PageElement(&DaysSuspend->SuspendingDay[i].Suspend, DaysSuspend->SuspendingDay[i].Day, "", &SetStyle, i + ID_SUSPEND);
		// Second half: Once (single-use skip) — no label (shared row with above)
		SuspendList[i + SUSPEND_DAYS].Var = new PageElement(&DaysSuspend->SuspendingDay[i].Once, "", "", &SetStyle, i + SUSPEND_DAYS + ID_SUSPEND);
	}
}

// -----------------------------------------------------------------------------
// WT_SETUP_Suspend
// Writes the suspension calendar parameter table into PageBuf.
// Three columns: Weekday label | Suspend checkbox | Once checkbox.
// -----------------------------------------------------------------------------
void WT_SETUP_Suspend(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("Weekday", "Suspend", "Once");
	for (int i = 0; i < SUSPEND_DAYS; i++){
		WT_StartLine();
		WT_AutoColumn(WT_LABEL, WT_FIRST, WT_LAST, &SuspendList[i], "L");
		WT_AutoColumn(WT_INPUT, WT_FIRST, WT_MORE, &SuspendList[i]);
		WT_AutoColumn(WT_INPUT, WT_NEXT,  WT_LAST, &SuspendList[i + SUSPEND_DAYS]);
		WT_EndLine();
	}
	WT_EndPageClass("susp");
}

// -----------------------------------------------------------------------------
// SendSuspendPageValues  (called every 10 ms from MaintainWebPageValues)
// Pushes any changed suspend/once values to all connected WebSocket clients.
// -----------------------------------------------------------------------------
void SendSuspendPageValues(){
	for (int i = 0; i < SUSPEND_DAYS * 2; i++){
		if (SuspendList[i].Var->HasChanged()){
			webParamUpdate(SuspendList[i].Var->GetName(), SuspendList[i].Var->GetID(""), SuspendList[i].Var->GetVarStyle());
			SuspendList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// UpdateSuspendPageValues  (WebSocket UPDATE handler)
// Called when the user presses the UPDATE button on the Manage Suspend page.
// Iterates the incoming JSON payload and writes matching values into the
// SuspendList PageVar entries, then persists to SPIFFS.
// -----------------------------------------------------------------------------
void UpdateSuspendPageValues(uint32_t clientId, JsonObject *root){
	for (JsonPair kv : *root){
		for (int i = 0; i < SUSPEND_DAYS * 2; i++){
			if (SuspendList[i].Var->GetID("") == kv.key().c_str()){
				SuspendList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	DaysSuspend->WriteData();
	WebMessUpdate(clientId, "Suspend Updated!", "color: red", true);
}

// =============================================================================
// SuspendDays Object Factory Functions
// =============================================================================

// -----------------------------------------------------------------------------
// CreateSuspendDays
// Allocates the global DaysSuspend object and calls Init() to assign day names,
// weekday numbers, and load stored values from SPIFFS.
// Called once from InitFileSystem() during boot.
// -----------------------------------------------------------------------------
void CreateSuspendDays(){
	DaysSuspend = new SuspendDays;
	DaysSuspend->Init();
}

// -----------------------------------------------------------------------------
// CreateSuspendMaps
// Allocates the SuspendList PageVar array for the web page.
// Called lazily on the first visit to /managesuspend.
// -----------------------------------------------------------------------------
void CreateSuspendMaps(){
	SuspendObject = true;
	MapSuspendPage();
}

SuspendDays::SuspendDays() {}   // CONSTRUCTOR
SuspendDays::~SuspendDays() {}  // DESTRUCTOR

void SuspendDays::Init() {

	SuspendingDay[SUSPEND_1].Day = "Sunday";
	SuspendingDay[SUSPEND_2].Day = "Monday";
	SuspendingDay[SUSPEND_3].Day = "Tuesday";
	SuspendingDay[SUSPEND_4].Day = "Wednesday";
	SuspendingDay[SUSPEND_5].Day = "Thursday";
	SuspendingDay[SUSPEND_6].Day = "Friday";
	SuspendingDay[SUSPEND_7].Day = "Saturday";

	SuspendingDay[SUSPEND_1].Weekday = 1;
	SuspendingDay[SUSPEND_2].Weekday = 2;
	SuspendingDay[SUSPEND_3].Weekday = 3;
	SuspendingDay[SUSPEND_4].Weekday = 4;
	SuspendingDay[SUSPEND_5].Weekday = 5;
	SuspendingDay[SUSPEND_6].Weekday = 6;
	SuspendingDay[SUSPEND_7].Weekday = 7;

	DaysSuspend->ReadData();
}

void SuspendDays::WriteData() {

	File ChannelDataFile = SPIFFS.open(StrSuspendDays, "w");
	DynamicJsonDocument doc(1024);

	String docDesc;
	for (int i=0; i < SUSPEND_DAYS; i++){
		docDesc = F("Suspend_"); docDesc += i;
		doc[docDesc] = SuspendingDay[i].Suspend;
		docDesc = F("Once_"); docDesc += i;
		doc[docDesc] = SuspendingDay[i].Once;
	}
	doc["LastProcessedDay"] = LastProcessedDay;

	if (ChannelDataFile)
	{
		serializeJsonPretty(doc, ChannelDataFile);
		ChannelDataFile.close();
	}
}

void SuspendDays::ReadData() {

	File ChannelDataFile = SPIFFS.open(StrSuspendDays, "r");
	if (ChannelDataFile)
	{
		size_t size = ChannelDataFile.size();
		if (size <= 1024)
		{
			std::unique_ptr<char[]> buf(new char[size]); // Buffer to store file contents
			ChannelDataFile.readBytes(buf.get(), size);
			DynamicJsonDocument doc(1024);
			auto error = deserializeJson(doc, buf.get());
			if (!error)
			{
				String docDesc;
				for (int i=0; i < SUSPEND_DAYS; i++){
					docDesc = F("Suspend_"); docDesc += i;
					if ( doc[docDesc].as<String>() != "null" ) SuspendingDay[i].Suspend = doc[docDesc];
					docDesc = F("Once_"); docDesc += i;
					if ( doc[docDesc].as<String>() != "null" ) SuspendingDay[i].Once = doc[docDesc];
				}
				if (doc["LastProcessedDay"].is<int>()) LastProcessedDay = doc["LastProcessedDay"];
			}
			else
			{
				DEBUG_PRINT("Error deserialize "); DEBUG_PRINTLN(StrSuspendDays);
			}
		}
		else
		{
			DEBUG_PRINT(StrSuspendDays);  DEBUG_PRINTLN(" File size is > 1024 ");
		}
		ChannelDataFile.close();
	}
}
