# H.E.R.O.N. (High Elevation Radio Observation Network)

The ground station for Swamp Launch rockets. It receives telemetry from the rocket's radio through the ground board and shows altitude, vertical speed, GPS position, battery and signal strength live on screen, along with a plot of the rocket's path over the ground. Every session is saved to a log file.

<!-- TODO: add a screenshot of the dashboard here -->

Runs on Windows.

## 1. One-time setup

You only need to do this once per computer.

1. **Install MSYS2** from [msys2.org](https://www.msys2.org/). Keep the default install folder (`C:\msys64`).
2. **Install the compiler and raylib** (the graphics library the dashboard uses). Open **MSYS2 UCRT64** from the Start menu and run:

   ```
   pacman -S mingw-w64-ucrt-x86_64-toolchain mingw-w64-ucrt-x86_64-raylib
   ```

   Press Enter to accept the defaults, then `Y` to confirm.
3. **Add the compiler to your PATH** so Windows can find it:
   1. In the Start menu, search for **Edit environment variables for your account** and open it.
   2. Under "User variables", select **Path**, click **Edit**, then **New**, and paste `C:\msys64\ucrt64\bin`.
   3. Click OK on every window, then close and reopen any terminals you have open.

   VS Code's guide walks through the same steps with pictures: [Using GCC with MinGW](https://code.visualstudio.com/docs/cpp/config-mingw).
4. **Get the code**, either way works:
   - With Git: `git clone https://github.com/Swamp-Launch-Rocket-Team/HERON.git`
   - Without Git: on the GitHub page, click **Code → Download ZIP**, then unzip it.

## 2. Open a terminal in the HERON folder

In File Explorer, open the `HERON` folder, click the address bar at the top, type `cmd` and press Enter. A Command Prompt window opens, already in the right folder.

> **Using PowerShell or the VS Code terminal?** Put `.\` in front of the commands below, for example `.\run.bat demo`.

## 3. Try the demo (no hardware needed)

```
run.bat demo
```

This plays a simulated flight, so you can check that everything works before connecting a board.

- **The first run takes a few minutes** while it builds the graphics libraries. It hasn't frozen. Later runs take seconds.
- Two windows open (see [What you should see](#what-you-should-see)).
- The flight lasts a little over a minute: 10 seconds on the pad, launch, apogee around 480 m, and landing about 50 seconds after launch. When it ends, the console window closes and the dashboard shows **NO SIGNAL**.

## 4. Run with the real ground board

1. Plug the ground board into USB.
2. Find its COM port: right-click Start, open **Device Manager** and expand **Ports (COM & LPT)**. The board shows up as something like `USB Serial Device (COM4)`. Not sure which one it is? Unplug the board and plug it back in; the one that disappears and comes back is the board.
3. Close anything else that might be using the port, such as a PlatformIO or Arduino serial monitor.
4. Run it with your port number:

   ```
   run.bat COM4
   ```

If the board isn't plugged in yet, or it resets, the ground station keeps trying and reconnects on its own.

## What you should see

Two windows:

- **gcs** (a console window) prints one line for every packet received: flight time, flight state, altitude, battery, GPS fix and satellites, position, and signal strength.
- **Swamp Launch GCS** (the dashboard) shows four panels:

| Panel | What it shows |
|---|---|
| Graphs (left) | Height above the pad, vertical speed (barometer and GPS) and total acceleration against T+. Tick boxes hide each graph; **Follow** keeps the last 30 seconds in view (dragging a graph turns it off). |
| Ground track (middle) | A satellite map around the pad with the pad, the rocket, the ground station and the rocket's track after launch (see below) |
| Data (top right) | **Flight**: time since the last packet, flight computer clock, T+ (time since launch), flight state (and how many times the flight computer rebooted)<br>**Altitude**: barometric altitude, vertical speed, height above the pad, peak altitude, GPS climb rate, temperature, air pressure<br>**Health and Link**: battery voltage, signal strength (RSSI), signal quality (SNR), callsign<br>**GPS**: GPS altitude, fix type and satellite count, position (click it to copy), ground speed and heading<br>**IMU**: total acceleration and each axis (in g), rotation rate |
| Rocket (bottom right) | A 3D model of the rocket that turns with the telemetry. Drag inside the panel to orbit the camera. |

About the ground track:

- The map is downloaded from USGS satellite imagery (about 4 km around the pad) when the first good GPS position arrives, and saved as `maps\live_site.jpg`. This needs an internet connection; without one, the test-site map in `maps\` is shown instead.
- Positions are in metres east and north of the first good GPS position of the session (normally the pad; the receiver's if the rocket has no fix yet), and the map stays put. The ground track works even if the dashboard is started after launch.
- While the rocket is on the pad or has landed, its marker shows its position averaged over 30 seconds, because a GPS that isn't moving still wanders a few metres. The ground station marker is averaged the same way. A jump of more than 30 m counts as a real move and shows up straight away.
- Only GPS fixes from 6 or more satellites are used on the map. Fixes from fewer satellites can be hundreds of metres off. When the latest fix has fewer, it is drawn as a hollow grey **Rocket (low-quality fix)** marker and the Data panel says it is not used on the map; it never moves the pad, the track or the averaged marker. The copy button always copies the latest position whatever its quality.

Status messages:

- **NO PACKET**: nothing has been received yet.
- **NO SIGNAL** (red): packets were arriving and then stopped. The panels turn grey and keep the last values received.

To stop, close both windows.

## Where the data goes

Every run creates two files in the `logs` folder, named after the date and time it started:

- `YYYYMMDD_HHMMSS_raw.log`: every line received from the board, exactly as it arrived, with the PC's time.
- `YYYYMMDD_HHMMSS_decoded.csv`: one row per packet in real units (metres, volts, g). Opens in Excel.

The `logs` folder is not uploaded to GitHub, so copy out any flights you want to keep.

## Troubleshooting

| You see | What to do |
|---|---|
| `g++ not found` | `C:\msys64\ucrt64\bin` isn't on your PATH (setup step 3). Close and reopen the terminal after changing it. |
| `'run.bat' is not recognized` | In PowerShell, type `.\run.bat`. Otherwise you're not in the HERON folder (step 2). |
| `usage: run.bat demo  or  run.bat COM4` | Add `demo` or your COM port after `run.bat`. Double-clicking run.bat won't work. |
| A popup saying a `.dll` was not found (for example `libraylib.dll`) | raylib isn't installed (setup step 2) or PATH isn't set (setup step 3). |
| `cannot find -lraylib` or `raylib.h: No such file` while building | raylib isn't installed (setup step 2). |
| The ground track shows the test-site map instead of the launch site | No internet connection when the first GPS position arrived. The track and markers still work; restart the dashboard once you're online to download the map. |
| The gcs window stays empty | Wrong COM port, or another program has the port open. Check Device Manager and close any serial monitors. |
| The dashboard says NO PACKET forever | The gcs window was closed, or the programs were started from a different folder. Start them with `run.bat` from the HERON folder. |

## How it works (for developers)

```
ground board (USB)  →  gcs.exe  →  logs\*_raw.log  →  display.exe
```

- **gcs.exe** reads lines from the COM port (or from piped input), checks and decodes each packet, prints it, and writes both log files.
- **display.exe** is the dashboard, built with [raylib](https://www.raylib.com/) (window and 3D view), [Dear ImGui](https://github.com/ocornut/imgui) (user interface, connected to raylib through [rlImGui](https://github.com/raylib-extras/rlImGui)) and [ImPlot](https://github.com/epezent/implot) (plots). It reads the newest `_raw.log` in `logs` and switches to a newer one within a second when a new session starts.
- **fake_rx.exe** prints a simulated flight in the same format as the ground board. `run.bat demo` pipes it into gcs: `fake_rx.exe | gcs.exe`.

The log file is the only connection between gcs.exe and display.exe. If you close the dashboard, reopening it (from the HERON folder) replays the current session from the start, so nothing is lost.

### Building by hand

`run.bat` builds everything before it launches. To build separately:

```
build_display.bat
g++ -Wall -Wextra -std=c++17 -I include gcs.cpp -o gcs.exe
g++ -Wall -Wextra -std=c++17 -I include fake_rx.cpp -o fake_rx.exe
```

Run `build_display.bat clean` after updating ImGui or ImPlot, so they get recompiled.

### Tests

```
g++ -Wall -Wextra -std=c++17 -I include main.cpp -o main.exe
main.exe
```

Each check prints PASS or FAIL, and the exit code is the number of failures.

### Files

| File | What it does |
|---|---|
| `gcs.cpp` | Receiver, decoder and logger |
| `display.cpp` | Dashboard |
| `fake_rx.cpp` | Simulated flight; change the constants at the top to change the flight |
| `main.cpp` | Unit tests |
| `include/rasalhague_packet.h` | The 107-byte telemetry packet (from Alex's flight firmware) |
| `include/rx_line.h` | Parses one line from the ground board: `{"type":"rx","rssi":...,"snr":...,"hex":"..."}` |
| `include/line_reader.h` | Splits incoming bytes into lines |
| `include/session_log.h` | Writes the raw log and the CSV |
| `include/log_tail.h` | Lets the dashboard follow the newest log |
| `include/flight_stats.h` | Height above pad, peak altitude, vertical speed, T+, flight computer reboots; ground-track positions and smoothing |
| `include/history.h` | Data behind the graphs |
| `include/orientation.h` | Works out the rocket's orientation from the telemetry for the 3D view |
| `include/rocket_view.h` | The 3D rocket panel |
| `include/map_box.h`, `map_download.cpp` | Downloads the USGS map around the pad (`map_download.cpp` is separate because `windows.h` clashes with raylib) |
| `include/map_image.h` | Loads the map image for the ground track |
| `maps/` | Test-site map; the downloaded `live_site` map is saved here too (not uploaded to GitHub) |
| `resources/models/` | Rocket 3D models by Will Huff (see `SOURCE.txt`) |
| `run.bat`, `build_display.bat` | Build and launch scripts |
| `third_party/` | Dear ImGui, ImPlot, rlImGui and stb source |

### Bench mode

`bench/` (not in Git) holds a local copy of Alex's flight firmware with a `bench` build that sends packets over USB instead of the radio, so the ground station can be tested on a desk.

**Never flash the normal (radio) build without an antenna attached.** The radio's 2 W amplifier can be damaged.
