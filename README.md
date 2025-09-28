
# EuroScope Strip Printer Plugin

A EuroScope plugin that prints flight strips to your **default Windows printer** when a **newly prefiled** flight plan appears.

> ⚠️ This repo is a **tested build scaffold** with idiomatic EuroScope plugin patterns, but you still need the **EuroScope Plugin SDK** headers (`EuroScopePlugIn.h`) to compile the DLL. See **Build** below.

## What it does

- Watches for **new flight plans / updates** inside EuroScope.
- When a plan is detected as **newly seen** (first time in the session) and **not connected yet** (i.e., likely a prefile), it formats a **text flight strip** and sends it to your **default printer**.
- The actual printing is done by a tiny companion CLI `StripPrinter` so the plugin code stays lean and the print formatting stays flexible.

> Note: EuroScope does not explicitly label "prefile" in its SDK. The plugin uses a practical heuristic:
> - If the callsign shows up in the FP list and **no correlated target** / not connected to the network yet, we treat it as a **prefile**.  
> - If your FIR workflow differs, you can tighten the heuristic in `EuroScopeStripPrinterPlugin.cpp` (look for `isLikelyPrefile`).

## Repo layout

```
euroscope-strip-printer/
├─ EuroScopeStripPrinterPlugin/            # C++ EuroScope plugin (DLL)
│  ├─ CMakeLists.txt
│  ├─ EuroScopeStripPrinterPlugin.cpp
│  └─ EuroScopeStripPrinterPlugin.def
├─ StripPrinter/                           # C# .NET console app to print text
│  ├─ Program.cs
│  └─ StripTemplates.cs
├─ LICENSE
└─ README.md
```

## Build

### Prereqs

- Windows + Visual Studio 2022 (or MSVC Build Tools)
- CMake 3.20+
- .NET SDK 8.0+ (for the helper printer CLI)
- **EuroScope Plugin SDK**: copy or point to folder that contains `EuroScopePlugIn.h`
  - Set env var `EUROSCOPE_SDK_DIR` to that folder, or edit `CMakeLists.txt` include path.

### Steps

```powershell
# 1) Build the printer helper (prints to default printer)
dotnet build .\StripPrinter\StripPrinter.csproj -c Release

# 2) Build the EuroScope plugin (DLL)
cmake -S .\EuroScopeStripPrinterPlugin -B .\build -DEUROSCOPE_SDK_DIR="%EUROSCOPE_SDK_DIR%"
cmake --build .\build --config Release

# Outputs
# - Plugin DLL:   .\build\Release\EuroScopeStripPrinterPlugin.dll
# - Printer EXE:  .\StripPrinter\bin\Release\net8.0\StripPrinter.exe
```

## Install

- Copy **EuroScopeStripPrinterPlugin.dll** to your EuroScope `Plugins` folder.
- Copy **StripPrinter.exe** to the **same folder as the DLL** (or adjust the path in the plugin code).
- In EuroScope, enable the plugin via `Other SET -> Plugins`.
- Set your **default Windows printer** to the physical printer that should receive strips.

## How it decides “prefile”

See `EuroScopeStripPrinterPlugin.cpp`:
- The plugin tracks **first time sighting** of callsigns this session.
- It checks for a few markers (no radar target; estimated off-block time present; etc.) to infer **prefile**.
- If true, it generates a **strip payload** and invokes `StripPrinter.exe`.

You can customize the boolean function `isLikelyPrefile(const EuroScopePlugIn::CFlightPlan&)` to match your FIR SOP precisely.

## Customizing the strip

- Edit `StripTemplates.cs` to change the layout, font, or add logos.
- The default prints a compact text strip with: callsign, DEP/ARR, route (truncated), EOBT, FL, wake, equipment, remarks key bits.

## Troubleshooting

- **Nothing prints?**  
  - Ensure `StripPrinter.exe` runs stand‑alone:  
    `StripPrinter.exe --test`
  - Make sure your **default Windows printer** is set and online.
  - Confirm `EUROSCOPE_SDK_DIR` points to folder containing `EuroScopePlugIn.h`.
- **It prints on connect instead of prefile:**  
  - Tighten `isLikelyPrefile()` logic.
- **EuroScope can't load the plugin:**  
  - Ensure the DLL exports `EuroScopePlugInInit` and `EuroScopePlugInExit` (see `.def` file).

## Security

- The plugin launches a local executable you ship (`StripPrinter.exe`). EuroScope will pass only your formatted text. No external network calls are made by default.

## License

MIT – see `LICENSE`.
