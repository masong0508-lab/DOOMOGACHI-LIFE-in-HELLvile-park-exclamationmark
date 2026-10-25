# side_b/l_vapula.dsc - Chain L "The Vapula plush, who is Fluffy" (S169-
# S171, post-game) and Chain U "Grumbleguts's little black book" (S172-S174;
# adults only, story_final.md sec.3 rules) (owner: S5; story_final.md
# sec.14.1). A duke the size of a cushion who rains when he's sad; and
# hell's biggest flirt, who is rejected kindly by everyone except, possibly,
# the Gate. Vapula talks in lower case now. Grumbleguts is innuendo only,
# never at a kid (GRUMBLEGUTS_TONE.md); his come-ons use the LOVE face.

# =============================================================================
# S169 "Small Weather" - Lord Vapula (999), a plush cloud in a jar - Ch5
# pet, talk
# =============================================================================
@pack quests kind=side
@mission S169 giver=999 side=park ch=5-5 at=45,30 verbs=pet chain=L step=1 later=5m reward=50 offer=s169_offer active=s169_again title="Small Weather" hint="Find the little cloud in the jar a hobby. Then introduce him to @E."
@pack sb_cloud kind=side minutes=120

script s169_offer
  sfx SFX_SPLASH
  say nar "(A small grey cloud, in a / jar on a cushion. {.}It / drizzles politely.)"
  [R8]
  say vapula SAD "hello. {..}i was a duke."
  [R8]
  say vapula SAD "now i'm cushion-sized. {.}and i / rain when i'm sad. {..}i'm / sad a lot."
  [HOLD 8]
  say vapula NEUTRAL "i'd like a hobby."
  choice vapula "Find him a hobby?" { "Let's try some." -> yes ; "Later." }
  end
yes:
  quest S169 active
  call s169_again
end

script s169_again
  choice nar "First hobby:" { "Tarot." -> tarot ; "Cooking." -> soup ; "Napping." -> nap }
tarot:
  sfx SFX_TIK
  say nar "(The Tower.)"
  [R13]
  sfx SFX_TIK
  say nar "(The Tower.)"
  [R13]
  sfx SFX_TIK
  [R13]
  say nar "(The Tower.)"
  [R8]
  say vapula NEUTRAL "it's a rainy deck." [PUNCH]
  jmp follow
soup:
  sfx SFX_SPLASH
  say nar "(Soul soup. He rains in it. / {..}It's very salty now.)"
  jmp follow
nap:
  say nar "(He's very good at napping. / {.}Suspiciously good.)"
follow:
  [R13]
  say vapula NEUTRAL "can i follow you? {.}like your / pet does?"
  [R8]
  say nar "(@E is not okay with this.)"
  push pet
  sfx SFX_BEEP
  say nar "beep."
  [R8]
  sfx SFX_SQUEAK
  say vapula NEUTRAL "squeak."
  [R8]
  sfx SFX_BEEP
  say nar "beep!"
  [R8]
  sfx SFX_SQUEAK
  say vapula NEUTRAL "...squeak."
  [HOLD 21]
  say nar "(Twenty seconds later they are / asleep on the same cushion.)"
  [R21]
  [MUTE 26]
  say vapula SAD "i was every sigh. {..}nobody / wanted me, and i was so big." [HEART]
  [HOLD 13]
  choice nar "You:" { "You're small now." -> small }
small:
  [R8]
  say vapula SAD "...is that better?"
  [HOLD 21]
  say nar "(@E puts a paw on him.)"
  [R8]
  sfx SFX_BEEP
  say nar "beep."
  reward 50 furn=vapula_cushion
  home PLACE_GIFT furn=vapula_cushion
  befriend vapula 3
  quest S169 done
  later 5m s169_later
end

script s169_later
  sfx SFX_SPLASH
  say nar "(Sootkin's sunflower is the / best-watered plant in the / park. {..}Nobody asks why.)"
end

# =============================================================================
# S170 "Vapula's Apology, Properly" - Lord Vapula (999) + Mayor Giggs (0)
# Ch5 - deliver, talk
# =============================================================================
@pack quests kind=side
@mission S170 giver=999 side=park ch=5-5 at=45,30 verbs=deliver chain=L step=2 later=6m reward=100 offer=s170_offer active=s170_again title="An Apology, Properly" trophy="Small Weather" hint="Carry the little cloud to the Mayor at Town Hall. Gently."
@pack sb_cloud kind=side

script s170_offer
  say vapula NEUTRAL "i sent him flowers once. {.}it / wasn't enough."
  [R8]
  say vapula NEUTRAL "i want to say it. {..}i wrote / a speech."
  [R8]
  say nar "(The speech is one word.)"
  choice vapula "Take him to the Mayor?" { "I'll carry you." -> yes ; "Later." }
  end
yes:
  quest S170 active
  call s170_again
end

script s170_again
  say nar "(You put the jar in a basket. / {.}It sloshes a little.)"
  verb deliver mayor cell=29,8 fragile then=s170_office   # map=hill
end

script s170_office
  say nar "(The Mayor's office. Two / very small, very scared / things, face to face.)"
  [HOLD 21]
  say vapula SAD "...sorry."
  [HOLD 13]
  say mayor HAPPY "Hee. {..}No. Not that."
  [R13]
  say mayor SAD "...Me too."
  [R13]
  sfx SFX_SPLASH
  say nar "(It rains in the office. {.}Neither / of them minds.)"
  [R21]
  say 531 NEUTRAL "Invoice. {.}Water damage, / municipal, times one."
  [R8]
  say nar "(The Mayor signs it.)"
  [R8]
  sfx SFX_SPLASH
  say nar "(Vapula rains on the invoice.)"
  [R21]
  [MUTE 26]
  [HOLD 26]
  say nar "(Two people made of other / people's fear, forgiving / each other. One word each.)" [HEART]
  [NO BUTTON]
  [HOLD 26]
  reward 100 furn=framed_invoice
  home PLACE_GIFT furn=framed_invoice
  befriend vapula 3
  befriend mayor 2
  quest S170 done
  later 6m s170_later
end

script s170_later
  say nar "(The Mayor's visitor chair / has a cloud-shaped cushion / now. {..}It's a little damp.)"
end

# =============================================================================
# S171 "The Ninth Cloud" - Lord Vapula (999) - Ch5
# combat (Breach 7), dream, housing
# =============================================================================
@pack quests kind=side
@mission S171 giver=999 side=park ch=5-5 at=45,30 verbs=combat,dream,housing chain=L step=3 later=8m reward=200 offer=s171_offer active=s171_again title="The Ninth Cloud" trophy="Duke of Nothing" hint="Ride Updraft's balloon to the Ninth Cloud with Vapula, one last time."
@pack sb_cloud kind=side

string S171_D1 "mist, and unsent letters"
string S171_D2 "a throne made of 'it's fine's"
string S171_D3 "the last of the swallowed feelings"
string S171_D4 "(the balloon. the park, below.)"
string S171_C1 "The last scraps of swallowed feeling."
string S171_C2 "The Ninth Cloud. Empty. Drifting."

script s171_offer
  say vapula SAD "my old home is still up / there. {.}the ninth cloud. / {..}empty. drifting."
  [R8]
  say vapula NEUTRAL "i want to see it once more. / {.}then i want it gone."
  [HOLD 8]
  say vapula SAD "i'm too small to fly."
  choice vapula "Take him up there?" { "Updraft has a balloon." -> yes ; "Later." }
  end
yes:
  quest S171 active
  call s171_again
end

script s171_again
  say 17 HAPPY "Up we go! {!}Hold onto your / jar."
  sfx SFX_WHOOSH
  cam CAM_PAN_TO cell=30,6
  [R21]
  cam CAM_BRAKE 6
  [R13]
  cam CAM_FOLLOW
  say nar "(The balloon rises over the / park, both sides at once, / and brakes in a cloud.)"
  stash 0 S171_D1
  stash 1 S171_D2
  stash 2 S171_D3
  stash 3 S171_D4
  push dream 0x4001
  say vapula NEUTRAL "the last scraps of what / nobody felt. // they'll fight. they don't / know they're over."
  push combat MAPID_HELLVILE_UNDERBELLY captions=S171_C1,S171_C2
  jlt v0 1 retreat
  say nar "(At the centre, his tiny / cloud room. A throne made of / 'it's fine's.)"
  [R13]
  choice vapula "take one thing?" { "The throne." -> throne ; "A letter." -> letter ; "Nothing." -> none }
  end
retreat:
  say 17 NEUTRAL "Too windy! {.}We'll try again / tomorrow."
  end
throne:
  reward 200 furn=vapula_tiny_throne unlock=705
  home PLACE_GIFT furn=vapula_tiny_throne
  jmp pop
letter:
  reward 200 unlock=705
  say nar "(The letter says: 'dear / anybody.' {..}That's all.)"
  jmp pop
none:
  reward 200 unlock=705
pop:
  [R13]
  sfx SFX_POP2
  shake SHK_SLAM
  say nar "(He pops the Ninth Cloud.)"
  [HOLD 13]
  sfx SFX_SPLASH
  say nar "(It rains on the whole park / for a day.)"
  [R21]
  [MUTE 26]
  say nar "(The rain is warm. Every / citizen stands in it. {.}Nobody / runs for cover.)" [HEART]
  [HOLD 21]
  say tallyworn SAD "First rain in ten years that / wasn't somebody's."
  befriend vapula 4
  befriend 17 2
  quest S171 done
  later 8m s171_later
end

script s171_later
  sfx SFX_THUD
  say nar "(Grumbleguts sits on the tiny / throne in your house.)"
  [R8]
  say nar "VOICE FROM THE THRONE: it's / fine."
  [R5]
  sfx SFX_SPLAT
  shake SHK_TAP
  say nar "(It collapses.)"
  [R8]
  say nar "GURGLE: gurgle. {r}" [GURGLE]
  [R13]
  say 57 HAPPY "Today's forecast: mild, with / a chance of plush."
end

# =============================================================================
# S172 "The Little Black Book" - Grumbleguts (729) - Ch4-5
# combat (the Underbelly), deliver, talk
# =============================================================================
@pack quests kind=side
@mission S172 giver=729 side=hell ch=4-5 after=BEAT_EP4_6 at=38,27 verbs=combat,deliver chain=U step=1 later=6m reward=60 offer=s172_offer active=s172_again title="The Little Black Book" hint="Bring Grumbleguts's little black book back from the Underbelly. Don't read it."
@pack sb_cloud kind=side

string S172_C1 "A demon is using his book as a doorstop."
string S172_C2 "Don't read it. Everyone has read it."

script s172_offer
  say grumble LOVE "Hey, hot stuff. {.}Do me a / favour, and I'll owe you / one. I'm good for it."
  [R8]
  say grumble NEUTRAL "I mislaid my little black / book. In the Underbelly. // {..}It's the size of a phone / directory."
  [HOLD 8]
  say grumble NEUTRAL "It's got every rejection I / ever got. {.}Annotated: / 'playing hard to get'."
  [R13]
  say grumble HAPPY "Don't read it."
  choice grumble "Fetch the book?" { "I won't read it." -> yes ; "Later." }
  end
yes:
  quest S172 active
  call s172_again
end

script s172_again
  push combat MAPID_HELLVILE_UNDERBELLY captions=S172_C1,S172_C2
  jlt v0 1 retreat
  sfx SFX_THUD
  say nar "(The book. {.}It is not little. / It is propping open a door / that weighs a ton.)"
  [R8]
  say nar "(You can just about lift it.)"
  verb deliver grumble cell=38,27 then=s172_back
  end
retreat:
  say grumble NEUTRAL "Underbelly's rough this time / of year. {.}Take your time, / darlin'."
end

script s172_back
  sfx SFX_SHWIP
  say nar "(Three pages fall out on the / way. You read them. {..}Everyone / does.)"
  [R13]
  say nar "'FANGELA: sniffed my ID and / said no. 3/10.'"
  [R13]
  say nar "'LADY SMOG: declined with a / curtsey. 6/10. Classy.'"
  [R13]
  say nar "'THE GATE: creaked. 9/10.'"
  [R8]
  say grumble LOVE "It creaked WARMLY. {r}" [PUNCH]
  [R8]
  say nar "(He turns to admire himself in / a lava puddle. {.}On the beat.)"
  [R13]
  say grumble HAPPY "Filed under 'progress'."
  [R21]
  [MUTE 21]
  say nar "(The last page is blank. At / the top, in big letters: / FRIENDS.)" [HEART]
  [HOLD 13]
  say nar "GURGLE: gurgle." [GURGLE]
  [R8]
  say grumble SAD "{d}...He says you can sign it / if you want."
  choice nar "The FRIENDS page:" { "Sign it." -> sign ; "Sign it with a heart." -> heart }
heart:
  say nar "(You add a heart. {..}He looks / at it for a long time.)"
  jmp done
sign:
  say nar "(You sign it. {.}He blows on the / ink so it dries.)"
done:
  [R13]
  say grumble HAPPY "Open late, darlin'."
  reward 60 furn=little_black_book
  home PLACE_GIFT furn=little_black_book
  befriend grumble 4
  quest S172 done
  later 6m s172_later
end

script s172_later
  sfx SFX_CREAK
  say nar "(The replica book opens by / itself. {.}A page, in his / writing: 'YOU? (no)'.)"
  [R13]
  say nar "(Under it, in different, / gurglier writing: '(yes)'.)"
end

# =============================================================================
# S173 "Profile Picture" - Grumbleguts (729) + Shutterbug (46) - Ch5
# photo, talk
# =============================================================================
@pack quests kind=side
@mission S173 giver=729 side=hell ch=5-5 at=38,27 verbs=photo chain=U step=2 later=6m reward=80 offer=s173_offer active=s173_again title="Profile Picture" hint="Shutterbug will take ONE photo of Grumbleguts for his dating profile."
@pack sb_cloud kind=side

script s173_offer
  say grumble LOVE "Since the Wall fell, the / dating ticker's gone WILD, / sugar."
  [R8]
  say grumble HAPPY "I want a profile. {.}Shutterbug / agreed to one photo. {..}One."
  choice grumble "Take his photo?" { "Work it." -> yes ; "Later." }
  end
yes:
  quest S173 active
  call s173_again
end

script s173_again
  say 46 NEUTRAL "One photo. {.}I mean it."
  sfx SFX_TIK
  say nar "(Pose one: the flex. Belly / first.)"
  [R13]
  sfx SFX_TIK
  say nar "(Pose two: the lean. Rear / first. {r})"
  [R13]
  sfx SFX_TIK
  [R13]
  say nar "(Pose three: the 'candid'. He / is eating.)"
  [R13]
  say 46 HAPPY "Say cheese."
  [R8]
  say grumble SHOCK "Is there cheese?" [PUNCH]
  [R8]
  say nar "(There is not cheese.)"
  verb photo grumble cell=38,26 then=s173_photo
end

script s173_photo
  sfx SFX_SPARKLE
  say nar "(The photo captures his / disappointment. {.}It's his / best angle.)"
  [R8]
  say nar "(Somehow it's also all three / poses.)"
  [R13]
  say grumble NEUTRAL "Now the bio. {.}Short. Mysterious. / Me."
  choice grumble "His bio:" { "Gatekeeper. Open late." -> late ; "Big. Loud. Yours." -> big ; "gate." -> gate }
late:
  say grumble LOVE "Ooh. {.}That's going straight on / the profile."
  jmp replies
big:
  say grumble LOVE "Darlin', you get me."
  jmp replies
gate:
  say grumble NEUTRAL "...gate. {..}Yeah. That's me."
replies:
  [R21]
  sfx SFX_BEEP
  say nar "(First reply: Honkers.)"
  [R8]
  sfx SFX_KAZOO
  say nar "Honk."
  [R13]
  sfx SFX_BEEP
  say nar "(Second reply: Gatekeep, the / Hillvile robot. 'ERROR 404: / NOT INTERESTED.')"
  [R13]
  sfx SFX_BEEP
  say nar "(Third reply: the Gate's / maintenance notice.)"
  [R8]
  say grumble HAPPY "That counts."
  [R21]
  [MUTE 21]
  say nar "(Later, alone, he rewrites / the bio.)" [HEART]
  [HOLD 13]
  say nar "'Big. Loud. Kept a gate ten / years so nobody would get / hurt.'"
  [HOLD 13]
  say nar "'Likes: everyone. {..}Looking / for: one.'"
  reward 80 furn=profile_photo
  home PLACE_GIFT furn=profile_photo
  befriend grumble 3
  befriend 46 1
  quest S173 done
  later 6m s173_later
end

script s173_later
  say nar "(Mother Nox's potluck seats / him next to Scaldie.)"
  [R8]
  say 513 NEUTRAL "...Lukewarm."
  [R8]
  say grumble LOVE "It's a start."
end

# =============================================================================
# S174 "The Exes" - Grumbleguts (729) + Oddsball (522) - Ch5
# combat (the Arena), cooking, talk (absorbs heart S95)
# =============================================================================
@pack quests kind=side
@mission S174 giver=522 side=hell ch=5-5 at=17,18 verbs=combat,cooking chain=U step=3 later=6m reward=150 offer=s174_offer active=s174_again title="The Exes" trophy="Playing Hard to Get" hint="Oddsball bets on how many exes turn up. Feed them, or fight them."
@pack sb_cloud kind=side

string S174_C1 "SHRIEKRA: He flirted with my sister."
string S174_C2 "SPEARMINT: He flirted with my DOOR."
string S174_C3 "KODIAK: It was fine, actually. I'm here for the food."

script s174_offer
  say 522 HAPPY "Taking bets! {!}A support group / for Grumbleguts's exes. At / Brimstone Bites. Tonight."
  [R8]
  say 522 NEUTRAL "How many turn up? {.}I've got / zero at ten to one."
  choice 522 "Go to the group?" { "This I've got to see." -> yes ; "Later." }
  end
yes:
  quest S174 active
  call s174_again
end

script s174_again
  jif SF_GRUMBLE_DEAD wake
  say nar "(Three turn up. All demons. / All adults. {.}All entirely / over it.)"
  [R8]
  say 522 SAD "...Three. {..}I bet zero."
  [R13]
  say 587 NEUTRAL "He told me I was cute when I / was scared."
  npc 591 RIPPLE 3
  sfx SFX_TINK
  say 591 SHOCK "He told ME that!"
  [R5]
  say 849 SHOCK "...ME too."
  [R13]
  sfx SFX_DOOR
  say nar "(Grumbleguts walks in. Rear / first. {r} With a tray.)"
  [R8]
  say nar "(Everyone gets up to leave.)"
  [R13]
  sfx SFX_CREAK
  say nar "(Except the Gate. On a trolley. / Honkers wheeled it in.)"
  [R8]
  say grumble SHOCK "You came."
  [R5]
  sfx SFX_CREAK
  say nar "(The Gate creaks. {r})"
  [R21]
  say grumble SAD "Stay. {.}Please. I'll cook."
  choice nar "The exes are leaving:" { "Cook for them." -> cook ; "Fight them for it." -> fight }
cook:
  push cooking 0x500
  push cooking 0x1100
  native CB_SIDE_GRUMBLE GRB_PACIFIST
  jmp sit
fight:
  push combat MAPID_ARENA captions=S174_C1,S174_C2,S174_C3
sit:
  native CB_SIDE_GRUMBLE GRB_EXES 4
  [R13]
  say nar "(Everyone sits down to eat. / {.}He does not flirt once. / Everyone notices.)"
  [R8]
  say nar "GURGLE: gurgle." [GURGLE]
  [R8]
  say 522 NEUTRAL "Translation: 'he's trying'."
  [R21]
  [MUTE 21]
  say 587 NEUTRAL "He's a lot." [HEART]
  [HOLD 13]
  say 849 NEUTRAL "{..}He never once made anyone / feel small."
  [HOLD 13]
  say nar "(He sucks in his gut.)"
  [HOLD 13]
  sfx SFX_BOING
  say nar "(It springs out. {r})"
  [R8]
  say nar "(He laughs at himself. {.}The / first time.)"
  reward 150 furn=gate_on_a_trolley unlock=707
  home PLACE_GIFT furn=gate_on_a_trolley
  befriend grumble 3
  befriend 522 2
  quest S174 done
  later 6m s174_later
  end
wake:
  say nar "(Three turn up anyway. All / demons. All adults. {.}They / brought food.)"
  [R8]
  say 522 SAD "...Three. {..}I bet zero."
  [R13]
  sfx SFX_CREAK
  say nar "(Honkers wheels the Gate in / on a trolley. {..}It does not / creak.)"
  [HOLD 21]
  [MUTE 21]
  say 849 NEUTRAL "He never once made anyone / feel small." [HEART]
  [HOLD 13]
  say 587 NEUTRAL "{..}He'd hate how quiet this / is."
  [R13]
  say nar "(So everyone eats loudly. / {.}For him.)"
  native CB_SIDE_GRUMBLE GRB_EXES 4
  reward 150 furn=gate_on_a_trolley unlock=707
  home PLACE_GIFT furn=gate_on_a_trolley
  befriend 522 2
  quest S174 done
  later 6m s174_later
end

script s174_later
  say nar "(The FRIENDS page has four / signatures now. {..}One of / them is a hinge.)"
end
