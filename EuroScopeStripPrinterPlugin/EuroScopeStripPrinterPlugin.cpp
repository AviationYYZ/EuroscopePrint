// EuroScopeStripPrinterPlugin.cpp
//
// Matches your SDK header expectations:
// - We neutralize DllSpecEuroScope AND also define ESINDEX to keep SDK typedefs valid.
// - CPlugIn ctor uses 5 args: (COMPATIBILITY_CODE, Name, Version, Author, Copyright).
// - Export signatures: void EuroScopePlugInInit(EuroScopePlugIn::CPlugIn**);
//                       void EuroScopePlugInExit(void);
// - We only override callbacks that exist in your header.
// - We launch StripPrinter.exe (must be beside EuroScope.exe) with a temp UTF-8 file.


#include <windows.h>
#include <string>
#include <unordered_set>
#include <sstream>
#include "EuroScopePlugIn.h"

using namespace EuroScopePlugIn;

// ---------- helper: write UTF-8 to temp file and launch StripPrinter.exe ----------
static void LaunchStripPrinterWithPayload(const std::string& utf8)
{
    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath)) return;

    wchar_t tempFile[MAX_PATH];
    if (!GetTempFileNameW(tempPath, L"ESFP", 0, tempFile)) return;

    // Write payload (UTF-8) to temp file
    HANDLE h = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(h);

    // Build command line:  "StripPrinter.exe" --file "<tempFile>"
    const wchar_t* exe = L"StripPrinter.exe";
    std::wstring cmd;
    cmd.reserve(512);
    cmd.append(L"\"");
    cmd.append(exe);
    cmd.append(L"\" --file \"");
    cmd.append(tempFile);
    cmd.append(L"\"");

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

// ---------- minimal plugin implementation ----------
class StripPrinterPlugin : public CPlugIn
{
public:
    StripPrinterPlugin()
    : CPlugIn(
        COMPATIBILITY_CODE,        // from your header
        "Strip Printer",           // plugin name
        "0.1.0",                   // version
        "Charlie Yablon",          // author
        "© 2025"                   // copyright
      )
    {
        // No SetPluginName/StartTimer in this SDK; rely on data-update callback.
    }

    ~StripPrinterPlugin() override = default;

    // Callback present in your SDK: fires when FP data changes.
    void OnFlightPlanFlightPlanDataUpdate(CFlightPlan fp) override
    {
        TryPrintFor(fp);
    }

private:
    std::unordered_set<std::string> printed_; // avoid duplicate printing per callsign

    static const char* nz(const char* s) { return s ? s : ""; }

    void TryPrintFor(const CFlightPlan& fp)
    {
        if (!fp.IsValid()) return;

        const char* cs = fp.GetCallsign();
        if (!cs || !cs[0]) return;

        // Only print once per callsign in this EuroScope session
        if (printed_.count(cs)) return;

        CFlightPlanData d = fp.GetFlightPlanData();
        const char* dep   = nz(d.GetOrigin());
        const char* arr   = nz(d.GetDestination());
        const char* route = nz(d.GetRoute());

        // Simple "likely prefile" heuristic: has basic FP fields.
        if (!dep[0] && !arr[0]) return;

        // Build a compact text strip (UTF-8)
        std::ostringstream ss;
        ss << "================ FLIGHT STRIP ================\n";
        ss << "CS: " << cs << "   DEP: " << dep << "   ARR: " << arr << "\n";
        ss << "ROUTE: " << route << "\n";
        ss << "==============================================\n";

        LaunchStripPrinterWithPayload(ss.str());
        printed_.insert(cs);
    }
};

// Global plugin instance pointer required by the SDK exports
static StripPrinterPlugin* g_plugin = nullptr;

// Export signatures MUST match the header (no extern "C" needed here, .def handles names)
void EuroScopePlugInInit(EuroScopePlugIn::CPlugIn** ppPlugInInstance)
{
    g_plugin = new StripPrinterPlugin();
    *ppPlugInInstance = g_plugin;
}

void EuroScopePlugInExit(void)
{
    delete g_plugin;
    g_plugin = nullptr;
}
