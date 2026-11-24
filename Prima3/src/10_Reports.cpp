// =============================================================================
// 10_Reports.cpp — Event Log Web Pages & SPIFFS Log Writers
// =============================================================================
// Implements the Reports navigation page, four log-viewer pages (last
// REPORT_LINES lines each), and the four SaveXxxReport() functions used
// throughout the firmware to append timestamped entries to SPIFFS log files.
//
// Log files and their writers:
//   GeneralLog.txt    — SaveGenReport()          guarded by LogGeneralEvents
//   IrrigationLog.txt — SaveIrrigationReport()   guarded by LogIrrigationEvents
//   BoreholeUsedLog.txt — SaveIrrigReport()      guarded by LogBoreholeUsedEvents
//   TankFilterLog.txt — SaveTankFilterReport()   guarded by LogTankFilterEvents
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// =============================================================================
// Shared timestamp helper — used by all four Save* functions
// =============================================================================
static String MakeTimestamp() {
	char buf[20];
	time_t now = time(nullptr);
	tm* ti = localtime(&now);
	sprintf(buf, "%04d-%02d-%02d %02d:%02d:%02d",
	        ti->tm_year + 1900, ti->tm_mon + 1, ti->tm_mday,
	        ti->tm_hour, ti->tm_min, ti->tm_sec);
	return String(buf);
}

// =============================================================================
// Shared log-file reader — used by all four append_Form_View* functions
// =============================================================================
static void ReadLogFile(const String &path, const String &displayName) {
	appendHtmlStr("<form> <a> </a><br><b>");
	appendHtmlStr(displayName.c_str());
	appendHtmlStr("</b><br>");

	f = SPIFFS.open(path, "r");
	if (!f) {
		appendHtmlStr("<a>File "); appendHtmlStr(displayName.c_str()); appendHtmlStr(" not available!</a><br>");
		return;
	}
	while (f.available()) {
		line = f.readStringUntil('\n');
		appendHtmlStr("<p>"); appendHtmlStr(line.c_str()); appendHtmlStr("</p>");
	}
	f.close();
	appendHtmlStr("</form>");
}

// =============================================================================
// Reports Landing Page  (GET /report)
// =============================================================================

void handleOthers(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	CompilePage_Reports(request);
}

void CompilePage_Reports(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(Delmess);         
	append_UL_ReportsMenu(2);           
	appendHtmlStr(webSocketScript);     
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
	Delmess = FA_MESS_06;
}

// -----------------------------------------------------------------------------
// append_UL_ReportsMenu
// Navigation menu for all report pages.  Index values:
//   2 = REPORTS landing   6 = GENERAL   7 = IRRIGATION
//   3 = BOREHOLE USED     8 = FILTER
// -----------------------------------------------------------------------------
void append_UL_ReportsMenu(int Index) {
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr("<a href='/'>HOME</a>");
	appendHtmlStr(Index == 2 ? "<a class='act'>REPORTS</a>" : "<a href='/report'>REPORTS</a>");
	appendHtmlStr("</div>");

	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == 6 ? "<a class='act'>GENERAL</a>"    : "<a class='warn' href='/reportgen'>GENERAL</a>");
	appendHtmlStr(Index == 7 ? "<a class='act'>IRRIGATION</a>" : "<a class='warn' href='/reportirrig'>IRRIGATION</a>");
	appendHtmlStr(Index == 3 ? "<a class='act'>BOREHOLE USED</a>" : "<a class='warn' href='/reportpump'>BOREHOLE USED</a>");
	appendHtmlStr(Index == 8 ? "<a class='act'>FILTER</a>"     : "<a class='warn' href='/reportfilter'>FILTER</a>");
	appendHtmlStr("</div>");
}

void SetMessColour(String Mess, String Color) {
	ACTION_INST = Mess;
	COLOR       = Color;
}

// =============================================================================
// General Log Report  (GET /reportgen)  — GeneralLog.txt
// =============================================================================

void handleGenReport(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	SetMessColour("General Report (Last " + String(REPORT_LINES) + ")", COLOR_DEFAULT);
	CompilePage_ViewGeneralReport(request);
}

void CompilePage_ViewGeneralReport(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);          
	append_UL_ReportsMenu(6);               
	appendHtmlStr(webSocketScript);         
	append_Form_ViewGeneralReport();        
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void append_Form_ViewGeneralReport() {
	ReadLogFile(StrGeneralLog, "GeneralLog.txt");
}

void SaveGenReport(String p_entry) {
	if (Params->LogGeneralEvents) {
		String Message = MakeTimestamp() + " " + p_entry;
		File f = SPIFFS.open(StrGeneralLog, "a");
		if (f) { f.println(Message); f.close(); }
	}
}

// =============================================================================
// Irrigation Log Report  (GET /reportirrig)  — IrrigationLog.txt
// =============================================================================

void handleIrrigationReport(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	SetMessColour("Irrigation Report (Last " + String(REPORT_LINES) + ")", COLOR_DEFAULT);
	CompilePage_ViewIrrigationReport(request);
}

void CompilePage_ViewIrrigationReport(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);          
	append_UL_ReportsMenu(7);               
	appendHtmlStr(webSocketScript);         
	append_Form_ViewIrrigationReport();     
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void append_Form_ViewIrrigationReport() {
	ReadLogFile(StrIrrigationLog, "IrrigationLog.txt");
}

void SaveIrrigationReport(String p_entry) {
	if (Params->LogIrrigationEvents) {
		String Message = MakeTimestamp() + " " + p_entry;
		File f = SPIFFS.open(StrIrrigationLog, "a");
		if (f) { f.println(Message); f.close(); }
	}
}

// =============================================================================
// Borehole Used Report  (GET /reportpump)  — BoreholeUsedLog.txt
// =============================================================================

void handleIrrigReport(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	SetMessColour("Borehole Used Report (Last " + String(REPORT_LINES) + ")", COLOR_DEFAULT);
	CompilePage_ViewIrrigReport(request);
}

void CompilePage_ViewIrrigReport(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);          
	append_UL_ReportsMenu(3);               
	appendHtmlStr(webSocketScript);         
	append_Form_ViewIrrigReport();          
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void append_Form_ViewIrrigReport() {
	ReadLogFile(StrBoreholeUsedLog, "BoreholeUsedLog.txt");
}

void SaveIrrigReport(String p_entry) {
	if (Params->LogBoreholeUsedEvents) {
		String Message = MakeTimestamp() + " " + p_entry;
		File f = SPIFFS.open(StrBoreholeUsedLog, "a");
		if (f) { f.println(Message); f.close(); }
	}
}

// =============================================================================
// Tank Filter Report  (GET /reportfilter)  — TankFilterLog.txt
// =============================================================================

void handleTankFilterReport(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;
	SetMessColour("Filter Report (Last " + String(REPORT_LINES) + ")", COLOR_DEFAULT);
	CompilePage_ViewTankFilterReport(request);
}

void CompilePage_ViewTankFilterReport(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);          
	append_UL_ReportsMenu(8);               
	appendHtmlStr(webSocketScript);         
	append_Form_ViewTankFilterReport();     
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void append_Form_ViewTankFilterReport() {
	ReadLogFile(StrTankFilterLog, "TankFilterLog.txt");
}

void SaveTankFilterReport(String p_entry) {
	if (Params->LogTankFilterEvents) {
		String Message = MakeTimestamp() + " " + p_entry;
		File f = SPIFFS.open(StrTankFilterLog, "a");
		if (f) { f.println(Message); f.close(); }
	}
}

// -----------------------------------------------------------------------------
// SetStartPumpTime
// Snapshots the current date and time into the StartYear … StartSeconds globals
// for use in pump-session duration calculations.
// -----------------------------------------------------------------------------
void SetStartPumpTime() {
	time_t now = time(nullptr);
	tm* timeInfo = localtime(&now);
	StartYear    = timeInfo->tm_year + 1900;
	StartMonth   = timeInfo->tm_mon;
	StartDay     = timeInfo->tm_mday;
	StartHours   = timeInfo->tm_hour;
	StartMinutes = timeInfo->tm_min;
	StartSeconds = timeInfo->tm_sec;
}
