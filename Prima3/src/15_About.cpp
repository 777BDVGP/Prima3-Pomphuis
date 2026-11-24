// =============================================================================
// 15_About.cpp — OTA Update Service & About Page
// =============================================================================
// Implements ArduinoOTA setup (with watchdog guard), the About web page,
// and About.json persistence (ip, gateway, subnet, dns).
// The About page is read-only: a 2-column OTA table, REBOOT/RESET ASP buttons,
// and a centred project-info block.  System Name is edited on the Settings page.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// ─── SPIFFS path for About.json ───────────────────────────────────────────────
static constexpr const char* ABOUT_FILE_PATH = "/About.json";

// ─── OTA State ────────────────────────────────────────────────────────────────
bool OTAInProgress      = false;
int  OTAPercentComplete = 0;

// =============================================================================
// About.json — IP Persistence
// =============================================================================


// -----------------------------------------------------------------------------
// SetupAbout
// Compares the current build time (__DATE__ " " __TIME__) with the stored
// buildStamp in /About.json.  If they differ, a new build has been uploaded
// (likely via USB cable), so we update buildStamp.
// Called once from setup() after WiFi is connected (to ensure we have NTP time).
// -----------------------------------------------------------------------------
void SetupAbout() {
    String currentBuild = String(__DATE__) + " " + String(__TIME__);
    DynamicJsonDocument doc(512);
    bool changed = false;

    File fr = SPIFFS.open(ABOUT_FILE_PATH, "r");
    if (fr) {
        deserializeJson(doc, fr);
        fr.close();
        if (!doc.containsKey("buildStamp") || doc["buildStamp"].as<String>() != currentBuild) {
            changed = true;
        }
    } else {
        changed = true;
    }

    if (changed) {
        doc["buildStamp"]   = currentBuild;
        File fw = SPIFFS.open(ABOUT_FILE_PATH, "w");
        if (fw) {
            serializeJson(doc, fw);
            fw.close();
            DEBUG_PRINTLN("About.json updated with new buildStamp");
        }
    }
}

// =============================================================================
// OTA Setup
// =============================================================================

// -----------------------------------------------------------------------------
// SetupOTA
// Configures ArduinoOTA with the system name as hostname and registers callbacks.
// The watchdog Ticker is detached during flash writes and re-attached on end/error.
// Called once from setup() after WiFi connects.
// -----------------------------------------------------------------------------
void SetupOTA() {
    String deviceName = Params->SysName;
    deviceName.trim();
    if (deviceName.length() == 0) deviceName = "PrimaPomphuis";

    ArduinoOTA.setPort(3232);
    ArduinoOTA.setHostname(deviceName.c_str());

    ArduinoOTA
        .onStart([]() {
            OTAInProgress      = true;
            OTAPercentComplete = 0;
            secondTick.detach();   // Disable watchdog — flash writes block > 5 s
            ws.textAll("{\"type\":\"ota_start\"}");
            DEBUG_PRINTLN("OTA Start");
        })
        .onEnd([]() {
            DEBUG_PRINTLN("\nOTA End");
            ws.textAll("{\"type\":\"ota_end\"}");
            OTAInProgress = false;
            secondTick.attach(1, ISRwatchdog);
        })
        .onProgress([](unsigned int progress, unsigned int total) {
            int pct = (progress / (total / 100));
            if (pct != OTAPercentComplete && pct % 5 == 0) {
                String msg = "{\"type\":\"ota_progress\",\"percent\":" + String(pct) + "}";
                ws.textAll(msg);
            }
            OTAPercentComplete = pct;
            DEBUG_PRINTF("OTA Progress: %u%%\r", OTAPercentComplete);
        })
        .onError([](ota_error_t error) {
            DEBUG_PRINTF("OTA Error[%u]\n", error);
            ws.textAll("{\"type\":\"ota_error\"}");
            OTAInProgress = false;
            secondTick.attach(1, ISRwatchdog);
        });

    ArduinoOTA.begin();
    DEBUG_PRINTLN("OTA Ready — " + WiFi.localIP().toString());
    DEBUG_PRINTLN("PIO upload: pio run -e ota -t upload");
}

// =============================================================================
// About Page — HTML Builders
// =============================================================================

// -----------------------------------------------------------------------------
// buildAboutTable
// Writes a 2-column read-only OTA info table into PageBuf using the WT_* system.
// Row 1: "OTA (Over-The-Air) Updates" | "Ready"  (green)
// Row 2: "OTA Terminal Command"        | "pio run -e ota -t upload --upload-port <IP>"
// ListObj is nullptr — safe because Type is "text" (never a <select>).
// -----------------------------------------------------------------------------
static void buildAboutTable() {
    String otaCmd = "pio run -e ota -t upload --upload-port " + WiFi.localIP().toString();

    resetPageBuf();
    WT_StartPage();
    WT_StartTable("PARAMETER", "VALUE", "");

    // Row 1 — OTA status (live: JS setOtaStatus() updates value + colour via WebSocket)
    WT_StartLine();
    WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text", "OTA (Over-The-Air) Updates", "OTA_L1",     "",          nullptr);
    WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "text", "Ready to Update",            "ota_status", COLOR_GREEN, nullptr, "readonly:");
    WT_EndLine();

    // Row 2 — PlatformIO upload command
    WT_StartLine();
    WT_Column(WT_LABEL, WT_FIRST, WT_MORE, "text", "OTA Terminal Command", "OTA_L2", "", nullptr);
    WT_Column(WT_LABEL, WT_NEXT,  WT_LAST, "text", otaCmd.c_str(),        "OTA_V2", "font-size:11px", nullptr);
    WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "", "", "", "", nullptr);
    WT_EndLine();

    WT_EndPage();
}

// -----------------------------------------------------------------------------
// buildProjectInfoDiv
// Returns a centred HTML block showing project name, build date, and PCB type.
// -----------------------------------------------------------------------------
static void buildProjectInfoDiv() {
    const char* bd = __DATE__;
    char bDay[3] = { (bd[4]==' ' ? '0' : bd[4]), bd[5], '\0' };
    char bMon[4] = { bd[0], bd[1], bd[2], '\0' };
    char bYr[5]  = { bd[7], bd[8], bd[9], bd[10], '\0' };
    char buildDate[32];
    snprintf(buildDate, sizeof(buildDate), "%s %s %s %s", bDay, bMon, bYr, __TIME__);

    // Same look as the FIRMWARE UPDATE block (appendFwUpdateBlock, 22_Update.cpp).
    appendHtmlStr("<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:14px 0 4px'>PROJECT DETAILS</div>"
                  "<table><tr><td>Project</td><td>Prima3 Pomphuis</td></tr>"
                  "<tr><td>Firmware</td><td>" FW_VERSION "</td></tr>"
                  "<tr><td>Build Date</td><td>");
    appendHtmlStr(buildDate);
    appendHtmlStr("</td></tr>"
                  "<tr><td>PCB</td><td>WEMOS_ESP32_2S_V5</td></tr>"
                  "<tr><td>Board</td><td>WEMOS LOLIN S3 Mini</td></tr>"
                  "<tr><td>IP</td><td>");
    appendHtmlStr(WiFi.localIP().toString().c_str());
    appendHtmlStr("</td></tr><tr><td>MAC</td><td>");
    appendHtmlStr(WiFi.macAddress().c_str());
    appendHtmlStr("</td></tr></table>");
}

// =============================================================================
// About Page — Route Handler
// =============================================================================

// -----------------------------------------------------------------------------
// handleAbout  (GET /about)
// Builds and serves the About page:
//   1. OTA info table (read-only, 2 columns via WT_* system)
//   2. REBOOT and RESET ASP buttons
//   3. Centred project info block (Project, Build Date, PCB)
// -----------------------------------------------------------------------------
void handleAbout(AsyncWebServerRequest *request) {
    if (!beginHtmlStream(request)) return;
    buildAboutTable();                            
    resetHtmlBuf();
    appendHtmlStr(webHeadPage);
    append_FileAction(FA_MESS_06);
    appendMainPageSel(PS_ABOUT);                  
    appendHtmlStr(webSocketScript);               
    // PC upload (ArduinoOTA) rows: only with SETTINGS "Show PC Upload (OTA) on
    // ABOUT".  ArduinoOTA itself stays on either way — hiding is cosmetic.
    if (Params->ShowOtaInfo) appendHtmlBuf(PageBuf, getPageBufLen());
    appendFwUpdateBlock();                        // FIRMWARE UPDATE (22_Update.cpp)
    webRebootButton();                            
    webResetASPButton();                          
    buildProjectInfoDiv();                        
    appendHtmlStr(webHTMLEnd);
    sendHtmlPage(request);
}
