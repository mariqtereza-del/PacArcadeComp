#!/bin/sh
# Usage: tools/checkpoint.sh S05   -> writes /home/claude/pacman-port_S05.zip (no ROM, no build junk). Send it to the user.
set -e
cd "$(dirname "$0")/../.."
L=${1:?give a label like S05}
rm -f "pacman-port_$L.zip"
zip -rq "pacman-port_$L.zip" pacman-port -x 'pacman-port/roms/*.zip' 'pacman-port/build/*' 'pacman-port/dist/*' '*/__pycache__/*'
ls -la "pacman-port_$L.zip"
