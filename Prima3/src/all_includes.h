// =============================================================================
// all_includes.h — Single Aggregate Include for All Firmware Modules
// =============================================================================
// Every .cpp file includes only this one header.  It pulls in all module
// headers in dependency order and then forward_decls.h for cross-file function
// prototypes.  Changing include order here can break compilation if a header
// depends on types or globals defined in an earlier header.
// =============================================================================
#pragma once

#include "01_Main.h"        // Global flags, libraries, pin aliases, watchdog
#include "02_Time.h"        // Software timer engine, NTP, date/time utilities
#include "03_Files.h"       // SPIFFS init, file manager route handlers
#include "04_WebPages.h"    // AsyncWebServer, WebSocket, PageVar/PageElement, WT_* builders
#include "05_ThisSystem.h"  // I/O arrays, debounce, button/pump control, AddRoutes
#include "08_Home.h"        // Home page, MaintainHomePage, PageVar list
#include "09_Suspend.h"     // SuspendDays class, 7-day suspension calendar
#include "09_WaterValves.h" // ElectricValves class, valve names and configs
#include "09_Irrigation.h"  // Irrigation zones, SheduleIrri state machine
#include "09_Tank.h"        // TankLevel, PresureSensor, MaintainTankFilling
#include "10_Reports.h"     // Event log viewers, SaveGenReport, SaveIrrigReport
#include "11_Actions.h"     // Actions page stub, pump-session start-time snapshot
#include "12_Settings.h"    // AllParams class, settings page, PageBuf
#include "13_TestValves.h"  // Manual per-valve test page
#include "15_About.h"       // OTA update service, About page, ArduinoOTA
#include "16_Ntfy.h"        // NTFY.sh push notifications, NtfyParams
#include "17_RainGauge.h"   // RAIN_OK_DO_IRRI from the RainGauge_PIO unit (hourly)
#include "22_Update.h"      // ABOUT firmware update: GitHub check/install + .bin upload
#include "forward_decls.h"  // All cross-file forward declarations (see CLAUDE.md)
