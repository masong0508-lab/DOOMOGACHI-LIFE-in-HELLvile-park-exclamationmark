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
--   waitframe N       run until the game's frame counter reaches N (boot sync)
--   waitf N           run until the game has run N more frames (game time)
--   down BUTTON       press and keep holding a pad button
--   up BUTTON         release it
--   shot NAME         raw screenshot -> OUT/NAME.raw (+ .txt with w h bpp)
--   vram NAME         raw 1024x512 VRAM dump -> OUT/NAME.vram
--   fps NAME          start measuring frame rate; "fpsend NAME" stops it
--   hcmd CMD IARG HEX run a game-side command through the g_hcmd mailbox
--                     (src/hcmd.c): scene, push, expect-scene, save, load,
--                     expect-save, type, log. HEX = the string argument.
--                     Waits for the game to acknowledge; failures are
--                     logged as "HARNESS: FAIL ...".
--   mark TEXT         log a marker line (run.py: no-stubs checkpoints)
--   quit              exit the emulator (also done after the last command)
-- Buttons: CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 START SELECT UP DOWN
--          LEFT RIGHT.
local ffi = require('ffi')

local out = os.getenv('HARNESS_OUT') or '.'
local cmdfile = os.getenv('HARNESS_CMDS')
local frame_addr = tonumber(os.getenv('HARNESS_FRAME') or '0', 16) or 0
local hang_limit = tonumber(os.getenv('HARNESS_HANG') or '900')
local hcmd_addr = tonumber(os.getenv('HARNESS_HCMD') or '0', 16) or 0

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

local function ram(addr) -- byte pointer into main RAM (KSEG0/1 address)
  return ffi.cast('uint8_t*', PCSX.getMemPtr()) + bit.band(addr, 0x1fffff)
end

-- g_hcmd layout (src/hcmd.c): 0 magic, 4 req, 8 ack, 12 status, 16 iarg,
-- 20 cmd[16], 36 arg[64], 100 reply[64]
local function hcmd_u32(off) return ffi.cast('uint32_t*', ram(hcmd_addr + off)) end
local function hcmd_str(off, len)
  local p, t = ram(hcmd_addr + off), {}
  for i = 0, len - 1 do
    if p[i] == 0 then break end
    t[#t + 1] = string.char(p[i])
  end
  return table.concat(t)
end
local function hcmd_put(off, len, str)
  local p = ram(hcmd_addr + off)
  for i = 0, len - 1 do
    p[i] = i < #str and str:byte(i + 1) or 0
  end
end
local function unhex(h)
  return (h:gsub('%x%x', function(x) return string.char(tonumber(x, 16)) end))
end

local function save_slice(path, slice)
  local f = Support.File.open(path, 'TRUNCATE')
  f:write(slice)
  f:close()
end

local idx = 1
local wait_left = 0
local wait_frame = nil
local quiet_wait = false
local vsyncs = 0
local last_frame, last_change = -1, 0
local fps_start = {}
local done = false
local quit_at = nil
local pending = nil       -- outstanding hcmd request

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
    elseif c.op == 'waitframe' then
      wait_frame = tonumber(c.arg) or 1
      return
    elseif c.op == 'waitf' then
      wait_frame = read_frame() + (tonumber(c.arg) or 1)
      quiet_wait = true
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
    elseif c.op == 'mark' then
      log('mark ' .. c.arg)
    elseif c.op == 'hcmd' then
      local name, iarg, hex = c.arg:match('^(%S+)%s+(%-?%d+)%s*(%x*)$')
      if hcmd_addr == 0 or hcmd_u32(0)[0] ~= 0x444d4348 then
        log('FAIL: hcmd ' .. (name or c.arg) .. ': no g_hcmd mailbox in this build')
      else
        local arg = unhex(hex or '')
        hcmd_put(20, 16, name)
        hcmd_put(36, 64, arg)
        ffi.cast('int32_t*', ram(hcmd_addr + 16))[0] = tonumber(iarg)
        local req = hcmd_u32(4)[0] + 1
        hcmd_u32(4)[0] = req
        pending = { name = name, arg = arg, req = req, since = vsyncs }
        return
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
  if pending then
    if hcmd_u32(8)[0] == pending.req then
      local ok = ffi.cast('int32_t*', ram(hcmd_addr + 12))[0] ~= 0
      local msg = string.format('hcmd %s %s -> %s', pending.name,
                                pending.arg:gsub('\n', '\\n'), hcmd_str(100, 64))
      log((ok and '' or 'FAIL: ') .. msg)
      pending = nil
    elseif vsyncs - pending.since > 1800 then
      log('FAIL: hcmd ' .. pending.name .. ' not acknowledged by the game')
      pending = nil
    else
      return
    end
  end
  if wait_frame then
    if frame_addr == 0 or f >= wait_frame then
      if not quiet_wait then
        log(string.format('boot sync: frame %d at vsync %d', f, vsyncs))
      end
      quiet_wait = false
      wait_frame = nil
    else
      return
    end
  end
  if wait_left > 0 then
    wait_left = wait_left - 1
    if wait_left > 0 then return end
  end
  step()
end)

log('started, ' .. #cmds .. ' commands, frame counter at ' .. string.format('%08x', frame_addr) ..
    ', mailbox at ' .. string.format('%08x', hcmd_addr))
