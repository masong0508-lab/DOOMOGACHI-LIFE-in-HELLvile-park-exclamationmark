-- harness.lua - headless PCSX-Redux test driver for the DOOMOGACHI PS1 build.
--
-- Loaded with `pcsx-redux -cli ... -dofile harness.lua` by run.py, which
-- passes everything through environment variables:
--   HARNESS_CMDS   low-level command file (one per line, see below)
--   HARNESS_OUT    output directory (screenshots, vram dumps, results)
--   HARNESS_FRAME  address (hex) of the game loop counter g_loop_frames:
--                  heartbeat (hang detection), frame rate, legacy sync
--   HARNESS_HCMD   address (hex) of the g_hcmd mailbox (src/hcmd.c), 0 = none
--   HARNESS_BEAT   address (hex) of g_io_beat (memory card I/O heartbeat)
--   HARNESS_HANG   vsyncs without a new frame before declaring a hang
--
-- LOCKSTEP (builds with the g_hcmd mailbox, i.e. all current builds): the
-- game stops at the top of its loop whenever its loop counter reaches
-- `stop_at`; this script then applies pad changes, takes screenshots of
-- exactly that frame and runs mailbox commands before letting it go on.
-- A script therefore sees the same frames as the native runner:
--   waitf N           let the game run N more frames (game time)
--   down BUTTON       press and keep holding a pad button
--   up BUTTON         release it
--   shot NAME         the frame just drawn -> OUT/NAME.vram + .txt
--                     ("vram X Y W H"; run.py crops it to a PNG)
--   dshot NAME        what the video output shows (display area, one
--                     frame behind; checks display mode / position)
--   vram NAME         raw 1024x512 VRAM dump -> OUT/NAME.vram
--   fps NAME / fpsend NAME   measure the frame rate in between
--   hcmd CMD IARG HEX game-side command through the mailbox: scene, push,
--                     expect-scene, save, load, expect-save, type, log
--                     (HEX = string argument); waits for the reply,
--                     failures are logged as "HARNESS: FAIL: ..."
--   mark TEXT         log a marker line (run.py: no-stubs checkpoints)
--   quit              exit the emulator (also done after the last command)
-- Legacy (no mailbox): `wait N` (vsyncs), `waitframe N` (absolute loop
-- counter), `shot` grabs the displayed picture (one frame behind).
-- Buttons: CROSS CIRCLE SQUARE TRIANGLE L1 R1 L2 R2 START SELECT UP DOWN
--          LEFT RIGHT.
local ffi = require('ffi')

local out = os.getenv('HARNESS_OUT') or '.'
local cmdfile = os.getenv('HARNESS_CMDS')
local frame_addr = tonumber(os.getenv('HARNESS_FRAME') or '0', 16) or 0
local hcmd_addr = tonumber(os.getenv('HARNESS_HCMD') or '0', 16) or 0
local beat_addr = tonumber(os.getenv('HARNESS_BEAT') or '0', 16) or 0
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

local function ram(addr) -- byte pointer into main RAM (KSEG0/1 address)
  return ffi.cast('uint8_t*', PCSX.getMemPtr()) + bit.band(addr, 0x1fffff)
end

local function read_frame()
  if frame_addr == 0 then return 0 end
  return ffi.cast('uint32_t*', ram(frame_addr))[0]
end

-- g_hcmd layout (src/hcmd.c): 0 magic, 4 req, 8 ack, 12 status, 16 iarg,
-- 20 lock, 24 stop_at, 28 paused, 32 shot_y, 36 cmd[16], 52 arg[64],
-- 116 reply[64]
local H_REQ, H_ACK, H_STATUS, H_IARG = 4, 8, 12, 16
local H_LOCK, H_STOP, H_PAUSED, H_SHOTY = 20, 24, 28, 32
local H_CMD, H_ARG, H_REPLY = 36, 52, 116
local function hu32(off) return ffi.cast('uint32_t*', ram(hcmd_addr + off)) end
local function hstr(off, len)
  local p, t = ram(hcmd_addr + off), {}
  for i = 0, len - 1 do
    if p[i] == 0 then break end
    t[#t + 1] = string.char(p[i])
  end
  return table.concat(t)
end
local function hput(off, len, str)
  local p = ram(hcmd_addr + off)
  for i = 0, len - 1 do
    p[i] = i < #str and str:byte(i + 1) or 0
  end
end
local function unhex(h)
  return (h:gsub('%x%x', function(x) return string.char(tonumber(x, 16)) end))
end
local function mailbox_ready()
  return hcmd_addr ~= 0 and hu32(0)[0] == 0x444d4348
end

local function save_slice(path, slice)
  local f = Support.File.open(path, 'TRUNCATE')
  f:write(slice)
  f:close()
end

local idx = 1
local vsyncs = 0
local last_frame, last_change = -1, 0
local fps_start = {}
local done = false
local quit_at = nil
local lock = false        -- lockstep established
local base = 0            -- lockstep: loop counter the script stands at
local pending = nil       -- outstanding mailbox request
local stopped = 0         -- vsyncs the game spent stopped by lockstep
local wait_left = 0       -- legacy: vsyncs to wait
local wait_frame = nil    -- legacy: loop counter to wait for

-- PCSX.quit() is issued on a later vsync, outside the command loop
local function finish(code)
  if done then return end
  done = true
  if lock then hu32(H_LOCK)[0] = 0 end
  log(string.format('END vsyncs=%d frames=%d code=%d', vsyncs, read_frame(), code or 0))
  if results then results:close(); results = nil end
  quit_at = vsyncs + 2
end

local function shot(name, display)
  if lock and not display then
    -- the game is stopped right after drawing frame `base`: grab it from
    -- the framebuffer it was drawn into
    save_slice(out .. '/' .. name .. '.vram', PCSX.GPU.getVRAM())
    local f = io.open(out .. '/' .. name .. '.txt', 'w')
    f:write(string.format('vram 0 %d 320 240\n', hu32(H_SHOTY)[0]))
    f:close()
    log(string.format('shot %s frame=%d vsync=%d', name, read_frame(), vsyncs))
  else
    local ss = PCSX.GPU.takeScreenShot()
    save_slice(out .. '/' .. name .. '.raw', ss.data)
    local f = io.open(out .. '/' .. name .. '.txt', 'w')
    f:write(string.format('%d %d %s\n', ss.width, ss.height, tostring(ss.bpp)))
    f:close()
    log(string.format('shot %s %dx%d frame=%d vsync=%d (display)', name, ss.width,
                      ss.height, read_frame(), vsyncs))
  end
end

-- run commands until one needs game time to pass; returns false when the
-- script is over
local function step()
  while idx <= #cmds do
    local c = cmds[idx]
    idx = idx + 1
    if c.op == 'waitf' then
      local n = tonumber(c.arg) or 1
      if lock then
        base = base + n
        hu32(H_STOP)[0] = base
      else
        wait_frame = read_frame() + n
      end
      return true
    elseif c.op == 'wait' then
      wait_left = tonumber(c.arg) or 1
      return true
    elseif c.op == 'waitframe' then
      if not lock then
        wait_frame = tonumber(c.arg) or 1
        return true
      end
    elseif c.op == 'down' then
      pad().setOverride(B[c.arg])
    elseif c.op == 'up' then
      pad().clearOverride(B[c.arg])
    elseif c.op == 'shot' then
      shot(c.arg)
    elseif c.op == 'dshot' then
      shot(c.arg, true)
    elseif c.op == 'vram' then
      save_slice(out .. '/' .. c.arg .. '.vram', PCSX.GPU.getVRAM())
      log('vram ' .. c.arg)
    elseif c.op == 'fps' then
      fps_start[c.arg] = { v = vsyncs, f = read_frame(), s = stopped }
    elseif c.op == 'fpsend' then
      -- vsyncs while the harness held the game (screenshots, commands)
      -- do not count
      local s = fps_start[c.arg]
      if s then
        local dv, df = vsyncs - s.v - (stopped - s.s), read_frame() - s.f
        log(string.format('fps %s %.2f (%d frames / %d vsyncs)', c.arg,
                          dv > 0 and df * 60.0 / dv or 0, df, dv))
      end
    elseif c.op == 'mark' then
      log('mark ' .. c.arg)
    elseif c.op == 'hcmd' then
      local name, iarg, hex = c.arg:match('^(%S+)%s+(%-?%d+)%s*(%x*)$')
      if not mailbox_ready() then
        log('FAIL: hcmd ' .. (name or c.arg) .. ': no g_hcmd mailbox in this build')
      else
        local arg = unhex(hex or '')
        hput(H_CMD, 16, name)
        hput(H_ARG, 64, arg)
        ffi.cast('int32_t*', ram(hcmd_addr + H_IARG))[0] = tonumber(iarg)
        local req = hu32(H_REQ)[0] + 1
        hu32(H_REQ)[0] = req
        pending = { name = name, arg = arg, req = req, since = vsyncs }
        return true
      end
    elseif c.op == 'quit' then
      return false
    end
  end
  return false
end

local last_beat = -1
local function hang_check(f)
  local beat = beat_addr ~= 0 and ffi.cast('uint32_t*', ram(beat_addr))[0] or 0
  if f ~= last_frame or beat ~= last_beat then
    last_frame = f
    last_beat = beat
    last_change = vsyncs
    return false
  end
  -- (frame 0 = still booting: BIOS, disc, memory card scan; the overall
  -- run.py --timeout covers a hang there)
  if frame_addr ~= 0 and f > 0 and vsyncs - last_change > hang_limit and not pending then
    log(string.format('HANG: no new frame for %d vsyncs (frame=%d)', vsyncs - last_change, f))
    local ss = PCSX.GPU.takeScreenShot()
    save_slice(out .. '/hang.raw', ss.data)
    local t = io.open(out .. '/hang.txt', 'w')
    t:write(string.format('%d %d %s\n', ss.width, ss.height, tostring(ss.bpp)))
    t:close()
    finish(3)
    return true
  end
  return false
end

local function on_vsync()
  vsyncs = vsyncs + 1
  if done then
    if quit_at and vsyncs >= quit_at then
      quit_at = nil
      PCSX.quit()
    end
    return
  end
  local f = read_frame()
  -- a stopped game is not hung: only count while it is free running
  if lock and hu32(H_PAUSED)[0] ~= 0 then
    last_change = vsyncs
    stopped = stopped + 1
  end
  if hang_check(f) then return end

  if not lock and hcmd_addr ~= 0 then
    -- wait for the exe to be in RAM, then stop the game at the top of its
    -- first frame
    if not mailbox_ready() then return end
    hu32(H_STOP)[0] = 0
    hu32(H_LOCK)[0] = 1
    lock = true
    log('lockstep on')
  end

  if pending then
    if hu32(H_ACK)[0] ~= pending.req then
      if vsyncs - pending.since > 3600 then
        log('FAIL: hcmd ' .. pending.name .. ' not acknowledged by the game')
        pending = nil
      end
      return
    end
    local ok = ffi.cast('int32_t*', ram(hcmd_addr + H_STATUS))[0] ~= 0
    local shown = pending.arg:gsub('\n', '\\n'):gsub('[%c]', '?')
    log(string.format('%shcmd %s %s -> %s', ok and '' or 'FAIL: ', pending.name, shown,
                      hstr(H_REPLY, 64)))
    pending = nil
  elseif lock then
    -- wait until the game stopped at the frame the script is at
    if hu32(H_PAUSED)[0] ~= base + 1 then return end
  else
    if wait_frame then
      if frame_addr ~= 0 and f < wait_frame then return end
      wait_frame = nil
    end
    if wait_left > 0 then
      wait_left = wait_left - 1
      if wait_left > 0 then return end
    end
  end
  if not step() then finish(0) end
end

-- keep a reference: an unreferenced listener is garbage collected and stops
HARNESS_LISTENER = PCSX.Events.createEventListener('GPU::Vsync', on_vsync)

log(string.format('started, %d commands, frame counter at %08x, mailbox at %08x', #cmds,
                  frame_addr, hcmd_addr))
