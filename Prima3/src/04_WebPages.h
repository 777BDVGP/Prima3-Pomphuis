#pragma once

// =============================================================================
// 04_WebPages.h — Web Server, WebSocket, Page Elements & HTML Templates
// =============================================================================
// Declares the AsyncWebServer and AsyncWebSocket instances, shared CSS/HTML
// templates stored in PROGMEM, all status-message constants, the PageElement
// and PageVar classes that bind C++ variables to browser form fields, and the
// WT_* table-builder primitive declarations.
// =============================================================================

// ─── WebSocket Variable ID Base Addresses ────────────────────────────────────
// Each page allocates a contiguous block of IDs starting at its base address.
// Browser elements are identified by these numeric IDs via JSON key strings.
#define ID_HOME       50    // Home page      — 9  variables  (50–58)
#define ID_SETTINGS  100    // Settings page  — 9  variables  (100–108)
#define ID_SUSPEND   150    // Suspend page   — 14 variables  (150–163)
#define ID_VALVES    200    // Valve names    — 14 variables  (200–213)
#define ID_VALCONF   250    // Valve configs  — 63 variables  (250–312)
#define ID_IRRISETUP 300    // Irrigation     — 18 × 9 zones  (300–461)
#define ID_TANK      350    // Tank settings  — 19 variables  (350–368)
#define ID_NTFY      500    // NTFY settings  — 16 variables  (500–515)

// ─── Web Server & WebSocket ───────────────────────────────────────────────────
// Both HTTP and WebSocket share port 80 on the same AsyncWebServer instance.
inline AsyncWebServer server(80);
inline AsyncWebSocket ws("/ws");   // WebSocket endpoint at ws://<ip>/ws

// ─── HTML Colour Style Strings ────────────────────────────────────────────────
// Used as the 'ATT' parameter in webParamUpdate() to colour browser elements.
// Declared as const char[] to avoid heap allocation — implicitly convert to String at call sites.
inline const char COLOR_DEFAULT[] = "color:#227c91";  // Teal  — normal informational text
inline const char COLOR_RED[]     = "color:#ff0000";  // Red   — error or alert
inline const char COLOR_BLUE[]    = "color:#3366ff";  // Blue  — action prompt
inline const char COLOR_GREEN[]   = "color:#33cc33";  // Green — success confirmation
inline const char COLOR_VIOLET[]  = "color:#ff00ff";  // Violet — reserved / special

// ─── Status Header Message Constants (FA_MESS_*) ─────────────────────────────
// Passed to append_FileAction() to select the message shown below the page title.
// SetMess can be set by a route handler to override the default on the next render.
#define FA_MESS_01    1   // "Files on SPIFFS"           (teal)
#define FA_MESS_02    2   // "Click on File to Download" (blue)
#define FA_MESS_03    3   // "Upload a File"             (blue)
#define FA_MESS_04    4   // "Click on File to Delete"   (blue)
#define FA_MESS_06    6   // "Selection Option"          (blue)  — default idle message
#define FA_MESS_07    7   // "Files on SPIFFS"           (blue)
#define FA_MESS_08    8   // "Select Report"             (blue)
#define FA_MESS_09    9   // "File Not Found"            (red)
#define FA_MESS_10   10   // "GeneralLog.txt deleted"    (red)
#define FA_MESS_11   11   // "IrrigationLog.txt deleted" (red)
#define FA_MESS_RED  12   // SpesialStr displayed in RED
#define FA_MESS_GRN  13   // SpesialStr displayed in GREEN

inline int Delmess = FA_MESS_06;   // Current delete/report page message (reset after display)

#define DEL_FNF    9   // File not found
#define DEL_GEN   10   // Delete GeneralLog
#define DEL_PUMP  11   // Delete IrrigationLog

// ─── WebSocket Connection Tracking ────────────────────────────────────────────
// Used by UpdateSelectMessDelay() to reset the H03 status message after a
// timed delay on the specific client that triggered the last action.
inline uint32_t Lastnum       = 0;     // Client ID of the last action sender
inline bool     LastnumUpdated = true; // false = a reset message is pending

// Header - Required on all pages (holds title, time, action & style)
inline char webHeadPage[] PROGMEM = R"=====(
<html>
<head>
	<meta name="viewport" content="width=device-width, initial-scale=1">
	<link rel="icon" href="data:,">
	<style>
		*{box-sizing:border-box;margin:0;padding:0}
		:root{--ok:#16a34a;--warn:#c2410c;--alert:#92400e;--err:#b91c1c;--mute:#475569}
		body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;background:#f8fafc;color:#0f172a;max-width:600px;margin:0 auto;padding:8px;font-size:13px}
		h2{font-size:20px;font-weight:800;letter-spacing:1px;color:#0284c7;text-align:center;padding:10px 0 2px}
		h4{font-size:12px;color:#475569;text-align:center;margin:2px 0}
		p{text-align:left;margin-left:8px;margin-top:0;margin-bottom:0}
		ul{list-style-type:none;margin:4px 0;padding:0;overflow:hidden;background:#e0f2fe;border-radius:4px}
		lh{float:left;border-right:1px solid #bae6fd}
		lh a{display:block;background:#0284c7;color:#fff;padding:6px 10px;text-decoration:none;font-size:12px;font-weight:700}
		li{float:left;border-right:1px solid #bae6fd}
		li a{display:block;color:#0369a1;padding:6px 10px;text-decoration:none;font-size:12px}
		li a:hover{background:#bae6fd}
		li a.red{color:#dc2626;background:#fff7ed}
		li a.red:hover{background:#dc2626;color:#fff}
		lk{float:left;background:#e0f2fe;font-size:12px}
		lk a{display:block;color:#0284c7;padding:4px 8px;text-decoration:none}
		lk a:hover{background:#bae6fd}
		.nav{display:flex;flex-wrap:wrap;border:1px solid #0284c7;border-radius:6px;overflow:hidden;margin:6px 0}
		.nav a{flex:1 1 auto;min-width:55px;padding:10px 4px;background:#bae6fd;color:#0369a1;text-align:center;text-decoration:none;font-size:12px;font-weight:700;border-right:1px solid #0284c7;white-space:nowrap}
		.nav a:last-child{border-right:none}
		.nav a:hover:not(.act):not(.warn){background:#7dd3fc}
		.nav a.act{background:#0284c7;color:#fff}
		.nav a.warn{background:#dc2626;color:#fff}
		.nav a.warn:hover{background:#b91c1c}
		table{width:100%;border-collapse:collapse;font-size:13px;margin:4px 0}
		.hd td,.hd th{color:#64748b;font-size:11px;font-weight:700;text-transform:uppercase;letter-spacing:.5px;border-bottom:2px solid #cbd5e1;padding:4px 6px;text-align:left}
		.hd th:last-child,.hd td:last-child{text-align:right}
		td,th{padding:5px 6px;border-bottom:1px solid #e2e8f0;word-break:break-word;overflow-wrap:break-word}
		table.compact td,table.compact th{padding:3px 4px}table.compact input[type=text],table.compact input[type=number],table.compact input[type=time],table.compact input[type=date],table.compact select{padding:1px 2px;font-size:11px}
		tr:not(.hd) td:first-child{width:100%}
		table.susp th:not(:first-child){min-width:65px;text-align:center}
		table.susp tr:not(.hd) td:not(:first-child){min-width:65px;text-align:center}
		tr:not(.hd) td:last-child{text-align:right;white-space:nowrap;padding-right:0}
		tr:not(.hd) td:last-child label{display:inline-block;vertical-align:middle}
		tr:not(.hd) th:first-child{width:100%}
		tr:not(.hd) th:last-child:not(:first-child){text-align:right;white-space:nowrap;padding-right:0}
		tr:not(.hd) th:last-child:not(:first-child) p{text-align:right;margin-left:0}
		table.grid tr:not(.hd) td:first-child{width:auto;text-align:left}
		table.grid tr:not(.hd) td:not(:first-child){padding-left:10px;padding-right:10px;text-align:center}
		table.grid tr:not(.hd) td:last-child{text-align:center;white-space:normal;padding-right:10px}
		table.grid tr:not(.hd) td{text-align:center}
		table.grid tr th{text-align:center;white-space:nowrap;padding-left:10px;padding-right:10px}
		table.grid tr th:first-child{text-align:left;white-space:normal;padding-left:6px;padding-right:6px}
		table.grid tr th:last-child:not(:first-child){text-align:center;white-space:nowrap;padding-right:10px}
		table.grid .hd th{text-align:center}
		table.grid .hd th:first-child{text-align:left}
		table.grid .hd th:last-child{text-align:center}
		input[type=time],input[type=text],input[type=number],input[type=date]{background:#fff;border:1px solid #cbd5e1;border-radius:4px;color:#0f172a;padding:3px 5px;font-size:12px;width:120px;max-width:45vw}
		select{background:#fff;border:1px solid #cbd5e1;border-radius:4px;color:#0f172a;padding:3px 5px;font-size:12px;width:120px;max-width:45vw}
		input[type=checkbox]{accent-color:#0284c7;cursor:pointer;width:16px;height:16px}
		input[type=time]:focus,input[type=text]:focus,input[type=number]:focus,input[type=date]:focus,select:focus{outline:1px solid #0284c7}
		input[type=button],input[type=submit]{background:#0284c7;border:none;color:#fff;padding:5px 14px;font-size:13px;cursor:pointer;border-radius:4px}
		input[type=button]:hover,input[type=submit]:hover{background:#0369a1}
		.btn{display:inline-block;padding:7px 18px;border-radius:5px;font-size:13px;font-weight:700;border:none;cursor:pointer;margin:4px;text-decoration:none}
		.btn-bl{background:#1d4ed8;color:#fff}.btn-bl:hover{background:#2563eb}
		.btn-rd{background:#dc2626;color:#fff}.btn-rd:hover{background:#ef4444}
		.btn-or{background:#d97706;color:#fff}.btn-or:hover{background:#f59e0b}
		.btn-gr{background:#15803d;color:#fff}.btn-gr:hover{background:#16a34a}
		.btns{text-align:center;padding:8px 0}
		.valve-btn{display:block;width:100%;padding:16px 0;margin:8px 0;text-align:center;font-size:20px;font-weight:800;border-radius:8px;color:#fff;text-decoration:none}
		.valve-open{background:#16a34a}.valve-open:hover{background:#15803d}
		.valve-closed{background:#2563eb}.valve-closed:hover{background:#1d4ed8}
		label{color:#0f172a}
	</style>
</head>

<body onload="javascript:init()">

)=====";

// WebSocket Script
inline char webSocketScript[] PROGMEM = R"=====(
<script>

	function init() {
		StartWebSocket();
	}

	function setOtaStatus(msg, color) {
		var el = document.getElementById("ota_status");
		if (el) { el.value = msg; el.style.color = color; }
	}

	var Socket;
	function StartWebSocket() {
		Socket = new WebSocket('ws://' + window.location.host + '/ws');

		Socket.onerror = function(event) {
			console.log("Socket.onerror");
			setTimeout(StartWebSocket, 300);
		};

		Socket.onclose = function(event) {
			console.log("Socket.onclose");
		};

		// Update web page elements from ESP32
		Socket.onmessage = function(event) {
			const jsonObject = JSON.parse(event.data);
			if (jsonObject.type === "ota_start")    { setOtaStatus("Upload starting...",                     "orange"); return; }
			if (jsonObject.type === "ota_progress") { setOtaStatus("Uploading... " + jsonObject.percent + "%", "orange"); return; }
			if (jsonObject.type === "ota_end")      { setOtaStatus("Busy rebooting...", "red"); setTimeout(function(){ location.reload(); }, 5000); return; }
			if (jsonObject.type === "ota_error")    { setOtaStatus("Update failed!",                         "red");    return; }
			var ElementAtt;
			for (const [key, value] of Object.entries(jsonObject)) {
				if (key === "Attribute") {
					const attrName = jsonObject["AttributeName"] || "style";
					if (ElementAtt !== null) { ElementAtt.setAttribute(attrName, value); }
				} else if (key === "AttributeName") {
					// consumed by the "Attribute" handler above
				} else {
					const Element = document.getElementById(key);
					if (Element === null) { ElementAtt = null; continue; }
					if (Element.tagName === 'SELECT') {
						Element.value = value;
						ElementAtt = Element;
					} else {
						if (Element !== null) {
							if (Element.type === "checkbox") {
								Element.checked = (value === "true");
								ElementAtt = null;
							} else {
								Element.value = value;
								Element.innerHTML = value;
								ElementAtt = Element;
							}
						} else {
							ElementAtt = null;
						}
					}
				}
			}
		}
	}

	// Read all inputs and send to ESP32 via WebSocket
	function UpdatePage() {
		const jsonObject = {};
		for (const input of document.querySelectorAll("input")) {
			jsonObject[input.id] = (input.type === "checkbox") ? (input.checked ? "true" : "false") : input.value;
		}
		for (const select of document.querySelectorAll("select")) {
			jsonObject[select.id] = select.value;
		}
		Socket.send(JSON.stringify(jsonObject));
	}

	function ButtonUser1() {
		Socket.send(JSON.stringify({"MAN1": "User 1"}));
	}

	function ButtonUser2() {
		Socket.send(JSON.stringify({"MAN2": "User 2"}));
	}

	function ButtonReboot() {
		Socket.send(JSON.stringify({"REBOOT": "YES"}));
	}

	// Build label+input table below header
	function createLabelInputTable(parameters, tableClass) {
		const tableElement = document.createElement("table");
		if (tableClass) tableElement.className = tableClass;
		const headerRow = document.createElement("tr");

		for (const parameter of parameters) {
			if (!parameter.name) continue;
			const headerCell = document.createElement("th");
			headerCell.textContent = parameter.name;
			headerRow.appendChild(headerCell);
		}

		if (headerRow.textContent.trim() !== '') {
			headerRow.querySelectorAll('th').forEach(cell => { cell.style.textAlign = 'left'; });
			tableElement.appendChild(headerRow);
		}

		for (const parameter of parameters) {
			if (parameter.name !== undefined) continue;
			if (parameter.sectionHeader !== undefined) {
				const row = document.createElement("tr");
				const cell = document.createElement("td");
				cell.colSpan = 2;
				cell.textContent = parameter.sectionHeader;
				cell.style.cssText = "font-weight:bold;padding:4px 6px;text-align:left;";
				row.appendChild(cell);
				tableElement.appendChild(row);
				continue;
			}
			const row = document.createElement("tr");

			for (const label of parameter.labels) {
				const cell = document.createElement("td");
				const labelElement = document.createElement("label");
				labelElement.textContent = label.text;
				labelElement.id = label.id;
				for (const a of (label.attributes || [])) { labelElement.setAttribute(a.n, a.v); }
				cell.appendChild(labelElement);
				row.appendChild(cell);
			}

			for (const input of parameter.inputs) {
				const cell = document.createElement("td");

				if (input.type === "select") {
					const inputElement = document.createElement("select");
					inputElement.id = input.id;
					for (const a of (input.attributes || [])) { inputElement.setAttribute(a.n, a.v); }
					for (const optionText of input.options) {
						const option = document.createElement("option");
						option.value = optionText;
						option.textContent = optionText;
						inputElement.appendChild(option);
					}
					inputElement.value = input.value;
					cell.appendChild(inputElement);
				} else {
					const inputElement = document.createElement("input");
					inputElement.type = input.type;
					inputElement.id = input.id;
					inputElement.value = input.value;
					if (input.type === "checkbox") {
						inputElement.checked = (input.value === "true");
					}
					for (const a of (input.attributes || [])) { inputElement.setAttribute(a.n, a.v); }
					cell.appendChild(inputElement);
				}

				row.appendChild(cell);
			}

			tableElement.appendChild(row);
		}

		document.body.appendChild(tableElement);
	}

</script>
)=====";

// Script tag referencing the cached /wss.js.  __DATE__ changes each build,
// busting the 24-hour browser cache whenever new firmware is uploaded.
#define webScriptTag "<script src='/wss.js?v=" __DATE__ "'></script>\n"

inline char webHTMLEnd[] PROGMEM = R"=====(
</body>
</html>
)=====";

inline char webUpateButton[] PROGMEM = R"=====(
	<div><br></div>
	<div>
		<b><a><input type="button" id="Update" onclick="UpdatePage()" value="UPDATE" style="font-size:14px"></a></b>
	</div>
)=====";

// ─── SelectOptions ────────────────────────────────────────────────────────────
// Holds a single dropdown option string.  An array of these is allocated by
// SetStringToSelect() when a PageElement is configured as a <select> field.
class SelectOptions {
public:
	String Option = "";   // The option text shown in the browser dropdown
};

// ─── PageElement Variable Type Tags ──────────────────────────────────────────
// Used internally by PageElement to dispatch the correct type conversion.
#define VTYPE_STRING 1
#define VTYPE_DOUBLE 2
#define VTYPE_FLOAT  3
#define VTYPE_INT    4
#define VTYPE_BOOL   5

// =============================================================================
// PageElement
// =============================================================================
// Binds a single C++ variable (String, double, float, int, or bool) to a
// browser form element.  Each instance stores:
//   - A pointer to the live application variable
//   - A snapshot of the last-sent value (used to detect changes)
//   - A fixed style string (label column CSS)
//   - A pointer to a variable style string (value column CSS, dynamically set)
//   - An integer ID that maps to an HTML element id="<prefix><ID>"
//
// For String types, SetStringToSelect() converts the field to a <select>
// dropdown with options added via AddOption().
//
// Typical usage:
//   new PageElement(&myVar, "Label Text", "", &myStyleStr, ID_BASE + index);
// =============================================================================
class PageElement {
public:
	// ── PSRAM allocation — keeps ~25 KB of PageElement structs out of DRAM ──
	void* operator new(size_t sz)  { void* p = ps_malloc(sz); return p ? p : malloc(sz); }
	void  operator delete(void* p) { free(p); }

	// ── Constructors — one overload per supported C++ type ───────────────────
	PageElement(String* InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(double* InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(float*  InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(int*    InName, String Desc, String FStyle, String* VStyle, int ID);
	PageElement(bool*   InName, String Desc, String FStyle, String* VStyle, int ID);
	~PageElement();

	// ── Value Access ──────────────────────────────────────────────────────────
	void   SetName(String InName);         // Write a new value to the referenced variable
	String GetName();                      // Read the current value as a String
	String GetNameLast();                  // Read the last-sent value (change-detection)
	void   UpdateLast();                   // Snapshot the current value as the new baseline
	bool   HasChanged();                   // True when current != last (zero heap allocations)

	// ── Style & Metadata ──────────────────────────────────────────────────────
	const String& GetVarStyle();           // Return the current variable-column CSS style
	const String& GetDescription();        // Return the label text for this field
	void          SetDescription(String desc); // Update the label text (e.g. for dynamic names)
	const String& GetFStyle();             // Return the fixed (label-column) CSS style

	// ── HTML Generation Helpers ───────────────────────────────────────────────
	String GetTypeStr();                   // Return HTML input type: "text", "number", "checkbox", or "select"
	String GetID(const char* Type);        // Return "<Type><ID>" (e.g. "V350") or just "<ID>" when Type=""

	// ── Dropdown (select) Support ─────────────────────────────────────────────
	void          SetStringToSelect(int Options); // Allocate the option list; call before AddOption()
	void          AddOption(String Option);       // Append one option to the dropdown list
	const String& GetOption(int Idx);             // Return option text by zero-based index
	int           GetOptionCount();               // Return total number of options

	// ── Per-Field HTML/CSS Attribute Setters ──────────────────────────────────
	// Call these after construction to add browser-enforced constraints and hints.
	// WT_Column reads these and emits them automatically into the attributes array.
	void SetTextAlign(String align);                        // CSS text-align: "left", "right", "center"
	void SetRange(float mn, float mx, float step = NAN);   // HTML min / max / step on number inputs
	void SetPlaceholder(String ph);                         // HTML placeholder on text/number inputs
	void SetReadOnly(bool ro = true);                       // HTML readonly attribute

	// ── Per-Field Attribute Accessors (used by WT_Column) ─────────────────────
	const String& GetAttrTextAlign();
	bool          HasAttrMin();
	bool          HasAttrMax();
	bool          HasAttrStep();
	float         GetAttrMin();
	float         GetAttrMax();
	float         GetAttrStep();
	const String& GetAttrPlaceholder();
	bool          GetAttrReadOnly();

private:
	String* NameS;
	double* NameD;
	float*  NameF;
	int*    NameI;
	bool*   NameB;
	int     Type;

	String NameSLast;
	double NameDLast;
	float  NameFLast;
	int    NameILast;
	String NameBLast;

	String  VarDesc  = "";
	String  FixStyle = "";
	String* VarStyle;
	int     VarID = 0;
	bool    NameStringSelect = false;
	int     OptionCount = 0;
	int     OptionIndex = 0;
	SelectOptions* OptionList;

	// Per-field attribute metadata — emitted as HTML/CSS attributes by WT_Column
	String AttrTextAlign   = "";    // CSS text-align value
	float  AttrMin         = NAN;   // HTML min attribute for number inputs
	float  AttrMax         = NAN;   // HTML max attribute for number inputs
	float  AttrStep        = NAN;   // HTML step attribute for number inputs
	String AttrPlaceholder = "";    // HTML placeholder attribute
	bool   AttrReadOnly    = false; // HTML readonly attribute
};

inline PageElement::PageElement(String* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameS = InName; NameSLast = *NameS;
	Type = VTYPE_STRING; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(double* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameD = InName; NameDLast = *NameD;
	Type = VTYPE_DOUBLE; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(float* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameF = InName; NameFLast = *NameF;
	Type = VTYPE_FLOAT; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(int* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameI = InName; NameILast = *NameI;
	Type = VTYPE_INT; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::PageElement(bool* InName, String Desc, String FStyle, String* VStyle, int ID) {
	NameB = InName; NameBLast = (*NameB ? "true" : "false");
	Type = VTYPE_BOOL; VarDesc = Desc; FixStyle = FStyle; VarStyle = VStyle; VarID = ID;
	OptionList = nullptr;
}

inline PageElement::~PageElement() {
	delete[] OptionList;
}

inline void PageElement::SetName(String InName) {
	switch (Type) {
		case VTYPE_STRING: *NameS = InName;              break;
		case VTYPE_DOUBLE: *NameD = InName.toDouble();   break;
		case VTYPE_FLOAT:  *NameF = InName.toFloat();    break;
		case VTYPE_INT:    *NameI = InName.toInt();      break;
		case VTYPE_BOOL:   *NameB = (InName == "true");  break;
	}
}

inline String PageElement::GetName() {
	switch (Type) {
		case VTYPE_STRING: return *NameS;
		case VTYPE_DOUBLE: return String(*NameD);
		case VTYPE_FLOAT:  return String(*NameF);
		case VTYPE_INT:    return String(*NameI);
		case VTYPE_BOOL:   return (*NameB ? "true" : "false");
	}
	return "";
}

inline const String& PageElement::GetVarStyle() {
	return *VarStyle;
}

inline String PageElement::GetNameLast() {
	switch (Type) {
		case VTYPE_STRING: return NameSLast;
		case VTYPE_DOUBLE: return String(NameDLast);
		case VTYPE_FLOAT:  return String(NameFLast);
		case VTYPE_INT:    return String(NameILast);
		case VTYPE_BOOL:   return NameBLast;
	}
	return "";
}

inline void PageElement::UpdateLast() {
	switch (Type) {
		case VTYPE_STRING: NameSLast = *NameS;                         break;
		case VTYPE_DOUBLE: NameDLast = *NameD;                         break;
		case VTYPE_FLOAT:  NameFLast = *NameF;                         break;
		case VTYPE_INT:    NameILast = *NameI;                         break;
		case VTYPE_BOOL:   NameBLast = (*NameB ? "true" : "false");    break;
	}
}

inline const String& PageElement::GetDescription()         { return VarDesc;  }
inline void          PageElement::SetDescription(String d) { VarDesc = d;      }
inline const String& PageElement::GetFStyle()              { return FixStyle;  }

inline bool PageElement::HasChanged() {
	switch (Type) {
		case VTYPE_STRING: return *NameS != NameSLast;
		case VTYPE_DOUBLE: return *NameD != NameDLast;
		case VTYPE_FLOAT:  return *NameF != NameFLast;
		case VTYPE_INT:    return *NameI != NameILast;
		case VTYPE_BOOL:   return NameBLast != (*NameB ? "true" : "false");
	}
	return false;
}

inline String PageElement::GetID(const char* Type) {
	if (Type && *Type) { String r = Type; r += VarID; return r; }
	return String(VarID);
}

inline void PageElement::SetStringToSelect(int Options) {
	NameStringSelect = true;
	OptionCount = Options;
	OptionList = new SelectOptions[OptionCount];
}

inline void PageElement::AddOption(String Option) {
	OptionList[OptionIndex].Option = Option;
	if (OptionIndex < (OptionCount - 1)) OptionIndex++;
}

inline const String& PageElement::GetOption(int Idx)  { return OptionList[Idx].Option; }
inline int            PageElement::GetOptionCount()   { return OptionCount; }

inline String PageElement::GetTypeStr() {
	switch (Type) {
		case VTYPE_STRING: return (NameStringSelect ? "select" : "text");
		case VTYPE_DOUBLE:
		case VTYPE_FLOAT:
		case VTYPE_INT:    return "number";
		case VTYPE_BOOL:   return "checkbox";
	}
	return "";
}

inline void   PageElement::SetTextAlign(String align)               { AttrTextAlign = align; }
inline void   PageElement::SetRange(float mn, float mx, float step) { AttrMin = mn; AttrMax = mx; AttrStep = step; }
inline void   PageElement::SetPlaceholder(String ph)                { AttrPlaceholder = ph; }
inline void   PageElement::SetReadOnly(bool ro)                     { AttrReadOnly = ro; }

inline const String& PageElement::GetAttrTextAlign()   { return AttrTextAlign;   }
inline bool          PageElement::HasAttrMin()         { return !isnan(AttrMin); }
inline bool          PageElement::HasAttrMax()         { return !isnan(AttrMax); }
inline bool          PageElement::HasAttrStep()        { return !isnan(AttrStep); }
inline float         PageElement::GetAttrMin()         { return AttrMin;         }
inline float         PageElement::GetAttrMax()         { return AttrMax;         }
inline float         PageElement::GetAttrStep()        { return AttrStep;        }
inline const String& PageElement::GetAttrPlaceholder() { return AttrPlaceholder; }
inline bool          PageElement::GetAttrReadOnly()    { return AttrReadOnly;    }

// ─── PageVar ──────────────────────────────────────────────────────────────────
// Thin wrapper used to build arrays of PageElement pointers (e.g. HomeList[],
// TankList[]).  The 'deleted' flag is reserved for future dynamic cleanup.
struct PageVar {
	PageElement* Var     = nullptr;  // Pointer to the bound PageElement instance
	bool         deleted = true;     // Reserved — not currently used at runtime
};
