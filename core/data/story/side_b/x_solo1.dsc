# side_b/x_solo1.dsc - batch-B stand-alones S176 S178 S179 S180 S181 S182
# (owner: S5; story_final.md sec.14.2): an undertaker who finds the joke
# graves are company, and the four Breach bosses' second chances - a widow
# who didn't know what to do with her hands, a cathedral that wants a choir,
# a candy king who wants bread, a foreman who was only ever asked for
# monsters. The Breach variants they earn are unlock rows (698..710).

# =============================================================================
# S176 "Rest in Pieces" - Mr. Solemn (555, Hellvile undertaker) - Ch4-5
# find, talk (the graveyard)
# =============================================================================
@pack quests kind=side
@mission S176 giver=555 side=hell ch=4-5 after=BEAT_EP4_2 at=13,37 verbs=find,housing later=6m reward=80 offer=s176_offer active=s176_again title="Rest in Pieces" hint="Mr. Solemn wants HaHaland's joke graves done properly. Ask their ghosts."
@pack sb_solo1 kind=side minutes=135

script s176_offer
  say 555 NEUTRAL "Your joke graves. {.}In the / HaHaland graveyard."
  [R13]
  say 555 NEUTRAL "They offend me. {..}Professionally."
  [HOLD 8]
  say 555 NEUTRAL "I would like them done / properly. {.}The ghosts / disagree. Speak to them."
  choice 555 "Fix the graves?" { "Properly." -> yes ; "Later." }
  end
yes:
  quest S176 active
  call s176_again
end

script s176_again
  say 555 NEUTRAL "Three of them. {.}By the fence."
  verb find cell=13,32 then=s176_cake   # map=hill
end

script s176_cake
  sfx SFX_SPARKLE
  say nar "(The ghost of a cake. {.}It / would like a candle.)"
  choice nar "Its headstone:" { "Add a candle." -> c1 ; "Add two candles." -> c1 }
c1:
  say nar "('Here lies the Great Cake / of '89. It was delicious.' / {.}Now with a candle.)"
  verb find cell=13,35 then=s176_tache
end

script s176_tache
  sfx SFX_SPARKLE
  say nar "(The ghost of a moustache. / {.}Just a moustache. It is very / expressive.)"
  [R8]
  say nar "(It wants serif.)"
  choice nar "The lettering:" { "Serif, obviously." -> c2 ; "A bigger moustache." -> c2 }
c2:
  [R8]
  say nar "(It twirls. {..}Satisfied.)"
  verb find cell=3,38 then=s176_huggins
end

script s176_huggins
  sfx SFX_SPARKLE
  say nar "('Rest in pieces, Mr. / Huggins the 1st.' {.}A nicer / font, please.)"
  [R13]
  say nar "(Three proper headstones. / Every joke still on them.)"
  verb deliver 555 cell=13,37 then=s176_read
end

script s176_read
  say 555 NEUTRAL "Here lies the Great Cake of / '89."
  [R8]
  say 555 NEUTRAL "{..}It was delicious."
  [HOLD 26]
  say nar "(His mouth moves one pixel.)" [PUNCH]
  [R21]
  [MUTE 21]
  say 555 SAD "Two hundred years. {.}Every / funeral, a clown honked." [HEART]
  [HOLD 13]
  say 555 SAD "{..}I used to think that was / disrespect."
  [R13]
  say 555 NEUTRAL "It was company."
  [R13]
  say 555 NEUTRAL "One more. {.}Engrave: 'Mr. / Huggins the 1st. Came back.'"
  reward 80 furn=tiny_headstone
  home PLACE_GIFT furn=tiny_headstone
  befriend 555 3
  quest S176 done
  later 6m s176_later
end

script s176_later
  sfx SFX_KAZOO
  say nar "(Honkers is invited back to / a funeral. He honks. {.}Mr. / Solemn nods.)"
  [R8]
  say nar "(Fluffy's stone is the one / grave he leaves exactly as / it is.)"
end

# =============================================================================
# S178 "The Weeping Widow's Plushie" - Pip (42) for the Weeping Widow
# (Breach 5's boss) - Ch3-4 after the graveyard nest - combat, find, deliver
# =============================================================================
@pack quests kind=side
@mission S178 giver=42 side=hill ch=3-4 after=BEAT_EP3_3 at=10,47 verbs=combat,find,deliver later=6m reward=120 offer=s178_offer active=s178_again title="The Weeping Widow's Plushie" hint="The Weeping Widow lost a plushie in 1887. Pip wants to find it. Breach 5."
@pack sb_solo1 kind=side

string S178_C1 "Don't hurt her. Dodge the tears."
string S178_C2 "She's been crying since 1887."
string S178_C3 "The Factory archives: drawer 1887."
string S178_C4 "A bunny, filed and forgotten."

script s178_offer
  say pip SAD "The lady in the graveyard / Breach. {.}She cries. All the / time."
  [R8]
  say pip NEUTRAL "She lost a plushie. {..}I know / about lost plushies."
  [HOLD 8]
  say pip HAPPY "Let's find it!"
  choice pip "Find the Widow's plushie?" { "Let's go, Pip." -> yes ; "Not yet." }
  end
yes:
  quest S178 active
  call s178_again
end

script s178_again
  say pip NEUTRAL "Get to her last room. {.}Don't / hurt her. Just listen."
  push combat MAPID_GRAVEYARD_SHIFT captions=S178_C1,S178_C2
  jlt v0 1 retreat
  say nar "(The last room. The Widow, / grey and see-through, / weeping into nothing.)"
  [R8]
  say nar "THE WIDOW: My bunny. {.}1887. / {..}They took him for the / Factory."
  [R13]
  say nar "(The Plushie Factory files / everything.)"
  push combat MAPID_PLUSHIE_FACTORY captions=S178_C3,S178_C4
  jlt v0 1 retreat
  call s178_mend
  end
retreat:
  say pip SAD "She's still crying. {.}We'll go / back. I'll bring tissues."
end

script s178_mend
  say nar "(Drawer 1887. A bunny, / dusty, one ear hanging by a / thread.)"
  [R8]
  say nar "(It is exactly like Mr. Flops.)"
  [R13]
  say pip NEUTRAL "..."
  [HOLD 21]
  say pip NEUTRAL "...Grand-Flops." [PUNCH]
  [R21]
  native CB_SIDE_CHAPTER
  jge v0 4 sutura
  say nana HAPPY "Give him here. {.}I sewed Mr. / Flops a bed. I can sew his / grandad an ear."
  jmp sewn
sutura:
  say 542 NEUTRAL "An 1887 ear. {.}I'll do it / tonight. Backstitch. Like / they did then."
sewn:
  [NIGHT]
  sfx SFX_SHWIP
  say nar "(By morning the ear is back / on. {.}It's a little crooked. / Pip says that's character.)"
  verb find cell=7,29 then=s178_return
end

script s178_return
  say nar "(You leave Grand-Flops on / the last step of Breach 5. / {.}The crying stops.)"
  [HOLD 21]
  say nar "(The puddles dry, one by one. / The joke ghosts applaud.)"
  [R21]
  [MUTE 26]
  say nar "THE WIDOW: I didn't know what / to do with my hands." [HEART]
  [HOLD 13]
  say nar "(Pip holds Grand-Flops out to / her.)"
  [HOLD 13]
  say pip NEUTRAL "Hold it."
  [HOLD 13]
  say nar "(She does.)"
  [NO BUTTON]
  [HOLD 26]
  reward 120 furn=grand_flops unlock=703
  home PLACE_GIFT furn=grand_flops
  befriend pip 3
  quest S178 done
  later 6m s178_later
end

script s178_later
  say nar "(Grand-Flops sits on your / shelf now. {.}Pip visits him. / Pip brings Mr. Flops.)"
end

# =============================================================================
# S179 "Bone Cathedral Acoustics" - Cantor Coo (426) + Dirgelet (766)
# Ch4-5 - combat (Breach 6), concert, studio
# =============================================================================
@pack quests kind=side
@mission S179 giver=426 side=hill ch=4-5 after=BEAT_EP4_2 at=10,28 verbs=combat,concert later=7m reward=120 offer=s179_offer active=s179_again title="Bone Cathedral Acoustics" hint="Clear Breach 6 so two choirs can sing in the Bone Cathedral."
# S179 is its own pack: Dirgelet's "Ten years I..." (SF sec.7.11).
@pack s179 kind=side minutes=35

string S179_C1 "The best acoustics in either town."
string S179_C2 "The Bishop is asleep in the pulpit."

script s179_offer
  sfx SFX_BELL
  say cantor HAPPY "The Bone Cathedral has the / best acoustics in either / town! {.}And demons."
  [R8]
  say cantor NEUTRAL "I want disco hymns in it. / {..}Dirgelet wants a dirge. / We'll both need it cleared."
  choice cantor "Clear the cathedral?" { "Sing it loud." -> yes ; "Later." }
  end
yes:
  quest S179 active
  call s179_again
end

script s179_again
  push combat MAPID_BONE_CATHEDRAL captions=S179_C1,S179_C2
  jlt v0 1 retreat
  sfx SFX_BELL
  say nar "(In the crypt, a plaque. The / Hellvile twin of the one on / the Wall.)"
  [R13]
  say nar "'...EVERYONE.' {.}Signed: G.G. / and M.N."
  [HOLD 13]
  say cantor SAD "It's the end of the Wall's / sentence. {..}They wrote it / together."
  [R21]
  say 766 SAD "A hymn both choirs can sing."
  choice nar "The hymn:" { "Major verse, minor verse." -> both ; "All in minor." -> minor ; "All in major." -> major }
minor:
  say cantor SAD "Lovely. {.}Nobody in Hillvile / can find the notes."
  jmp chorus
major:
  say 766 SAD "Lovely. {.}Nobody in Hellvile / can stop crying."
  jmp chorus
both:
  say cantor HAPPY "One of each! {!}And a chorus / in the middle we all share."
chorus:
  [R13]
  push concert -7426
  sfx SFX_CHOIR
  say nar "(Coo plays at disco speed. / Dirgelet weeps at dirge / speed. It shouldn't work.)"
  [R8]
  say nar "(It works because Riffmaw / tunes the bells by playing / too loud.)"
  [R13]
  say nar "(Seraphail sings backwards. / {.}It still sounds lovely.)"
  [R8]
  sfx SFX_CREAK
  say nar "(The Bishop wakes up in the / pulpit. {..}He joins the bass / section.)"
  native CB_SIDE_ONCE SONG_TWO_CHOIRS
  [MUTE 21]
  [HOLD 21]
  say 766 SAD "Ten years I sang into a Wall / because it was the only / thing that didn't say stop." [HEART]
  [HOLD 13]
  say 766 SAD "{..}It's a better one to sing / through."
  native CB_SIDE_MUSIC
  reward 120 furn=bone_organ_mini unlock=704
  home PLACE_GIFT furn=bone_organ_mini
  befriend cantor 3
  befriend 766 4
  quest S179 done
  later 7m s179_later
  end
retreat:
  say cantor NEUTRAL "Demons have no rhythm. {.}Try / again!"
end

script s179_later
  sfx SFX_BELL
  say nar "(The Hillvile bells ring / forwards now. {.}On the disco / beat. Forever.)"
end

# =============================================================================
# S180 "The Sweet Tooth Sewers" - Nana Puddin (1) + the Gumdrop Gutterking
# (Breach 3's boss) - Ch3-5 - combat (Breach 3), cooking
# =============================================================================
@pack quests kind=side
@mission S180 giver=1 side=hill ch=3-5 after=BEAT_EP3_3 at=25,17 verbs=combat,cooking later=6m reward=100 offer=s180_offer active=s180_again title="The Sweet Tooth Sewers" hint="Nana's sourdough starter is in the candy sewers. The Gutterking has it."
@pack sb_solo1 kind=side

string S180_C1 "The starter is bubbling in the throne room."
string S180_C2 "Don't lick the floor."

script s180_offer
  say nana SAD "My sourdough starter rolled / down a drain. {.}Into the / candy sewers."
  [R8]
  say nana NEUTRAL "Thirty years old, that / starter. {..}It has opinions."
  choice nana "Fetch the starter?" { "Into the sewers!" -> yes ; "Later." }
  end
yes:
  quest S180 active
  call s180_again
end

script s180_again
  push combat MAPID_SUGAR_RUSH_SEWERS captions=S180_C1,S180_C2
  jlt v0 1 retreat
  say nar "(The throne room. The starter, / bubbling, as big as a dog.)"
  [R8]
  say nar "(And on the throne, made of / ten years of frosting: the / Gumdrop Gutterking.)"
  [R13]
  say nar "GUTTERKING: Don't lick the / floor."
  [R5]
  sfx SFX_SPLAT
  say nar "(He licks the floor.)"
  [R8]
  say nar "GUTTERKING: Do as I say."
  [R13]
  say nar "GUTTERKING: Bake me / something that ISN'T sugar. / {.}Then it's yours."
  push cooking 0xE00
  sfx SFX_EAT
  say nar "(Pickle pizza. {.}He eats it. / He cries.)"
  [R8]
  say nar "GUTTERKING: It's so SOUR. / {..}I've never been sad about / something sour before."
  [R13]
  say nar "(He gives back the starter. And / 'the good sugar'.)"
  [R21]
  [MUTE 21]
  say nana SAD "Everything sweet we didn't / want went down here, and / grew a king." [HEART]
  [HOLD 13]
  say nar "GUTTERKING: I'm a very sad / king."
  [HOLD 13]
  say nana HAPPY "Come up. {.}I'll teach you bread."
  [R13]
  sfx SFX_THUD
  say nar "(He can't fit through the door.)"
  [R8]
  sfx SFX_WELD
  say nar "(Rivet widens the door.)"
  reward 100 furn=candy_floor_tile unlock=701
  home PLACE_GIFT furn=candy_floor_tile
  befriend nana 3
  quest S180 done
  later 6m s180_later
  end
retreat:
  say nana NEUTRAL "Come back up for a cupcake / first. {.}Sugar's good for / courage."
end

script s180_later
  say nar "(The bakery sells sourdough / now. {.}On Thursdays, a sticky / king sells it with Nana.)"
  [R8]
  say nar "(He licks every loaf. {..}They / sell anyway.)"
end

# =============================================================================
# S181 "Foreman Fluffnuts Retires" - Rivet (13) for Foreman Fluffnuts
# (Breach 4's boss) - Ch4-5 - combat (Breach 4), timing, weld
# =============================================================================
@pack quests kind=side
@mission S181 giver=13 side=hill ch=4-5 after=BEAT_EP4_3 at=53,10 verbs=combat,timing,weld later=6m reward=120 offer=s181_offer active=s181_again title="Foreman Fluffnuts Retires" hint="Shut the Plushie Factory conveyor down. Give the foreman a new job."
@pack sb_solo1 kind=side

string S181_C1 "The conveyor is still running."
string S181_C2 "There are no orders now. He doesn't know."

script s181_offer
  say rivet NEUTRAL "The Plushie Factory. {.}The / foreman's still running the / conveyor."
  [R8]
  say rivet NEUTRAL "Nobody's given an order in / weeks. {..}I want that / conveyor."
  choice rivet "Shut it down?" { "Bring the wrench." -> yes ; "Later." }
  end
yes:
  quest S181 active
  call s181_again
end

script s181_again
  push combat MAPID_PLUSHIE_FACTORY captions=S181_C1,S181_C2
  jlt v0 1 retreat
  say rivet NEUTRAL "Three valves. {.}On the beat, / like my mum's hammer."
  push timing pattern=three skin=thump
  sfx SFX_THUD
  say nar "(The conveyor stops.)"
  [HOLD 21]
  say nar "(Foreman Fluffnuts stands / beside it. {.}He doesn't know / what to do with his hands.)"
  [R13]
  say rivet NEUTRAL "A furniture-maker. {.}I'll weld, / you hold."
  push workshop
  sfx SFX_WELD
  say nar "(The conveyor starts again. / {.}Plush comes out. A plush of / you.)"
  [R8]
  sfx SFX_BEEP
  say nar "(Then one of @E. {..}With horns.)"
  [R13]
  sfx SFX_SQUEAK
  say nar "(Then a Mr. Flops.)"
  [R8]
  sfx SFX_SQUEAK
  say nar "(Then another. {.}Faster.)"
  [R5]
  sfx SFX_SQUEAK
  say nar "(Forty identical Mr. Flopses.)"
  [R13]
  sfx SFX_DOOR
  say nar "(Pip walks in.)"
  [HOLD 21]
  say pip NEUTRAL "{d}...Brothers." [PUNCH]
  [R21]
  [MUTE 21]
  say nar "FLUFFNUTS: I made monsters / because somebody asked." [HEART]
  [HOLD 13]
  say nar "FLUFFNUTS: {..}Nobody ever asked / for anything else."
  [HOLD 13]
  say rivet NEUTRAL "I'm asking. {.}Make something / soft."
  [R13]
  say nar "(He does. {.}He cries into it. / It's absorbent.)"
  reward 120 unlock=702
  befriend rivet 2
  befriend pip 1
  quest S181 done
  later 6m s181_later
  end
retreat:
  say rivet NEUTRAL "Conveyor's still going. {.}So / are we. Again."
end

script s181_later
  say nar "(The Plushie Press takes / orders now. {.}The first one: / a plush of the Gate.)"
  [R8]
  say nar "(Ordered by Grumbleguts. Paid / in full.)"
end

# =============================================================================
# S182 "The Upside-Down Map" - Scoutmaster (15) - Ch4-5 - find, photo
# =============================================================================
@pack quests kind=side
@mission S182 giver=15 side=hill ch=4-5 after=BEAT_EP4_2 at=8,10 verbs=find,photo later=6m reward=80 offer=s182_offer active=s182_again title="The Upside-Down Map" hint="Find the upside-down map on the gatepost. Photograph the mirrored pairs."
@pack sb_solo1 kind=side

script s182_offer
  say 15 HAPPY "I have every badge. {.}Even / sewing. EVEN sewing."
  [R8]
  say 15 SAD "There is no badge for 'both / sides of the park'. {..}I need / a map. There's only one."
  [HOLD 8]
  say 15 NEUTRAL "Sprinklepop's soul left hers / on the Wall. {.}Upside down."
  choice 15 "Find the map?" { "Be prepared!" -> yes ; "Later." }
  end
yes:
  quest S182 active
  call s182_again
end

script s182_again
  say 15 NEUTRAL "The old gatepost. {.}By the / gate."
  verb find cell=53,31 then=s182_map
end

script s182_map
  sfx SFX_PICKUP
  say nar "(The map, tucked in the / gatepost. Upside down. {.}Drawn / in glitter.)"
  [R13]
  say nar "(He reads it upside down to / correct it. {..}He gets lost in / his own town.)"
  [R8]
  say 15 SHOCK "I have a badge for this."
  [R8]
  say 15 SAD "...I do not have a badge for / this."
  [R13]
  say nar "(The landmarks mirror each / other across the old Wall / line. Photograph the pairs.)"
  verb photo nana cell=25,18 then=s182_bakery   # map=hill
end

script s182_bakery
  sfx SFX_SPARKLE
  say nar "(Nana's bakery. {.}Across the / line, its twin.)"
  verb photo 566 cell=20,23 then=s182_charlotte   # map=hell
end

script s182_charlotte
  sfx SFX_SPARKLE
  say nar "(Charlotte's bakery. Same / window. Same oven. {.}Black / icing.)"
  verb photo 513 cell=43,18 then=s182_sauna   # map=hell
end

script s182_sauna
  sfx SFX_SPARKLE
  say nar "(Scaldie's sauna, mirroring / Steamy's bath house. Same / tiles, upside down.)"
  [R21]
  say nox NEUTRAL "We planned it together. {.}Forty / years ago." [HEART]
  [HOLD 13]
  say nox HAPPY "{..}Hee. Hee."
  [R8]
  say nar "(She has his laugh too.)"
  [NO BUTTON]
  [HOLD 26]
  say 15 HAPPY "A badge. {.}Two halves of a / heart. One pastel. One / purple."
  [R8]
  say nar "(Both wobble. {.}One drips.)"
  reward 80 furn=both_sides_badge
  home PLACE_GIFT furn=both_sides_badge
  befriend 15 3
  befriend nox 1
  quest S182 done
  later 6m s182_later
end

script s182_later
  sfx SFX_BEEP
  say nar "(@E is wearing the Both / Sides badge. {.}Nobody gave it / to @E.)"
end
