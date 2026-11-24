#pragma once

// =============================================================================
// wifi_secrets.example.h — template for wifi_secrets.h
// =============================================================================
// You do NOT need this file: without wifi_secrets.h the controller opens the
// WiFi hotspot "Prima3-Setup" and you choose your network on a phone.
// Only for networks built into the firmware: copy this file to
// wifi_secrets.h (same folder) and fill it in.  It is in .gitignore.
// =============================================================================

	#define WIFI_PASS_HOME   "your-password-here"

// Built-in networks: SSID, password (05_WiFi.cpp).  They always join on the
// firmware's fixed IP NAV_IP (01_Main.h — DEBUG 1 = .120, DEBUG 0 = .118).
// Extra networks can be added on the /wifi page or the Prima3-Setup hotspot.
	#define WIFI_KNOWN_NETS \
		{ "MyHomeWiFi", WIFI_PASS_HOME },

// Optional: default names of the two borehole users (otherwise "User 1" /
// "User 2"; they can always be changed on the Tank page).
// 	#define DEFAULT_USER_1 "Neighbour"
// 	#define DEFAULT_USER_2 "Me"
