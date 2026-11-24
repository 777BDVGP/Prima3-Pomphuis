#pragma once

// =============================================================================
// 15_About.h — OTA Update Service & About Page
// =============================================================================
// Includes ArduinoOTA, declares OTA state variables exported from 15_About.cpp,
// and defines the WebSocket ID base and navigation index for the About page.
// =============================================================================

#include <ArduinoOTA.h>

// ─── Navigation Index ─────────────────────────────────────────────────────────
#define PS_ABOUT        10   // Navigation index constant for About page

// ─── OTA State (defined in 15_About.cpp) ─────────────────────────────────────
extern bool OTAInProgress;       // true while an OTA upload is in progress
extern int  OTAPercentComplete;  // Upload progress 0–100 %

