// overlay.h: the picture layout constants and the small drawing helpers used by the menu (the F3 debug overlay was removed in 2.1.0).
// The picture is 224 x 288 pixels; the output buffer is 352 x 288: the picture on the left and a 128-pixel side panel (key help or menu) on the right.
#pragma once
#include <cstdint>
#include "../board/bus.h"
#include "../video/gfx.h"

namespace overlay {
const int kPictureW = 224, kPictureH = 288, kPanelW = 128, kOutW = kPictureW + kPanelW, kOutH = kPictureH;
} // namespace overlay
