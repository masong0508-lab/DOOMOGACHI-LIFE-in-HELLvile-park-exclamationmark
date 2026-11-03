# W6 step 2 — real-hardware check (GBA / PS1 / DC)

## Hub B shortcut (LS-QA)
1. New island, give two citizens a problem (or wait for them). On the hub press **B**.
   - Expect: the care screen of the lowest-id citizen with a problem opens (SELECT sfx), no START menu.
2. **A** on Help! -> problem cleared. **B** twice to return to the hub.
3. Press **B** again -> the *next* citizen (round robin), wrapping to the first after the last.
4. Clear every problem, press **B** on the hub -> the START menu opens as before.
5. Help! on a "wants item" problem with the item NOT in the bag and enough H$: toast "Bought X: -N H$", then helped. With too little H$: "You don't have one... it's okay.", problem stays, no money lost.

## Press latch (LS-IN)
6. Hold B on the hub through the scene change: the care screen must not instantly close or re-trigger (one press = one action).
7. Mash B/A for 5 s on the hub and on the care screen: no double-open, no stuck scene, no lost input after returning.
8. Hold a direction on the hub (btn_repeat), then press B: cursor still repeats afterwards.

## Mature slots (only with Mature = "On + pack" and hellvile_after_dark.dmpk loaded)
9. Date a partner who passes the adult gate, advance days: scene slots queue, play once when the overworld is idle, return to the overworld.
10. Mature = Off / On / Censored: slots play the plain fade-to-black, never the pack text.
