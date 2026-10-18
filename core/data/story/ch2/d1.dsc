# ch2/d1.dsc - Dream D1 "Teeth Like Muffins" (owner: S2; story_final.md
# sec.11). 90 seconds, at night, a forced Peek at a Dream: SCENE_DREAM's
# scripted mode (dream.c, DREAM_ARG_SCRIPTED(DREAM_SCRIPTED_D1)) draws the
# vignettes and floats the stashed captions in letter by letter (slots
# 0-2, the wake-up line in 3). The vape variant: the muffins are grape and
# the key is purple.
@pack d1 kind=episode chapter=2 minutes=2

script d1_dream
  jif SF_VAPE_SAMPLE grape
  stash 0 "The Mingle. Everyone's teeth are muffins."
  stash 1 "A bite. A bite. A bite that is a key."
  jmp rest
grape:
  stash 0 "The Mingle. Everyone's teeth are grape muffins."
  stash 1 "A bite. A bite. A bite that is a purple key."
rest:
  stash 2 "is this your room? it's nice. can i stay?"
  stash 3 "...it's just your bedroom."
  push dream 0x4000
end
