#!/usr/bin/env bash
# enhance_logo.sh - rebuild trailer/assets/dippinn_logo.mp4 from the original
# "Dippinn Productions" bumper with AI upscaling (Real-ESRGAN, ncnn/Vulkan;
# runs on CPU via Mesa lavapipe if there is no GPU).
#
#   usage: enhance_logo.sh ORIGINAL.mp4 WORKDIR [OUT.mp4]
# needs: ffmpeg, python3.11 + pip wheel realesrgan-ncnn-py 2.0.0, libomp5,
#        mesa-vulkan-drivers (CPU Vulkan) if no GPU.
#
# Pipeline (source is soft, noisy 1080p60, 12.5 s):
#   trim 0.5..10.6 s, 30 fps
#   0.5..8.0 s (sky + transition): hqdn3d pre-denoise -> 640x360 (area)
#       -> realesr-animevideov3 x3 -> 1920x1080 -> temporal-only denoise
#       (anti-flicker) -> CAS sharpen -> gentle saturation/contrast lift
#   8.0..10.6 s (static title): 2.3 s of frames averaged (tmix) to remove
#       compression noise -> 640x360 -> realesrgan-x4plus x4 -> 2560x1440
#       -> lanczos 1920x1080, held, 0.6 s fade out
#   audio: +12 dB, light FFT denoise, matching fade
set -euo pipefail
src="$1"; work="$2"; out="${3:-$(dirname "$0")/../assets/dippinn_logo.mp4}"
py="${PY:-python3.11}"
mkdir -p "$work/src" "$work/up"
ffmpeg -v error -y -ss 0.5 -to 8.0 -i "$src" -vf "fps=30,hqdn3d=2:2:3:3,scale=640:360:flags=area" "$work/src/%04d.png"
ffmpeg -v error -y -ss 8.2 -to 10.5 -i "$src" -vf "fps=30,tmix=frames=60,scale=640:360:flags=area" -update 1 "$work/title_clean_640.png"
LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/usr/lib/llvm-18/lib" "$py" - "$work" <<'PY'
import glob, os, sys
from PIL import Image
from realesrgan_ncnn_py import Realesrgan
w = sys.argv[1]
if not os.path.exists(f"{w}/title_x4plus.png"):
    Realesrgan(gpuid=0, model=4, tilesize=128).process_pil(
        Image.open(f"{w}/title_clean_640.png").convert("RGB")).save(f"{w}/title_x4plus.png")
r = Realesrgan(gpuid=0, model=1, tilesize=128)
for f in sorted(glob.glob(f"{w}/src/*.png")):
    o = f"{w}/up/" + os.path.basename(f)
    if not os.path.exists(o):
        r.process_pil(Image.open(f).convert("RGB")).save(o)
PY
"$(dirname "$0")/compose_logo.sh" "$src" "$work" "$out"
