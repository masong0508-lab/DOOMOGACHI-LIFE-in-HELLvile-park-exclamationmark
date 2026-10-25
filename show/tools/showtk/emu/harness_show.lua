-- harness_show.lua - the show's capture driver for PCSX-Redux (owner: ENGINE;
-- spec 11.1). A FORK of ps1/harness/harness.lua (never edited): the same
-- lockstep on g_hcmd (the game stops at the top of its loop, after the GPU
-- finished, with g_hcmd.shot_y = the VRAM row of the frame just drawn), plus
-- the show's mailbox g_show_mbox (show/include/show/show_mbox.h).
--
-- Environment (set by showtk/capture.py):
--   SHOW_HCMD, SHOW_MBOX, SHOW_FRAME  hex addresses of g_hcmd, g_show_mbox,
--                                     g_loop_frames (from show.map)
--   SHOW_FIFO   path the frames are streamed to (a FIFO; capture.py reads it)
--   SHOW_RUNS   "shot:frame:count:last;..." one run per item: seek to
--               (shot, frame), capture `count` frames, stop after shot `last`
--   SHOW_EPISODE  episode number (0 = 101)
--   SHOW_HANG   vsyncs without a new frame = hang (default 600)
--   SHOW_LOG    the harness log (results, HANG with shot and frame)
--
-- Stream, per captured frame: the 176-byte ShowMbox record as published for
-- that frame, then 240 rows x 640 bytes of BGR555 (x 0-319 at shot_y).
-- Settings: PCSX.settings.spu.Speed = 0 (unthrottled, spec 11.1); NEVER set
-- UseNullSync (it hangs).
local ffi = require('ffi')
local unthrottled = pcall(function() PCSX.settings.spu.Speed = 0 end)

local hcmd_addr = tonumber(os.getenv('SHOW_HCMD') or '0', 16) or 0
local mbox_addr = tonumber(os.getenv('SHOW_MBOX') or '0', 16) or 0
local frame_addr = tonumber(os.getenv('SHOW_FRAME') or '0', 16) or 0
local fifo_path = os.getenv('SHOW_FIFO')
local hang_limit = tonumber(os.getenv('SHOW_HANG') or '600')
local episode = tonumber(os.getenv('SHOW_EPISODE') or '0')
local logpath = os.getenv('SHOW_LOG') or 'harness_show.log'

local MBOX_BYTES, ROW_BYTES, ROWS = 176, 640, 240

local runs = {}
for item in (os.getenv('SHOW_RUNS') or ''):gmatch('[^;]+') do
  local s, f, n, l = item:match('^(%-?%d+):(%-?%d+):(%d+):(%-?%d+)$')
  if s then runs[#runs + 1] = { shot = tonumber(s), frame = tonumber(f), count = tonumber(n),
                                last = tonumber(l) } end
end

local logf = io.open(logpath, 'w')
local function log(msg)
  print('HARNESS: ' .. msg)
  io.stdout:flush()
  if logf then logf:write(msg .. '\n'); logf:flush() end
end

local function ram(addr)
  return ffi.cast('uint8_t*', PCSX.getMemPtr()) + bit.band(addr, 0x1fffff)
end
local function u32(addr) return ffi.cast('uint32_t*', ram(addr)) end
local function i32(addr) return ffi.cast('int32_t*', ram(addr)) end

-- g_hcmd (ps1/src/hcmd.c): 0 magic, 20 lock, 24 stop_at, 28 paused, 32 shot_y
local H_LOCK, H_STOP, H_PAUSED, H_SHOTY = 20, 24, 28, 32
-- g_show_mbox (show_mbox.h)
local M_REQ_SHOT, M_REQ_FRAME, M_CUR_SHOT, M_CUR_FRAME = 12, 16, 20, 24
local M_SEQ, M_REQ_EP, M_REQ_LAST, M_STATE = 100, 104, 108, 112

local function hcmd_ready() return hcmd_addr ~= 0 and u32(hcmd_addr)[0] == 0x444d4348 end
local function mbox_ready() return mbox_addr ~= 0 and u32(mbox_addr)[0] == 0x424d4853 end

local fifo = nil
local REC = MBOX_BYTES + ROW_BYTES * ROWS
local buf = ffi.new('uint8_t[?]', REC)
short_writes = 0
local vsyncs, lock, base, done, quit_at = 0, false, 0, false, nil
local run_i, left, captured, last_seq = 0, 0, 0, -1
local last_frame, last_change = -1, 0

local function finish(code, why)
  if done then return end
  done = true
  if lock then u32(hcmd_addr + H_LOCK)[0] = 0 end
  log(string.format('END vsyncs=%d captured=%d code=%d %s', vsyncs, captured, code, why or ''))
  if fifo then fifo:close(); fifo = nil end
  if logf then logf:close(); logf = nil end
  quit_at = vsyncs + 2
end

-- start the next run: write the seek request while the game is stopped
local function next_run()
  run_i = run_i + 1
  local r = runs[run_i]
  if not r then return false end
  i32(mbox_addr + M_REQ_EP)[0] = episode
  i32(mbox_addr + M_REQ_LAST)[0] = r.last
  i32(mbox_addr + M_REQ_FRAME)[0] = r.frame
  i32(mbox_addr + M_REQ_SHOT)[0] = r.shot
  left = r.count
  log(string.format('run %d: shot %d frame %d count %d last %d', run_i, r.shot, r.frame,
                    r.count, r.last))
  return true
end

local function capture()
  local seq = u32(mbox_addr + M_SEQ)[0]
  if seq == last_seq then
    log(string.format('FAIL: no frame published at stop %d (state %d)', base,
                      u32(mbox_addr + M_STATE)[0]))
    return false
  end
  last_seq = seq
  local y = u32(hcmd_addr + H_SHOTY)[0]
  local v = PCSX.GPU.getVRAM()
  local src = ffi.cast('const uint8_t*', v.data)
  ffi.copy(buf, ram(mbox_addr), MBOX_BYTES)
  for r = 0, ROWS - 1 do
    ffi.copy(buf + MBOX_BYTES + r * ROW_BYTES, src + ((y + r) % 512) * 2048, ROW_BYTES)
  end
  -- one blocking stdio write per record. NOT Support.File: its writes are
  -- libuv thread-pool requests that reach a FIFO torn and out of order
  -- (measured: the stream lost sync after 2 frames)
  if not fifo:write(ffi.string(buf, REC)) then log('FAIL: the frame stream closed'); return false end
  fifo:flush()
  captured = captured + 1
  left = left - 1
  return true
end

local function step_game()
  base = base + 1
  u32(hcmd_addr + H_STOP)[0] = base
end

local function on_vsync()
  vsyncs = vsyncs + 1
  if done then
    if quit_at and vsyncs >= quit_at then quit_at = nil; PCSX.quit() end
    return
  end
  local f = frame_addr ~= 0 and u32(frame_addr)[0] or 0
  local paused = lock and u32(hcmd_addr + H_PAUSED)[0] or 0
  if f ~= last_frame or paused ~= 0 then last_frame = f; last_change = vsyncs end
  if lock and f > 0 and paused == 0 and vsyncs - last_change > hang_limit then
    log(string.format('HANG: no new frame for %d vsyncs; last published shot %d frame %d; '
                      .. 'making shot %d frame %d', vsyncs - last_change,
                      i32(mbox_addr + M_CUR_SHOT)[0], i32(mbox_addr + M_CUR_FRAME)[0],
                      i32(mbox_addr + M_CUR_SHOT)[0], i32(mbox_addr + M_CUR_FRAME)[0] + 1))
    finish(3, 'hang')
    return
  end
  if not lock then
    if not (hcmd_ready() and mbox_ready()) then return end
    u32(hcmd_addr + H_STOP)[0] = 0
    u32(hcmd_addr + H_LOCK)[0] = 1
    lock = true
    log('lockstep on')
    return
  end
  if paused ~= base + 1 then return end      -- not stopped at our frame yet
  -- stopped at the top of loop `base`, after the GPU finished frame base
  if run_i > 0 and left > 0 then
    if not capture() then finish(4, 'no frame'); return end
  end
  if left <= 0 then
    if not fifo then fifo = io.open(fifo_path, 'wb'); fifo:setvbuf('full', 1048576) end
    if not next_run() then finish(0, 'done'); return end
  end
  step_game()
end

HARNESS_LISTENER = PCSX.Events.createEventListener('GPU::Vsync', on_vsync)
log(string.format('unthrottled %s', tostring(unthrottled)))
log(string.format('started: %d run(s), hcmd %08x, mbox %08x, frame %08x', #runs, hcmd_addr,
                  mbox_addr, frame_addr))
