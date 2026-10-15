-- S30A: reference run in MAME (0.264). Run with:
--   OUT=<prefix> mame pacman -rompath <dir with pacman.zip> -video none -sound none -nothrottle -seconds_to_run 3000 -skip_gameinfo
--        -autoboot_script tools/analysis/mame_level256.lua
-- It plays the same scripted player and uses the same harness shortcuts as tests/test_level256_full.cpp (read its header):
--   dots-eaten counter 4E0E set to 243 once per level; lives topped up to 3 when below 2; at 60,000 frames into level 256 the rack-test switch is held
--   until the level counter changes, then the game plays on and its first maze is finished the same way.
-- Writes <OUT>_levels.txt (one table line per level start, 300 frames after it), <OUT>_events.txt (same line formats as the C++ test's ours_events.txt)
-- and <OUT>_vram300.bin (video RAM 4000-43FF then colour RAM 4400-47FF, 300 frames into level 256).
local m = manager.machine
local mem = m.devices[":maincpu"].spaces["program"]
local P0, P1 = m.ioport.ports[":IN0"], m.ioport.ports[":IN1"]
local out = os.getenv("OUT") or "mame_l256"
local function rd(a) return mem:read_u8(a) end
local function wr(a, v) mem:write_u8(a, v) end
local function pc(a) local v = rd(a) | rd(a+1)<<8 | rd(a+2)<<16 | rd(a+3)<<24; local n=0; while v ~= 0 do n = n + (v & 1); v = v >> 1 end; return n * 5 end
local rng = (7 * 2654435761 + 12345) & 0xFFFFFFFF
local hold, dir, f = 0, 2, 0
local names = {"P1 Up", "P1 Left", "P1 Right", "P1 Down"}
local fields = {}
for i, n in ipairs(names) do fields[i] = P0.fields[n] end
local coin, start, rackf = P0.fields["Coin 1"], P1.fields["1 Player Start"], P0.fields["Rack Test (Cheat)"]
local lvlf = assert(io.open(out .. "_levels.txt", "w")); local evf = assert(io.open(out .. "_events.txt", "w"))
local rack, rack_applied, pulse = false, false, 0
local function apply(fr)
  coin:set_value((fr >= 400 and fr < 405) and 1 or 0); start:set_value((fr >= 420 and fr < 424) and 1 or 0)
  for i = 1, 4 do fields[i]:set_value(0) end
  if fr >= 430 then
    if hold <= 0 then rng = (rng * 1664525 + 1013904223) & 0xFFFFFFFF; dir = ((rng >> 24) & 3) + 1; hold = 10 + ((rng >> 16) & 31) end
    hold = hold - 1; fields[dir]:set_value(1)
  end
  -- in MAME this switch is a toggling DIP field: every press EDGE (set_value(1) after set_value(0)) flips it, and the flip shows at the port a frame or two later
  if pulse > 0 then rackf:set_value(0); pulse = 0
  elseif rack ~= rack_applied then rackf:set_value(1); rack_applied = rack; pulse = 1 end
end
local function row(n)
  return string.format("%d %d %d %d %d %d %d %d %d %d %d %d %d %d", n, pc(0x4D46), pc(0x4D4A), pc(0x4D4E), pc(0x4D52), pc(0x4D56), pc(0x4D5A), pc(0x4D5E),
    rd(0x4DBD) | rd(0x4DBE) << 8, rd(0x4DB8), rd(0x4DB9), rd(0x4DBA), rd(0x4DBB), rd(0x4DBC))
end
local function tiles()
  local d, e = 0, 0
  for a = 0x4000, 0x43FF do local v = rd(a); if v == 0x10 then d = d + 1 elseif v == 0x14 then e = e + 1 end end
  return d, e
end
local last, poked, pend, pend_f, l256, phase, wrapf = -1, -1, -1, 0, -1, 0, -1
local i32, maxeaten, row_done, poked_after = {}, 0, false, -1
apply(0)
emu.register_frame_done(function()
  f = f + 1                                    -- frames run so far (the C++ test's R.f)
  local lvl = rd(0x4E13); local s3 = rd(0x4E00) == 3
  local ikey = lvl + (phase >= 4 and 1000 or 0)
  if s3 and rd(0x4E04) == 32 and not i32[ikey] then i32[ikey] = true; evf:write(string.format("I %d %d\n", lvl, f)) end
  if s3 and lvl ~= last then
    evf:write(string.format("L %d %d\n", lvl, f))
    if last == 255 and phase >= 3 then wrapf = f; rack = false; phase = 4 end
    last = lvl; pend = lvl; pend_f = f + 300
  end
  if pend >= 0 and f == pend_f and s3 then lvlf:write(row(pend + 1) .. "\n"); pend = -1 end
  if phase == 0 and last == 255 then l256 = f; phase = 1 end
  if phase >= 1 and phase <= 2 then
    if rd(0x4E0E) > maxeaten then maxeaten = rd(0x4E0E) end
    if f == l256 + 299 and m.video.frameskip ~= nil then m.video.frameskip = 0 end     -- (when run with -video soft -frameskip 10: draw the frame that is photographed)
    if f == l256 + 300 then
      if os.getenv("SNAP") then m.video:snapshot() end                              -- screenshot of the level-256 screen, for the pixel comparison
      local d, e = tiles(); evf:write(string.format("V %d %d %d\n", d, e, rd(0x4E0E)))
      local o = assert(io.open(out .. "_vram300.bin", "wb")); for a = 0x4000, 0x47FF do o:write(string.char(rd(a))) end; o:close()
    end
    if f == l256 + 60000 then evf:write(string.format("X %d\n", maxeaten)); phase = 2 end
  end
  local pl = rd(0x4E00) == 3 and rd(0x4E04) == 3 and rd(0x4DA5) == 0
  if phase == 2 and f >= l256 + 60000 and pl then rack = true; evf:write(string.format("R %d %d\n", f, rd(0x4E13))); phase = 3 end
  if phase == 4 and wrapf >= 0 and f == wrapf + 300 and s3 and not row_done then
    local d, e = tiles(); row_done = true
    evf:write(string.format("W %d %d %d\n", d, e, rd(0x4E0E))); evf:write(row(rd(0x4E13) + 1) .. "\n")
  end
  -- harness shortcuts
  pl = rd(0x4E00) == 3 and rd(0x4E04) == 3 and rd(0x4DA5) == 0
  if pl and rd(0x4E13) < 255 and phase < 3 and poked ~= rd(0x4E13) and rd(0x4E0E) < 200 then wr(0x4E0E, 243); poked = rd(0x4E13) end
  if phase == 4 and row_done and poked_after ~= rd(0x4E13) and pl and rd(0x4E0E) < 200 then wr(0x4E0E, 243); poked_after = rd(0x4E13) end
  if pl and rd(0x4E14) < 2 then wr(0x4E14, 3); wr(0x4E15, 2) end
  if phase == 4 and f >= wrapf + 3000 then lvlf:close(); evf:close(); m:exit(); return end
  apply(f)
end)
