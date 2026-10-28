# HDR: what is possible (X47, researched 2026-10-15; the HDR LOOK added in 2.1.0, see the last section)

## Decision
**Drop HDR output for now. Do not change library for it.** The program stays on SDL2 (2.32.10 for the Windows build) and keeps its SDR glow from X41.
If HDR ever becomes wanted, the route is a separate, larger step "move to SDL3" (a change of library affecting the whole app); it is not worth it for a 1980 game that only has about 16 colours.

## Evidence
1. **SDL2 has no HDR output.** Checked in the headers that our Windows build is compiled against (SDL 2.32.10, `include/SDL2/SDL_render.h`, `SDL_video.h`, `SDL_pixels.h`): there is no HDR, scRGB or colourspace symbol, so there is no way to tell the renderer or the window that the output is HDR (SDL2 can hold a 10-bit texture format, but nothing marks the window output as HDR; I did not test whether Windows would show it as HDR, and it very likely would not). Our Linux test machine (SDL 2.30.0) is the same. (Command and result in docs/evidence/X47_hdr.txt.)
2. **SDL3 has it, with limits.**
   - `SDL_SetRenderDrawColorspace`: "the default is SRGB, but you can change it to other colorspaces such as SDL_COLORSPACE_SCRGB for HDR rendering"; available since SDL 3.0.0. https://wiki.libsdl.org/SDL3/SDL_SetRenderDrawColorspace
   - `SDL_PROP_DISPLAY_HDR_ENABLED_BOOLEAN` ("true if the display has HDR headroom above the SDR white point"), "for informational and diagnostic purposes only, as not all platforms provide this information"; `SDL_GetDisplayProperties` is available since SDL 3.2.0. https://wiki.libsdl.org/SDL3/SDL_GetDisplayProperties
   - A user report about the game Oolite says HDR on Linux needs SDL 3.2.6 or newer plus a Wayland compositor with colour management and a recent Mesa; it gives no SDL3 detail for Windows. https://bb.oolite.space/viewtopic.php?p=303740
3. **SDL2 history:** a 2019 forum question "Does SDL support HDR10 rendering?" has no answer on the page. https://discourse.libsdl.org/t/does-sdl-support-hdr10-rendering/25843

## What I could not verify
- I could not test SDL3 on a real HDR monitor or on Windows from here, so I do not claim that SDL3 HDR works on the user's PC; I only know what its documentation says. The Windows details of SDL3's HDR path were not found in the pages I could read.
- No test program was needed, so nothing was sent to the user's PC.

## What HDR would give this game
Only brighter highlights for the CRT glow (X41) and the bright maze/ghost colours on an HDR display. The picture is drawn with a few flat colours, so most of the picture would look the same; the glow would be the only visible gain.

## Nearest alternative (already built)
SDR glow (X41), mask (X40), curve (X42), presets (X43) and the Ambient surround (X46) give the "bright light on a dark screen" feel without HDR. Possible small extra later, not planned: a stronger glow preset.

## If the user wants HDR anyway
Next step would be named "Move to SDL3 (prerequisite for HDR)": port the window/renderer/audio/input/gamepad code from SDL2 to SDL3, re-run the full regression, then add an HDR option that is only offered when the display reports HDR. Estimated as a large step; it touches every file that includes SDL.


## 2.1.0: the HDR setting (DISPLAY > HDR and HDR INTENSITY)
The user asked (2026-10-28) for an HDR On/Off button and an intensity slider in the display settings, with what it affects left to the builder. The decision above still holds: **SDL2 cannot send an HDR signal**, and the library was not changed. So the setting is an honest, clearly named **HDR look**: a picture effect on the ordinary SDR picture that imitates what an HDR screen gives this game (bright things really bright against dark things). It is **not** an HDR10 / scRGB signal and it does not switch the monitor into HDR mode. The menu, the log line, README and this note all say so.

What it does (source: `src/app/hdr.h`; only on the finished picture, like glow, mask and curve; Purist mode never uses it; the machine, the replay hashes and the achievements are not touched):
1. dark and middle tones get darker (up to 35 % in linear light at intensity 10): deeper blacks, a darker maze;
2. bright tones (pellets, text, Pac-Man, the ghosts) get brighter and slightly more colourful, but only as far as the colour keeps its hue (pink does not turn white, yellow stays yellow);
3. a soft bloom (box blurs of radius 1 and 3 picture pixels) of the brightest parts is added, up to 0.55 at intensity 10.
Intensity 1 to 10 scales all three; pure black stays pure black. The slider is the row HDR INTENSITY (Left / Right, or click the left / right half of the row); the value is also shown as a number. `settings.cfg` keys: `hdr on|off`, `hdr_intensity 1..10`. Test aids: `--hdr on|off`, `--hdr-level N`. Tests: tests/test_fps_hdr.sh (unit checks of the effect: black stays black, deterministic, darker maze and bigger bloom with rising intensity, hue kept; menu rows and slider; settings round trip).

If a real HDR output is wanted later, the route is still "Move to SDL3" (see above); this setting would then switch from the look to the real signal.
