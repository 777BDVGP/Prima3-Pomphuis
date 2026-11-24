#pragma once

// =============================================================================
// 22_Update.h — Firmware update from GitHub or from an uploaded .bin (ABOUT)
// =============================================================================
// Ported from RAINGAUGE/RainGauge_PIO 22_Update (2026-11-24).
// ABOUT page, "FIRMWARE UPDATE":
//
//   CHECK FOR UPDATE  reads FW_UPDATE_URL "version.json" from GitHub Pages:
//                     {"version":"1.1.0","bin":"firmware/prima3.bin","notes":"..."}
//   INSTALL           downloads that .bin into the spare OTA slot and restarts.
//   UPLOAD .bin       a file chosen on a PC / phone (POST /update).
//
// Both ways write the NEW firmware into the spare app partition (2 x 1.5 MB,
// partitions_ota.csv); the controller only switches to it when the whole
// image arrived and verified.  A broken download / wrong file leaves the
// running firmware untouched.  Settings and logs (SPIFFS) are never touched.
// A file must be an ESP32-S3 image AND contain FW_MARKER, so firmware of
// another project is refused.
//
// Not while irrigating or while the borehole pump runs (the restart would
// cut the run short) — MaintainFwUpdate() refuses with a status message.
//
// HTTPS: only the roots in GH_ROOT_CERTS are trusted (certificate IS
// checked).  The clock must be set (NTP).  mbedTLS buffers go to PSRAM while
// downloading.
//
// The GitHub check / install run in loop() and BLOCK it (HTTPClient) — the
// one accepted exception to "no blocking network calls in loop()": started by
// hand, takes ~20-60 s.  The watchdog is fed.
// =============================================================================

	#define FW_UPDATE_URL   "https://777bdvgp.github.io/Prima3-Pomphuis/"   // GitHub Pages of the public repo
	#define FW_MARKER       "#PRIMA3-POMPHUIS-S3-FIRMWARE"                  // must be in every Prima3 .bin ('#' once)

	inline String FwLatest   = "-";     // version on GitHub (after CHECK)
	inline String FwBinPath  = "";      // "firmware/prima3.bin" from version.json
	inline String FwNotes    = "";
	inline String FwStatus   = "-";     // shown live on ABOUT
	inline volatile bool FwBusy = false;  // an update (GitHub or upload) is running
