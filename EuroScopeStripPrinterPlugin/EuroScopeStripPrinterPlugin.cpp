#include <windows.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <sstream>
#include "EuroScopePlugIn.h"
#pragma comment(linker, "/export:EuroScopePlugInInit")
#pragma comment(linker, "/export:EuroScopePlugInExit")
using namespace EuroScopePlugIn;

// =====================
// CONFIG: EDIT THIS LIST
// =====================

// Airports considered “in my control zone” (ICAO codes, case-insensitive).
// Example: Montreal TWR might include CYUL (Dorval) and CYHU (St-Hubert).
static std::vector<std::string> kAirports = {
    "CYTZ"   // <-- add/remove your aerodromes here
    "CYYZ"
    "CYXU"
    "CYKF"
};

// Filter mode — choose one:
//   "departures"  → only print if DEP ICAO is in kAirports
//   "arrivals"    → only print if ARR ICAO is in kAirports
//   "both"        → print if either DEP or ARR is in kAirports
static const char* kFilterMode = "both";

// =====================
// END CONFIG
// =====================

static inline std::string upcase(const char* s) {
    if (!s) return {};
    std::string r(s);
    std::transform(r.begin(), r.end(), r.begin(), [](unsigned char c){ return (char)std::toupper(c); });
    return r;
}

static inline bool in_list(const std::string& s, const std::vector<std::string>& v) {
    for (const auto& x : v) if (s == x) return true;
    return false;
}

// Write UTF-8 payload to a temp file and launch StripPrinter.exe with --file <temp>
static void LaunchStripPrinterWithPayload(const std::string& utf8)
{
    wchar_t tempPath[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, tempPath)) return;

    wchar_t tempFile[MAX_PATH];
    if (!GetTempFileNameW(tempPath, L"ESFP", 0, tempFile)) return;

    HANDLE h = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;

    DWORD written = 0;
    WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    CloseHandle(h);

    const wchar_t* exe = L"StripPrinter.exe";
    std::wstring cmd;
    cmd.reserve(512);
    cmd.append(L"\"");
    cmd.append(exe);
    cmd.append(L"\" --file \"");
    cmd.append(tempFile);
    cmd.append(L"\"");

    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
    { CloseHandle(pi.hThread); CloseHandle(pi.hProcess); }
}

// Make a stable signature from FP fields we trust across SDK versions
static std::string MakeSignature(const EuroScopePlugIn::CFlightPlanData& d)
{
    // Only use widely-available getters to stay compatible
    const std::string dep = upcase(d.GetOrigin());
    const std::string arr = upcase(d.GetDestination());
    const std::string rte = d.GetRoute() ? d.GetRoute() : "";
    return dep + "|" + arr + "|" + rte;
}

// Pretty text payload for printing
static std::string BuildPayload(const char* title,
                                const std::string& cs,
                                const EuroScopePlugIn::CFlightPlanData& d)
{
    const std::string dep = upcase(d.GetOrigin());
    const std::string arr = upcase(d.GetDestination());
    const char* route = d.GetRoute() ? d.GetRoute() : "";

    std::ostringstream ss;
    ss << "================ " << title << " ================\n";
    ss << "CS: " << cs << "   DEP: " << dep << "   ARR: " << arr << "\n";
    ss << "ROUTE: " << route << "\n";
    ss << "==============================================\n";
    return ss.str();
}

class StripPrinterPlugin : public CPlugIn
{
public:
    StripPrinterPlugin()
    : CPlugIn(
        COMPATIBILITY_CODE,
        "Strip Printer (CZ+Amendments)",
        "0.3.0",
        "Charlie Yablon",
        "© 2025"
      )
    {}

    ~StripPrinterPlugin() override = default;

    // Fires when ES receives/updates a flight plan from the network (including amendments).
    void OnFlightPlanFlightPlanDataUpdate(CFlightPlan fp) override
    {
        TryPrintFor(fp);
    }

private:
    // Track if we've printed an initial strip for a callsign (avoid duplicates)
    std::unordered_set<std::string> printedInitial_;
    // Track the last known signature so we can detect amendments
    std::unordered_map<std::string, std::string> lastSig_;

    static const char* nz(const char* s) { return s ? s : ""; }

    bool PassesControlZoneFilter(const CFlightPlanData& d) const
    {
        const std::string dep = upcase(d.GetOrigin());
        const std::string arr = upcase(d.GetDestination());

        // Normalize airport list to uppercase once
        static std::vector<std::string> airportsUC = []{
            std::vector<std::string> out;
            out.reserve(kAirports.size());
            for (auto a : kAirports) {
                std::transform(a.begin(), a.end(), a.begin(), [](unsigned char c){ return (char)std::toupper(c); });
                out.push_back(a);
            }
            return out;
        }();

        const bool depIn = in_list(dep, airportsUC);
        const bool arrIn = in_list(arr, airportsUC);

        if (_stricmp(kFilterMode, "departures") == 0) return depIn;
        if (_stricmp(kFilterMode, "arrivals")   == 0) return arrIn;
        /* both */                                return depIn || arrIn;
    }

    void TryPrintFor(const CFlightPlan& fp)
    {
        if (!fp.IsValid()) return;

        const char* csRaw = fp.GetCallsign();
        if (!csRaw || !csRaw[0]) return;

        const std::string cs = upcase(csRaw);
        CFlightPlanData d = fp.GetFlightPlanData();

        // Must have at least one of DEP/ARR and be in our CZ by filter
        const std::string dep = upcase(d.GetOrigin());
        const std::string arr = upcase(d.GetDestination());
        if (dep.empty() && arr.empty()) return;
        if (!PassesControlZoneFilter(d)) return;

        // Compute current signature
        const std::string sigNow = MakeSignature(d);
        auto it = lastSig_.find(cs);

        if (it == lastSig_.end()) {
            // First time we see this CS in this session → initial print
            const std::string payload = BuildPayload("FLIGHT STRIP", cs, d);
            LaunchStripPrinterWithPayload(payload);
            printedInitial_.insert(cs);
            lastSig_[cs] = sigNow;
            return;
        }

        // We saw it before; if signature changed → amendment
        if (it->second != sigNow) {
            const std::string payload = BuildPayload("AMENDMENT", cs, d);
            LaunchStripPrinterWithPayload(payload);
            it->second = sigNow; // update stored signature
        }
        // else: no substantial change → do nothing
    }
};

// SDK-required exports
static StripPrinterPlugin* g_plugin = nullptr;

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