# ch4/ep43.dsc - Chapter 4: Ep 4.3 "Mother Nox's Three Favours" (owner: S3;
# story_final.md sec.13): Sootkin's sunflower, Lady Smog (the last exorcism,
# the two-faith rite), Anvil and Rivet. Any order. L_help_done branches on
# Smog's outcome (SF sec.13b).
@pack ep43 kind=episode chapter=4 minutes=40

string O43_NOX "Meet Mother Nox at her chapel (NW)"
string O43_FAVOURS "Help Sootkin, Lady Smog and Anvil (@0/3)"
string O43_FLOWERS "Pick Hillvile flowers for Sootkin"
string O43_BACK "Tell Mother Nox (chapel)"

script s43_nox
  sfx SFX_DOOR
  say nar "(Mother Nox's chapel kitchen. A pot of bone soup. Two bowls set on the table.)"
  sfx SFX_TINK
  say nar "(She sets a third without / looking.)"
  [R5]
  say nox NEUTRAL "So you are the one who / popped Vapula like a / balloon."
  [R5]
  say nox NEUTRAL "Hillvile frosting on your / sleeve. {..}Giggs still bakes / those?"
  npc nox STILL
  [MUTE 21]
  [HOLD 21]
  say nar "(She turns back to the soup. / That is the whole answer.)"
  npc nox THAW
  [R8]
  say nox NEUTRAL "Three of mine need help / before Hellvile trusts / Hillvile."
  [R5]
  say nox NEUTRAL "Sootkin by the gate, who / keeps a secret. Lady Smog, / still full of old smoke."
  [R5]
  say nox SAD "And Anvil at the forge, who sets a plate every night for a daughter who ran."
  [R13]
  say nar "(Wisp drifts over to the / second bowl.)"
  [R5]
  say nox NEUTRAL "{d}That one's not yours."
end

script s43_nox_idle
  say nox NEUTRAL "Sootkin, Lady Smog, Anvil. / The soup will keep. / It always keeps."
end

# ---- 1. Sootkin's sunflower ----
script s43_sootkin_ask
  say sootkin SHOCK "Oh! A Hillvile person! / Please don't tell. I grow a / sunflower under the gate."
  [R5]
  say sootkin SAD "It has never seen real sun. / Could you bring it some / Hillvile flowers?"
  [R5]
  say nar "(Pick flowers from a flower / bed in HaHaland, then bring / them to Sootkin.)"
end

script s43_sootkin
  take ITEM_FLOWERS 1
  sfx SFX_SPARKLE
  say sootkin HAPPY "Oh! They're so BRIGHT! / Look, sunny, you have / friends now!"
  [R8]
  say nar "(In the bunch, a little paper packet of seeds.//GRAMPY'S, it says, in shaky letters.)"
  [R13]
  say sootkin SHOCK "...Those are MY seeds. / I threw them over the fence / ten years ago!"
  jif SF_GRAMPY_LOST alone
  say grampy HAPPY "The mouse! You're the / mouse! You whistled!"
  [R8]
  sfx SFX_PLUCK
  say nar "(They whistle together. / Badly. Perfectly.)" [HEART]
  [HOLD 21]
  jmp done
alone:
  [R13]
  say sootkin SAD "...Somebody kept them. All / this time. I'll plant them / for him."
done:
  setf SF_C4_SOOTKIN
  befriend sootkin 4
end

script s43_sunflower
  sfx SFX_CREAK
  say nar "(The replanted sunflower / turns to face the Hillvile / side. Then slowly back.)"
  [R13]
  say sootkin NEUTRAL "{d}It can't decide."
end

# ---- 2. Lady Smog: the two-faith rite ----
script s43_smog
  say nar "(Lady Smog coughs out puffs of black vapor.//Father Fuzz came over the gate for this.)"
  [R5]
  say fuzz SAD "My first case. Eleven years / ago. My hands won't stop / shaking."
  insert 3 3
  [MUTE 13]
  [HOLD 13]
  say nox NEUTRAL "You're the stuffed one. We / have the same rite."
  [R5]
  say nox HAPPY "Ours goes backwards."
  choice nar "Free Lady Smog?" { "Begin the two-faith rite!" -> go ; "Not yet..." -> no }
go:
  push exorcism ID_LADY_SMOG
  jlt v0 0 lost
  jlt v0 1 holds
  setf SF_C4_LADY_SMOG
  befriend smog 4
  say smog SICK "I haven't tasted air in / eleven years."
  [R8]
  say smog HAPPY "{big}It's / DELICIOUS.{/}"
  [R5]
  say fuzz SAD "Lady Smog, eleven years / ago, I-"
  [R5]
  say smog NEUTRAL "You hugged me. {.}Nobody / else did. {d}It didn't work. / It was lovely." [HEART]
  [R13]
  say fuzz SAD "The hug was never for the / demon."
  [R13]
  say fuzz NEUTRAL "It was for the one holding / the demon." [HEART]
  [HOLD 26]
  jmp jar
lost:
  setf SF_C4_LADY_SMOG
  [MUTE 26]
  say nar "(Her spirit drifts out, smog in a gown. She curtsies to Father Fuzz.)" [HEART]
  [HOLD 21]
  say nar "(He plants Mr. Huggins the / 8th at her door, as a guard / of honour.)"
jar:
  [R13]
  sfx SFX_TINK
  say nar "(The last puff floats out the window, to//Vex's jar, and goes in with the rest of him.)"
  end
holds:
  say nar "(The smoke clings on. Lady / Smog coughs: ...again... / please...)"
no:
end

# ---- 3. Anvil and Rivet ----
script s43_rivet_no
  say rivet ANGRY "I'm not going in there. / I'm busy. I'm VERY busy. / {.}I'm holding this wall up."
end

script s43_anvil
  say rivet SAD "...Fine. FINE. Come with me. / Walk in front."
  sfx SFX_WELD
  say nar "(The forge. Anvil at the anvil, back to//the door, hammering. A table set for four.)"
  [R8]
  say nar "(One plate has never been / used. It has been set every / night for ten years.)" [HEART]
  npc rivet STILL
  [MUTE 64]
  [HOLD 26]
  say rivet SAD "...Mum?"
  npc anvil FREEZE
  [HOLD 21]
  say nar "(The hammer stops mid-swing / and stays up.)"
  [R5]
  say anvil SHOCK "...RIVET?!"
  npc anvil THAW
  npc rivet THAW
  shake SHK_POW
  sfx SFX_THUD
  say nar "(They hug like two vices closing. Sparks. The welding mask falls off.)"
  [R13]
  say rivet SAD "I saw the pipes. Under the / fountain. Your mark."
  [R5]
  say anvil SAD "They took my gift and made / it a drain. I couldn't stop / them."
  [R8]
  say anvil HAPPY "{..}I made you a spoon. Ten / years ago. For when you / came back."
  [R13]
  say rivet LOVE "{d}...It's a REALLY good / spoon."
  [R21]
  say nar "(Dinner. Everybody passes / the dishes. Nobody passes / the salt. Nobody needs to.)"
  [R5]
  say ID_SPROCKET NEUTRAL "I didn't read your letters."
  [R8]
  say ID_SPROCKET SAD "...I read them every night."
  sfx SFX_TINK
  say nar "(Bolt drops a wrench. A candle on the table. Rivet's sign, on the wall: 0 days.)"
  [R13]
  sfx SFX_SCRATCH
  say nar "(Rivet's tail is on fire / again.)"
  setf SF_C4_RIVET_HOME
  befriend anvil 4
  befriend rivet 2
end

# ---- all three: back to Nox (SF sec.13b: Smog saved or lost) ----
script s43_done
  jif SF_SMOG_LOST lost
  say nox HAPPY "You did it. Sootkin is / singing, Lady Smog is / breathing..."
  jmp anvil
lost:
  say nox SAD "Sootkin is singing. Lady / Smog is resting, at last. / We'll set her a bowl."
anvil:
  say nox HAPPY "...and Anvil is crying / into a spoon."
  [R13]
  say nox NEUTRAL "Wisp has been tugging at my / sleeve all day. Hell Pits, / number 13. Go."
end
