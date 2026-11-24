#pragma once

// =============================================================================
// 03_Files.h — SPIFFS File System Globals & Web File-Manager Definitions
// =============================================================================
// Declares shared file handles, the sorted directory listing structure, web
// page state variables, and SPIFFS filename constants used across the firmware.
// =============================================================================

// ─── Temporary String Buffers for HTML Link Generation ───────────────────────
	inline String OP01 = "";   // Assembled href URL fragment (e.g. "/download?f_File=...")
	inline String OP03 = "";   // Assembled link label text

// ─── File Action Status Message ───────────────────────────────────────────────
// SetMess is set by file route handlers to override the default FA_MESS_06
// status message on the next page render, then cleared to 0 after use.
	inline int    SetMess    = 0;    // Pending status message code (FA_MESS_* constant, or 0 = none)
	inline String SpesialStr = "";   // Custom message text used with FA_MESS_RED / FA_MESS_GRN

// ─── Global SPIFFS File Handles ───────────────────────────────────────────────
// Declared here so that 02_Time.cpp (reduce_file) and 10_Reports.cpp can share
// the same handles without re-declaring them.
	inline File   f;      // Primary file handle
	inline File   ftemp;  // Temporary file handle (used during log-file trimming)
	inline String line;   // Line buffer for readStringUntil() calls

// ─── Sorted Directory Listing ─────────────────────────────────────────────────
// CreateDFileList() populates SortedFiles[] with up to File_array_LEN entries
// and bubble-sorts them alphabetically by file extension + name.
	#define File_array_LEN   50   // Maximum files shown in the SPIFFS browser
	#define Filename_LEN     32   // Maximum characters in a stored filename

	struct FileNameStruct
	{
		char FileName[Filename_LEN];   // Display name (shown in the file list table)
		char SortName[Filename_LEN];   // Sort key (extension first for grouped ordering)
		int  FileSize;                 // File size in bytes
	};
	inline int            StructIdx;                    // Number of valid entries in SortedFiles[]
	inline FileNameStruct SortedFiles[File_array_LEN];  // Sorted directory listing array

// ─── Report Display Limit ─────────────────────────────────────────────────────
	#define REPORT_LINES   25   // Maximum log lines shown on the report web pages

// ─── File Page Navigation Index Constants ────────────────────────────────────
// Passed to FilePageSel() to highlight the currently active menu item.
	#define PS_FILES_HOME      1   // Active page = home (linked from file menu)
	#define PS_FILES           2   // Active page = SPIFFS file listing
	#define PS_FILES_DOWNLOAD  3   // Active page = file download
	#define PS_FILES_UPLOAD    4   // Active page = file upload
	#define PS_FILES_DELETE    5   // Active page = file delete

// ─── SPIFFS Filename Constants ────────────────────────────────────────────────
// Centralised so that any rename only needs changing in one place.
	inline String StrFormatComplete = F("/FormatComplete.txt");   // Presence = SPIFFS already formatted
	inline String StrGeneralLog     = F("/GeneralLog.txt");       // System event log
	inline String StrBoreholeUsedLog = F("/BoreholeUsedLog.txt");  // Borehole used event log (tank fill, manual pump, user pump)
	inline String StrIrrigationLog   = F("/IrrigationLog.txt");    // Irrigation zone event log
	inline String StrTankFilterLog   = F("/TankFilterLog.txt");    // Tank filter event log (filter ON/OFF, pressure)

	inline String StrElectricValves = F("/WaterValves.json");     // Valve names, delays & configurations
	inline String StrSuspendDays    = F("/SuspendDays.json");     // 7-day irrigation suspension calendar
	inline String StrIrrigation     = F("/Irrigation.json");      // 9-zone irrigation schedules

// ─── Protected files ─────────────────────────────────────────────────────────
// /WiFiConfig.json holds WiFi passwords (05_WiFi.cpp): never listed, viewed
// or downloaded by the FILES pages.  Accepts "name" or "/name".
	inline bool IsProtectedFile(String name) {
		if (name.startsWith("/")) name = name.substring(1);
		return name.equalsIgnoreCase("WiFiConfig.json") || name.equalsIgnoreCase("WifiSettings.json");
	}
