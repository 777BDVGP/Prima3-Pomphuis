// =============================================================================
// 13_TestValves.cpp — Manual Per-Valve Test Page Route Handlers
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// renderTestValvesPage
// Shared page renderer for both routes below. Does NOT touch valve state —
// callers apply whatever state change they need first, then call this to
// paint the current ElecValves->WaterValve[k].RequiredState of each valve.
// (OutPort[k].OUTStat is not usable here: MaintainValveConfig() drives the
// physical pin straight from RequiredState and never mirrors it back into
// OutPort[].OUTStat for the valve indices, so RequiredState is the only
// reliably-maintained "current state" for a valve.)
// -----------------------------------------------------------------------------
static void renderTestValvesPage(AsyncWebServerRequest *request) {
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendMainPageSel(PS_TESTVALVES);
	appendHtmlStr(webSocketScript);

	appendHtmlStr("<div class='btns'>");
	for (int k = 0; k < VALVE_COUNT; k++) {
		bool isOpen = (ElecValves->WaterValve[k].RequiredState == VALVE_OPEN);
		String row  = "<a class='valve-btn ";
		row += isOpen ? "valve-open" : "valve-closed";
		row += "' href='/togglevalve?idx=" + String(k) + "'>";
		row += TestValveLabel[k];
		row += "</a>";
		appendHtmlStr(row.c_str());
	}
	appendHtmlStr("</div>");

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// handleTestValves  (GET /testvalves)
// Only reachable while NORMAL_OPERATION is false — redirects to /settings
// otherwise, so this page can never be opened while automatic control is
// live. Every fresh entry forces all valves back to the Default configuration
// before rendering, per spec.
// -----------------------------------------------------------------------------
void handleTestValves(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;

	if (NORMAL_OPERATION) {
		request->redirect("/settings");
		return;
	}

	ElecValves->SwitchValveConfig(CONFIG_DEF, VALVE_OPEN);   // Force Default position on entry
	renderTestValvesPage(request);
}

// -----------------------------------------------------------------------------
// handleToggleValve  (GET /togglevalve?idx=0..6)
// Flips one physical valve directly, bypassing SwitchValveConfig() (which only
// ever drives all 7 valves at once from a named configuration — there is no
// per-valve API). RequiredState/UpdateTime are updated too so ElectricValves'
// own bookkeeping stays consistent; UpdateTime is left in the past so
// MaintainValveConfig() will not re-touch this pin on its next 50 ms tick.
// Refused (redirect) unless NORMAL_OPERATION is false, same as handleTestValves().
// -----------------------------------------------------------------------------
void handleToggleValve(AsyncWebServerRequest *request) {
	if (!beginHtmlStream(request)) return;

	if (NORMAL_OPERATION || !request->hasParam("idx")) {
		request->redirect("/settings");
		return;
	}

	int idx = request->getParam("idx")->value().toInt();
	if (idx >= 0 && idx < VALVE_COUNT) {
		bool newState = !ElecValves->WaterValve[idx].RequiredState;
		digitalWrite(OutPort[idx].Port, newState);
		OutPort[idx].OUTStat = newState;                         // keep mirror in sync per project convention
		ElecValves->WaterValve[idx].RequiredState = newState;
		ElecValves->WaterValve[idx].UpdateTime    = millis();
	}

	renderTestValvesPage(request);
}
