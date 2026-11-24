// =============================================================================
// 11_Actions.cpp — Actions Page (Stub)
// =============================================================================
// Placeholder for a future automated actions/sequences page.
// Currently renders an empty page body.  Add action form elements to
// append_Form_Actions() and register handlers in AddRoutes() when implemented.
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// handleActions  (GET /actions)
// Serves the (currently empty) actions page.
// -----------------------------------------------------------------------------
void handleActions(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	CompilePage_Actions(request);
}

// -----------------------------------------------------------------------------
// CompilePage_Actions
// Builds and serves the actions page.  The navigation menu is omitted until
// this page is integrated into the main navigation.
// -----------------------------------------------------------------------------
void CompilePage_Actions(AsyncWebServerRequest *request)
{
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_06);
	//     appendMainPageSel(PS_ACTIONS);  // Uncomment when added to navigation
	append_Form_Actions();                    
	appendHtmlStr(webSocketScript);           
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// append_Form_Actions
// Returns the HTML form content for the actions page.
// Currently returns an empty string — to be implemented.
// -----------------------------------------------------------------------------
void append_Form_Actions()
{
}
