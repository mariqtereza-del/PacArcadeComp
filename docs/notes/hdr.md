# HDR: what is possible (X47, researched 2026-10-15)

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
