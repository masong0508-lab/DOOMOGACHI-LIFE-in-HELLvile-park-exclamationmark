# core/tests/story_register/p0.dsc - the seeded bad tree's prologue (E5;
# see bad.dsc). The real P0 pack holds no script, only the crayon beat
# script C code plays; kid_cast.txt lists it as a kids' scene by its pack
# name (@scene p0), so its captions are a kids' scene with no kid named.
@pack p0 kind=episode chapter=1 minutes=2

beatscript p0_egg
  +0 shot 0 ; caption "Every year, the whole park / had a potluck."
  +2 caption "Everybody got drunk and / naked. Damn." # expect: K1
  +8 end
end
