"""Guard the PS2 graphics-first boot sequence against regressions.

The test reads the actual PS2 EE entry source, ensuring the game produces
its first visible GS diagnostics before it attempts IOP/RPC services.
This verifies source ordering, not that an emulator displayed the result.
"""
from pathlib import Path

SOURCE = Path(__file__).resolve().parents[1] / "src" / "game_main.c"


def test_boot_order():
    code = SOURCE.read_text(encoding="utf-8")
    main = code.split("int main(void)", 1)[1]
    first_gs_init = main.find("gsKit_init_screen(gs);")
    first_visible = main.find("show_boot_stage(gs,0);")
    sif_rpc = main.find("SifInitRpc(0);")
    card = main.find("ps2_memcard_init()")
    disc = main.find("ps2_talk_disc_init()")
    audio = main.find("ps2_audio_init()")
    game = main.find("doomo_init();")
    first_frame = main.find("doomo_frame(buttons);")
    marks = [first_gs_init, first_visible, sif_rpc, card,
             disc, audio, game, first_frame]
    assert all(x >= 0 for x in marks), "Missing a required PS2 boot event"
    assert marks == sorted(marks), (
        "Graphics boot must be complete before IOP modules, and IOP "
        "initialization must precede doomo_init and the game loop"
    )
    assert "gs->PSM=GS_PSM_CT16;" in main
    assert "gs->ZBuffering=GS_SETTING_OFF;" in main


if __name__ == "__main__":
    test_boot_order()
    print("PS2 graphics-before-IOP boot order: passed")
