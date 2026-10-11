-- harness.lua - headless PCSX-Redux test driver for the DOOMOGACHI PS1 build.
--
-- Loaded with `pcsx-redux -cli ... -dofile harness.lua` by run.py, which
-- passes everything through environment variables:
--   HARNESS_CMDS   low-level command file (one per line, see below)
--   HARNESS_OUT    output directory (screenshots, vram dumps, results)
--   HARNESS_FRAME  address (hex) of the game's frame counter g_frame, used
--                  as a heartbeat (hang detection) and for the frame rate
--   HARNESS_HANG   vsyncs without a new frame before declaring a hang
--
-- Commands (times are in VSYNCS, 60 per second NTSC):
--   wait N            run N vsyncs
--   down BUTTON       press and keep holding a pad button
--   up BUTTON         release it
--   shot NAME         raw screenshot -> OUT/NAME.raw (+ .txt with w h bpp)
--   vram NAME         raw 1024x512 VRAM dump -> OUT/NAME.vram
--   fps NAME          start measuring frame rate; "fpsend NAME" stops it
--   quit              exit the emulator (also done after the last command)
-- Buttons: CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 START SELECT UP DOWN
--          LEFT RIGHT.
local ffi = require('ffi')

local out = os.getenv('HARNESS_OUT') or '.'
local cmdfile = os.getenv('HARNESS_CMDS')
local frame_addr = tonumber(os.getenv('HARNESS_FRAME') or '0', 16) or 0
local hang_limit = tonumber(os.getenv('HARNESS_HANG') or '900')

local cmds = {}
if cmdfile then
  for line in io.lines(cmdfile) do
    local op, arg = line:match('^(%S+)%s*(.*)$')
    if op then cmds[#cmds + 1] = { op = op, arg = arg } end
  end
end

local results = io.open(out .. '/harness_results.txt', 'w')
local function log(msg)
  print('HARNESS: ' .. msg)
  io.stdout:flush()
  if results then results:write(msg .. '\n'); results:flush() end
end

local B = PCSX.CONSTS.PAD.BUTTON
local function pad() return PCSX.SIO0.slots[1].pads[1] end

local function read_frame()
  if frame_addr == 0 then return 0 end
  local mem = ffi.cast('uint8_t*', PCSX.getMemPtr())
  local p = ffi.cast('uint32_t*', mem + bit.band(frame_addr, 0x1fffff))
  return p[0]
end

local function save_slice(path, slice)
  local f = Support.File.open(path, 'TRUNCATE')
  f:write(slice)
  f:close()
end

local idx = 1
local wait_left = 0
local vsyncs = 0
local last_frame, last_change = -1, 0
local fps_start = {}
local done = false
local quit_at = nil

-- PCSX.quit() is issued on a later vsync, outside the command loop
local function finish(code)
  if done then return end
  done = true
  log(string.format('END vsyncs=%d frames=%d code=%d', vsyncs, read_frame(), code or 0))
  if results then results:close(); results = nil end
  quit_at = vsyncs + 2
end

local function step()
  while idx <= #cmds do
    local c = cmds[idx]
    idx = idx + 1
    if c.op == 'wait' then
      wait_left = tonumber(c.arg) or 1
      return
    elseif c.op == 'down' then
      pad().setOverride(B[c.arg])
    elseif c.op == 'up' then
      pad().clearOverride(B[c.arg])
    elseif c.op == 'shot' then
      local ss = PCSX.GPU.takeScreenShot()
      save_slice(out .. '/' .. c.arg .. '.raw', ss.data)
      local f = io.open(out .. '/' .. c.arg .. '.txt', 'w')
      f:write(string.format('%d %d %s\n', ss.width, ss.height, tostring(ss.bpp)))
      f:close()
      log(string.format('shot %s %dx%d frame=%d vsync=%d', c.arg, ss.width, ss.height,
                        read_frame(), vsyncs))
    elseif c.op == 'vram' then
      save_slice(out .. '/' .. c.arg .. '.vram', PCSX.GPU.getVRAM())
      log('vram ' .. c.arg)
    elseif c.op == 'fps' then
      fps_start[c.arg] = { v = vsyncs, f = read_frame() }
    elseif c.op == 'fpsend' then
      local s = fps_start[c.arg]
      if s then
        local dv, df = vsyncs - s.v, read_frame() - s.f
        log(string.format('fps %s %.2f (%d frames / %d vsyncs)', c.arg,
                          dv > 0 and df * 60.0 / dv or 0, df, dv))
      end
    elseif c.op == 'quit' then
      finish(0)
      return
    end
  end
  finish(0)
end

-- keep a reference: an unreferenced listener is garbage collected and stops
HARNESS_LISTENER = PCSX.Events.createEventListener('GPU::Vsync', function()
  vsyncs = vsyncs + 1
  if done then
    if quit_at and vsyncs >= quit_at then
      quit_at = nil
      PCSX.quit()
    end
    return
  end
  local f = read_frame()
  if f ~= last_frame then
    last_frame = f
    last_change = vsyncs
  elseif frame_addr ~= 0 and f > 0 and vsyncs - last_change > hang_limit then
    log(string.format('HANG: no new frame for %d vsyncs (frame=%d)', vsyncs - last_change, f))
    local ss = PCSX.GPU.takeScreenShot()
    save_slice(out .. '/hang.raw', ss.data)
    local t = io.open(out .. '/hang.txt', 'w')
    t:write(string.format('%d %d %s\n', ss.width, ss.height, tostring(ss.bpp)))
    t:close()
    finish(3)
    return
  end
  if wait_left > 0 then
    wait_left = wait_left - 1
    if wait_left > 0 then return end
  end
  step()
end)

log('started, ' .. #cmds .. ' commands, frame counter at ' .. string.format('%08x', frame_addr))
