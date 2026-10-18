# ch1/ep14.dsc - Ep 1.4 "Three Neighbours" (owner: S2; story_final.md
# sec.10). Meet, by name and verb, the three people who will be possessed
# later, so it hurts. Any order. Replaces the old "talk to three people"
# counter (C1_FRIENDS): quest_step bit C12_MET_* per neighbour; the third
# one queues e14_done (story_ch12.c). Also met: Bonejangles and Miss
# Lulla's kids, two beats each.
@pack ep14 kind=episode chapter=1 minutes=30

script e14_steamy
  say steamy HAPPY "A new face! Welcome to the / bath house. Well. The pond. / Same thing, more bubbles."
  say steamy "Help me fill the tubs? Every / tub gets a duck. House / rule."
  choice nar "(Fill the tubs?)" { "Fill them!" -> fill ; "Maybe later." -> later }
later:
  say steamy "Any time! The water's always / warm. That's my whole job."
  end
fill:
  sfx SFX_SPLASH
  say nar "(Splash. Splash. Splash.)"
  say steamy HAPPY "That's Duckworth. That's / Quackers. That's..."
  [R5]
  say steamy SHOCK "{.}I don't know that one."
  [MUTE 13]
  [HOLD 13]
  sfx SFX_SQUEAK
  say nar "(The strange duck squeaks. / By itself. It is facing / the Wall.)"
  [R8]
  sfx SFX_SQUEAK
  say nar "(Steamy squeaks a duck back. / Nervously.)"
  say steamy HAPPY "Come for a soak any time, / @P. Everybody needs to / be warm."  [HEART]
  [R21]
  befriend steamy 4
  native CB_C12 C12A_BIT C12_MET_STEAMY
end

script e14_steamy_again
  say steamy HAPPY "Duckworth says hi. The other / one doesn't say anything. / It just... looks."
end

script e14_hattie
  say hattie HAPPY "Open mic! Well, open bench. / Help me with my timing? A / comic is ALL timing."
  say hattie "Press A on the punchline. / Not before. NEVER before."
  [R13]
  push timing pattern=rimshot skin=laugh
  jge v0 3 great
  jge v0 1 okay
  sfx SFX_TIK
  say nar "(...a cricket.)"
  say hattie SAD "Tough crowd. You ARE the / crowd. Still."
  jmp closer
great:
  sfx SFX_HAPPY
  say nar "(A snort-laugh ripples all / the way to the back row.)"
  jmp closer
okay:
  say nar "(A polite chuckle.)"
closer:
  say hattie HAPPY "My closer! So I said, / 'Mayor, what's on the / other side?'"
  say hattie HAPPY "He said: 'Hee. Hee.'"
  [R8]
  sfx SFX_HAPPY
  say nar "(Big laugh.)"
  [MUTE 21]
  [R13]
  say nar "(In the back row, the mascot / does not laugh.)"
  befriend hattie 4
  native CB_C12 C12A_BIT C12_MET_HATTIE
end

script e14_hattie_again
  say hattie HAPPY "Knock knock. {.}Who's there? / {..}See, that's the trick. / Nobody ever says."
end

script e14_grampy
  say grampy HAPPY "Sunny! Help an old bean pick / tomatoes? Five. My back / says five."
  choice nar "(A fat red tomato.)" { "Pick it" }
  sfx SFX_POP2
  say grampy HAPPY "Good, Sunny."
  [R8]
  choice nar "(Another one.)" { "Pick it" }
  sfx SFX_POP2
  say grampy HAPPY "Lovely, Sonny."
  [R8]
  choice nar "(Another one.)" { "Pick it" }
  sfx SFX_POP2
  say grampy "Steady, Bunny."
  [R8]
  choice nar "(Another one.)" { "Pick it" }
  sfx SFX_POP2
  [R5]
  say grampy NEUTRAL "{.}Careful there, Toaster."
  choice nar "(The last one.)" { "Pick it" }
  sfx SFX_POP2
  say grampy HAPPY "Five! You're a natural, / Toaster. Take a salad / home. Same thing."
  [R13]
  give ITEM_SALAD
  [R8]
  say grampy NEUTRAL "These seeds came over the / fence years back. Never / asked who threw 'em."
  [MUTE 21]
  [R13]
  say grampy SLEEPY "...where was I?"
  befriend grampy 4
  native CB_C12 C12A_BIT C12_MET_GRAMPY
end

script e14_grampy_again
  say grampy SLEEPY "Toaster! Or... no. Hm. / You've got a toaster sort / of face. It's a compliment."
end

script e14_bonejangles
  say bonejangles HAPPY "Clack clack! I teach / xylophone. I AM a / xylophone. Hug?"
  choice nar "(Hug Bonejangles?)" { "Hug!" -> hug ; "Wave" -> wave }
hug:
  sfx SFX_PLUCK
  [R3]
  sfx SFX_PLUCK
  [R3]
  sfx SFX_PLUCK
  say nar "(He plays a chord.)"
  [R13]
  befriend bonejangles 3
  end
wave:
  say bonejangles "Clack."
  [R8]
end

script e14_lulla
  say lulla HAPPY "Hello! Shh, the little ones / are... oh. They've seen / your Doomogachi."
  npc lulla RIPPLE 6
  sfx SFX_BEEP
  say nar "(Eleven small voices:) / BEEP!"
  [R8]
  sfx SFX_BEEP
  say nar "(@E, very quietly:) / beep."
  befriend lulla 2
end

script e14_done
  [R13]
  sfx SFX_BLIP
  say nar "(Your phone buzzes. A text / from Nana:)"
  say nana HAPPY "You've made friends! Come / get a cupcake to share. / xo"
  beat C1_CUPCAKE
end
