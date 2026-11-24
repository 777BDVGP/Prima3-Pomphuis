// =============================================================================
// 16_Ntfy.cpp — NTFY.sh Push Notifications & NTFY Configuration Web Page
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// NtfyParams — Constructor / Destructor / Persistence
// =============================================================================

NtfyParams::NtfyParams()  {}
NtfyParams::~NtfyParams() {}

void NtfyParams::WriteData()
{
	File f = SPIFFS.open("/NTFY.json", "w");
	if (!f) return;
	DynamicJsonDocument doc(768);
	doc["NtfyCode"]            = NtfyCode;
	doc["NtfyCode2"]           = NtfyCode2;
	doc["SysNameUsr1"]         = SysNameUsr1;
	doc["BoreholeMaxTime"]     = BoreholeMaxTime;
	doc["BoreholeMaxTimeUsr1"] = BoreholeMaxTimeUsr1;
	doc["TankFillMaxHours"]    = TankFillMaxHrsUsr2;
	doc["TankFillMaxHours2"]   = TankFillMaxHrsUsr1;
	doc["NtfyOnBoreholeRetry"]       = NtfyOnBoreholeRetry;
	doc["EnableMaxPumpNotifyUsr2"]   = EnableMaxPumpNotifyUsr2;
	doc["EnableTankFillNotifyUsr2"]  = EnableTankFillNotifyUsr2;
	doc["EnableBackwashNotify"]      = EnableBackwashNotify;
	doc["EnableMaxPumpNotifyUsr1"]   = EnableMaxPumpNotifyUsr1;
	doc["EnableTankFillNotifyUsr1"]  = EnableTankFillNotifyUsr1;
	doc["RebootOnMaxPumpUsr2"]       = RebootOnMaxPumpUsr2;
	serializeJsonPretty(doc, f);
	f.close();
}

void NtfyParams::ReadData()
{
	File f = SPIFFS.open("/NTFY.json", "r");
	if (!f) return;
	if (f.size() > 768) { f.close(); return; }
	DynamicJsonDocument doc(768);
	if (!deserializeJson(doc, f)) {
		if (!doc["NtfyCode"].isNull())            NtfyCode            = doc["NtfyCode"].as<String>();
		if (!doc["NtfyCode2"].isNull())           NtfyCode2           = doc["NtfyCode2"].as<String>();
		if (!doc["SysNameUsr1"].isNull())         SysNameUsr1         = doc["SysNameUsr1"].as<String>();
		if (!doc["BoreholeMaxTime"].isNull())     BoreholeMaxTime      = doc["BoreholeMaxTime"].as<int>();
		if (!doc["BoreholeMaxTimeUsr1"].isNull()) BoreholeMaxTimeUsr1 = doc["BoreholeMaxTimeUsr1"].as<int>();
		if (!doc["TankFillMaxHours"].isNull())    TankFillMaxHrsUsr2  = doc["TankFillMaxHours"].as<int>();
		if (!doc["TankFillMaxHours2"].isNull())   TankFillMaxHrsUsr1  = doc["TankFillMaxHours2"].as<int>();
		if (!doc["NtfyOnBoreholeRetry"].isNull())      NtfyOnBoreholeRetry      = doc["NtfyOnBoreholeRetry"].as<bool>();
		if (!doc["EnableMaxPumpNotifyUsr2"].isNull())  EnableMaxPumpNotifyUsr2  = doc["EnableMaxPumpNotifyUsr2"].as<bool>();
		if (!doc["EnableTankFillNotifyUsr2"].isNull()) EnableTankFillNotifyUsr2 = doc["EnableTankFillNotifyUsr2"].as<bool>();
		if (!doc["EnableBackwashNotify"].isNull())     EnableBackwashNotify     = doc["EnableBackwashNotify"].as<bool>();
		if (!doc["EnableMaxPumpNotifyUsr1"].isNull())  EnableMaxPumpNotifyUsr1  = doc["EnableMaxPumpNotifyUsr1"].as<bool>();
		if (!doc["EnableTankFillNotifyUsr1"].isNull()) EnableTankFillNotifyUsr1 = doc["EnableTankFillNotifyUsr1"].as<bool>();
		if (!doc["RebootOnMaxPumpUsr2"].isNull())      RebootOnMaxPumpUsr2      = doc["RebootOnMaxPumpUsr2"].as<bool>();
	}
	f.close();
}

// =============================================================================
// CreateNtfyParams  (called from InitFileSystem in 03_Files.cpp)
// Allocates the global NtfyParams instance, loads saved values, writes back
// defaults for any missing keys, then builds the NTFY page-variable map.
// =============================================================================
void CreateNtfyParams() {
	NtfyData = new NtfyParams;
	NtfyData->ReadData();
	NtfyData->WriteData();
	MapNtfyPage();
}

// =============================================================================
// MapNtfyPage
// Builds the NtfyList PageVar array that binds NTFY form elements to
// NtfyParams fields via WebSocket.
// =============================================================================
void MapNtfyPage() {
	NtfyPageObject = true;
	NtfyList = new PageVar[PAGE_VAR_NTFY];
	for (int i = 0; i < PAGE_VAR_NTFY; i++)
	{
		switch (i)
		{
			case NTFY_CODE:
				NtfyList[i].Var = new PageElement(&NtfyData->NtfyCode,
				                      "NTFY Topic Name to " + TankWaterLevel->User_2, "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_CODE2:
				NtfyList[i].Var = new PageElement(&NtfyData->NtfyCode2,
				                      "NTFY Topic Name to " + TankWaterLevel->User_1, "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_MAX_PUMP_TIME:
				NtfyList[i].Var = new PageElement(&NtfyData->BoreholeMaxTime,            F("Max Pumping time (min)"),                "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_TANKFILL_HOURS:
				NtfyList[i].Var = new PageElement(&NtfyData->TankFillMaxHrsUsr2,         F("Tank filling at least within (Hours)"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_BACKWASH:
				NtfyList[i].Var = new PageElement(&FilterSensor->BackwashWarning,        F("Warning to Backwash (kPa)"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_FILTER_EXCEEDED:
				NtfyList[i].Var = new PageElement(&FilterSensor->FilterPressureExceeded, F("Filter Pressure Exceeded"),  "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_BH_RETRY:
				NtfyList[i].Var = new PageElement(&NtfyData->NtfyOnBoreholeRetry,       F("Borehole Retry Notify"),                "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_MAX_PUMP_TIME_USR1:
				NtfyList[i].Var = new PageElement(&NtfyData->BoreholeMaxTimeUsr1,        F("Max Pumping time (min)"),                "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_TANKFILL_HOURS2:
				NtfyList[i].Var = new PageElement(&NtfyData->TankFillMaxHrsUsr1,         F("Tank filling at least within (Hours)"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_MAX_PUMP_USR2:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableMaxPumpNotifyUsr2,    F("Enable"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_TANKFILL_USR2:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableTankFillNotifyUsr2,   F("Enable"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_BACKWASH:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableBackwashNotify,       F("Enable"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_MAX_PUMP_USR1:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableMaxPumpNotifyUsr1,    F("Enable"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_EN_TANKFILL_USR1:
				NtfyList[i].Var = new PageElement(&NtfyData->EnableTankFillNotifyUsr1,   F("Enable"), "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_SYSNAME_USR1:
				NtfyList[i].Var = new PageElement(&NtfyData->SysNameUsr1,
				                      "System Name for " + TankWaterLevel->User_1 + " messages", "", &NtfyStyle, i + ID_NTFY);
				break;
			case NTFY_REBOOT_MAX_PUMP_USR2:
				NtfyList[i].Var = new PageElement(&NtfyData->RebootOnMaxPumpUsr2,   F("Reboot when Max Pumping time exceeded"), "", &NtfyStyle, i + ID_NTFY);
				break;
		}
	}
}

// =============================================================================
// UpdateNtfyPageLabels
// Refreshes the topic-name row labels with the current usernames from TankLevel.
// Called at every page serve so the labels stay in sync if names are edited.
// =============================================================================
void UpdateNtfyPageLabels() {
	if (!NtfyPageObject) return;
	NtfyList[NTFY_CODE].Var->SetDescription ("NTFY Topic Name to " + TankWaterLevel->User_2);
	NtfyList[NTFY_CODE2].Var->SetDescription("NTFY Topic Name to " + TankWaterLevel->User_1);
}

// =============================================================================
// handleNtfy  (GET /ntfy)
// Serves the NTFY configuration page.
// =============================================================================
void handleNtfy(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	UpdateNtfyPageLabels();
	WT_SETUP_Ntfy();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_NTFY);
	appendHtmlStr(webSocketScript);
	appendHtmlBuf(PageBuf, getPageBufLen());
	appendHtmlStr(webUpateButton);
	appendNtfyTestButton();
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// =============================================================================
// WT_SETUP_Ntfy
// Populates PageBuf with the NTFY parameter table HTML.
// =============================================================================
void WT_SETUP_Ntfy() {
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("NTFY Parameter", "Value", "");
	WT_EditRow(&NtfyList[NTFY_CODE]);
	WT_EditRow(&NtfyList[NTFY_CODE2]);
	WT_EditRow(&NtfyList[NTFY_SYSNAME_USR1]);

	String hdrFiltUser2 = "Filter Backwash (" + TankWaterLevel->User_2 + ")";
	WT_SectionHeader(hdrFiltUser2.c_str());
	WT_CheckEditRow(&NtfyList[NTFY_EN_BACKWASH],        &NtfyList[NTFY_BACKWASH]);
	WT_RightCheckRow(&NtfyList[NTFY_FILTER_EXCEEDED]);

	String hdrBoreUser2 = "Borehole Pumping (" + TankWaterLevel->User_2 + ")";
	WT_SectionHeader(hdrBoreUser2.c_str());
	WT_CheckEditRow(&NtfyList[NTFY_EN_MAX_PUMP_USR2],  &NtfyList[NTFY_MAX_PUMP_TIME]);
	WT_RightCheckRow(&NtfyList[NTFY_REBOOT_MAX_PUMP_USR2]);
	WT_CheckEditRow(&NtfyList[NTFY_EN_TANKFILL_USR2],  &NtfyList[NTFY_TANKFILL_HOURS]);
	WT_RightCheckRow(&NtfyList[NTFY_BH_RETRY]);

	String hdrBoreUser1 = "Borehole Pumping (" + TankWaterLevel->User_1 + ")";
	WT_SectionHeader(hdrBoreUser1.c_str());
	WT_CheckEditRow(&NtfyList[NTFY_EN_MAX_PUMP_USR1],  &NtfyList[NTFY_MAX_PUMP_TIME_USR1]);
	WT_CheckEditRow(&NtfyList[NTFY_EN_TANKFILL_USR1],  &NtfyList[NTFY_TANKFILL_HOURS2]);

	WT_EndPage();
}

// =============================================================================
// SendNtfyPageValues  (called every 10 ms from MaintainWebPageValues)
// Detects changed NtfyParams values and pushes them to connected browsers.
// =============================================================================
void SendNtfyPageValues() {
	if (!NtfyPageObject) return;
	for (int i = 0; i < PAGE_VAR_NTFY; i++) {
		if (NtfyList[i].Var->GetName() != NtfyList[i].Var->GetNameLast()) {
			webParamUpdate(NtfyList[i].Var->GetName(), NtfyList[i].Var->GetID(""), NtfyList[i].Var->GetVarStyle());
			NtfyList[i].Var->UpdateLast();
		}
	}
}

// =============================================================================
// UpdateNtfyPageValues  (called from UnpackJsonPayload when ID_NTFY received)
// Applies browser form changes to NtfyParams and persists to SPIFFS.
// =============================================================================
void UpdateNtfyPageValues(uint32_t clientId, JsonObject *root) {
	for (JsonPair kv : *root) {
		for (int i = 0; i < PAGE_VAR_NTFY; i++) {
			if (NtfyList[i].Var->GetID("") == kv.key().c_str()) {
				NtfyList[i].Var->SetName(String(kv.value().as<const char*>()));
			}
		}
	}
	WebMessUpdate(clientId, "NTFY Updated!", "color: red", true);
	NtfyData->WriteData();
	FilterSensor->WriteData();   // Persist BackwashWarning and FilterPressureExceeded
}

// =============================================================================
// NTFY.sh Push Notification Engine
// =============================================================================

// NtfyTask — permanent FreeRTOS task (Core 0, priority 1, stack 4096 B)
// Blocks on NtfyQueue (1-second timeout).  On message receipt it POSTs to ntfy.sh.
void NtfyTask(void *pvParameters)
{
	NtfyMsg msg;
	for (;;)
	{
		if (xQueueReceive(NtfyQueue, &msg, pdMS_TO_TICKS(1000)) == pdTRUE)
		{
			WiFiClient client;
			HTTPClient http;
			String url = "http://ntfy.sh/";
			url += msg.code;
			http.setTimeout(5000);
			if (http.begin(client, url))
			{
				http.addHeader("Content-Type", "text/plain");
				http.addHeader("X-Priority",   "urgent");
				http.POST(msg.message);
				http.end();
			}
		}
	}
}

// MessageNTFY — queue a push notification; returns immediately.
// Silently dropped if queue is full, WiFi is down, or topic is blank.
void MessageNTFY(String message)
{
	if (NtfyQueue == NULL)               return;
	if (WiFi.status() != WL_CONNECTED)   return;
	String code = NtfyData->NtfyCode;
	code.trim();
	if (code.length() == 0)              return;

	NtfyMsg msg;
	memset(&msg, 0, sizeof(NtfyMsg));
	message.toCharArray(msg.message, sizeof(msg.message));
	code.toCharArray   (msg.code,    sizeof(msg.code));

	xQueueSend(NtfyQueue, &msg, 0);
}

// MessageNTFY2 — queue a push notification to NtfyCode2 (User 1).
// Mirrors MessageNTFY but uses NtfyCode2 as the topic.
void MessageNTFY2(String message)
{
	if (NtfyQueue == NULL)               return;
	if (WiFi.status() != WL_CONNECTED)   return;
	String code = NtfyData->NtfyCode2;
	code.trim();
	if (code.length() == 0)              return;

	NtfyMsg msg;
	memset(&msg, 0, sizeof(NtfyMsg));
	message.toCharArray(msg.message, sizeof(msg.message));
	code.toCharArray   (msg.code,    sizeof(msg.code));

	xQueueSend(NtfyQueue, &msg, 0);
}

// SetupNTFY — creates the FreeRTOS queue and launches NtfyTask.
// Called from setup() after WiFi connects.
void SetupNTFY() {
	NtfyQueue = xQueueCreate(1, sizeof(NtfyMsg));
	BaseType_t res = xTaskCreatePinnedToCore(NtfyTask, "NtfyTask", 4096, NULL, 1, &NtfyTaskHandle, 0);
	if (res == pdPASS) DEBUG_PRINTLN(F("NtfyTask created OK"));
	else               DEBUG_PRINTLN(F("NtfyTask creation FAILED"));
}

// =============================================================================
// Web Helpers
// =============================================================================

// appendNtfyTestButton — appends two TEST NTFY buttons (one per user) with
// dynamic labels built from the current TankLevel usernames.
void appendNtfyTestButton() {
	appendHtmlStr("<script>"
	              "function ButtonNtfyTest(){fetch('/testntfy');}"
	              "function ButtonNtfyTest2(){fetch('/testntfy2');}"
	              "</script>");
	appendHtmlStr("<div><br></div><div>");
	String b1 = "<b><a><input type=\"button\" id=\"TESTNTFY\"  onclick=\"ButtonNtfyTest()\"  style=\"font-size:14px\" value=\"TEST NTFY to ";
	b1 += TankWaterLevel->User_2; b1 += "\"></a></b>&nbsp;";
	appendHtmlStr(b1.c_str());
	String b2 = "<b><a><input type=\"button\" id=\"TESTNTFY2\" onclick=\"ButtonNtfyTest2()\" style=\"font-size:14px\" value=\"TEST NTFY to ";
	b2 += TankWaterLevel->User_1; b2 += "\"></a></b>";
	appendHtmlStr(b2.c_str());
	appendHtmlStr("</div>");
}

// handleTestNtfy  (GET /testntfy)
// Queues a test push notification to NtfyCode (User 2).
void handleTestNtfy(AsyncWebServerRequest *request)
{
	MessageNTFY("TEST FROM " + Params->SysName + " to " + TankWaterLevel->User_2);
	WebMessUpdate(Lastnum, "Test NTFY sent to " + TankWaterLevel->User_2 + "!", "color: red", true);
	request->send(200, "text/plain", "OK");
}

// handleTestNtfy2  (GET /testntfy2)
// Queues a test push notification to NtfyCode2 (User 1).
void handleTestNtfy2(AsyncWebServerRequest *request)
{
	MessageNTFY2("TEST FROM " + NtfyData->SysNameUsr1 + " to " + TankWaterLevel->User_1);
	WebMessUpdate(Lastnum, "Test NTFY sent to " + TankWaterLevel->User_1 + "!", "color: red", true);
	request->send(200, "text/plain", "OK");
}
