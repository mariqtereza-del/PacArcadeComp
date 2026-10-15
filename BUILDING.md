# Building Maze Chase and running the tests

Two ways to get a program you can run:
- **Just play:** download the Windows release (see [README.md](README.md)). No building needed.
- **Build it yourself** (this page): to change the program, or to run it on Linux.

In every case **you need your own `pacman.zip`** (the ROM set) to *play*. You do **not** need it to *build* the program or to run the ROM-free tests.

Words you will meet: a **terminal** is the window where you type commands (copy each line exactly, press Enter after it). The **project folder** is the folder that contains `README.md`, `src` and `tools`.

---

## A. Linux (tested: this is how the project is developed)
Works on Ubuntu or Debian (and similar). Open a terminal.

1. **Install the tools** (once; it asks for your password):
   ```
   sudo apt-get update
   sudo apt-get install -y g++ python3 libsdl2-dev
   ```
2. **Go to the project folder** (the folder you downloaded or cloned):
   ```
   cd maze-chase
   ```
3. **Build the program:**
   ```
   sh tools/build_app.sh
   ```
   It ends with `built build/app/pacman-port`.
4. **Put your ROM in place:** make a folder `roms` in the project folder and put your `pacman.zip` in it (`roms/pacman.zip`). The program also finds the zip next to itself or in the current folder, or you can name it: `build/app/pacman-port --rom /path/to/pacman.zip`.
5. **Run the game:**
   ```
   build/app/pacman-port
   ```
   Press **5** (coin), **1** (start), **Esc** (menu).
6. **Run the tests that need no ROM:**
   ```
   sh tests/run_rom_free.sh
   ```
   It takes under a minute and prints `ROM-FREE TESTS PASSED` at the end. It checks that the Z80 core and the window program build and run, and runs the controls, vibration, records and achievement tests plus the documentation checks. Some of these tests have a second part that plays the game; without the ROM it prints `needs build/rom/prog.bin` and skips that part (the first part still ran). Everything that plays the game needs the ROM: step 7.
7. **Run everything (needs your ROM in `roms/pacman.zip`):** first install the Python helpers once:
   ```
   sudo apt-get install -y xvfb xdotool
   pip install z80 pillow numpy --break-system-packages
   ```
   then
   ```
   sh tests/run_all.sh
   ```
   This takes about **90 minutes** and ends with `ALL TESTS PASSED`. Two quicker checks that prove the machine still behaves exactly like the original: `sh tests/test_purist.sh` and `sh tests/test_replay.sh`.

(Two tests, `tests/test_rom_tables.py` and its data, belong to the original project and are not in the GitHub copy; `tests/run_all.sh` therefore skips them if they are missing.)

## B. Windows
### B1. Easiest: Windows Subsystem for Linux (WSL), then follow part A
*Not tested by us on a real Windows machine: tell us how it goes.*
1. Click Start, type **PowerShell**, right-click it, **Run as administrator**, type `wsl --install` and press Enter. Restart the PC when asked and finish the Ubuntu set-up (it asks you to make a user name and password).
2. Open **Ubuntu** from the Start menu and follow **part A** above. (The game window appears on your Windows desktop on Windows 11 and recent Windows 10.)

### B2. MSYS2 (a Windows-native build)
*Not tested by us on a real Windows machine; the commands follow MSYS2's standard package names.*
1. Install MSYS2 from https://www.msys2.org (run the installer, accept the defaults).
2. Start **MSYS2 MINGW64** from the Start menu (the one with the blue icon).
3. Install the tools:
   ```
   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-python
   ```
4. Go to the project folder (Windows drives appear as `/c/...`):
   ```
   cd /c/Users/YOURNAME/Downloads/maze-chase
   ```
5. Build: `sh tools/build_app.sh`. Put `pacman.zip` next to the built `build/app/pacman-port.exe` (or in a `roms` folder in the project folder) and start it with `build/app/pacman-port.exe`.

## C. Building the Windows release (`MazeChase.exe` and the zip) on Linux (tested)
This is how the downloadable release is made: Linux builds a Windows program with the MinGW cross-compiler. The exe needs no other files.
1. Install the compiler and zip tool:
   ```
   sudo apt-get install -y gcc-mingw-w64-x86-64 g++-mingw-w64-x86-64 zip
   ```
2. Download the SDL2 development package for MinGW (version 2.32.10) from https://github.com/libsdl-org/SDL/releases/tag/release-2.32.10 : the file **`SDL2-devel-2.32.10-mingw.tar.gz`**. Unpack it:
   ```
   mkdir -p /tmp/sdl2win && tar xzf SDL2-devel-2.32.10-mingw.tar.gz -C /tmp/sdl2win
   ```
   (If you unpack elsewhere, set `SDL2_MINGW=/your/folder/SDL2-2.32.10/x86_64-w64-mingw32` before step 3.)
3. Build the release:
   ```
   sh tools/package.sh
   ```
   The result is the folder `dist/MazeChase` and the file `dist/MazeChase-1.0.1-win64.zip` (program, README, licences, REPORT-A-PROBLEM.bat; never a ROM).
4. Only the exe is built here: to test it you need a Windows PC.

## D. Making the clean copy for GitHub
`sh tools/export_github.sh` (see [docs/GITHUB_UPLOAD_GUIDE.md](docs/GITHUB_UPLOAD_GUIDE.md)) copies the project without the ROM and everything made from it, then scans the copy for ROM bytes and refuses to finish if it finds any. It needs your ROM (only to search for it) and runs on Linux.

## Troubleshooting
- `sdl2-config: not found` or `SDL.h: No such file`: the SDL2 development package is missing (`sudo apt-get install libsdl2-dev`).
- `g++: command not found`: install `g++` (step A1).
- The game says it cannot find the ROM: put `pacman.zip` in `roms/` (or use `--rom`).
- No window appears on a server or in a container without a screen: use a machine with a desktop, or start a virtual screen (`Xvfb`) as the tests do.
- A test fails after you changed code: that is the tests doing their job. Read the `FAIL:` line; see [MODDING.md](MODDING.md) section 5.
