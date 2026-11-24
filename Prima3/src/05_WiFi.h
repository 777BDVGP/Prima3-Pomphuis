#pragma once

// =============================================================================
// 05_WiFi.h — WiFi join (saved + built-in networks) + setup hotspot
// =============================================================================
// SetupWiFi() (called once from setup()):
//
//   1. BOOT (GPIO 0) held LOW at power-on -> setup hotspot "Prima3-Setup"
//      (open, http://192.168.4.1/, captive DNS, 5 min then reboot).
//   2. Candidates = networks saved in /WiFiConfig.json (added on the hotspot
//      or the /wifi page, max 8) + the built-in WIFI_KNOWN_NETS from the
//      git-ignored wifi_secrets.h (fixed IP = NAV_IP, so DEBUG picks .118/.120).
//      Scan, try them strongest in range first, then the ones not seen.
//   3. Every join scans all channels and picks the STRONGEST access point of
//      the SSID (mesh / multi-AP) — logged with its BSSID in the General Log.
//   4. Tried WIFI_ROUNDS times; if nothing joins, the setup hotspot opens for
//      5 minutes (maybe the WiFi password changed), then reboot and try again.
//
// /wifi page "WiFi Networks" lists the networks (never the passwords), with
// DELETE (saved ones only) and ADD.
// =============================================================================

// Filled in by SetupWiFi() — shown on the /wifi page.
	inline String WiFiSource = "";   // "saved" or "built-in"

bool SetupWiFi();
