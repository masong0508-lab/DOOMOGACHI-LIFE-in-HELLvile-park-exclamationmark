"""The pure GS boot diagnostic must not accidentally gain IOP services.

This is intentionally a source-level guard; EE cross-link and live Play!
captures remain separate gates.
"""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
gs = (ROOT / "src" / "gs_only.c").read_text(encoding="utf-8")
make = (ROOT / "Makefile").read_text(encoding="utf-8")


def test_gs_only():
    forbidden = (
        "SifInitRpc(", "SifLoadModule(", "padInit(", "padPortOpen(",
        "mcInit(", "sceCdInit(", "audsrv_init(", "doomo_init(",
        "ps2_audio_init(", "ps2_memcard_init(", "ps2_talk_disc_init(",
    )
    for token in forbidden:
        assert token not in gs, f"GS-only diagnostic added IOP dependency: {token}"
    for token in ("gsKit_init_screen(", "gsKit_clear(", "gsKit_prim_sprite(",
                  "gsKit_queue_exec(", "gsKit_sync_flip("):
        assert token in gs, f"Missing real GS display operation: {token}"
    assert "gs->PSM=GS_PSM_CT32;" in gs
    assert "gs->PSM=GS_PSM_CT16;" in gs
    assert "#if defined(PS2_GS_PROBE_CT16)" in gs
    assert "#if defined(PS2_GS_PROBE_ALPHA)" in gs
    assert "gs->PrimAlphaEnable=GS_SETTING_ON;" in gs
    assert "gs->PrimAlphaEnable=GS_SETTING_OFF;" in gs
    assert "gs-probes: gs-only $(GS_CT16_BIN) $(GS_ALPHA_BIN)" in make
    assert "-DPS2_GS_PROBE_CT16=1" in make
    assert "-DPS2_GS_PROBE_ALPHA=1" in make
    assert "for(;;)" in gs and "++field;" in gs
    assert "gs-only: $(GS_ONLY_BIN)" in make
    assert "-lgskit -ldmakit" in make


if __name__ == "__main__":
    test_gs_only()
    print("PS2 pure GS diagnostic has no IOP startup dependencies")
