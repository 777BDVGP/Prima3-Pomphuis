// =============================================================================
// 03_Files.cpp — SPIFFS Initialisation & Web File-Manager Route Handlers
// =============================================================================
// Handles SPIFFS startup and first-boot formatting, creates all persistent
// data objects, and provides HTTP route handlers for the browser-based file
// manager (list, download, upload, delete).
// =============================================================================

#include <Arduino.h>
#include "all_includes.h"

// -----------------------------------------------------------------------------
// InitFileSystem
// Mounts SPIFFS.  If the format-complete marker file is absent (first boot or
// after an explicit erase) the partition is reformatted and the marker is
// re-created.  Then all runtime data objects are constructed and loaded from
// their respective JSON files on SPIFFS.
// Note: diagnostic text is appended to FSInitLog (printed in setup() after
//       the boot delay so it is not lost in the Serial buffer).
// -----------------------------------------------------------------------------
void InitFileSystem(){

	File f;

	SPIFFS.begin();

	if (!SPIFFS.exists(StrFormatComplete)){
		// First-boot path: format the SPIFFS partition
		FSInitLog += "Please wait 30 secs for SPIFFS to be formatted.....\n";
		if (SPIFFS.format()){
			SPIFFS.begin();   // Remount after format — required before any file operations
			FSInitLog += "SPIFFS formatted successfully\n";

			f = SPIFFS.open(StrFormatComplete, "w");
			if (!f){
				FSInitLog += "ERROR: Could not create FormatComplete.txt\n";
			}
			else{
				f.println("Deleting this file will cause SPIFFS to be reformatted on the next boot.");
				f.close();
				FSInitLog += "FormatComplete.txt created\n";
			}
		}
		else{
			FSInitLog += "ERROR: SPIFFS format failed\n";
		}
	}
	else{
		FSInitLog += "File System mounted successfully\n";
	}

	// ── Create All Runtime Data Objects ──────────────────────────────────────
	// Each function constructs its object and calls ReadData() to load stored
	// settings from SPIFFS JSON files.
	CreateIrrigation();       // 9-zone irrigation schedules (09_Irrigation.cpp)
	CreateSheduleObj();       // Irrigation schedule state machine (09_Irrigation.cpp)
	CreateSuspendDays();      // 7-day suspension calendar (09_Suspend.cpp)
	CreateElectricValves();   // 7 solenoid valves + 9 configurations (09_WaterValves.cpp)
	CreateAllParams();        // System parameters: name, logging flags, boot delay (12_Settings.cpp)
	CreatePresureSensors();   // TankLevel and FilterSensor objects — must precede CreateNtfyParams (09_Tank.cpp)
	CreateNtfyParams();       // NTFY push notification settings — binds to FilterSensor fields (16_Ntfy.cpp)

	// Allocate the HTML assembly buffer from 2 MB PSRAM — 4 bytes of DRAM for the
	// pointer; zero DRAM for the 22 KB buffer.  Must run after SPIFFS.begin() so
	// the heap/PSRAM allocator is fully initialised.
	HtmlBuf = (char*)ps_malloc(HTML_BUF_SIZE);
	if (HtmlBuf) HtmlBuf[0] = '\0';
	else FSInitLog += "ERROR: ps_malloc failed for HtmlBuf\n";
}

// =============================================================================
// SPIFFS Web File Manager — Route Handlers
// =============================================================================

// ─── File Listing Page ────────────────────────────────────────────────────────

// -----------------------------------------------------------------------------
// FilesPage  (GET /files)
// POWER_MONITOR-style file manager: filesystem usage bar, sortable file table
// with inline DL / View / Del actions, in-page file viewer, and upload form.
// File data is fetched from /api/files via JS fetch() on page load.
// -----------------------------------------------------------------------------
void FilesPage(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);
	append_FileAction(FA_MESS_07);
	FilePageSel(PS_FILES);
	appendHtmlStr(webSocketScript);

	// Filesystem usage bar (populated by JS)
	appendHtmlStr("<div id='fsinfo' style='padding:4px 8px 2px'>"
	              "<p style='color:#475569;font-size:12px;padding:4px 0'>Loading&hellip;</p></div>");

	// File list header row
	appendHtmlStr("<div style='display:flex;align-items:center;justify-content:space-between;"
	              "padding:4px 8px 2px'>"
	              "<b style='font-size:13px;color:#0f172a'>Files on SPIFFS</b>"
	              "<button onclick='loadFiles()' style='background:#0284c7;border:none;color:#fff;"
	              "padding:4px 10px;font-size:12px;border-radius:4px;cursor:pointer'>"
	              "&#8635; Refresh</button></div>");

	// File table placeholder (replaced by JS)
	appendHtmlStr("<div id='ftbl'>"
	              "<p style='color:#475569;padding:10px;font-size:13px'>Loading&hellip;</p></div>");

	// File viewer panel (hidden until View is clicked)
	appendHtmlStr("<div id='vpanel' style='display:none;margin:4px 0'>");
	appendHtmlStr("<div style='display:flex;align-items:center;justify-content:space-between;"
	              "background:#e2e8f0;padding:4px 8px;border-radius:4px 4px 0 0'>"
	              "<span id='vfname' style='font-family:monospace;font-size:11px;color:#0f172a'></span>"
	              "<button onclick='closeView()' style='background:#dc2626;border:none;color:#fff;"
	              "padding:3px 8px;font-size:12px;border-radius:4px;cursor:pointer'>"
	              "&#10005; Close</button></div>");
	appendHtmlStr("<pre id='vcontent' style='max-height:320px;overflow:auto;background:#f8fafc;"
	              "border:1px solid #e2e8f0;border-radius:0 0 4px 4px;padding:8px;"
	              "font-size:11px;white-space:pre-wrap;word-break:break-all;margin:0'></pre>");
	appendHtmlStr("</div>");

	// Upload section
	appendHtmlStr("<div style='padding:8px;margin-top:4px'>"
	              "<b style='font-size:13px;color:#0f172a'>Upload File</b><br><br>"
	              "<form method='POST' action='/fupload' enctype='multipart/form-data'>"
	              "<input type='file' name='fupload' "
	              "style='margin-bottom:10px;display:block;width:100%;font-size:13px'>"
	              "<button class='btn btn-bl' type='submit'>&#8679; Upload</button>"
	              "</form></div>");

	appendHtmlStr(R"JS(<script>
function fmtSz(b){if(b<1024)return b+' B';if(b<1048576)return (b/1024).toFixed(1)+' KB';return (b/1048576).toFixed(1)+' MB';}
function loadFiles(){
  fetch('/api/files').then(function(r){return r.json();}).then(function(j){
    var pct=Math.round(j.used/j.total*100);
    document.getElementById('fsinfo').innerHTML=
      "<div style='padding:4px 8px 2px;font-size:12px;color:#475569'>Used: "+fmtSz(j.used)+" of "+fmtSz(j.total)+" ("+pct+"%)</div>"
      +"<div style='background:#e2e8f0;border-radius:4px;height:6px;margin:0 8px 8px'>"
      +"<div style='background:#0284c7;height:6px;border-radius:4px;width:"+pct+"%'></div></div>";
    if(!j.files||j.files.length===0){document.getElementById('ftbl').innerHTML="<p style='color:#475569;padding:10px;font-size:13px'>No files.</p>";return;}
    function extOf(n){var i=n.lastIndexOf('.');return i<0?'':n.substring(i+1).toLowerCase();}
    function nameColor(n){var e=extOf(n);return e==='json'?'#dc2626':e==='txt'?'#0f172a':'#15803d';}
    j.files.sort(function(a,b){var ea=extOf(a.name),eb=extOf(b.name);return ea<eb?-1:ea>eb?1:a.name<b.name?-1:a.name>b.name?1:0;});
    var h='<table><tr class="hd"><td>File Name</td><td style="text-align:right">Size</td><td></td></tr>';
    j.files.forEach(function(f){
      var enc=encodeURIComponent(f.name);
      var col=nameColor(f.name);
      h+='<tr><td style="font-family:monospace;font-size:12px;color:'+col+'">'+f.name+'</td>'
        +'<td style="font-size:12px;color:#64748b;white-space:nowrap;text-align:right">'+fmtSz(f.size)+'</td>'
        +'<td style="white-space:nowrap;padding-right:0;text-align:right">'
        +'<a href="/download?f_File='+enc+'" style="display:inline-block;padding:3px 8px;background:#0284c7;color:#fff;border-radius:4px;font-size:11px;text-decoration:none;margin-right:3px">&#8595;&nbsp;DL</a>'
        +'<button onclick="viewFile(\''+f.name+'\')" style="padding:3px 8px;background:#475569;color:#fff;border:none;border-radius:4px;font-size:11px;cursor:pointer;margin-right:3px">&#128065;&nbsp;View</button>'
        +'<button onclick="delFile(\''+f.name+'\')" style="padding:3px 8px;background:#dc2626;color:#fff;border:none;border-radius:4px;font-size:11px;cursor:pointer">&#10005;&nbsp;Del</button>'
        +'</td></tr>';
    });
    h+='</table>';
    document.getElementById('ftbl').innerHTML=h;
  }).catch(function(){document.getElementById('ftbl').innerHTML="<p style='color:#dc2626;padding:10px;font-size:13px'>Failed to load.</p>";});
}
function delFile(name){
  if(!confirm('Delete '+name+'?\nThis cannot be undone.'))return;
  fetch('/files/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'f='+encodeURIComponent(name)})
    .then(function(r){return r.json();})
    .then(function(j){if(j.ok)loadFiles();else alert('Delete error: '+j.msg);})
    .catch(function(e){alert('Delete failed: '+e);});
}
function viewFile(name){
  document.getElementById('vcontent').textContent='Loading…';
  document.getElementById('vfname').textContent=name;
  document.getElementById('vpanel').style.display='block';
  document.getElementById('vpanel').scrollIntoView({behavior:'smooth'});
  fetch('/files/view?f='+encodeURIComponent(name))
    .then(function(r){return r.text();})
    .then(function(t){document.getElementById('vcontent').textContent=t;})
    .catch(function(){document.getElementById('vcontent').textContent='(error loading file)';});
}
function closeView(){document.getElementById('vpanel').style.display='none';document.getElementById('vcontent').textContent='';}
loadFiles();
</script>)JS");

	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// FilePageSel
// Builds the file-manager navigation tab strip.
// -----------------------------------------------------------------------------
void FilePageSel(int Index){
	appendHtmlStr("<div class='nav'>");
	appendHtmlStr(Index == PS_FILES_HOME ? "<a class='act'>HOME</a>" : "<a href='/'>HOME</a>");
	appendHtmlStr(Index == PS_FILES      ? "<a class='act'>FILES</a>" : "<a href='/files'>FILES</a>");
	appendHtmlStr("</div>");
}

// -----------------------------------------------------------------------------
// handleApiFiles  (GET /api/files)
// Returns a JSON object with SPIFFS usage stats and an array of all files.
// Used by the FilesPage JS to build the file table without a server-side reload.
// -----------------------------------------------------------------------------
void handleApiFiles(AsyncWebServerRequest *request) {
	DynamicJsonDocument doc(4096);
	doc["used"]  = (size_t)SPIFFS.usedBytes();
	doc["total"] = (size_t)SPIFFS.totalBytes();
	JsonArray arr = doc.createNestedArray("files");
	File root = SPIFFS.open("/");
	File f    = root.openNextFile();
	while (f) {
		String fname = String(f.name());
		if (fname.startsWith("/")) fname = fname.substring(1);
		if (IsProtectedFile(fname)) { f = root.openNextFile(); continue; }
		JsonObject o = arr.createNestedObject();
		o["name"] = fname;
		o["size"] = (size_t)f.size();
		f = root.openNextFile();
	}
	String json;
	serializeJson(doc, json);
	request->send(200, "application/json", json);
}

// -----------------------------------------------------------------------------
// handleFileView  (GET /files/view?f=<filename>)
// Serves the named SPIFFS file as text/plain so the browser can display it
// inside the in-page viewer panel.
// -----------------------------------------------------------------------------
void handleFileView(AsyncWebServerRequest *request) {
	if (!request->hasArg("f")) { request->send(400, "text/plain", "Missing filename"); return; }
	String path = "/" + request->arg("f");
	if (IsProtectedFile(path)) { request->send(403, "text/plain", "Protected file"); return; }
	if (!SPIFFS.exists(path)) { request->send(404, "text/plain", "Not found"); return; }
	request->send(SPIFFS, path, "text/plain");
}

// -----------------------------------------------------------------------------
// handleFileDeleteJson  (GET /files/delete?f=<filename>)
// Deletes the named SPIFFS file and returns a JSON result so the JS caller
// can refresh the file list without a full page reload.
// -----------------------------------------------------------------------------
void handleFileDeleteJson(AsyncWebServerRequest *request) {
	if (!request->hasArg("f")) {
		request->send(200, "application/json", "{\"ok\":false,\"msg\":\"missing param\"}");
		return;
	}
	String target = request->arg("f");
	if (!target.startsWith("/")) target = "/" + target;

	// Walk SPIFFS to obtain the exact stored path — avoids SPIFFS.remove() quirks
	// with special characters (spaces etc.) that arise from path construction.
	String exactPath = "";
	{
		File root = SPIFFS.open("/");
		File entry = root.openNextFile();
		while (entry) {
			String stored = String(entry.name());
			if (!stored.startsWith("/")) stored = "/" + stored;
			if (stored == target) { exactPath = stored; break; }
			entry = root.openNextFile();
		}
		// root and entry go out of scope — shared_ptr ref-count → 0 → files closed
	}

	if (exactPath.length() == 0) {
		// Include what we were looking for to help diagnose mismatches
		String j = "{\"ok\":false,\"msg\":\"not found: " + target + "\"}";
		request->send(200, "application/json", j);
		return;
	}
	if (SPIFFS.remove(exactPath)) {
		request->send(200, "application/json", "{\"ok\":true}");
	} else {
		request->send(200, "application/json", "{\"ok\":false,\"msg\":\"remove failed\"}");
	}
}

// ─── File Download ────────────────────────────────────────────────────────────

// -----------------------------------------------------------------------------
// File_Download  (GET /download)
// If a filename query parameter is present, streams the file directly to the
// browser.  Otherwise renders the download selection page.
// -----------------------------------------------------------------------------
void File_Download(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	if (request->args() > 0 && request->hasParam("f_File")){
		DownloadFile(request, request->getParam((size_t)0)->value());
	}
	else{
		CompilePage_Download(request);
	}
}

// -----------------------------------------------------------------------------
// CompilePage_Download
// Builds and serves the file download selection page.
// -----------------------------------------------------------------------------
void CompilePage_Download(AsyncWebServerRequest *request){
	resetHtmlBuf();    
	appendHtmlStr(webHeadPage);           
	append_FileAction(FA_MESS_02);        
	FilePageSel(PS_FILES_DOWNLOAD);       
	appendHtmlStr(webSocketScript);       
	append_Form_Download();               
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// append_Form_Download
// Builds a clickable list of all SPIFFS files; each entry is a hyperlink that
// triggers the download via the "/download?f_File=<name>" route.
// -----------------------------------------------------------------------------
void append_Form_Download()
{
	char rowbuf[256];
	CreateDFileList("/");
	appendHtmlStr("<form><table>");
	for (int y = 0; y < StructIdx; y++){
		snprintf(rowbuf, sizeof(rowbuf),
		         "<tr><th><p><lk><a href='/download?f_File=%s'>%s</a></lk></p></th></tr>",
		         SortedFiles[y].FileName, SortedFiles[y].FileName);
		appendHtmlStr(rowbuf);
	}
	appendHtmlStr("</table></form>");
}

// -----------------------------------------------------------------------------
// DownloadFile
// Streams the named SPIFFS file to the browser as a binary download.
// Redirects back to the download page with an error message if not found.
// -----------------------------------------------------------------------------
void DownloadFile(AsyncWebServerRequest *request, String filename)
{
	String path = "/" + filename;
	if (SPIFFS.exists(path) && !IsProtectedFile(path)){
		request->send(SPIFFS, path, "application/octet-stream", true);
	}
	else{
		SetMess    = FA_MESS_RED;
		SpesialStr = "File "; SpesialStr += filename; SpesialStr += " not available";
		CompilePage_Download(request);
	}
}

// ─── File Upload ──────────────────────────────────────────────────────────────

// -----------------------------------------------------------------------------
// File_Upload  (GET /upload)
// Serves the multipart upload form.  The actual file data arrives at /fupload
// and is handled by handleFileUpload().
// -----------------------------------------------------------------------------
void File_Upload(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	resetHtmlBuf();
	appendHtmlStr(webHeadPage);               
	append_FileAction(FA_MESS_03);            
	FilePageSel(PS_FILES_UPLOAD);             
	appendHtmlStr(webSocketScript);           
	appendHtmlStr("<h3>  </h3>");             
	appendHtmlStr("<FORM action='/fupload' method='post' enctype='multipart/form-data'>");  
	appendHtmlStr("<input class='buttons' style='width:90%' type='file' name='fupload' id='fupload' value=''><br>");  
	appendHtmlStr("<br><button class='buttons' style='width:50%' type='submit'>Upload File</button><br><br>");  
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// handleFileUpload  (multipart handler for POST /fupload)
// Called repeatedly by ESPAsyncWebServer as upload data arrives in chunks.
// On the first call (index == 0) any existing file with the same name is
// removed and a new file is opened for writing.  On the final call the file
// is closed and a status message is set for the redirect back to /files.
// -----------------------------------------------------------------------------
File UploadFile;
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
{
	if (index == 0){
		if (!filename.startsWith("/")) filename = "/" + filename;
		SPIFFS.remove(filename);                   // Remove any previous version of this file
		UploadFile = SPIFFS.open(filename, "w");   // Open for writing
	}

	if (UploadFile) UploadFile.write(data, len);   // Append this chunk to the file

	if (final){
		if (UploadFile){
			UploadFile.close();
			SetMess    = FA_MESS_GRN;
			SpesialStr = "File "; SpesialStr += filename; SpesialStr += " uploaded successfully";
		}
		else{
			SetMess    = FA_MESS_RED;
			SpesialStr = "File upload failed";
		}
		// The redirect to /files is handled by the onRequest lambda registered in AddRoutes()
	}
}

// ─── File Delete ──────────────────────────────────────────────────────────────

// -----------------------------------------------------------------------------
// File_Delete  (GET /delete)
// If a filename query parameter is present, deletes that file.
// Otherwise renders the delete selection page.
// -----------------------------------------------------------------------------
void File_Delete(AsyncWebServerRequest *request){
	if (!beginHtmlStream(request)) return;
	if (request->args() > 0){
		SPIFFS_file_delete(request, request->getParam((size_t)0)->value());
	}
	else{
		Compile_PageDelete(request);
	}
}

// -----------------------------------------------------------------------------
// Compile_PageDelete
// Builds and serves the file delete selection page.
// -----------------------------------------------------------------------------
void Compile_PageDelete(AsyncWebServerRequest *request)
{
	resetHtmlBuf();    
	appendHtmlStr(webHeadPage);           
	append_FileAction(FA_MESS_02);        
	FilePageSel(PS_FILES_DELETE);         
	appendHtmlStr(webSocketScript);       
	append_Form_Delete();                 
	appendHtmlStr(webHTMLEnd);
	sendHtmlPage(request);
}

// -----------------------------------------------------------------------------
// append_Form_Delete
// Builds a clickable list of all SPIFFS files; clicking a filename sends a
// GET request to "/delete?f_File=<name>" which triggers deletion.
// -----------------------------------------------------------------------------
void append_Form_Delete()
{
	char rowbuf[256];
	CreateDFileList("/");
	appendHtmlStr("<form><table>");
	for (int y = 0; y < StructIdx; y++){
		snprintf(rowbuf, sizeof(rowbuf),
		         "<tr><th><p><lk><a href='/delete?f_File=%s'>%s</a></lk></p></th></tr>",
		         SortedFiles[y].FileName, SortedFiles[y].FileName);
		appendHtmlStr(rowbuf);
	}
	appendHtmlStr("</table></form>");
}

// -----------------------------------------------------------------------------
// SPIFFS_file_delete
// Attempts to delete the named file from SPIFFS.  Sets the status message to
// green on success or red on failure, then reloads the delete page.
// -----------------------------------------------------------------------------
void SPIFFS_file_delete(AsyncWebServerRequest *request, String filename)
{
	File dataFile = SPIFFS.open("/" + filename, "r");
	if (dataFile){
		dataFile.close();
		if (SPIFFS.remove("/" + filename)){
			SetMess    = FA_MESS_GRN;
			SpesialStr = "File "; SpesialStr += filename; SpesialStr += " deleted successfully";
		}
		else{
			SetMess    = FA_MESS_RED;
			SpesialStr = "File "; SpesialStr += filename; SpesialStr += " could not be deleted";
		}
	}
	else{
		SetMess    = FA_MESS_RED;
		SpesialStr = "File "; SpesialStr += filename; SpesialStr += " not found";
	}
	Compile_PageDelete(request);
}

// =============================================================================
// SPIFFS Directory Builder
// =============================================================================

// -----------------------------------------------------------------------------
// CreateDFileList
// Scans the SPIFFS directory at 'dir', populates SortedFiles[] with up to
// File_array_LEN entries, then bubble-sorts them alphabetically by a sort key
// formed from "<extension>.<name>" so that files are grouped by type.
// Sets StructIdx to the actual number of entries found.
// -----------------------------------------------------------------------------
void CreateDFileList(const char *dir)
{
	StructIdx = 0;
	File root = SPIFFS.open(dir);
	File file = root.openNextFile();

	// ── Populate the array ────────────────────────────────────────────────────
	while (file > 0){
		String FileName = file.name();
		int Start = FileName.indexOf(".") + 1;
		// Sort key: extension first, then filename — groups files by type
		String SortName = FileName.substring(Start, String(file.name()).length());
		SortName += String(file.name()).substring(0, Start);

		if (StructIdx < File_array_LEN && !IsProtectedFile(FileName)){
			FileName.toCharArray(SortedFiles[StructIdx].FileName, FileName.length() + 1);
			SortName.toCharArray(SortedFiles[StructIdx].SortName, SortName.length() + 1);
			SortedFiles[StructIdx].FileSize = file.size();
			StructIdx++;
		}
		file = root.openNextFile();
	}

	// ── Bubble-sort alphabetically by SortName ────────────────────────────────
	for (int x = StructIdx; x > 0; x--){
		for (int y = 0; y < (x - 1); y++){
			String TOPS = String(SortedFiles[y].SortName);
			String BOTS = String(SortedFiles[y + 1].SortName);
			if (TOPS > BOTS){
				// Swap entries y and y+1
				int    TOPZ = SortedFiles[y].FileSize;
				String TOPF = String(SortedFiles[y].FileName);
				String BOTF = String(SortedFiles[y + 1].FileName);

				BOTS.toCharArray(SortedFiles[y].SortName,     BOTS.length() + 1);
				BOTF.toCharArray(SortedFiles[y].FileName,     BOTF.length() + 1);
				SortedFiles[y].FileSize = SortedFiles[y + 1].FileSize;

				TOPS.toCharArray(SortedFiles[y + 1].SortName, TOPS.length() + 1);
				TOPF.toCharArray(SortedFiles[y + 1].FileName, TOPF.length() + 1);
				SortedFiles[y + 1].FileSize = TOPZ;
			}
		}
	}
}
