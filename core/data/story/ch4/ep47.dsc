# ch4/ep47.dsc - Chapter 4: Ep 4.7 "Hellvile Park" (the ending; owner: S3;
# story_final.md sec.13): the united park, four festival stalls you run
# (cooking, fishing, the photo booth, the comedy tent: the refrain's third
# time), Grumbleguts at the buffet ([3x]; the belt on the Mayor's hat is the
# festival's button), the Unity Stage, the credits parade.
@pack ep47 kind=episode chapter=4 minutes=25

string O47_STALLS "Lend a hand at the festival stalls (@0/4)"
string O47_STAGE "Everyone is gathering at the Unity Stage (N)"

script s47_open
  music SONG_FESTIVAL
  say nar "(The Wall is rubble. The / river sparkles. The whole / park is ONE park.)"
  [R5]
  say nar "(Hillvile and Hellvile pour in from both sides. Somebody started a festival.)"
  [R13]
  say nar "(Four stalls need a hand: / Nana's kitchen, the Lake of / Fire, the photo booth...)"
  [R5]
  say nar "(...and the comedy tent.)"
end

script s47_cook
  say nana HAPPY "Sweet pea! Charlotte brought her cookies. Burnt. On PURPOSE."
  [R5]
  say ID_CHARLOTTE HAPPY "Thirty years of burnt cookies through//a loose brick. She always wrote back."
  push cooking 0
  [R8]
  say nar "(Both kinds of cookie, one / plate. Everybody takes one / of each.)"
  native CB_C34 C34A_BIT C34_ST_COOK
end

script s47_fish
  sfx SFX_SPLASH
  say nar "(The Lake of Fire. Hillvile kids fish with//Hellvile kids. Nobody catches a thing.)"
  [R8]
  say nar "(Gerald the goldfish swims / past, in the lava. He is / fine.)"
  [R13]
  say nar "(He is having a great day.)"
  native CB_C34 C34A_BIT C34_ST_FISH
end

script s47_photo
  sfx SFX_SHWIP
  say ID_SHUTTERBUG HAPPY "Everybody squeeze in! Both / sides! Big smiles! Bigger! / Horns in!"
  [R8]
  sfx SFX_TIK
  say nar "(The festival polaroid. You / get a copy. It's still / developing.)"
  [R5]
  say nar "(Sprinklepop's little soul / bounces in at the edge, map / held upside down.)"
  native CB_C34 C34A_BIT C34_ST_PHOTO
end

# (chain I done, S133: EXORCISM UNFAILED)
script s47_photo_home
  sfx SFX_SHWIP
  say ID_SHUTTERBUG HAPPY "Everybody squeeze in! Both / sides! Big smiles! Bigger! / Horns in!"
  [R8]
  sfx SFX_TIK
  say nar "(The festival polaroid. You / get a copy. It's still / developing.)"
  [R5]
  say nar "(Sprinklepop bounces in with a heart balloon, map the right way up.)"
  native CB_C34 C34A_BIT C34_ST_PHOTO
end

script s47_joke
  sfx SFX_TIK
  say hattie NEUTRAL "So. {..}We don't talk about / the other side."
  [R13]
  say hattie HAPPY "{big}Because we're STANDING / in it!{/}"
  npc hattie RIPPLE 6
  sfx SFX_HAPPY
  say nar "(Both sides laugh. Haze / snorts. The Mayor goes:)"
  [R5]
  say mayor HAPPY "Ha!"
  [MUTE 21]
  [HOLD 21]
  say nar "(Everyone stares at him.)"
  [R5]
  say mayor SHOCK "...Ha?"
  [R8]
  sfx SFX_HAPPY
  say nar "(They laugh harder.)"
  native CB_C34 C34A_BIT C34_ST_JOKE
end

script s47_buffet
  [SCRATCH]
  say nar "(Grumbleguts at the buffet, back to the crowd. The table empties around him.)"
  [R13]
  [SCRATCH]
  say nar "(Every time you look back, / he is a bit bigger.)"
  [R13]
  [SCRATCH]
  say nar "(Bigger. The whole crowd / is not looking. Very hard.)"
  [R13]
  say ID_HONKERS NEUTRAL "Napkin?"
  [R5]
  say grumble HAPPY "For you, sweetheart, I'll / use it. {..}Nice fold."
  [R5]
  say nar "(The napkin is not / interested.)"
  [R21]
  sfx SFX_BOING
  say nar "(His belt pings off. It sails across the//park and knocks off the Mayor's hat. {r})"
  [R8]
  say mayor SHOCK "MY HEAD."
end

script s47_stage
  music SONG_FESTIVAL
  sfx SFX_FANFARE
  say mayor HAPPY "Citizens of Hillvile! / Citizens of Hellvile! / Citizens of... BOTH!"
  [R5]
  say mayor HAPPY "Population: one thousand / and one." [HEART]
  [R5]
  say nar "(@E, horned, sits on his / hat. Beeps once.)"
  [R5]
  say nox NEUTRAL "Tonight there is no Wall. / There is only the park. / OUR park."
  [R5]
  say pip HAPPY "And Mr. Flops! He has felt / horns now. So his brother / doesn't feel weird."
  [R5]
  say ID_WISP HAPPY "and my chair."
  [R5]
  say fuzz HAPPY "Bless this park. Bless every single one of you. Even you, Vex."
  [R5]
  say vex NEUTRAL "...thanks for saying Vex."
  [R5]
  say vex SAD "I'm selling plain water / vapes now. They're terrible."
  [R5]
  say tallyworn HAPPY "Day one."
  jif SF_WEAPON_BRUTAL bonk
  say rivet HAPPY "Enough speeches! Hit it!"
  end
bonk:
  say rivet HAPPY "Enough speeches! Bonk it!"
end

script s47_stall_idle
  say nar "(This stall is done. The / festival goes on without / needing you. Nice.)"
end
