// =============================================================================
// 04_WebPages.cpp — Web Service Maintenance, Server Setup & Page Builder Utilities
// =============================================================================
// Implements MaintainWebService() (driven from loop(), see main.cpp),
// AsyncWebServer and WebSocket event handling, JSON payload parsing,
// status-message management, the page-header builder, and the complete set
// of WT_* table-builder primitives used by every configuration page to
// generate dynamic JavaScript tables.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// MaintainWebService  (called every pass of loop(), self-timed internally)
// Drives all web-related maintenance: cleans up stale WebSocket connections,
// pushes any changed variable values to connected browsers, refreshes the
// displayed date/time, and updates the home-page status indicators.
// -----------------------------------------------------------------------------
void MaintainWebService() {
	static uint32_t lastOTACheck  = 0;   // Throttle ArduinoOTA.handle() to 5 Hz
	static uint32_t lastWebPage   = 0;   // Throttle MaintainWebPage() to 20 Hz
	static uint32_t lastHomePage  = 0;   // Throttle MaintainHomePage() to 2 Hz
	uint32_t now = millis();

	// ArduinoOTA discovery only needs polling at ~5 Hz — no benefit at 100 Hz
	if (now - lastOTACheck >= 200) {
		lastOTACheck = now;
		ArduinoOTA.handle();
	}

	if (!OTAInProgress) {
		// String scanning and WebSocket client cleanup only need to run at 20 Hz —
		// running at 100 Hz burns CPU on String comparisons every cycle.
		if (now - lastWebPage >= 50) {
			lastWebPage = now;
			MaintainWebPage();   // Clean up clients, push changed values (04_WebPages.cpp)
		}
		UpdateWebPageTime();     // Refresh the date/time display every second (02_Time.cpp)

		// Home-page status strings only need refreshing at 2 Hz —
		// running at 100 Hz creates String objects on every cycle, causing heap churn.
		if (now - lastHomePage >= 500) {
			lastHomePage = now;
			MaintainHomePage();  // Refresh home-page status strings (08_Home.cpp)
		}
	}
}

// -----------------------------------------------------------------------------
// beginHtmlStream — Layer 1 browse-time heap guard.
// Called at the top of every page handler before any page-building work.
// Checks two heap conditions:
//   1. ESP.getFreeHeap()     < HEAP_FREE_MIN    (18 KB) — total free too low
//   2. ESP.getMaxAllocHeap() < HEAP_MAXBLK_MIN  (10 KB) — heap too fragmented
// If either fails: sends HTTP 503 with a 1-second auto-retry meta tag so the
// browser retries silently, then returns false.  The caller must return immediately.
// After 60 consecutive failures (~60 s) the device reboots to recover from
// persistent heap fragmentation that the browser retry loop cannot clear.
// Returns true when heap is healthy and page building may proceed.
// -----------------------------------------------------------------------------
bool beginHtmlStream(AsyncWebServerRequest *request)
{
	static uint32_t failCount = 0;
	uint32_t freeHeap    = ESP.getFreeHeap();
	uint32_t maxAlloc    = ESP.getMaxAllocHeap();
	if (freeHeap < HEAP_FREE_MIN || maxAlloc < HEAP_MAXBLK_MIN) {
		failCount++;
		SaveGenReport("503: heap=" + String(freeHeap) + " maxBlk=" + String(maxAlloc));
		request->send(503, "text/html",
		    "<html><head><meta http-equiv='refresh' content='1'></head>"
		    "<body>Low memory &mdash; retrying&hellip;</body></html>");
		if (failCount >= 60) {
			SaveGenReport("REBOOT - Heap fragmentation: " + String(failCount) + " consecutive 503s");
			delay(200);
			ESP.restart();
		}
		return false;
	}
	failCount = 0;
	return true;
}

// -----------------------------------------------------------------------------
// SetupWebPage
// Registers all HTTP routes, attaches the WebSocket event handler, and starts
// the AsyncWebServer.  Called once from setup() after WiFi connects.
// -----------------------------------------------------------------------------
void SetupWebPage(){
	AddRoutes();                   // Register all HTTP route handlers (05_ThisSystem.cpp)
	ws.onEvent(webSocketEvent);    // Attach WebSocket event dispatcher
	server.addHandler(&ws);
	server.begin();
}

// -----------------------------------------------------------------------------
// MaintainWebPage  (called every 10 ms from loop() via MaintainWebService())
// Performs periodic web-layer housekeeping:
//   - Removes clients that have disconnected without a proper close frame.
//   - Detects changed PageVar values and pushes updates to all browsers.
//   - Resets the H03 status message back to "Selection Option" after its timer.
// -----------------------------------------------------------------------------
void MaintainWebPage(){
	ws.cleanupClients();          // Release memory for disconnected WebSocket clients
	MaintainWebPageValues();      // Scan all PageVar lists for changes (05_ThisSystem.cpp)
	UpdateSelectMessDelay();      // Auto-reset the H03 status message if the timer expired
}

// -----------------------------------------------------------------------------
// webSocketEvent  (WebSocket event callback)
// Dispatched by the AsyncWebSocket library for every client event.
// WS_EVT_CONNECT  — Sends the system name and current time to the new client.
// WS_EVT_DATA     — Passes complete text frames to UnpackJsonPayload().
// WS_EVT_DISCONNECT / WS_EVT_ERROR / WS_EVT_PONG — No action required.
// -----------------------------------------------------------------------------
void webSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){

	switch (type){
		case WS_EVT_DISCONNECT:
		break;

		case WS_EVT_CONNECT: {
			// Push current system name and date/time to the newly connected browser
			String ActTime = DateTime;
			webParamUpdate(Params->SysName, "H01", "");
			webParamUpdate(ActTime,         "H02", "");
			IrrigationOverviewSync = true;   // Force irrigation button resync to new client
		}
		break;

		case WS_EVT_DATA: {
			AwsFrameInfo *info = (AwsFrameInfo*)arg;
			// Only process complete, single-frame text messages
			if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT){
				data[len] = 0;   // Null-terminate for safe JSON parsing
				UnpackJsonPayload(client->id(), data, len);
			}
		}
		break;

		case WS_EVT_ERROR:
		case WS_EVT_PONG:
		break;
	}
}

// -----------------------------------------------------------------------------
// webParamUpdate
// Broadcasts a single key/value pair (and optional attribute change) to all
// connected WebSocket clients as a compact JSON message.
// The browser's Socket.onmessage handler updates the DOM element whose id
// matches 'ID', then applies setAttribute(ATTName, ATT) if ATT is non-empty.
// Parameters:
//   Name    — The new value to display (string, number, or "true"/"false")
//   ID      — HTML element id (e.g. "V350", "H02")
//   ATT     — Attribute value to set; empty string = no attribute change
//   ATTName — Attribute name (default "style"); pass e.g. "class" or "min" for others
// -----------------------------------------------------------------------------
void webParamUpdate(const String& Name, const String& ID, const String& ATT, const char* ATTName){
	if (ws.count() > 0){
		StaticJsonDocument<256> doc;
		doc[ID] = Name;
		if (ATT.length() > 0){
			doc["Attribute"] = ATT;
			if (strcmp(ATTName, "style") != 0) doc["AttributeName"] = ATTName;
		}
		char buf[256];
		serializeJson(doc, buf, sizeof(buf));
		ws.textAll(buf);
	}
}

// -----------------------------------------------------------------------------
// UnpackJsonPayload
// Parses an incoming WebSocket JSON payload sent by the browser's UpdatePage()
// function.  The first numeric key in the object identifies which page the
// update originates from (matched against ID_* constants), allowing the correct
// page-specific update handler to be called.
// Key 0 handles global actions (REBOOT, and reserved MAN1/MAN2 stubs).
// -----------------------------------------------------------------------------
void UnpackJsonPayload(uint32_t clientId, uint8_t *payload, size_t length){
	DynamicJsonDocument doc((length + 500));

	DeserializationError error = deserializeJson(doc, payload);
	if (error){
		SaveGenReport F("Error parsing JSON payload");
		return;
	}

	JsonObject root = doc.as<JsonObject>();

	// Determine the originating page from the first numeric key in the object
	JsonObject::iterator it = root.begin();
	String firstKey = it->key().c_str();
	int WEBPAGE = firstKey.toInt();   // Matches an ID_* constant for the relevant page

	switch (WEBPAGE)
	{
		case ID_HOME:
			// Home page is read-only — no incoming updates expected
			break;

		case ID_SETTINGS:
			UpdateSettingsPageValues(clientId, &root);
			break;

		case ID_SUSPEND:
			if (SuspendObject) UpdateSuspendPageValues(clientId, &root);
			break;

		case ID_VALVES:
			if (ElectricValvesObject) UpdateWaterValvesPageValues(clientId, &root);
			break;

		case ID_VALCONF:
			if (ElectricValvesObject) UpdateConfigPageValues(clientId, &root);
			break;

		case ID_IRRISETUP:
			if (IrrigSetupObject) UpdateIrrigSetupPageValues(clientId, &root);
			break;

		case ID_TANK:
			if (TankPageObject) UpdateTankPageValues(clientId, &root);
			break;

		case ID_NTFY:
			if (NtfyPageObject) UpdateNtfyPageValues(clientId, &root);
			break;

		case 0:
			// Global action keys — not tied to a specific page number
			for (JsonPair kv : root){
				if (kv.key() == "MAN1"){
					// Reserved: manual pump control for User 1 (not yet implemented)
				}
				else if (kv.key() == "MAN2"){
					// Reserved: manual pump control for User 2 (not yet implemented)
				}
				else if (kv.key() == "REBOOT" && kv.value() == "YES"){
					DEBUG_PRINTLN("Manual reboot requested via WebSocket");
					WebMessUpdate(clientId, "Rebooting — communications will be lost!", "color: red", true);
					SaveGenReport F("REBOOT - Manual reboot via web interface");
					delay(500);
					ESP.restart();
				}
			}
			break;
	}
}

// -----------------------------------------------------------------------------
// WebMessUpdate
// Pushes a status message to the H03 element on all connected browsers.
// When Reset = true, records the client ID and starts TIM_RESET_OPTION_MESS
// so that UpdateSelectMessDelay() can restore the default message after 2 s.
// -----------------------------------------------------------------------------
void WebMessUpdate(uint32_t clientId, const String& Mess, const String& Att, bool Reset){
	webParamUpdate(Mess, "H03", Att);
	if (Reset){
		Lastnum        = clientId;
		LastnumUpdated = false;
		activateTimer(TIM_RESET_OPTION_MESS);
	}
}

// -----------------------------------------------------------------------------
// UpdateSelectMessDelay  (called every 10 ms from MaintainWebPage)
// Restores the H03 status message to "Selection Option" once TIM_RESET_OPTION_MESS
// expires.  Only acts when a timed reset is pending (LastnumUpdated == false).
// -----------------------------------------------------------------------------
void UpdateSelectMessDelay(){
	if (!LastnumUpdated){
		if (TimerExpired(TIM_RESET_OPTION_MESS)){
			WebMessUpdate(Lastnum, "Selection Option", "color:#3366ff", false);
			LastnumUpdated = true;
		}
	}
}

// -----------------------------------------------------------------------------
// append_FileAction
// Builds the three-line page header block present on every web page:
//   H01 — System name (pushed live via WebSocket)
//   H02 — Date and time (pushed live via WebSocket)
//   H03 — Status message in the colour matching ReqMess (or SetMess if overridden)
// SetMess is cleared to 0 after use so the override only applies once.
// -----------------------------------------------------------------------------
void append_FileAction(int ReqMess)
{
	const char* Message;
	const char* LColor;
	int ActMess = ReqMess;
	if (SetMess != 0) ActMess = SetMess;   // Route handler may override the default message

	switch (ActMess)
	{
		case FA_MESS_01: Message = "Files on SPIFFS";                    LColor = COLOR_DEFAULT; break;
		case FA_MESS_02: Message = "Click on File to Download";          LColor = COLOR_BLUE;    break;
		case FA_MESS_03: Message = "Upload a File";                      LColor = COLOR_BLUE;    break;
		case FA_MESS_04: Message = "Click on File to Delete";            LColor = COLOR_BLUE;    break;
		case FA_MESS_06: Message = "Selection Option";                   LColor = COLOR_BLUE;    break;
		case FA_MESS_07: Message = "Files on SPIFFS";                    LColor = COLOR_BLUE;    break;
		case FA_MESS_08: Message = "Select Report";                      LColor = COLOR_BLUE;    break;
		case FA_MESS_09: Message = "File Not Found";                     LColor = COLOR_RED;     break;
		case FA_MESS_10: Message = "File GeneralLog.txt was deleted";    LColor = COLOR_RED;     break;
		case FA_MESS_11: Message = "File BoreholeUsedLog.txt was deleted";  LColor = COLOR_RED;     break;
		case FA_MESS_RED: Message = SpesialStr.c_str();                  LColor = COLOR_RED;     break;
		case FA_MESS_GRN: Message = SpesialStr.c_str();                  LColor = COLOR_GREEN;   break;
		default:          Message = "";                                  LColor = COLOR_BLUE;    break;
	}

	appendHtmlStr("<h2 id=\"H01\">"); appendHtmlStr(Params->SysName.c_str()); appendHtmlStr("</h2>");
	appendHtmlStr("<h4 id=\"H02\">"); appendHtmlStr(DateTime.c_str());         appendHtmlStr("</h4>");
	appendHtmlStr("<h4 id=\"H03\" style="); appendHtmlStr(LColor); appendHtmlStr(">"); appendHtmlStr(Message); appendHtmlStr("</h4>");

	SetMess = 0;   // Clear the one-shot override
}

// =============================================================================
// WT_* Table Builder Primitives
// =============================================================================
// These functions write fragments of a JavaScript 'parameters' array into
// PageBuf.  The browser's createLabelInputTable() function reads this array
// on page load and builds the HTML table dynamically, avoiding the overhead of
// generating full HTML on the ESP32.
//
// Each cell emits an 'attributes' array of {n, v} objects — the browser calls
// element.setAttribute(a.n, a.v) for each entry.  WT_Column always emits at
// least a style attribute; PageElement metadata (min/max/step/placeholder/
// readonly) and caller-supplied ExtraAttrs are appended automatically.
//
// Typical call sequences:
//   WT_EditRow(&list[i]);                                     // label + input
//   WT_DisplayRow(&list[i]);                                  // 2 labels (read-only row)
//   WT_AutoColumn(WT_INPUT, WT_FIRST, WT_LAST, &list[i]);    // single cell
// =============================================================================

// -----------------------------------------------------------------------------
// WT_StartTable
// Opens the parameters array and emits column header objects for up to three
// named columns.  Pass "" for Hdr2/Hdr3 to omit those columns.
// -----------------------------------------------------------------------------
void WT_StartTable(const char* Hdr1, const char* Hdr2, const char* Hdr3){
	WT_Header(Hdr1);
	if (Hdr2 && *Hdr2) WT_Header(Hdr2);
	if (Hdr3 && *Hdr3) WT_Header(Hdr3);
}

// -----------------------------------------------------------------------------
// WT_Column
// Emits one cell of a table row into PageBuf.  Handles label and input types,
// builds an 'attributes' array from Style, PageElement metadata, and ExtraAttrs,
// and generates <select> option lists when Type == "select".
//
// Parameters:
//   WT_Type    — WT_LABEL or WT_INPUT
//   WT_Begin   — WT_FIRST (open the labels/inputs array) or WT_NEXT (array already open)
//   WT_Finish  — WT_LAST (close array) or WT_MORE (keep open for next column)
//   Type       — HTML type string: "text", "number", "checkbox", "time", or "select"
//   Labval     — Display text (label) or current value (input); "" = emit empty array
//   ID         — HTML element id string
//   Style      — Base CSS style string; text-align from PageElement is merged in
//   ListObj    — PageVar entry (provides option list for <select> and attribute metadata)
//   ExtraAttrs — Optional comma-separated "name:value" pairs, e.g. "min:0,max:100"
// -----------------------------------------------------------------------------
void WT_Column(int WT_Type, int WT_Begin, int WT_Finish, const char* Type, const char* Labval, const char* ID, const char* Style, PageVar *ListObj, const char* ExtraAttrs){

	if (WT_Type == WT_LABEL) {
		if (!Labval || !*Labval) { WT_StartLabelEmpty(); return; }
		if (WT_Begin == WT_FIRST) WT_StartLabel();
		WT_TypeCell(Type);
		WT_TextLabel(Labval);
	} else if (WT_Type == WT_INPUT) {
		if (!Labval || !*Labval) { WT_StartInputEmpty(); return; }
		if (WT_Begin == WT_FIRST) WT_StartInput();
		WT_TypeCell(Type);
		WT_ValueInput(Labval);
	}

	WT_IDCell(ID);

	// Merge text-align into style (stack buffer, no heap)
	const char* effectiveStyle = Style ? Style : "";
	char fullStyleBuf[128];
	if (ListObj && ListObj->Var) {
		const String& ta = ListObj->Var->GetAttrTextAlign();
		if (ta.length() > 0) {
			if (Style && *Style)
				snprintf(fullStyleBuf, sizeof(fullStyleBuf), "%s;text-align:%s", Style, ta.c_str());
			else
				snprintf(fullStyleBuf, sizeof(fullStyleBuf), "text-align:%s", ta.c_str());
			effectiveStyle = fullStyleBuf;
		}
	}

	WT_AttrsStart();
	WT_Attr("style", effectiveStyle);

	// Input-specific HTML attributes from PageElement metadata (stack buffers, no heap)
	if (WT_Type == WT_INPUT && ListObj && ListObj->Var) {
		char numBuf[16];
		if (ListObj->Var->HasAttrMin())  { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrMin());  WT_Attr("min",  numBuf); }
		if (ListObj->Var->HasAttrMax())  { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrMax());  WT_Attr("max",  numBuf); }
		if (ListObj->Var->HasAttrStep()) { snprintf(numBuf, sizeof(numBuf), "%g", ListObj->Var->GetAttrStep()); WT_Attr("step", numBuf); }
		const String& ph = ListObj->Var->GetAttrPlaceholder();
		if (ph.length() > 0) WT_Attr("placeholder", ph.c_str());
		if (ListObj->Var->GetAttrReadOnly()) WT_Attr("readonly", "");
	}

	// Extra attributes: comma-separated "name:value" pairs parsed without heap
	if (ExtraAttrs && *ExtraAttrs) {
		const char* p = ExtraAttrs;
		while (*p) {
			const char* comma = strchr(p, ',');
			size_t pairLen = comma ? (size_t)(comma - p) : strlen(p);
			const char* colon = (const char*)memchr(p, ':', pairLen);
			if (colon && colon > p) {
				char aname[32], aval[64];
				size_t nLen = (size_t)(colon - p);
				size_t vLen = pairLen - nLen - 1;
				if (nLen < sizeof(aname) && vLen < sizeof(aval)) {
					memcpy(aname, p, nLen); aname[nLen] = '\0';
					memcpy(aval, colon + 1, vLen); aval[vLen] = '\0';
					WT_Attr(aname, aval);
				}
			}
			if (!comma) break;
			p = comma + 1;
		}
	}

	WT_AttrsEnd();

	if (Type && strcmp(Type, "select") == 0 && ListObj && ListObj->Var) {
		WT_OptionsStart();
		int cnt = ListObj->Var->GetOptionCount();
		for (int i = 0; i < cnt; i++) WT_OptionsValues(ListObj->Var->GetOption(i).c_str());
		WT_OptionsEnd();
	}

	if (WT_Finish == WT_MORE) WT_NextColumn();
	else                      WT_EndCell();
}

// -----------------------------------------------------------------------------
// WT_AutoColumn
// Convenience wrapper: derives Type, value, ID, and style from the PageVar so
// the caller does not need to repeat .Var->GetTypeStr() etc. at every call site.
// IDPrefix is prepended to the numeric ID — pass "L" for label IDs, "" for inputs.
// -----------------------------------------------------------------------------
void WT_AutoColumn(int WT_Type, int WT_Begin, int WT_Finish, PageVar *pv, const char* IDPrefix, const char* ExtraAttrs){
	String id = pv->Var->GetID(IDPrefix);   // one heap alloc for the computed ID string
	if (WT_Type == WT_LABEL) {
		WT_Column(WT_LABEL, WT_Begin, WT_Finish, "text",
		          pv->Var->GetDescription().c_str(), id.c_str(),
		          pv->Var->GetFStyle().c_str(), pv, ExtraAttrs);
	} else {
		String typeStr = pv->Var->GetTypeStr();  // computed type string
		String nameStr = pv->Var->GetName();     // current variable value as string
		WT_Column(WT_INPUT, WT_Begin, WT_Finish,
		          typeStr.c_str(), nameStr.c_str(),
		          id.c_str(), pv->Var->GetFStyle().c_str(),
		          pv, ExtraAttrs);
	}
}

// -----------------------------------------------------------------------------
// WT_DisplayRow
// Emits a complete read-only row: description label | live-value label | empty inputs.
// The value cell uses GetVarStyle() so it picks up dynamic colour changes pushed
// via WebSocket (home page pattern, repeated 6+ times across Tank and About pages).
// -----------------------------------------------------------------------------
void WT_DisplayRow(PageVar *pv){
	String idL = pv->Var->GetID("L");
	String idV = pv->Var->GetID("V");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_MORE, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(),
	          pv->Var->GetFStyle().c_str(), pv);
	WT_Column(WT_LABEL, WT_NEXT, WT_LAST, "text",
	          nameStr.c_str(), idV.c_str(),
	          pv->Var->GetVarStyle().c_str(), pv);
	WT_Column(WT_INPUT, WT_FIRST, WT_LAST, "", "", "", "", pv);
	WT_EndLine();
}

// -----------------------------------------------------------------------------
// WT_EditRow
// Emits a complete editable row: description label | input field.
// typeOverride replaces GetTypeStr() when non-empty (e.g. "time" for time fields).
// ExtraAttrs are forwarded to WT_Column (e.g. "min:0,max:100").
// -----------------------------------------------------------------------------
void WT_EditRow(PageVar *pv, const char* typeOverride, const char* ExtraAttrs){
	String typeStr;
	const char* typeToUse;
	if (typeOverride && *typeOverride) {
		typeToUse = typeOverride;
	} else {
		typeStr   = pv->Var->GetTypeStr();
		typeToUse = typeStr.c_str();
	}
	String idL   = pv->Var->GetID("L");
	String idIn  = pv->Var->GetID("");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(),
	          "", pv);
	WT_Column(WT_INPUT, WT_FIRST, WT_LAST, typeToUse,
	          nameStr.c_str(), idIn.c_str(),
	          pv->Var->GetFStyle().c_str(), pv, ExtraAttrs);
	WT_EndLine();
}

// -----------------------------------------------------------------------------
// WT_CheckEditRow — emits a 3-column row: label | checkbox | input field.
// cbPv  = the enable/disable boolean PageVar (rendered as a checkbox)
// valPv = the numeric/string PageVar for the alert threshold value
// The label is taken from valPv's description.
// -----------------------------------------------------------------------------
void WT_CheckEditRow(PageVar *cbPv, PageVar *valPv, const char* typeOverride){
	String typeStr;
	const char* typeToUse;
	if (typeOverride && *typeOverride) {
		typeToUse = typeOverride;
	} else {
		typeStr   = valPv->Var->GetTypeStr();
		typeToUse = typeStr.c_str();
	}
	String idL   = valPv->Var->GetID("L");
	String idCb  = cbPv->Var->GetID("");
	String idIn  = valPv->Var->GetID("");
	String nameStr = valPv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          valPv->Var->GetDescription().c_str(), idL.c_str(),
	          "", valPv);
	WT_Column(WT_INPUT, WT_FIRST, WT_MORE, "checkbox",
	          cbPv->Var->GetName().c_str(), idCb.c_str(),
	          cbPv->Var->GetFStyle().c_str(), cbPv);
	WT_Column(WT_INPUT, WT_NEXT,  WT_LAST, typeToUse,
	          nameStr.c_str(), idIn.c_str(),
	          valPv->Var->GetFStyle().c_str(), valPv);
	WT_EndLine();
}

// -----------------------------------------------------------------------------
// WT_RightCheckRow — emits a 2-column row: label | (empty spacer) + checkbox.
// The hidden spacer occupies the left input slot so the checkbox lands in the
// right column, visually aligned with the numeric inputs in WT_CheckEditRow rows.
// -----------------------------------------------------------------------------
void WT_RightCheckRow(PageVar *pv){
	String idL  = pv->Var->GetID("L");
	String idIn = pv->Var->GetID("");
	String nameStr = pv->Var->GetName();
	WT_StartLine();
	WT_Column(WT_LABEL, WT_FIRST, WT_LAST, "text",
	          pv->Var->GetDescription().c_str(), idL.c_str(), "", pv);
	// Spacer (hidden input, no id) + checkbox on the right
	WT_Column(WT_INPUT, WT_FIRST, WT_MORE, "hidden", " ", "", "", pv);
	WT_Column(WT_INPUT, WT_NEXT,  WT_LAST, "checkbox",
	          nameStr.c_str(), idIn.c_str(), pv->Var->GetFStyle().c_str(), pv);
	WT_EndLine();
}

void WT_StartPage(){
	appendToPageBuf("<script>const parameters = [");
}

void WT_EndPage(){
	appendToPageBuf("]; createLabelInputTable(parameters); </script>");
}

void WT_EndPageClass(const char* cls){
	appendToPageBuf("]; createLabelInputTable(parameters,'");
	appendToPageBuf(cls);
	appendToPageBuf("'); </script>");
}

void WT_Header(const char* Header){
	appendToPageBuf("{name: \""); appendToPageBuf(Header); appendToPageBuf("\", labels: [], inputs: [],},");
}

void WT_SectionHeader(const char* Header){
	appendToPageBuf("{sectionHeader: \""); appendToPageBuf(Header); appendToPageBuf("\"},");
}

void WT_StartLine(){
	appendToPageBuf("{");
}

void WT_EndLine(){
	appendToPageBuf("},");
}

void WT_StartLabel(){
	appendToPageBuf("labels: [{");
}

void WT_StartLabelEmpty(){
	appendToPageBuf("labels:[],");
}

void WT_StartInput(){
	appendToPageBuf("inputs:[{");
}

void WT_StartInputEmpty(){
	appendToPageBuf("inputs:[],");
}

void WT_NextColumn(){
	appendToPageBuf("},{");
}

void WT_EndCell(){
	appendToPageBuf("},],");
}

void WT_TypeCell(const char* Type){
	appendToPageBuf("type: \""); appendToPageBuf(Type); appendToPageBuf("\",");
}

void WT_TextLabel(const char* Txt){
	appendToPageBuf("text: \""); appendToPageBuf(Txt); appendToPageBuf("\",");
}

void WT_ValueInput(const char* Val){
	appendToPageBuf("value: \""); appendToPageBuf(Val); appendToPageBuf("\",");
}

void WT_IDCell(const char* ID){
	appendToPageBuf("id: \""); appendToPageBuf(ID); appendToPageBuf("\",");
}

void WT_AttrsStart(){
	appendToPageBuf("attributes:[");
}

void WT_Attr(const char* n, const char* v){
	appendToPageBuf("{n:\""); appendToPageBuf(n);
	appendToPageBuf("\",v:\""); appendToPageBuf(v);
	appendToPageBuf("\"},");
}

void WT_AttrsEnd(){
	appendToPageBuf("],");
}

void WT_OptionsStart(){
	appendToPageBuf("options: [");
}

void WT_OptionsValues(const char* AVal){
	appendToPageBuf("\""); appendToPageBuf(AVal); appendToPageBuf("\",");
}

void WT_OptionsEnd(){
	appendToPageBuf("],");
}
////// END - Page generation for a table //////
