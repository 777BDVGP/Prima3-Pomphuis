// =============================================================================
// forward_decls.h — Cross-File Function Forward Declarations
// =============================================================================
// PlatformIO compiles each .cpp independently — unlike the Arduino IDE, it does
// NOT auto-generate function prototypes.  Every function called from a file
// other than its own .cpp must be declared here.
//
// Grouping rule: declarations are grouped by the .cpp file that *defines*
// the function, not by the file that calls it.  Add new declarations under the
// correct group when implementing a new cross-file function.
// =============================================================================
#pragma once

// ─── 01_Main.cpp ─────────────────────────────────────────────────────────────
void InitHardware();
void WatchdogSetup();
void ISRwatchdog();
void LEDToggle();
void MaintainGatewayWatch();
void ISR_LED_Setup();
void UpdateTimerTimes();
void HeartbeatSetMode(HbMode mode);

// ─── 02_Time.cpp ─────────────────────────────────────────────────────────────
void SetupTime();
void MaintainTimers();
void MaintainMidnightReboot();
void MaintainHeapGuard();
void UpdateWebPageTime();
void CompileDateTime();
void reduce_file(String filename, int Entries);
void activateTimer(int index);
void deactivateTimer(int index);
bool TimerExpired(int index);
void SetValueAndAtivate(String Units, int TimerUnits, int index);
void SetTimerValue(int Units, int index);
int  RemainingSec(int index);
int  RemainingMin(int index);

String GetCurrentDateAndTime();
String GetCurrentDate(int daysOffset);
String GetCurrentTime();
String IncrementDateWithOneDay(String date);
String AddTimeToCurrentDateTime(String baseDateTime, String period);

// ─── 03_Files.cpp ─────────────────────────────────────────────────────────────
extern String FSInitLog;   // Defined in main.cpp; filled by InitFileSystem()
void   InitFileSystem();
void   FilesPage(AsyncWebServerRequest *request);
void   FilePageSel(int Index);
void   handleApiFiles(AsyncWebServerRequest *request);
void   handleFileView(AsyncWebServerRequest *request);
void   handleFileDeleteJson(AsyncWebServerRequest *request);
void   File_Download(AsyncWebServerRequest *request);
void   CompilePage_Download(AsyncWebServerRequest *request);
void   append_Form_Download();
void   DownloadFile(AsyncWebServerRequest *request, String filename);
void   File_Upload(AsyncWebServerRequest *request);
void   handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void   File_Delete(AsyncWebServerRequest *request);
void   Compile_PageDelete(AsyncWebServerRequest *request);
void   append_Form_Delete();
void   SPIFFS_file_delete(AsyncWebServerRequest *request, String filename);
void   CreateDFileList(const char * dir);

// ─── 04_WebPages.cpp ──────────────────────────────────────────────────────────
bool   beginHtmlStream(AsyncWebServerRequest *request);
void   SetupWebPage();
void   MaintainWebPage();
void   MaintainWebService();
void   webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void   webParamUpdate(const String& Name, const String& ID, const String& ATT, const char* ATTName = "style");
void   UnpackJsonPayload(uint32_t clientId, uint8_t * payload, size_t length);
void   WebMessUpdate(uint32_t clientId, const String& Mess, const String& Att, bool Reset);
void   UpdateSelectMessDelay();
void   append_FileAction(int ReqMess);
void   WT_StartTable(const char* Hdr1, const char* Hdr2, const char* Hdr3);
void   WT_Column(int WT_Type, int WT_Begin, int WT_Finish, const char* Type, const char* Labval, const char* ID, const char* Style, PageVar *ListObj, const char* ExtraAttrs = "");
void   WT_AutoColumn(int WT_Type, int WT_Begin, int WT_Finish, PageVar *pv, const char* IDPrefix = "", const char* ExtraAttrs = "");
void   WT_DisplayRow(PageVar *pv);
void   WT_EditRow(PageVar *pv, const char* typeOverride = "", const char* ExtraAttrs = "");
void   WT_CheckEditRow(PageVar *cbPv, PageVar *valPv, const char* typeOverride = "");
void   WT_RightCheckRow(PageVar *pv);
void   WT_StartPage();
void   WT_EndPage();
void   WT_EndPageClass(const char* cls);
void   WT_Header(const char* Header);
void   WT_SectionHeader(const char* Header);
void   WT_StartLine();
void   WT_EndLine();
void   WT_StartLabel();
void   WT_StartLabelEmpty();
void   WT_StartInput();
void   WT_StartInputEmpty();
void   WT_NextColumn();
void   WT_EndCell();
void   WT_TypeCell(const char* Type);
void   WT_TextLabel(const char* Txt);
void   WT_ValueInput(const char* Val);
void   WT_IDCell(const char* ID);
void   WT_AttrsStart();
void   WT_Attr(const char* n, const char* v);
void   WT_AttrsEnd();
void   WT_OptionsStart();
void   WT_OptionsValues(const char* AVal);
void   WT_OptionsEnd();

// ─── 05_ThisSystem.cpp ────────────────────────────────────────────────────────
void SetupSystemIOs();
void PinDebounce();
void MaintainButtonStatus();
void MaintainRemotePumpWatchdog();
void MaintainWebPageValues();
void AddRoutes();

// ─── 08_Home.cpp ──────────────────────────────────────────────────────────────
void   handleRoot(AsyncWebServerRequest *request);
void   appendMainPageSel(int Index);
void   WT_HomePage();
void   MapHomePage();
void   SendHomePageValues();
void   MaintainHomePage();
void   webRebootButton();

// ─── 09_Irrigation.cpp ────────────────────────────────────────────────────────
void   handleIrrigation(AsyncWebServerRequest *request);
void   IrrigationPage(AsyncWebServerRequest *request);
void   CompilePage_Irrigation(AsyncWebServerRequest *request);
void   IrrigatePageSel(int Index);
void   append_Form_Irrigation();
void   handleIrrigSetupNext(AsyncWebServerRequest *request);
void   handleIrrigSetupPrev(AsyncWebServerRequest *request);
void   MapIrrigSetupPage(int Line);
void   CompilePage_IrrigSetup(AsyncWebServerRequest *request, int Line);
void   Irrigate_Next_Prev();
void   WT_SETUP_IrrigSetup(int Line);
void   SendIrrigSetupPageValues();
void   SendIrrigationOverviewValues();
void   UpdateIrrigSetupPageValues(uint32_t clientId, JsonObject *root);
void   handleResumeIrrig(AsyncWebServerRequest *request);
void   handleSuspendIrrig(AsyncWebServerRequest *request);
void   CreateIrrigation();
void   CreateSheduleObj();
void   SetupIrrigateShedule(int Line, String Type);
String GetScheduledDateAndTime(int Entry);
void   UpdateToNextDate(int Entry);
void   SuspendAllIrrigation();
void   IrrigationInit();
void   IrrigationShedule();

// ─── 09_Tank.cpp ──────────────────────────────────────────────────────────────
void   handleTank(AsyncWebServerRequest *request);
void   CompilePage_Tank(AsyncWebServerRequest *request);
void   MapTankPage();
void   WT_SETUP_Tank();
void   SendTankPageValues();
void   UpdateTankPageValues(uint32_t clientId, JsonObject *root);
void   CreatePresureSensors();
void   MaintainTankFilling();
void   MaintainStatusOfValves();
bool   StartBorehole();
void   StopBorehole();

// ─── 09_Suspend.cpp ───────────────────────────────────────────────────────────
void   handleManageSuspend(AsyncWebServerRequest *request);
void   CompilePage_Suspend(AsyncWebServerRequest *request);
void   MapSuspendPage();
void   WT_SETUP_Suspend();
void   SendSuspendPageValues();
void   UpdateSuspendPageValues(uint32_t clientId, JsonObject *root);
void   CreateSuspendDays();
void   CreateSuspendMaps();

// ─── 09_WaterValves.cpp ───────────────────────────────────────────────────────
void   handleWaterValves(AsyncWebServerRequest *request);
void   CompilePage_WaterValves(AsyncWebServerRequest *request);
void   MapWaterValvesPage();
void   WT_SETUP_WaterValves();
void   SendWaterValvesPageValues();
void   UpdateWaterValvesPageValues(uint32_t clientId, JsonObject *root);
void   handleValvesConfigs(AsyncWebServerRequest *request);
void   CompilePage_ValvesConfigs(AsyncWebServerRequest *request);
void   MapWaterConfigPage();
void   WT_SETUP_Config();
void   SendConfigPageValues();
void   UpdateConfigPageValues(uint32_t clientId, JsonObject *root);
void   CreateElectricValves();
void   CreateValvesConfigMaps();

// ─── 10_Reports.cpp ───────────────────────────────────────────────────────────
void   handleOthers(AsyncWebServerRequest *request);
void   CompilePage_Reports(AsyncWebServerRequest *request);
void   append_UL_ReportsMenu(int Index);
void   SetMessColour(String Mess, String Color);
void   handleGenReport(AsyncWebServerRequest *request);
void   CompilePage_ViewGeneralReport(AsyncWebServerRequest *request);
void   append_Form_ViewGeneralReport();
void   SaveGenReport(String p_entry);
void   handleIrrigationReport(AsyncWebServerRequest *request);
void   CompilePage_ViewIrrigationReport(AsyncWebServerRequest *request);
void   append_Form_ViewIrrigationReport();
void   SaveIrrigationReport(String p_entry);
void   handleIrrigReport(AsyncWebServerRequest *request);
void   CompilePage_ViewIrrigReport(AsyncWebServerRequest *request);
void   append_Form_ViewIrrigReport();
void   SaveIrrigReport(String p_entry);
void   handleTankFilterReport(AsyncWebServerRequest *request);
void   CompilePage_ViewTankFilterReport(AsyncWebServerRequest *request);
void   append_Form_ViewTankFilterReport();
void   SaveTankFilterReport(String p_entry);
void   SetStartPumpTime();

// ─── 11_Actions.cpp ───────────────────────────────────────────────────────────
void   handleActions(AsyncWebServerRequest *request);
void   CompilePage_Actions(AsyncWebServerRequest *request);
void   append_Form_Actions();

// ─── 12_Settings.cpp ──────────────────────────────────────────────────────────
void   CreateAllParams();
void   MapSettingsPage();
void   handleSettings(AsyncWebServerRequest *request);
void   WT_SETUP_Settings();
void   SendSettingsPageValues();
void   UpdateSettingsPageValues(uint32_t clientId, JsonObject *root);
void   handleDisableSystem(AsyncWebServerRequest *request);
void   handleEnableSystem(AsyncWebServerRequest *request);
void   handleStartFilter(AsyncWebServerRequest *request);
void   handleStopFilter(AsyncWebServerRequest *request);
void   handleTankStartFilter(AsyncWebServerRequest *request);
void   handleTankStopFilter(AsyncWebServerRequest *request);
void   TankPageSel();
void   handleStartBorehole(AsyncWebServerRequest *request);
void   handleStopBorehole(AsyncWebServerRequest *request);
void   handlePumpStart(AsyncWebServerRequest *request);
void   handlePumpStop(AsyncWebServerRequest *request);
void   handlePumpStatus(AsyncWebServerRequest *request);
void   webResetASPButton();
void   handleResetASP(AsyncWebServerRequest *request);
void   webWifiEditButton();
void   handleRemoveWifi(AsyncWebServerRequest *request);
void   appendSettingsActionButtons();

// ─── 13_TestValves.cpp ────────────────────────────────────────────────────────
void   handleTestValves(AsyncWebServerRequest *request);
void   handleToggleValve(AsyncWebServerRequest *request);

// ─── 16_Ntfy.cpp ──────────────────────────────────────────────────────────────
void   CreateNtfyParams();
void   MapNtfyPage();
void   handleNtfy(AsyncWebServerRequest *request);
void   WT_SETUP_Ntfy();
void   SendNtfyPageValues();
void   UpdateNtfyPageValues(uint32_t clientId, JsonObject *root);
void   SetupNTFY();
void   MessageNTFY(String message);
void   MessageNTFY2(String message);
void   UpdateNtfyPageLabels();
void   appendNtfyTestButton();
void   handleTestNtfy(AsyncWebServerRequest *request);
void   handleTestNtfy2(AsyncWebServerRequest *request);

// ─── 05_WiFi.cpp ──────────────────────────────────────────────────────────────
bool   SetupWiFi();
void   ClearWifiSettings();
void   handleWifiNets(AsyncWebServerRequest *request);
void   handleWifiNetAdd(AsyncWebServerRequest *request);
void   handleWifiNetDelete(AsyncWebServerRequest *request);
void   MaintainWifiNets();

// ─── 22_Update.cpp ────────────────────────────────────────────────────────────
void   handleFwCheck(AsyncWebServerRequest* r);
void   handleFwInstall(AsyncWebServerRequest* r);
void   handleFwUploadData(AsyncWebServerRequest* request, String filename, size_t index, uint8_t* data, size_t len, bool final);
void   handleFwUploadDone(AsyncWebServerRequest* request);
void   MaintainFwUpdate();
void   SendFwValues();
void   appendFwUpdateBlock();

// ─── 15_About.cpp ─────────────────────────────────────────────────────────────
void   SetupAbout();
void   SetupOTA();
void   handleAbout(AsyncWebServerRequest *request);

// ─── main.cpp ─────────────────────────────────────────────────────────────────
void PrintTestMsg(String Message, uint32_t seconds);
void TestOutPins();
void TestInPins();
void TestingTask(void *pvParameters);

// ─── 17_RainGauge.cpp ─────────────────────────────────────────────────────────
void   SetupRainGauge();
void   RainGaugeTask(void *pvParameters);
void   MaintainRainGauge();
bool   AnyZoneRespectsRain();
