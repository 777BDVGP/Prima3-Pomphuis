// =============================================================================
// 08_Home.cpp — Home Page Route Handler & Real-Time Status Maintenance
// =============================================================================
// Builds the home-page HTML and maintains the nine real-time status rows that
// are pushed to all connected browsers via WebSocket every 10 ms.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// handleRoot  (GET /)
// Allocates the HomeList PageVar array on first call, then builds and serves
// the home page.  PageBuf is used for the WT table to avoid heap fragmentation
// from repeated String concatenation.
// -----------------------------------------------------------------------------
void handleRoot(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	// SaveGenReport("handleRoot " + String(ESP.getFreeHeap())); //BDV TAKEOUT

	if (!MainPageObject) MapHomePage();   // One-time PageVar list initialisation

	WT_HomePage();
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_HOME);               
	appendHtmlStr(webSocketScript);           
	appendHtmlBuf(PageBuf, getPageBufLen());  
	appendHtmlStr(webHTMLEnd);

	sendHtmlPage(request);

}

// -----------------------------------------------------------------------------
// appendMainPageSel
// Builds the top navigation <ul> menu shared by HOME, IRRIGATION, TANK,
// REPORTS, SETTINGS, and FILES.  The active page is rendered as a non-clickable
// <lh> element; all other tabs are rendered as clickable <li> links.
// -----------------------------------------------------------------------------
void appendMainPageSel(int Index){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == PS_HOME       ? "<a class='act'>HOME</a>"        : "<a href='/'>HOME</a>");
	appendHtmlStr(Index == PS_IRRIGATION ? "<a class='act'>IRRIGATION</a>"  : "<a href='/irrigation'>IRRIGATION</a>");
	appendHtmlStr(Index == PS_TANK       ? "<a class='act'>TANK</a>"        : "<a href='/tank'>TANK</a>");
	appendHtmlStr(Index == PS_REPORTS    ? "<a class='act'>REPORTS</a>"     : "<a href='/report'>REPORTS</a>");
	appendHtmlStr(Index == PS_SETTINGS   ? "<a class='act'>SETTINGS</a>"    : "<a href='/settings'>SETTINGS</a>");
	appendHtmlStr(Index == PS_ABOUT      ? "<a class='act'>ABOUT</a>"       : "<a href='/about'>ABOUT</a>");
	// TEST VALVES is only shown while the system is disabled (NORMAL_OPERATION
	// false) — this is the same condition that shows "ENABLE SYSTEM" in the
	// settings action buttons. Keeps the manual valve test unreachable while
	// automatic control is live, since nothing gates ElectricValves against a
	// manual toggle fighting a real irrigation/tank-fill cycle.
	if (!NORMAL_OPERATION)
		appendHtmlStr(Index == PS_TESTVALVES ? "<a class='act'>TEST VALVES</a>" : "<a href='/testvalves'>TEST VALVES</a>");
	appendHtmlStr(Index == PS_NTFY       ? "<a class='act'>NTFY</a>"        : "<a href='/ntfy'>NTFY</a>");
	appendHtmlStr("<a href='/files'>FILES</a>");
	appendHtmlStr("</div>");
}

// -----------------------------------------------------------------------------
// WT_HomePage
// Writes the home-page WT parameter table into PageBuf.
// Produces a two-column table: description label | live value (coloured cell).
// The input column is empty (home page is read-only; values are WebSocket-pushed).
// -----------------------------------------------------------------------------
void WT_HomePage(){
	resetPageBuf();
	WT_StartPage();
	WT_StartTable("DESCRIPTIONS", "INSTANT VALUES", "");

	for (int i = 0; i < PAGE_VAR_HOME; i++){
		WT_DisplayRow(&HomeList[i]);
	}

	WT_EndPage();
}

// -----------------------------------------------------------------------------
// MapHomePage
// Allocates the HomeList[] PageVar array and binds each entry to its
// corresponding live status string and colour-attribute pointer.
// Called once on the first GET / request; MainPageObject prevents re-entry.
// -----------------------------------------------------------------------------
void MapHomePage(){
	MainPageObject = true;
	HomeList = new PageVar[PAGE_VAR_HOME];
	String DefaultStyle = F("color: black; font-size:14px");

	for (int i = 0; i < PAGE_VAR_HOME; i++){
		switch (i){
			case HOME_CONTROLLERST:
				HomeList[i].Var = new PageElement(&StrControllerStatus,      F("Controller Status:"),       DefaultStyle, &StrControllerStatusAtt,     i + ID_HOME); break;
			case HOME_TANKFILLIRRG:
				HomeList[i].Var = new PageElement(&StrTankFillIrrigation,    F("Tank Fill/Irrigation activity:"),    DefaultStyle, &StrTankFillIrrigationAtt,   i + ID_HOME); break;
			case HOME_TANKLEVEL:
				HomeList[i].Var = new PageElement(&StrTankLevel,             F("Tank Level:"),              DefaultStyle, &StrTankLevelAtt,             i + ID_HOME); break;
			case HOME_TANKFLOATSTA:
				HomeList[i].Var = new PageElement(&StrTankFloatStatus,       F("Tank Float Status:"),       DefaultStyle, &StrTankFloatStatusAtt,       i + ID_HOME); break;
			case HOME_BOREHOLESTA:
				HomeList[i].Var = new PageElement(&StrBoreholeStatus,        F("Borehole Pump Permission:"), DefaultStyle, &StrBoreholeStatusAtt,        i + ID_HOME); break;
			case HOME_BOREHOLEOPER:
				HomeList[i].Var = new PageElement(&StrBoreholeOperation,     F("Borehole Operation:"),      DefaultStyle, &StrBoreholeOperationAtt,     i + ID_HOME); break;
			case HOME_FILTERPUMPAC:
				HomeList[i].Var = new PageElement(&StrFilterPumpActivity,    F("Tank Filter Pump Activity:"), DefaultStyle, &StrFilterPumpActivityAtt,    i + ID_HOME); break;
			case HOME_FILTERPRESSU:
				HomeList[i].Var = new PageElement(&StrFilterPressure,        F("Tank Filter Pressure:"),    DefaultStyle, &StrFilterPressureAtt,        i + ID_HOME); break;
			case HOME_BOREHOLERETR:
				HomeList[i].Var = new PageElement(&StrBoreholeRetry,         F("Borehole Retry Time:"),     DefaultStyle, &StrBoreholeRetryAtt,         i + ID_HOME); break;
			case HOME_SUSPENDSTAT:
				HomeList[i].Var = new PageElement(&StrSuspendStatus,         F("Irrigation Suspend Status:"), DefaultStyle, &StrSuspendStatusAtt,         i + ID_HOME); break;
			// case HOME_RAININLASTDA:
			// 	HomeList[i].Var = new PageElement(&StrRaininlastdays, F("Rain in last 2 days:"), DefaultStyle, &StrRaininlastdaysAtt, i+ID_HOME); break;
		}
	}
}

// -----------------------------------------------------------------------------
// webRebootButton
// Returns an HTML button that calls ButtonReboot() via WebSocket when clicked.
// Rendered on the Settings page as a quick-access soft reboot control.
// -----------------------------------------------------------------------------
void webRebootButton()
{
	appendHtmlStr("<div><br></div><div><b><a>");
	appendHtmlStr("<b><a><input type=\"button\" id=\"REBOOT\" onclick=\"ButtonReboot()\" style=\"font-size:14px;background-color:#dc2626;color:#fff;border:none;padding:6px 14px;border-radius:4px;cursor:pointer\" value=\"REBOOT\"</a></b></div>");
}

// -----------------------------------------------------------------------------
// SendHomePageValues  (called every 10 ms from MaintainWebPageValues)
// Iterates the HomeList[] array and pushes any variable whose value has changed
// since the last send to all connected WebSocket clients.
// UpdateLast() snapshots the new value to prevent re-sending unchanged data.
// -----------------------------------------------------------------------------
void SendHomePageValues(){
	for (int i = 0; i < PAGE_VAR_HOME; i++){
		if (HomeList[i].Var->HasChanged()){
			webParamUpdate(HomeList[i].Var->GetName(), HomeList[i].Var->GetID("V"), HomeList[i].Var->GetVarStyle());
			HomeList[i].Var->UpdateLast();
		}
	}
}

// -----------------------------------------------------------------------------
// MaintainHomePage  (called every 10 ms from loop() via MaintainWebService())
// Evaluates current system state and updates all StrXxx / StrXxxAtt string
// pairs so that SendHomePageValues() can detect changes and push them.
// Each section is commented with the row name it drives.
// -----------------------------------------------------------------------------
void MaintainHomePage(){

	// ── Boot stabilisation delay ──────────────────────────────────────────────
	// TIM_STABILIZE_DELAY is activated in setup() just before tasks launch.
	// While it is still running, return early so all home-page rows stay blank.
	// This prevents the browser seeing transient wrong values during early boot.
	if (!HomePageReady) {
		if (!TimerExpired(TIM_STABILIZE_DELAY)) return;
		HomePageReady = true;
	}

	// ── Controller Status ─────────────────────────────────────────────────────
	// Green = running normally; Red = NORMAL_OPERATION disabled
	if (NORMAL_OPERATION){
		StrControllerStatus    = HOME_CONTROL_ACTIVE;
		StrControllerStatusAtt = ATT_GREEN;
	}
	else{
		StrControllerStatus    = HOME_CONTROL_NACTIVE;
		StrControllerStatusAtt = ATT_RED;
	}

	// ── Tank Fill / Irrigation Activity ──────────────────────────────────────
	// Displays the name of the active zone plus elapsed time HH:MM:SS, or the
	// tank-fill elapsed time, or the manual pump elapsed time.
	if (IrriActivityLine >= 0){
		unsigned long elapsedSec = (millis() - IrriActivityStartMs) / 1000;
		StrTankFillIrrigation    = "Irrigate " + IrriObject->Selection[IrriActivityLine].SelectDescription + " " + fmtDur(elapsedSec);
		StrTankFillIrrigationAtt = ATT_YELLOW;
	}
	else if (BoreholeStartedByTank){
		unsigned long elapsedSec = (millis() - TankFillStartMs) / 1000;
		StrTankFillIrrigation    = "Busy Filling " + fmtDur(elapsedSec);
		StrTankFillIrrigationAtt = ATT_YELLOW;
	}
	else if (MANUAL_STARTED){
		unsigned long elapsedSec = (millis() - ManualStartMs) / 1000;
		StrTankFillIrrigation    = "Manual " + fmtDur(elapsedSec);
		StrTankFillIrrigationAtt = ATT_ORANGE;
	}
	else{
		StrTankFillIrrigation    = "No Action";
		StrTankFillIrrigationAtt = ATT_GREEN;
	}

	// ── Tank Level ────────────────────────────────────────────────────────────
	// Red when below the fill start threshold; green otherwise.
	StrTankLevel = String(TankWaterLevel->getValue()) + "%";
	StrTankLevelAtt = (TankWaterLevel->getValue() < TankWaterLevel->StartFillLevel) ? ATT_RED : ATT_GREEN;

	// ── Tank Float Switch Status ──────────────────────────────────────────────
	if (InPort[IIDX_TANK_FULL].Input){
		StrTankFloatStatus    = "Full";
		StrTankFloatStatusAtt = ATT_GREEN;
	}
	else{
		StrTankFloatStatus    = "Low";
		StrTankFloatStatusAtt = ATT_RED;
	}

	// ── Borehole Enable / Disable Status ─────────────────────────────────────
	StrBoreholeStatus    = TankWaterLevel->BoreholOperation;
	StrBoreholeStatusAtt = (TankWaterLevel->BoreholOperation == TANK_BORHOLE_ENABLE) ? ATT_GREEN : ATT_RED;

	// ── Borehole Operation — who is currently pumping ─────────────────────────
	{
		bool user2Now = InPort[IIDX_ME_PUMP_BH].Input;
		bool user1Now = InPort[IIDX_OTHER_PUMP_HB].Input;

		// Edge detection — capture start times
		if (user2Now && !PrevUser2Pumping)  User2StartMs = millis();
		if (user1Now && !PrevUser1Pumping)  User1StartMs = millis();

		// User_1 stopped — log elapsed time to IrrigationLog.txt
		if (!user1Now && PrevUser1Pumping) {
			unsigned long durSec = (millis() - User1StartMs) / 1000;
			SaveIrrigReport(TankWaterLevel->User_1 + " Pumping: Dur: " + fmtDur(durSec));
		}

		PrevUser2Pumping = user2Now;
		PrevUser1Pumping = user1Now;

		// Initialise inactivity timers on first call (gives a clean window from boot)
		if (LastUser2PumpMs == 0) LastUser2PumpMs = millis();
		if (LastUser1PumpMs == 0) LastUser1PumpMs = millis();

		if (!user2Now && !user1Now) {
			StrBoreholeOperation    = "Not Pumping";
			StrBoreholeOperationAtt = ATT_GREEN;
			NtfyBoreholeMaxSent      = false;   // reset when nobody is pumping
			NtfyBoreholeMaxSentUsr1  = false;
		}
		else if (user2Now) {
			unsigned long durSec = (millis() - User2StartMs) / 1000;
			StrBoreholeOperation    = TankWaterLevel->User_2 + " is pumping " + fmtDur(durSec);
			StrBoreholeOperationAtt = ATT_YELLOW;
			LastUser2PumpMs              = millis();   // keep inactivity timer fresh while pump runs
			NtfyTankFillOverdueSentUsr2  = false;      // allow re-alert after next inactivity period
			if (NtfyData->EnableMaxPumpNotifyUsr2 &&
			    durSec >= (unsigned long)NtfyData->BoreholeMaxTime * 60 && !NtfyBoreholeMaxSent) {
				NtfyBoreholeMaxSent = true;
				MessageNTFY(TankWaterLevel->User_2 + " pumping > " +
				            String(NtfyData->BoreholeMaxTime) + " min on " + Params->SysName);
				if (NtfyData->RebootOnMaxPumpUsr2) {
					SaveGenReport("REBOOT - Max pumping time exceeded (" + TankWaterLevel->User_2 + ")");
					delay(1000);   // give the NTFY queue a chance to hand off to NtfyTask before reboot
					ESP.restart();
				}
			}
		}
		else {
			unsigned long durSec = (millis() - User1StartMs) / 1000;
			StrBoreholeOperation        = TankWaterLevel->User_1 + " is pumping " + fmtDur(durSec);
			StrBoreholeOperationAtt     = ATT_RED;
			LastUser1PumpMs             = millis();   // keep inactivity timer fresh while pump runs
			NtfyTankFillOverdueSentUsr1 = false;      // allow re-alert after next inactivity period
			if (NtfyData->EnableMaxPumpNotifyUsr1 &&
			    durSec >= (unsigned long)NtfyData->BoreholeMaxTimeUsr1 * 60 && !NtfyBoreholeMaxSentUsr1) {
				NtfyBoreholeMaxSentUsr1 = true;
				MessageNTFY2(TankWaterLevel->User_1 + " pumping > " +
				             String(NtfyData->BoreholeMaxTimeUsr1) + " min on " + NtfyData->SysNameUsr1);
			}
		}

		// Inactivity alert — fire once when User 2 pump has been absent for TankFillMaxHrsUsr2
		if (NtfyData->EnableTankFillNotifyUsr2 &&
		    !user2Now && !NtfyTankFillOverdueSentUsr2 &&
		    (millis() - LastUser2PumpMs) > (unsigned long)NtfyData->TankFillMaxHrsUsr2 * 3600UL * 1000UL) {
			NtfyTankFillOverdueSentUsr2 = true;
			MessageNTFY("Tank fill overdue: " + TankWaterLevel->User_2 +
			            " not pumped in " + String(NtfyData->TankFillMaxHrsUsr2) +
			            "h on " + Params->SysName);
		}

		// Inactivity alert — fire once when User 1 pump has been absent for TankFillMaxHrsUsr1
		if (NtfyData->EnableTankFillNotifyUsr1 &&
		    !user1Now && !NtfyTankFillOverdueSentUsr1 &&
		    (millis() - LastUser1PumpMs) > (unsigned long)NtfyData->TankFillMaxHrsUsr1 * 3600UL * 1000UL) {
			NtfyTankFillOverdueSentUsr1 = true;
			MessageNTFY2("Tank fill overdue: " + TankWaterLevel->User_1 +
			             " not pumped in " + String(NtfyData->TankFillMaxHrsUsr1) +
			             "h on " + NtfyData->SysNameUsr1);
		}
	}

	// ── Filter Pump Activity ──────────────────────────────────────────────────
	// Shows "Busy Circulating" when borehole is running, a countdown when the
	// post-fill delay is active, or "Not Circulating" when the pump is off.
	{
		bool filterOn       = OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat;
		bool boreholeRunning = digitalRead(OutPort[OIDX_BOREHOLE].Port);   // HIGH = running (active-high relay)

		if (filterOn){
			if (boreholeRunning){
				StrFilterPumpActivity    = "Busy Circulating";
				StrFilterPumpActivityAtt = ATT_YELLOW;
			}
			else{
				// Show countdown regardless of TankIsFull — Phase 11 now stops the
				// filter based on timer + borehole state alone, not fill-level state.
				int    remSec    = RemainingSec(TIM_TANK_ON_DELAY);
				String countdown = String(remSec / 60) + ":" + (remSec % 60 < 10 ? "0" : "") + String(remSec % 60);
				StrFilterPumpActivity    = "Circulating - Off in " + countdown;
				StrFilterPumpActivityAtt = ATT_YELLOW;
			}
		}
		else{
			StrFilterPumpActivity    = "Not Circulating";
			StrFilterPumpActivityAtt = ATT_GREEN;
		}
	}

	// ── Filter Pressure ───────────────────────────────────────────────────────
	// Only meaningful when the filter pump is running; show "Filter Off" otherwise.
	if (OutPort[OIDX_FILTER_PUMP_RELAY].OUTStat){
		StrFilterPressure    = String(FilterSensor->getValue()) + " kPa";
		StrFilterPressureAtt = FilterSensor->FilterPressureExceeded ? ATT_RED : ATT_GREEN;
	}
	else{
		StrFilterPressure    = "Filter Off";
		StrFilterPressureAtt = ATT_GREEN;
	}

	// ── Borehole Retry Countdown ──────────────────────────────────────────────
	// Shows seconds remaining before the next borehole start attempt is allowed.
	if (Timer[TIM_BOREHOLE_RETRY].Status){
		int retrySec = RemainingSec(TIM_BOREHOLE_RETRY);
		StrBoreholeRetry    = "Retry in " + String(retrySec) + " Sec";
		StrBoreholeRetryAtt = ATT_ORANGE;
	}
	else{
		StrBoreholeRetry    = "None";
		StrBoreholeRetryAtt = ATT_GREEN;
	}

	// ── Suspend Status ────────────────────────────────────────────────────────
	if (SUSPEND_IRRI){
		StrSuspendStatus    = "No Irrigation Today";
		StrSuspendStatusAtt = ATT_YELLOW;
	}
	else{
		StrSuspendStatus    = "Normal Irrigation";
		StrSuspendStatusAtt = ATT_GREEN;
	}

	// ── Rain in Last 2 Days (not fitted) ─────────────────────────────────────
	StrRaininlastdays    = " ";
	StrRaininlastdaysAtt = ATT_WHITE;
}