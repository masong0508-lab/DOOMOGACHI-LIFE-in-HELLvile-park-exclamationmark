"""AT4.2: shw_format.py round-trips with shw_format.h (the player's header).

1. The host gcc compiles the header; sizeof of every struct equals Python's struct size,
   and every SHWR_/SHWP_/SOP_/SHW_ constant has the same value.
2. A compiled episode (the dialogue proof) is read by a C program through the header's
   structs; its dump of the index and every chunk's record list equals read_episode().
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile

import pytest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "show", "tools"))

from showtk import shw_format as F  # noqa: E402
from showtk import showc  # noqa: E402

INC = os.path.join(ROOT, "show", "include")
HDR = os.path.join(INC, "show", "shw_format.h")
GCC = shutil.which("gcc") or shutil.which("cc")

SIZES = {"ShwEpHeader": F.EP_HEADER, "ShwShotIndex": F.SHOT_INDEX, "ShwShotHdr": F.SHOT_HDR, "ShwRec": F.REC,
         "ShwPlane": F.PLANE, "ShwImage": F.IMAGE, "ShwSlug": F.SLUG, "ShwAudioK": F.AUDIO_K, "ShwBand": F.BAND}


def _consts():
    src = re.sub(r"/\*.*?\*/", "", open(HDR).read(), flags=re.S)
    names = re.findall(r"\b((?:SHWR|SHWP|SHWF|SHWPF|SHWI|SHWB|SOP)_[A-Z0-9_]+)\b\s*=", src)
    names += re.findall(r"#define\s+(SHW_[A-Z_]+|SHOW_OP_FLAG)\s", src)
    return sorted(set(names))


def _run_c(body, tmp):
    c = os.path.join(tmp, "t.c")
    open(c, "w").write('#include <stdio.h>\n#include <stdlib.h>\n#include <string.h>\n'
                       '#include "show/shw_format.h"\n' + body)
    exe = os.path.join(tmp, "t")
    subprocess.check_call([GCC, "-std=c99", "-Wall", "-Werror", "-I", INC, c, "-o", exe])
    return exe


def py_value(name):
    if hasattr(F, name):
        return getattr(F, name)
    alias = {"SHW_CHUNK_MAX": F.SHW_CHUNK_MAX}
    return alias[name]


@pytest.mark.skipif(GCC is None, reason="no host C compiler")
def test_sizes_and_constants_match_header():
    names = _consts()
    assert len(names) > 40
    with tempfile.TemporaryDirectory() as tmp:
        body = "int main(void){\n"
        for s in SIZES:
            body += '  printf("%s %%u\\n", (unsigned)sizeof(%s));\n' % (s, s)
        for n in names:
            body += '  printf("%s %%ld\\n", (long)(%s));\n' % (n, n)
        body += "  return 0;}\n"
        out = subprocess.check_output([_run_c(body, tmp)]).decode().split("\n")
    got = dict(l.split() for l in out if l)
    for s, st in SIZES.items():
        assert int(got[s]) == st.size, s
    for n in names:
        assert int(got[n]) == py_value(n), n


C_DUMP = r"""
int main(int argc, char **argv){
  FILE *f = fopen(argv[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
  unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
  const ShwEpHeader *h = (const ShwEpHeader *)b;
  printf("ep %u %u %u %u %u\n", h->magic, h->abi, h->nshots, h->total_frames, h->episode);
  for (int i = 0; i < h->nshots; i++) {
    const ShwShotIndex *x = (const ShwShotIndex *)(b + h->index_off) + i;
    char id[SHW_ID_LEN + 1]; memcpy(id, x->id, SHW_ID_LEN); id[SHW_ID_LEN] = 0;
    printf("shot %s %u %u %u %u %u\n", id, x->start_frame, x->len, x->px, x->chunk_bytes, x->flags);
    const unsigned char *c = b + (long)x->chunk_sector * SHW_SECTOR;
    const ShwShotHdr *sh = (const ShwShotHdr *)c;
    printf("hdr %u %u %u %u\n", sh->magic, sh->nrec, sh->shot, sh->bytes);
    const unsigned char *p = c + sizeof(ShwShotHdr);
    for (int r = 0; r < sh->nrec; r++) {
      const ShwRec *rec = (const ShwRec *)p;
      printf("rec %u %u %u\n", rec->type, rec->n, rec->bytes);
      p += sizeof(ShwRec) + ((rec->bytes + 3) & ~3u);
    }
  }
  return 0;
}
"""


@pytest.mark.skipif(GCC is None, reason="no host C compiler")
def test_c_reads_what_the_compiler_writes():
    r = showc.compile_episode("dxp", root=os.path.join(ROOT, "show", "tests", "dx_proof"),
                              series=os.path.join(ROOT, "show", "series"))
    ep = F.read_episode(r.shw)
    want = ["ep %d %d %d %d %d" % (ep["magic"], ep["abi"], ep["nshots"], ep["total_frames"], ep["episode"])]
    for s in ep["shots"]:
        want.append("shot %s %d %d %d %d %d" % (s["id"], s["start_frame"], s["len"], s["px"], s["chunk_bytes"],
                                                 s["flags"]))
        h = s["chunk_hdr"]
        want.append("hdr %d %d %d %d" % (h["magic"], h["nrec"], h["shot"], h["bytes"]))
        for t, (n, pay) in s["records"].items():
            want.append("rec %d %d %d" % (t, n, len(pay)))
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "DXP.SHW")
        open(p, "wb").write(r.shw)
        out = subprocess.check_output([_run_c(C_DUMP, tmp), p]).decode().strip().split("\n")
    assert out == want
    assert ep["total_frames"] == 600 and [s["id"] for s in ep["shots"]] == ["dx01", "dx02", "dx03", "dx04"]
