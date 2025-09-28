
// EuroScopeStripPrinterPlugin.cpp
//
// Build notes:
// - Requires EuroScope SDK header: EuroScopePlugIn.h
// - Exported functions: EuroScopePlugInInit / EuroScopePlugInExit
// - Tested as a scaffold; adjust virtual method names if your SDK version differs.

#include <windows.h>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <memory>

#include "EuroScopePlugIn.h"

using namespace EuroScopePlugIn;

static const int kTimerIntervalMs = 1000;
static const wchar_t* kPrinterExeName = L"StripPrinter.exe";

// Minimal safe launcher
static void LaunchStripPrinter(const std::wstring& payload)
{
    // Write payload to a temp file and pass the path (avoids cmdline length limits)
    wchar_t tempPath[MAX_PATH]; GetTempPathW(MAX_PATH, tempPath);
    wchar_t tempFile[MAX_PATH]; GetTempFileNameW(tempPath, L"ESFP", 0, tempFile);

    HANDLE h = CreateFileW(tempFile, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, nullptr);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    std::string utf8;
    // naive UTF-16LE -> UTF-8: use WideCharToMultiByte
    int len = WideCharToMultiByte(CP_UTF8, 0, payload.c_str(), (int)payload.size(), nullptr, 0, nullptr, nullptr);
    if (len > 0) {
        utf8.resize(len);
        WideCharToMultiByte(CP_UTF8, 0, payload.c_str(), (int)payload.size(), utf8.data(), len, nullptr, nullptr);
        WriteFile(h, utf8.data(), (DWORD)utf8.size(), &written, nullptr);
    }
    CloseHandle(h);

    std::wstring cmd = L"\"";
    cmd += kPrinterExeName;
    cmd += L"\" --file \"";
    cmd += tempFile;
    cmd += L"\"";

    STARTUPINFOW si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    } else {
        // Fallback: try ShellExecute
        ShellExecuteW(nullptr, L"open", kPrinterExeName, (L"--file \"" + std::wstring(tempFile) + L"\"").c_str(), nullptr, SW_HIDE);
    }
}

class StripPrinterPlugin : public CPlugIn
{
public:
    StripPrinterPlugin()
        : CPlugIn(RadarScreen),
          m_lastTick(::GetTickCount64())
    {
        SetPluginName("StripPrinter");
        SetAuthorName("ChatGPT");
        SetVersion("1.0.0");
        DisplayUserMessage("StripPrinter", "Init", "StripPrinter plugin loaded", 0, 0, 0, 0, 0);
        StartTimer(kTimerIntervalMs, 0);
    }

    virtual ~StripPrinterPlugin() {}

    // Called by EuroScope when EuroScope is shutting down
    void OnTerminate() override
    {
        StopTimer();
    }

    // Periodic tick – scan flight plans for new prefiles
    void OnTimer(int Counter) override
    {
        UNREFERENCED_PARAMETER(Counter);
        scanFlightPlans();
    }

    // Optional: respond to FP updates immediately (SDK name may vary by version)
    void OnFlightPlanDataUpdate(CFlightPlan FlightPlan) override
    {
        maybePrintFor(FlightPlan);
    }

private:
    std::unordered_set<std::string> seen_;
    ULONGLONG m_lastTick;

    // SDK helpers: safely to std::string (ASCII-safe)
    static std::string ws2s(const wchar_t* w)
    {
        if (!w) return {};
        int len = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
        std::string s; s.resize(len ? len - 1 : 0);
        if (len) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, nullptr, nullptr);
        return s;
    }

    bool isLikelyPrefile(const CFlightPlan& fp)
    {
        // Heuristic:
        // - Not correlated to a connected radar target
        // - Has EOBT or DEP/ARR defined
        // (Adjust to your SOP. Depending on SDK, availability of methods may vary.)
        bool hasTarget = false;
        try {
            CSimulatedRadarTarget tg = fp.GetCorrelatedRadarTarget();
            hasTarget = tg.IsValid();
        } catch (...) {
            hasTarget = false;
        }

        // If there's no target, treat as prefile candidate
        if (hasTarget) return false;

        // Must have at least a route or dep/arr to be meaningful
        auto strip = fp.GetFlightPlanData();
        bool hasBasic =
            strip.GetOrigin() != nullptr &&
            strip.GetDestination() != nullptr &&
            (strip.GetOrigin()[0] != 0 || strip.GetDestination()[0] != 0);

        return hasBasic;
    }

    void scanFlightPlans()
    {
        // Iterate flight plans
        for (CFlightPlan fp = FlightPlanSelectFirst();
             fp.IsValid();
             fp = FlightPlanSelectNext(fp))
        {
            maybePrintFor(fp);
        }
    }

    void maybePrintFor(const CFlightPlan& fp)
    {
        if (!fp.IsValid()) return;
        auto cs = ws2s(fp.GetCallsign());

        if (seen_.count(cs)) return; // already handled
        if (!isLikelyPrefile(fp)) return;

        seen_.insert(cs);
        auto payload = buildStripPayload(fp);
        LaunchStripPrinter(payload);
        DisplayUserMessage("StripPrinter", "Print", (std::string("Printed prefile strip: ") + cs).c_str(), 0, 0, 0, 0, 0);
    }

    std::wstring buildStripPayload(const CFlightPlan& fp)
    {
        auto dp = fp.GetFlightPlanData();
        std::wstring callsign = fp.GetCallsign();
        std::wstring dep = dp.GetOrigin() ? dp.GetOrigin() : L"";
        std::wstring arr = dp.GetDestination() ? dp.GetDestination() : L"";
        std::wstring route = dp.GetRoute() ? dp.GetRoute() : L"";
        std::wstring alt = dp.GetInitialAltitude() ? dp.GetInitialAltitude() : L"";
        std::wstring eobt = dp.GetDepartureTime() ? dp.GetDepartureTime() : L"";
        std::wstring wake = dp.GetWakeTurbulence() ? dp.GetWakeTurbulence() : L"";
        std::wstring equip = dp.GetEquipment() ? dp.GetEquipment() : L"";

        // Keep to a ~80 char width text strip
        std::wstringstream ss;
        ss << L"================ FLIGHT STRIP ================\n";
        ss << L"CS: " << callsign << L"   DEP: " << dep << L"   ARR: " << arr << L"\n";
        ss << L"ROUTE: " << route << L"\n";
        ss << L"FL: " << alt << L"   EOBT: " << eobt << L"   WTC: " << wake << L"\n";
        ss << L"EQUIP: " << equip << L"\n";
        ss << L"==============================================\n";
        return ss.str();
    }
};

// Required exports for EuroScope to instantiate the plugin
static std::unique_ptr<StripPrinterPlugin> g_plugin;

extern "C" __declspec(dllexport) CPlugIn* EuroScopePlugInInit()
{
    g_plugin = std::make_unique<StripPrinterPlugin>();
    return g_plugin.get();
}

extern "C" __declspec(dllexport) void EuroScopePlugInExit()
{
    g_plugin.reset();
}
