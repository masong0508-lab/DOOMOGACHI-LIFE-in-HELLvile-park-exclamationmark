# show/DEFERRED.md - what the pilot does not need, so it is not built

One line each: what, and why. Every workflow appends.

## ENGINE

- `show vault push` and the remote vault (private repo / orphan branch / release assets): the creator decides where the vault lives; the archives wait in `<scratch>/vault/`.
- Packing the TTS/ASR models into the vault (~1.2 GB): pinned by hash in `show/vault.lock` this week, packed after the release (disk is tight).
- `show/hooks/session_start.sh` (the opt-in restore hook): `show vault fetch DIR` restores by hand; nothing in this week's container needs restoring.
- `show/tools/requirements.lock`: the tools use what this container has (numpy, Pillow, pytest); `show doctor` checks them.
- Gaze hunk (e), `ps1-show-wide`, `ps1-show-dev` (PCdrv), `runner-show`, P2 draw trace (`show_wrap.c`): deferred to 2027 by the spec itself (§1.2, §4.3, §8.8).
