// =============================================================================
// 05_WiFi.cpp — WiFi join (saved + built-in networks) + setup hotspot
// =============================================================================
// Ported from RAINGAUGE/RainGauge_PIO 05_WiFi.cpp (1.10.1), adapted:
//   - Built-in networks (WIFI_KNOWN_NETS, git-ignored wifi_secrets.h) always
//     take the firmware's NAV_IP / NAV_GW / NAV_SN / NAV_DNS (01_Main.h), so
//     DEBUG still picks .120 (dev) or .118 (production).  They are NOT copied
//     into /WiFiConfig.json — the firmware stays the single source for them.
//   - /WiFiConfig.json holds the EXTRA networks added on the setup hotspot or
//     on SETTINGS → WiFi Networks (max 8, newest first).  A saved entry with
//     the same SSID as a built-in one replaces it.
//   - All candidates are tried strongest-in-range first (one scan per round).
//   - Every join scans ALL channels and joins the STRONGEST access point of
//     the SSID (mesh / multi-AP) — the ESP32 default fast scan joins the first
//     AP it finds on the channel sweep, which is often not the nearest one.
//
// /WiFiConfig.json  { "nets": [ { ssid, pass, dhcp, ip, gw, sn, dns }, ... ] }
//   Holds WiFi passwords, so the FILES page never lists, shows or downloads it
//   (IsProtectedFile, 03_Files.h).
//
// The setup hotspot "Prima3-Setup" (open, http://192.168.4.1/, captive DNS)
// opens when:
//   - BOOT (GPIO 0) is held LOW at power-on, or
//   - no network could be joined in WIFI_ROUNDS rounds (≈ 3-4 minutes).
// It ADDS the network it is given and always ends in a reboot (save, cancel or
// WIFI_PORTAL_MS timeout) — a pump controller must never sit in the hotspot
// forever.  It uses the main AsyncWebServer `server` (routes are registered
// only for the hotspot; the device reboots before SetupWebPage() would run).
// =============================================================================

#include <Arduino.h>
#include <ESPmDNS.h>
#include <DNSServer.h>
#include <climits>
#include "all_includes.h"
#include "05_WiFi.h"

// Optional built-in networks (wifi_secrets.h is NOT in Git).  Without it
// (public firmware) a new controller opens the Prima3-Setup hotspot.
#if __has_include("wifi_secrets.h")
	#include "wifi_secrets.h"
#endif

struct KnownNet {
	const char* ssid;
	const char* pass;
};

#ifdef WIFI_KNOWN_NETS
	static const KnownNet KNOWN_NETS[] = { WIFI_KNOWN_NETS };
	static const int KNOWN_NET_COUNT = sizeof(KNOWN_NETS) / sizeof(KNOWN_NETS[0]);
#else
	static const KnownNet* const KNOWN_NETS = nullptr;   // setup hotspot / saved list only
	static const int KNOWN_NET_COUNT = 0;
#endif

#define WIFI_HOSTNAME        "primapomphuis"    // mDNS: http://primapomphuis.local/
#define WIFI_SETUP_SSID      "Prima3-Setup"     // setup hotspot (open, 192.168.4.1)
#define WIFI_CONFIG_FILE     "/WiFiConfig.json"
#define WIFI_OLD_FILE        "/WifiSettings.json"   // pre-list file — deleted when the list is written
#define WIFI_ROUNDS          6                  // full (scan + try all) rounds before the setup hotspot
#define WIFI_SINGLE_TRIES    2                  // attempts per round when there is only ONE candidate
#define WIFI_MAX_NETS        8                  // networks kept in /WiFiConfig.json
#define WIFI_MAX_CAND        (WIFI_MAX_NETS + 4)
#define WIFI_CONNECT_MS      15000UL            // per join attempt
#define WIFI_ROUND_GAP_MS    10000UL            // pause between rounds (router may be rebooting)
#define WIFI_PORTAL_MS       (5UL * 60UL * 1000UL)  // setup hotspot: 5 min, then reboot

// A network to join (saved entry or built-in one)
struct WifiCfg {
	String    ssid, pass;
	bool      dhcp = true;
	IPAddress ip, gw, sn, dns;
	bool      builtIn = false;   // from WIFI_KNOWN_NETS (not in the file)
};

// ── Helpers ───────────────────────────────────────────────────────────────────

static WifiCfg knownToCfg(const KnownNet& k){
	WifiCfg c;
	c.ssid = k.ssid; c.pass = k.pass; c.dhcp = false;
	c.ip = NAV_IP; c.gw = NAV_GW; c.sn = NAV_SN; c.dns = NAV_DNS;
	c.builtIn = true;
	return c;
}

// Wait for WL_CONNECTED, blinking the PCB LED so "searching" is visible.
static bool waitConnected(){
	unsigned long t = millis();
	while (WiFi.status() != WL_CONNECTED) {
		if (millis() - t > WIFI_CONNECT_MS) {
			DEBUG_PRINTLN("WiFi: connection timed out");
			return false;
		}
		delay(100);
		digitalWrite(LEDOUT, !digitalRead(LEDOUT));
	}
	return true;
}

static void dropConnection(){
	WiFi.disconnect(false);
	delay(300);
}

static bool joinWith(const WifiCfg& c){
	DEBUG_PRINTF("WiFi: joining %s (%s)\n", c.ssid.c_str(), c.dhcp ? "DHCP" : c.ip.toString().c_str());
	if (c.dhcp) WiFi.config(IPAddress(0,0,0,0), IPAddress(0,0,0,0), IPAddress(0,0,0,0));   // 0.0.0.0 = DHCP
	else        WiFi.config(c.ip, c.gw, c.sn, c.dns);
	// Several access points may share one SSID (mesh / multi-AP, Omada). The
	// ESP32's default fast scan joins the FIRST one found on the channel sweep;
	// scan all channels and pick the strongest instead.
	WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
	WiFi.setSortMethod(WIFI_CONNECT_AP_BY_SIGNAL);
	WiFi.begin(c.ssid.c_str(), c.pass.c_str());
	if (!waitConnected()) return false;
	SaveGenReport("WiFi: joined " + c.ssid + " via AP " + WiFi.BSSIDstr() +
	              " (channel " + String(WiFi.channel()) + ", " + String(WiFi.RSSI()) + " dBm)");
	return true;
}

// ── /WiFiConfig.json — a LIST of networks ────────────────────────────────────

static void netFromJson(JsonVariantConst o, WifiCfg& c){
	c.ssid = o["ssid"] | "";
	c.pass = o["pass"] | "";
	c.dhcp = o["dhcp"] | true;
	if (!c.dhcp){
		if (!c.ip.fromString(o["ip"] | "")){ c.dhcp = true; return; }   // no usable fixed IP: DHCP
		// gw / sn / dns may be missing = 0.0.0.0 = learned from the router at the
		// first join (needsLearning) and then saved.
		c.gw.fromString(o["gw"] | "");
		c.sn.fromString(o["sn"] | "");
		c.dns.fromString(o["dns"] | "");
	}
}

// Fixed IP without a known gateway: learn gateway / subnet / DNS by DHCP first.
static bool needsLearning(const WifiCfg& c){
	return !c.dhcp && ((uint32_t)c.gw == 0 || (uint32_t)c.sn == 0);
}

// Fills out[] (max maxN), returns how many networks are saved.
static int loadNets(WifiCfg* out, int maxN){
	File fr = SPIFFS.open(WIFI_CONFIG_FILE, "r");
	if (!fr) return 0;
	DynamicJsonDocument doc(4096);
	DeserializationError err = deserializeJson(doc, fr);
	fr.close();
	if (err) return 0;
	int n = 0;
	if (doc["nets"].is<JsonArrayConst>()){
		for (JsonVariantConst o : doc["nets"].as<JsonArrayConst>()){
			if (n >= maxN) break;
			netFromJson(o, out[n]);
			if (out[n].ssid.length()) n++;
		}
	}
	return n;
}

static void saveNets(const WifiCfg* nets, int n){
	DynamicJsonDocument doc(4096);
	JsonArray a = doc.createNestedArray("nets");
	for (int i = 0; i < n; i++){
		JsonObject o = a.createNestedObject();
		o["ssid"] = nets[i].ssid;
		o["pass"] = nets[i].pass;
		o["dhcp"] = nets[i].dhcp;
		if (!nets[i].dhcp){
			o["ip"] = nets[i].ip.toString();
			if ((uint32_t)nets[i].gw)  o["gw"]  = nets[i].gw.toString();    // not yet learned = left out
			if ((uint32_t)nets[i].sn)  o["sn"]  = nets[i].sn.toString();
			if ((uint32_t)nets[i].dns) o["dns"] = nets[i].dns.toString();
		}
	}
	File fw = SPIFFS.open(WIFI_CONFIG_FILE, "w");
	if (!fw) { SaveGenReport F("WiFi: could not write /WiFiConfig.json"); return; }
	serializeJson(doc, fw);
	fw.close();
	SPIFFS.remove(WIFI_OLD_FILE);   // the old single-network file is not used any more
}

// Add a network (newest first).  The same SSID again replaces its entry;
// with a full list the oldest entry drops off.
static void addNet(const WifiCfg& c){
	WifiCfg nets[WIFI_MAX_NETS + 1];
	int n = loadNets(nets + 1, WIFI_MAX_NETS);
	nets[0] = c;
	int k = 1;
	for (int i = 1; i <= n; i++)
		if (nets[i].ssid != c.ssid && k < WIFI_MAX_NETS) nets[k++] = nets[i];
	saveNets(nets, k);
}

// Remove a network by SSID.  Returns false when it was not in the list.
static bool deleteNet(const String& ssid){
	WifiCfg nets[WIFI_MAX_NETS];
	int n = loadNets(nets, WIFI_MAX_NETS), k = 0;
	for (int i = 0; i < n; i++) if (nets[i].ssid != ssid) nets[k++] = nets[i];
	if (k == n) return false;
	saveNets(nets, k);
	return true;
}

// Saved networks first, then the built-in ones whose SSID is not saved.
static int buildCandidates(WifiCfg* out){
	int n = loadNets(out, WIFI_MAX_NETS);
	for (int i = 0; i < KNOWN_NET_COUNT && n < WIFI_MAX_CAND; i++){
		bool saved = false;
		for (int j = 0; j < n; j++) if (out[j].ssid == KNOWN_NETS[i].ssid) saved = true;
		if (!saved) out[n++] = knownToCfg(KNOWN_NETS[i]);
	}
	return n;
}

// ── One round: scan, then try every candidate, strongest in range first ─────
// Candidates not seen in the scan (hidden SSID, weak signal) are tried last.
// With a single candidate it gets WIFI_SINGLE_TRIES attempts, otherwise one.

static bool joinRound(){
	WifiCfg cand[WIFI_MAX_CAND];
	int n = buildCandidates(cand);
	if (n == 0) return false;

	int rssi[WIFI_MAX_CAND];
	for (int i = 0; i < n; i++) rssi[i] = INT_MIN;
	int found = WiFi.scanNetworks();
	for (int j = 0; j < found; j++)
		for (int i = 0; i < n; i++)
			if (WiFi.SSID(j) == cand[i].ssid && WiFi.RSSI(j) > rssi[i]) rssi[i] = WiFi.RSSI(j);
	WiFi.scanDelete();

	bool tried[WIFI_MAX_CAND] = { false };
	int tries = (n == 1) ? WIFI_SINGLE_TRIES : 1;
	for (int k = 0; k < n; k++){
		int best = -1;
		for (int i = 0; i < n; i++)
			if (!tried[i] && (best < 0 || rssi[i] > rssi[best])) best = i;
		tried[best] = true;
		WifiCfg& c = cand[best];
		for (int t = 0; t < tries; t++){
			bool learned = false;
			if (needsLearning(c)){
				// Join by DHCP once to learn the router's gateway / subnet / DNS.
				WifiCfg d = c; d.dhcp = true;
				if (!joinWith(d)){ dropConnection(); continue; }
				c.gw = WiFi.gatewayIP();
				c.sn = WiFi.subnetMask();
				if ((uint32_t)c.dns == 0) c.dns = WiFi.dnsIP();
				dropConnection();
				learned = true;
			}
			if (joinWith(c)){
				WiFiSource = c.builtIn ? "built-in" : "saved";
				if (learned){
					WifiCfg all[WIFI_MAX_NETS];          // store what was learned, same place in the list
					int m = loadNets(all, WIFI_MAX_NETS);
					for (int i = 0; i < m; i++) if (all[i].ssid == c.ssid) all[i] = c;
					saveNets(all, m);
					SaveGenReport("WiFi: " + c.ssid + " on " + c.ip.toString() + " - gateway " + c.gw.toString() +
					              " learned from the router and saved");
				}
				return true;
			}
			dropConnection();
		}
	}
	return false;
}

// ── Setup hotspot ────────────────────────────────────────────────────────────
// The async_tcp handler only copies the form into pendingCfg + raises a flag;
// the loop below saves it and reboots.  Always ends in a reboot.

static WifiCfg       pendingCfg;
static volatile bool pendingSave   = false;
static volatile bool pendingCancel = false;
static String        portalNetOptions;   // <option>s from the scan at start

static const char PORTAL_PAGE_1[] PROGMEM = R"HTML(<!DOCTYPE html><html><head>
<meta name='viewport' content='width=device-width,initial-scale=1'><title>Prima3 WiFi Setup</title>
<style>body{font-family:sans-serif;max-width:420px;margin:0 auto;padding:12px;background:#f8fafc;color:#0f172a}
h2{color:#0284c7;text-align:center;margin:6px 0}label{display:block;margin:10px 0 3px;font-weight:600;font-size:14px}
input,select{width:100%;box-sizing:border-box;padding:8px;font-size:15px;border:1px solid #cbd5e1;border-radius:5px}
.r{display:flex;gap:16px;margin-top:6px}.r label{font-weight:400;margin:0}.r input{width:auto}
button{width:100%;padding:12px;margin-top:16px;font-size:16px;font-weight:700;border:0;border-radius:6px;color:#fff;background:#16a34a}
.c{background:#64748b}p{font-size:13px;color:#475569}</style></head><body>
<h2>Prima3 Pomphuis &mdash; WiFi Setup</h2>
<p>Choose the WiFi network the controller must join. After SAVE it restarts and joins it.
Then open <b>http://primapomphuis.local/</b> (or the fixed IP you chose) on a phone or PC on that network.</p>
<form method='POST' action='/save'>
<label>WiFi network</label><select name='ssid'>)HTML";

static const char PORTAL_PAGE_2[] PROGMEM = R"HTML(<option value=''>Other (type below)...</option></select>
<label>...or type the network name</label><input name='ssid2' placeholder='only if not in the list'>
<label>WiFi password</label><input name='pass' type='password' placeholder='leave empty for an open network'>
<label>IP address</label>
<div class='r'><label><input type='radio' name='mode' value='dhcp' onclick="f(0)"> Automatic (DHCP)</label>
<label><input type='radio' name='mode' value='fixed' checked onclick="f(1)"> Fixed IP</label></div>
<div id='fx'>
<label>Fixed IP</label><input name='ip' value=')HTML";

static const char PORTAL_PAGE_3[] PROGMEM = R"HTML('>
<label>Gateway / router (empty = learned from the router)</label><input name='gw'>
<label>Subnet mask</label><input name='sn' value='255.255.255.0'>
<label>DNS (empty = same as gateway)</label><input name='dns'>
<p>A fixed IP is needed: the alarm controller and the rain gauge reach this controller by its IP.</p></div>
<button type='submit'>SAVE &amp; RESTART</button></form>
<form method='POST' action='/cancel'><button class='c' type='submit'>CANCEL (keep the old setting)</button></form>
<script>function f(x){document.getElementById('fx').style.display=x?'block':'none';}</script>
</body></html>)HTML";

static void portalSend(AsyncWebServerRequest* req){
	String page;
	page.reserve(4000 + portalNetOptions.length());
	page += FPSTR(PORTAL_PAGE_1);
	page += portalNetOptions;
	page += FPSTR(PORTAL_PAGE_2);
	page += IPAddress(NAV_IP).toString();
	page += FPSTR(PORTAL_PAGE_3);
	req->send(200, "text/html", page);
}

static String argTrim(AsyncWebServerRequest* req, const char* name){
	String v = req->hasArg(name) ? req->arg(name) : String();
	v.trim();
	return v;
}

// Network from a form (setup hotspot page and SETTINGS "ADD NETWORK"):
// ssid (or ssid2 typed by hand), pass, mode = dhcp / fixed, ip, gw, sn, dns.
// Returns "" when fine, otherwise the reason.
static String parseNetForm(AsyncWebServerRequest* req, WifiCfg& c){
	c.ssid = argTrim(req, "ssid2");
	if (c.ssid.length() == 0) c.ssid = argTrim(req, "ssid");
	c.pass = req->hasArg("pass") ? req->arg("pass") : String();
	c.dhcp = argTrim(req, "mode") != "fixed";
	if (c.ssid.length() == 0 || c.ssid.length() > 32) return "Choose or type a WiFi network name (max 32 characters).";
	if (c.pass.length() > 63)                         return "The WiFi password can be at most 63 characters.";
	if (!c.dhcp){
		String gw = argTrim(req, "gw"), sn = argTrim(req, "sn"), dns = argTrim(req, "dns");
		if (!c.ip.fromString(argTrim(req, "ip"))) return "The fixed IP must be like 192.168.1.118.";
		if (gw.length() == 0){
			// No gateway given: learned from the router at the first join (with subnet and DNS).
			c.gw = c.sn = c.dns = IPAddress();
			return "";
		}
		if (!c.gw.fromString(gw))                 return "The gateway must be like 192.168.1.1 (or leave it empty: learned from the router).";
		if (sn.length() == 0)                     c.sn = IPAddress(255, 255, 255, 0);
		else if (!c.sn.fromString(sn))            return "The subnet mask must be like 255.255.255.0.";
		if (dns.length() == 0)                    c.dns = c.gw;
		else if (!c.dns.fromString(dns))          return "DNS must be like 192.168.1.1 (or leave it empty).";
	}
	return "";
}

static void portalSave(AsyncWebServerRequest* req){
	WifiCfg c;
	String err = parseNetForm(req, c);
	if (err.length()){
		req->send(200, "text/html", "<meta name='viewport' content='width=device-width,initial-scale=1'>"
		          "<p style='font-family:sans-serif;color:#b91c1c'>" + err + "</p><p><a href='/'>Back</a></p>");
		return;
	}
	pendingCfg  = c;
	pendingSave = true;
	req->send(200, "text/html", "<meta name='viewport' content='width=device-width,initial-scale=1'>"
	          "<div style='font-family:sans-serif;text-align:center;padding:20px'><h3>Saved</h3>"
	          "<p>The controller restarts and joins <b>" + c.ssid + "</b>.</p>"
	          "<p>Connect your phone back to your normal WiFi and open<br><b>" +
	          (c.dhcp ? String("http://primapomphuis.local/") : "http://" + c.ip.toString() + "/") + "</b></p>"
	          "<p style='color:#64748b;font-size:13px'>If it cannot join, the " WIFI_SETUP_SSID " hotspot comes back "
	          "after a few minutes so you can try again.</p></div>");
}

static void runSetupPortal(uint32_t timeoutMs, const String& why){
	SaveGenReport("WiFi: setup hotspot " WIFI_SETUP_SSID " started (" + why + ")");
	HeartbeatSetMode(HB_MODE_PORTAL);   // red — setup hotspot up
	WiFi.disconnect(true);
	delay(200);
	WiFi.mode(WIFI_AP_STA);

	// Networks in range, strongest first, each SSID once.
	int n = WiFi.scanNetworks();
	{
		portalNetOptions = "";
		bool used[64] = { false };
		int m = n < 64 ? n : 64;
		for (int k = 0; k < m; k++){
			int best = -1;
			for (int j = 0; j < m; j++)
				if (!used[j] && (best < 0 || WiFi.RSSI(j) > WiFi.RSSI(best))) best = j;
			if (best < 0) break;
			used[best] = true;
			String s = WiFi.SSID(best);
			if (s.length() == 0 || s.indexOf('\'') >= 0 || s.indexOf('<') >= 0) continue;
			if (portalNetOptions.indexOf("value='" + s + "'") >= 0) continue;
			portalNetOptions += "<option value='" + s + "'>" + s + "  (" + String(WiFi.RSSI(best)) + " dBm)</option>";
		}
	}
	WiFi.scanDelete();

	WiFi.softAP(WIFI_SETUP_SSID);
	IPAddress apIP = WiFi.softAPIP();
	DNSServer dns;
	dns.start(53, "*", apIP);     // every name -> the controller: phones show the page by themselves

	server.on("/",       HTTP_GET,  portalSend);
	server.on("/save",   HTTP_POST, portalSave);
	server.on("/cancel", HTTP_POST, [](AsyncWebServerRequest* r){
		pendingCancel = true;
		r->send(200, "text/html", "<p style='font-family:sans-serif'>Cancelled - the controller restarts with its old setting.</p>");
	});
	server.onNotFound([apIP](AsyncWebServerRequest* r){ r->redirect("http://" + apIP.toString() + "/"); });
	server.begin();
	DEBUG_PRINTLN("WiFi: setup hotspot " WIFI_SETUP_SSID " at " + apIP.toString());

	MsTick.attach_ms(150, ISR_LED_Setup);   // fast red blink = setup mode

	uint32_t start = millis();
	for (;;){
		dns.processNextRequest();
		if (pendingSave){
			delay(1500);                  // let the "Saved" page reach the phone
			addNet(pendingCfg);
			SaveGenReport("WiFi: setup added network " + pendingCfg.ssid +
			              (pendingCfg.dhcp ? String(" (DHCP)") : " on " + pendingCfg.ip.toString()));
			ESP.restart();
		}
		if (pendingCancel){ delay(1000); ESP.restart(); }
		if (timeoutMs && millis() - start > timeoutMs){
			SaveGenReport F("WiFi: setup hotspot timed out - restarting to try the networks again");
			ESP.restart();
		}
		delay(5);
	}
}

// ── Public API ────────────────────────────────────────────────────────────────

// Deletes the saved (extra) networks.  The built-in ones stay in the firmware.
void ClearWifiSettings(){
	SPIFFS.remove(WIFI_CONFIG_FILE);
	SPIFFS.remove(WIFI_OLD_FILE);
	DEBUG_PRINTLN("WiFi: saved networks removed");
}

bool SetupWiFi() {
	WiFi.persistent(false);          // our own file, not the IDF NVS copy
	WiFi.setHostname(WIFI_HOSTNAME);
	WiFi.mode(WIFI_STA);
	WiFi.setSleep(WIFI_PS_NONE);

	// BOOT (GPIO 0) held LOW at power-on → setup hotspot (add / change a network).
	if (digitalRead(ASPRESET) == LOW) {
		DEBUG_PRINTLN("WiFi: BOOT button held — opening the setup hotspot");
		runSetupPortal(WIFI_PORTAL_MS, "BOOT button held at power-on");
	}

	// Nothing saved and nothing built in: nothing to try — set up (no time limit).
	if (!SPIFFS.exists(WIFI_CONFIG_FILE) && KNOWN_NET_COUNT == 0)
		runSetupPortal(0, "no WiFi network saved yet");

	for (int round = 0; round < WIFI_ROUNDS; round++) {
		if (round > 0) {
			DEBUG_PRINTF("WiFi: round %d failed - retry in %lu s\n", round, WIFI_ROUND_GAP_MS / 1000);
			delay(WIFI_ROUND_GAP_MS);
		}
		if (joinRound()) {
			HeartbeatSetMode(HB_MODE_NORMAL);   // blue — WiFi joined, running normally
			digitalWrite(LEDOUT, HIGH);
			delay(500);
			digitalWrite(LEDOUT, LOW);
			DEBUG_PRINT("WiFi: connected to "); DEBUG_PRINT(WiFi.SSID());
			DEBUG_PRINT(" ("); DEBUG_PRINT(WiFiSource); DEBUG_PRINT("), IP = ");
			DEBUG_PRINTLN(WiFi.localIP().toString());
			if (MDNS.begin(WIFI_HOSTNAME)) {
				MDNS.addService("http", "tcp", 80);
				DEBUG_PRINTLN("WiFi: mDNS started as " WIFI_HOSTNAME ".local");
			}
			return true;
		}
	}
	// Nothing joined for several minutes: open the setup hotspot for a while
	// (maybe the password changed), then reboot and try again.
	runSetupPortal(WIFI_PORTAL_MS, "no network could be joined");
	return false;   // not reached — the hotspot always reboots
}

// =============================================================================
// /wifi page — "WiFi Networks": list (never the passwords), DELETE, ADD
// =============================================================================
// Built in async_tcp; it only READS the file.  ADD / DELETE (POST
// /wifinet/add, /wifinet/delete) copy the request into one pending slot and
// raise a flag; MaintainWifiNets() (loop) writes the file.  Changes are used
// at the next start.

static WifiCfg       wnPendingAdd;
static String        wnPendingDel;
static volatile bool wnAddReq = false, wnDelReq = false;

static String htmlEsc(const String& s){
	String o; o.reserve(s.length() + 8);
	for (size_t i = 0; i < s.length(); i++){
		char c = s[i];
		if      (c == '&')  o += "&amp;";
		else if (c == '<')  o += "&lt;";
		else if (c == '>')  o += "&gt;";
		else if (c == '"')  o += "&quot;";
		else if (c == 39)   o += "&#39;";
		else                o += c;
	}
	return o;
}

static const char WN_BTN[] = "border:none;border-radius:4px;color:#fff;cursor:pointer;";

static const char WN_SECTION_TOP[] PROGMEM = R"HTML(
<div style='text-align:center;font-weight:800;letter-spacing:2px;color:#0284c7;font-size:15px;margin:16px 0 4px'>WIFI NETWORKS</div>
<p style='color:#475569;font-size:12px;padding:0 12px 4px'>At start-up the controller tries these networks, the strongest one in range first,
and joins the strongest access point of that network. If none can be reached for about 4 minutes it opens the hotspot <b>Prima3-Setup</b>
(also: hold BOOT at power-on) &mdash; connect a phone to it and add a network there. Changes are used at the next start. Passwords are never shown.</p>
<table style='width:100%;font-size:13px'>)HTML";

static const char WN_SECTION_ADD[] PROGMEM = R"HTML(</table>
<div style='margin:8px 12px;padding:8px;border:1px solid #cbd5e1;border-radius:6px;font-size:13px'>
<b>Add a network</b> (the same name again replaces that entry)<br>
<input id='wn_ssid' placeholder='WiFi network name' style='width:220px;margin:4px 0'>
<input id='wn_pass' type='password' placeholder='password (empty = open network)' style='width:220px;margin:4px 0'><br>
<label><input type='radio' name='wn_mode' id='wn_dhcp' onclick="wnFix(0)"> Automatic IP (DHCP)</label>
<label><input type='radio' name='wn_mode' id='wn_fixed' checked onclick="wnFix(1)"> Fixed IP</label>
<div id='wn_fx'>
<input id='wn_ip' placeholder='fixed IP, e.g. 192.168.1.118' style='width:220px;margin:2px 0'>
<input id='wn_gw' placeholder='gateway (empty = learned from the router)' style='width:220px;margin:2px 0'>
<input id='wn_sn' value='255.255.255.0' style='width:220px;margin:2px 0'>
<input id='wn_dns' placeholder='DNS (empty = gateway)' style='width:220px;margin:2px 0'></div>
<div style='margin-top:6px'><input type='button' value='ADD NETWORK' onclick='wnAdd()' style='font-size:14px;background-color:#166534;color:#fff;border:none;padding:6px 14px;border-radius:4px;cursor:pointer'></div>
<span id='wn_msg' style='font-size:12px'></span></div>
<script>
function wnFix(x){document.getElementById('wn_fx').style.display=x?'block':'none';}
function wnPost(u,b){var m=document.getElementById('wn_msg');
 fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
 .then(function(r){return r.text();}).then(function(t){m.textContent=t;if(t.indexOf('OK')==0)setTimeout(function(){location.reload();},1200);})
 .catch(function(){m.textContent='No answer from the controller';});}
function wnAdd(){var v=function(i){return encodeURIComponent(document.getElementById(i).value.trim());};
 var b='ssid='+v('wn_ssid')+'&pass='+encodeURIComponent(document.getElementById('wn_pass').value)+
 '&mode='+(document.getElementById('wn_fixed').checked?'fixed':'dhcp')+'&ip='+v('wn_ip')+'&gw='+v('wn_gw')+'&sn='+v('wn_sn')+'&dns='+v('wn_dns');
 wnPost('/wifinet/add',b);}
function wnDel(s,cur){if(!confirm('Delete the WiFi network '+s+'?'+(cur?' The controller is connected to it now: it stays connected until the next start, then tries the other networks.':'')))return;
 wnPost('/wifinet/delete','ssid='+encodeURIComponent(s));}
</script>
)HTML";

// GET /wifi — the WiFi Networks page.
void handleWifiNets(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	WifiCfg nets[WIFI_MAX_CAND];
	int n = buildCandidates(nets);
	String cur = WiFi.SSID();

	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	appendHtmlStr("<div class='nav'><a href='/'>HOME</a><a href='/settings'>SETTINGS</a><a class='act'>WIFI</a></div>");

	String now = "<div style='font-size:13px;text-align:center;margin-top:10px'>Connected to <b>" + htmlEsc(cur) +
	             "</b> via AP " + WiFi.BSSIDstr() + " (channel " + String(WiFi.channel()) + ", " +
	             String(WiFi.RSSI()) + " dBm) &mdash; IP " + WiFi.localIP().toString() +
	             " (" + WiFiSource + ")</div>";
	appendHtmlStr(now.c_str());

	appendHtmlStr(WN_SECTION_TOP);
	if (n == 0) appendHtmlStr("<tr><td colspan='3'>(no networks)</td></tr>");
	for (int i = 0; i < n; i++){
		String s  = htmlEsc(nets[i].ssid);
		bool   on = (nets[i].ssid == cur);
		String row = "<tr><td style='width:40%;padding:4px 12px'><b>" + s + "</b>" +
		             (on ? String(" <span style='color:#16a34a;font-size:11px'>(connected)</span>") : String("")) +
		             "</td><td style='font-size:12px;width:45%;text-align:left'>" +
		             (nets[i].dhcp ? String("automatic IP") : "fixed " + nets[i].ip.toString() +
		              ((uint32_t)nets[i].gw ? String("") : String(" (gateway learned at the first start)"))) +
		             "</td><td style='width:15%;white-space:nowrap'>";
		if (nets[i].builtIn)
			row += "<span style='font-size:11px;color:#64748b'>built in</span>";
		else
			row += "<button style='" + String(WN_BTN) + "background:#dc2626;padding:2px 8px;font-size:11px' data-s='" + s +
			       "' onclick='wnDel(this.dataset.s," + (on ? "1" : "0") + ")'>DELETE</button>";
		row += "</td></tr>";
		appendHtmlStr(row.c_str());
	}
	appendHtmlStr(WN_SECTION_ADD);
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

void handleWifiNetAdd(AsyncWebServerRequest *request){
	if (wnAddReq || wnDelReq){ request->send(200, "text/plain", "Busy - try again"); return; }
	WifiCfg c;
	String err = parseNetForm(request, c);
	if (err.length()){ request->send(200, "text/plain", err); return; }
	wnPendingAdd = c;
	wnAddReq = true;
	request->send(200, "text/plain", "OK - " + c.ssid + " added");
}

void handleWifiNetDelete(AsyncWebServerRequest *request){
	if (wnAddReq || wnDelReq){ request->send(200, "text/plain", "Busy - try again"); return; }
	String s = argTrim(request, "ssid");
	if (s.length() == 0){ request->send(200, "text/plain", "No network given"); return; }
	wnPendingDel = s;
	wnDelReq = true;
	request->send(200, "text/plain", "OK - " + s + " deleted");
}

// Called every 10 ms from loop().
void MaintainWifiNets(){
	if (wnAddReq){
		addNet(wnPendingAdd);
		SaveGenReport("WiFi: network " + wnPendingAdd.ssid + " added on the WiFi page" +
		              (wnPendingAdd.dhcp ? String(" (automatic IP)") : " (fixed " + wnPendingAdd.ip.toString() + ")"));
		wnPendingAdd = WifiCfg();
		wnAddReq = false;
	}
	if (wnDelReq){
		if (deleteNet(wnPendingDel)) SaveGenReport("WiFi: network " + wnPendingDel + " deleted on the WiFi page");
		else                         SaveGenReport("WiFi: " + wnPendingDel + " not deleted (built in or not saved)");
		wnDelReq = false;
	}
}
