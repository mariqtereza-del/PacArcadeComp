#!/bin/sh
# S31 evidence commands (run from the project root after: sh tools/build_app.sh). Simulated refresh rates do not need a monitor.
A=build/app/pacman-port; R="--rom roms/pacman.zip --scripted --no-audio"
$A $R --ticks 2400 --sim-refresh 180                     # exact pacing at 180 Hz
$A $R --ticks 2400 --sim-refresh 180 --pacing smooth     # smoothed pacing at 180 Hz
$A $R --ticks 2400 --sim-refresh 144; $A $R --ticks 2400 --sim-refresh 144 --pacing smooth
$A $R --ticks 2400 --sim-refresh 60;  $A $R --ticks 2400 --sim-refresh 60 --pacing smooth
$A $R --ticks 2400 --sim-refresh 180 --keys F3@200,F5@400,F6@600,F7@800,F8@1000,F3@1300,F6@1600,F11@1800,F11@2000
# real window under a virtual screen: Xvfb :99 ; DISPLAY=:99 $A $R --scale 2 ; xdotool key F3 ; import -window root shot.png
