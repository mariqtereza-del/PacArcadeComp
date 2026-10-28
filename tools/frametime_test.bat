@echo off
rem X48 frame-time test: runs the program 20 seconds with every extra on, then 20 seconds with all off, and writes two small text files next to it.
echo Press F11 as soon as the window opens (full screen). Do not touch anything else for 20 seconds.
pacman-port.exe --crt-mask strong --glow strong --curve strong --surround ambient --scanlines --fps 144 --measure 20 --report frametime_all_on.txt --no-hiscore
echo Now the same with everything off. Press F11 again as soon as the window opens.
pacman-port.exe --measure 20 --report frametime_all_off.txt --no-hiscore
echo Done. Please send me the two files frametime_all_on.txt and frametime_all_off.txt.
pause
