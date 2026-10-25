"""WP17: AT17.1 every fixture fails its lint; AT17.2 --release refuses an unsigned or stale
sign-off; AT17.3 the release package has every sec.13.7 item."""
import json
import os
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1] / "tools"))

from showtk import deliver, ffmpeg as ff, release  # noqa: E402
from showtk.review import synthetic  # noqa: E402
from showtk.sidecars import Timeline, load_timeline  # noqa: E402
from showtk.standards import lints, report, signoff  # noqa: E402

FIXTURES = sorted((HERE / "fixtures").glob("*.json"))


def load(p):
    return Timeline(json.loads(Path(p).read_text()), p)


@pytest.mark.parametrize("fx", FIXTURES, ids=[p.stem for p in FIXTURES])
def test_at17_1_every_fixture_fails_its_lint(fx):
    tl = load(fx)
    want = tl.raw["expect"]
    rules = {d.rule for d in lints.run(tl)}
    assert want in rules, "%s (%s) did not trip %s: %s" % (fx.name, tl.raw["why"], want, rules)


def test_at17_1_every_rule_has_a_fixture():
    covered = {json.loads(p.read_text())["expect"] for p in FIXTURES}
    assert set(lints.HARD) <= covered
    assert {"STD-TAGSCAN", "STD-FLASH", "STD-FPS", "STD-STICKER", "STD-LOCK"} <= covered


def test_clean_control_passes():
    assert [d.format() for d in lints.run(load(HERE / "clean.json")) if d.is_error] == []


def test_slur_list_is_hashed():
    assert "zorbnak" not in (HERE.parents[1] / "series" / "lints.shw").read_text()
    assert len(lints.term_hash("x")) == 64


def test_signoff_parse():
    assert all(ok for ok, _ in signoff.parse_reply("all 8 fine").values())
    r = signoff.parse_reply("ok except 7: thumbnail too cute")
    assert not r[7][0] and r[1][0] and "thumbnail" in r[7][1]


@pytest.fixture(scope="module")
def ep(tmp_path_factory):
    if not ff.have_ffmpeg():
        pytest.skip("ffmpeg")
    root = tmp_path_factory.mktemp("wp17")
    out = synthetic.make(root)
    old = {k: os.environ.get(k) for k in ("SHOW_BUILD", "SHOW_EPISODES")}
    os.environ["SHOW_BUILD"] = str(root / "build")
    os.environ["SHOW_EPISODES"] = str(root / "eps")
    tl = json.loads(out["timeline"].read_text())
    tl["shots"][0]["scene"] = "co_calendar"        # an adult-free... no child: the calendar page
    tl["shots"][1]["scene"] = "co_storybook"
    tl["acts"] = [{"id": "CO", "title": "Cold open"}]
    out["timeline"].write_text(json.dumps(tl))
    (out["build"] / "lint.txt").write_text("ep101 co01..co03 [AUD-FRAME0] ok: every cut has a sound\n")
    yield out
    for k, v in old.items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = v


def test_at17_2_release_refuses_unsigned_then_stale(ep):
    with pytest.raises(deliver.DeliverError) as e:
        deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=ep["masters"], release=True)
    assert "unsigned" in str(e.value)
    signoff.write("ep101", "all 8 fine", render_hash="0" * 64, version="web")
    with pytest.raises(deliver.DeliverError) as e:
        deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=ep["masters"], release=True)
    assert "stale" in str(e.value)
    signoff.write("ep101", "ok except 7: not sure about the thumbnail")
    with pytest.raises(deliver.DeliverError) as e:
        deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=ep["masters"], release=True)
    assert "open" in str(e.value) and "7" in str(e.value)


def test_at17_3_release_package_has_every_item(ep):
    deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=ep["masters"])
    signoff.write("ep101", "all 8 fine")            # bound to the newest manifest's render hash
    m = deliver.deliver("ep101", "web", rng="co01..co03", masters_dir=ep["masters"], release=True)
    pkg = Path(m["_paths"]["release"])
    items = json.loads((pkg / "package.json").read_text())["items"]
    assert set(release.ITEMS) <= set(items)
    for k, name in items.items():
        assert (pkg / name.split("#")[0]).exists(), k
    sheet = (pkg / "upload_sheet.md").read_text()
    assert "not made for kids" in sheet and "over 18" in sheet
    desc = (pkg / "description.txt").read_text()
    assert "TV-MA" in desc and "Not made for kids" in desc and "00:00 Cold open" in desc
    tags = (pkg / "tags.txt").read_text().lower()
    assert "kids" not in tags
    from PIL import Image
    im = Image.open(pkg / "thumbnail.png")
    assert im.size == (1280, 720)
    # the thumbnail comes from a scene with no child in it (co01, the calendar page)
    assert json.loads((pkg / "package.json").read_text())["thumbnail_frame"] < 90


def test_standards_report_lists_the_kids_scene(ep):
    txt, htm, diags = report.build(load_timeline("ep101"), ep["masters"])
    t = Path(txt).read_text()
    assert "scene co_storybook" in t and "clean" in t
    assert not [d for d in diags if d.is_error]
    assert (Path(htm).parent / "standards" / "kids_co_storybook.png").exists()
