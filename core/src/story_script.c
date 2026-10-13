/* story_script.c - the main plot of Story Mode, beat by beat (owner:
 * story). Each beat has: who stands where (script_cast), what the HUD says
 * (script_objective), what talking / walking / entering does
 * (script_npc / script_trigger / script_place), what a finished sub-scene
 * means (script_resume), and how a test reaches it (script_debug_*).
 *
 * Tone: sugar first. Chapter 1 is a pastel daydream, chapter 2 lets the
 * purple in, chapter 3 is Doom, chapter 4 is a hug.
 */
#include "story_int.h"

#define ME   DOOMO_PLAYER_ID
#define NAR  (-1)

static void beat(int b) { story_set_beat(b); }
static void flag(int f) { story_set_flag(f, 1); }
static int  has(int f) { return story_flag(f); }
static int  cur(void) { return story_beat(); }

static int alive(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status != RES_EMPTY && r->status != RES_DEAD;
}

static int possessed(int id)
{
    const DoomoResident *r = town_res(id);
    return r && r->status == RES_POSSESSED;
}

static void possess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status == RES_EMPTY || r->status == RES_DEAD) return;
    r->stat[STAT_CORRUPTION] = 100;
    r->status = RES_POSSESSED;
    r->exorcism = 0;
    portrait_invalidate(id);
}

static void unpossess(int id)
{
    DoomoResident *r = town_res(id);
    if (!r || r->status != RES_POSSESSED) return;
    r->status = RES_ALIVE;
    r->stat[STAT_CORRUPTION] = 10;
    portrait_invalidate(id);
}

static void chapter_card(int ch)
{
    if (ch >= 1 && ch <= 4 && !has(SF_TITLE_CH1 + ch - 1)) {
        flag(SF_TITLE_CH1 + ch - 1);
        story_push_cutscene(CUT_CHAPTER1 + ch - 1, CTX_FREE, 0);
    }
}

/* ======================================================================
 * CHAPTER 1 - Welcome to HaHaland
 * ==================================================================== */
static const StoryLine L_mayor_hello[] = {
    { ID_MAYOR, "Oh my stars! A new face! Welcome, welcome, WELCOME to HaHaland!" },
    { ID_MAYOR, "I'm Mayor Gigglesworth. Hee hee! Everyone just calls me Mayor Giggs." },
    { ID_MAYOR, "And you must be @P! The happiest town on Earth is SO happy to have you!" },
    { ID_MAYOR, "That's Brimstone Bites, our bakery-cafe. Don't worry about the name. It's ironic!" },
    { ID_MAYOR, "Down south is the Hillvile side of Hellvile Park. Flowers! Ducks! A fountain that giggles!" },
    { ME, "Hellvile Park?" },
    { ID_MAYOR, "Ah. Yes. The park has two sides. Ours is Hillvile, the good side. The OTHER side..." },
    { ID_MAYOR, "...is behind the Wall. We don't talk about the other side. Hee. Hee." },
    { ID_MAYOR, "ANYWAY! Your cottage is up on Hillvile Lane. I gave the key to Nana Pudding to keep safe." },
    { ID_MAYOR, "Pop into Brimstone Bites and ask her for it. Tell her Mayor Giggs sent you! Toodles!" },
    SAY_END
};

static const StoryLine L_nana_key[] = {
    { ID_NANA, "Well hello, sweet pea! You must be @P. The Mayor told me all about you." },
    { ID_NANA, "Your key? Oh dear. Oh my. I may have... baked it into a muffin." },
    { ID_NANA, "Don't fret! I found it. It only smells a teensy bit like blueberries." },
    { NAR, "You got the House Key! It is warm and slightly sticky." },
    { ID_NANA, "And one more thing, dearie. Every new neighbour gets a Doomogachi!" },
    { ID_NANA, "It's only an egg now. Keep it warm, feed it, love it. It'll hatch into something wonderful." },
    { ID_NANA, "...Probably wonderful. The last one hatched into a little demon. Ha! Kidding. Mostly." },
    { NAR, "You got a Doomogachi egg! It wiggles happily." },
    { ID_NANA, "Run along home now. Hillvile Lane, up the stairs north-west. The cottage with the pink door!" },
    SAY_END
};

static const StoryLine L_home[] = {
    { NAR, "Home sweet home! It smells like fresh paint and cinnamon rolls." },
    { NAR, "The egg wobbles in your arms. Crack... crack... CRACK!" },
    SAY_END
};

static const StoryLine L_hatched[] = {
    { NAR, "@E is here! It blinks up at you with enormous, sparkly eyes." },
    { NAR, "@E will follow you everywhere. Check on it from the START menu: feed it, clean up, play!" },
    { NAR, "Somewhere outside, a little kid is crying. Maybe go and see?" },
    SAY_END
};

static const StoryLine L_pip_lost[] = {
    { ID_PIP, "*sniff* ...h-hi. I'm Pip. I'm not crying. My eyes are just doing a rain." },
    { ID_PIP, "I lost Mr. Flops! He's my bunny plush. One ear flops and one ear doesn't." },
    { ID_PIP, "We were at the beach and a big wave said WHOOSH and then he was gone..." },
    { ME, "Don't worry, Pip. I'll find him!" },
    { ID_PIP, "R-really? You're the bravest grown-up ever! Mr. Flops loves sand. Check the beach!" },
    SAY_END
};

static const StoryLine L_bunny_found[] = {
    { NAR, "Something fluffy is poking out of the sand..." },
    { NAR, "It's Mr. Flops! He is a bit damp, but still smiling his stitched little smile." },
    SAY_END
};

static const StoryLine L_pip_thanks[] = {
    { ID_PIP, "MR. FLOPS!!! You're alive! You smell like seaweed! I love you!" },
    { ID_PIP, "Thank you, @P! You can have my best sticker. It's a skull. Skulls are cute." },
    { ID_PIP, "You should make friends with EVERYONE! HaHaland has so many people. I counted to twenty." },
    { NAR, "Talk to the citizens around town to make friends!" },
    SAY_END
};

static const StoryLine L_three_friends[] = {
    { NAR, "Three new friends! Your heart feels three sizes bigger." },
    { NAR, "Your phone buzzes. A text from Nana Pudding: \"Come by the cafe, sweet pea! xo\"" },
    SAY_END
};

static const StoryLine L_nana_cupcake[] = {
    { ID_NANA, "Three new friends already? You're a natural, sweet pea!" },
    { ID_NANA, "Could you do an old sheep a favour? Take this cupcake to Miss Lulla in the park." },
    { ID_NANA, "She runs the daycare and she loves my cupcakes more than life itself." },
    { NAR, "You got a Cupcake! It has a little sugar heart on top." },
    SAY_END
};

static const StoryLine L_lulla[] = {
    { ID_LULLA, "Is that... is that a Nana Pudding cupcake? With the little sugar heart?" },
    { ID_LULLA, "*nom* ...I'm going to cry. I'm crying. Thank you, @P." },
    { ID_LULLA, "Oh, have you seen the purple fog by the Wall? The children keep pointing at it." },
    { ID_LULLA, "It giggles at night. Fog shouldn't giggle. Could you take a peek? Carefully!" },
    SAY_END
};

static const StoryLine L_fog_after[] = {
    { NAR, "The fog curls through the cracks in the Wall like little purple fingers..." },
    { NAR, "A voice, sweet as syrup: \"Hillvile... come play with us... we have FLAVOURS...\"" },
    { ID_MAYOR, "@P! Step away from there! Nobody goes near the Wall. NOBODY." },
    { ID_MAYOR, "It's just... weather. Hellvile weather. Hee. Hee. Let's all go home and have cocoa." },
    SAY_END
};

/* ======================================================================
 * CHAPTER 2 - Something in the Vapor
 * ==================================================================== */
static const StoryLine L_ch2_open[] = {
    { NAR, "A few days pass. Then, one morning, a brand new shop opens right next to the Wall." },
    { NAR, "Its sign glows purple: CLOUD NINE VAPOR. Grand opening! Free samples!" },
    SAY_END
};

static const StoryLine L_vex_hello[] = {
    { ID_VEX, "Well, well. A fresh face! Welcome to Cloud Nine Vapor, friend. Grand opening!" },
    { ID_VEX, "I'm Vex. I'm from out of town. WAY out. Other-side-of-a-Wall out of town." },
    { ID_VEX, "Today's flavour is @V. Tastes like a hug from someone who wants something." },
    SAY_END
};

static const char *const O_vape[] = { "No thanks!", "...Just one puff." };

static const StoryLine L_vex_no[] = {
    { ID_VEX, "Suit yourself. You'll come around. Everyone in Hillvile comes around." },
    { NAR, "On your way out you notice people leaving the shop... humming. All the same note." },
    SAY_END
};

static const StoryLine L_vex_yes[] = {
    { NAR, "You take a tiny puff. The world goes soft and purple. Something giggles inside your ear." },
    { ID_VEX, "There it is. The Cloud Nine feeling. Come back soon, @P. We'll be waiting." },
    { NAR, "You feel... weird. You should probably see how everyone else is doing." },
    SAY_END
};

static const StoryLine L_scream[] = {
    { NAR, "A scream echoes from the plaza! It sounds like Steamy from the bath house!" },
    SAY_END
};

static const StoryLine L_steamy_possessed[] = {
    { ID_STEAMY, "heh... heh heh... the bath house is CLOSED, little @P. Forever and ever." },
    { ID_STEAMY, "Steamy isn't home right now. We moved in. The flavours were SO good." },
    { NAR, "Steamy's eyes glow red. His voice sounds like three people talking at once." },
    { ID_MAYOR, "Oh my stars, oh my STARS! Get Father Fuzz! The Church of St. Cuddles, west of the plaza!" },
    SAY_END
};

static const StoryLine L_fuzz_hello[] = {
    { ID_FUZZ, "Bless you, child. I felt it too. A darkness has moved into our Steamy." },
    { ID_FUZZ, "I am Father Fuzz. Stuffed with love and a little bit of buckwheat." },
    { ID_FUZZ, "An exorcism is a gentle art: a prayer in rhythm, holy water, and the demon's true name." },
    { ID_FUZZ, "Let's practise on Mr. Huggins, my plush dummy. Follow my lead!" },
    SAY_END
};

static const StoryLine L_fuzz_practised[] = {
    { ID_FUZZ, "Splendid! You have the paws of a healer." },
    { ID_FUZZ, "Now, the real thing. I'll meet you by poor Steamy in the plaza. Hurry!" },
    SAY_END
};

static const char *const O_exorcise[] = { "Begin the exorcism!", "Not yet..." };

static const StoryLine L_steamy_saved[] = {
    { ID_STEAMY, "W-what happened? Why do I taste grape? I don't even LIKE grape..." },
    { ID_FUZZ, "The demon is gone. As it fled it shrieked its true name: @D." },
    { ID_FUZZ, "@D... that is a citizen of Hellvile. A real person, from the other side of the Wall." },
    { ID_FUZZ, "Someone over there is sending their worst through that vapor. Stay watchful, @P." },
    SAY_END
};

static const StoryLine L_steamy_lost[] = {
    { ID_FUZZ, "...I'm so sorry, child. Steamy's soul is lost to the vapor." },
    { ID_FUZZ, "When you are ready, we can lay him to rest. Only then. Only after we tried everything." },
    { ID_FUZZ, "Remember the name the demon screamed: @D. A citizen of Hellvile." },
    SAY_END
};

static const StoryLine L_rivet_hello[] = {
    { ID_RIVET, "So YOU'RE the one poking demons with holy water. Nice. I'm Rivet." },
    { ID_RIVET, "Yeah, yeah, I'm an imp. Ran away from Hellvile years ago. Don't make it weird." },
    { ID_RIVET, "Listen. I know that vapor. Lord Vapula has been cooking something big over there." },
    { ID_RIVET, "Holy water won't cut it if things go sideways. You need something with a trigger." },
    SAY_END
};

static const char *const O_weapon[] = { "Cute! (Bubble Wand)", "Brutal! (Pump Shotgun)" };

static const StoryLine L_rivet_cute[] = {
    { ID_RIVET, "A bubble wand?! HA! I love it. Demons HATE being adorable." },
    { ID_RIVET, "Here's your armory. Weld new stuff whenever you find scrap. Mind the sparks!" },
    SAY_END
};

static const StoryLine L_rivet_brutal[] = {
    { ID_RIVET, "Ohhh yes. A boomstick. Rivet approves. Hellsteel stock, pink grip. Classy." },
    { ID_RIVET, "Here's your armory. Weld new stuff whenever you find scrap. Mind the sparks!" },
    SAY_END
};

static const StoryLine L_rivet_vex[] = {
    { ID_RIVET, "Oh, and hey. Your Vex guy? He just slipped into his cellar with a big glowing crate." },
    { ID_RIVET, "Imps know a portal crate when they see one. Go check it out. Bring the boom." },
    SAY_END
};

static const char *const O_descend[] = { "Go down! (Hell Breach)", "Not yet" };

static const StoryLine L_cellar_won[] = {
    { NAR, "You smash the portal crate. The vape wraith shrieks and pops like a soap bubble." },
    { NAR, "Vex is nowhere to be seen. Only a note, in curly purple handwriting:" },
    { NAR, "\"Too late, Hillvile. The big cloud is already rising. Kisses! - V\"" },
    { NAR, "The ground starts to shake..." },
    SAY_END
};

static const StoryLine L_cellar_lost[] = {
    { NAR, "You stumble back up the stairs, coughing purple. Catch your breath and try again." },
    SAY_END
};

/* ======================================================================
 * CHAPTER 3 - Hell Invades
 * ==================================================================== */
static const StoryLine L_invaded[] = {
    { ID_MAYOR, "@P! Oh thank goodness! The sky RIPPED and DEMONS came out of the CLOUD!" },
    { ID_MAYOR, "Everyone's hiding in the church with Father Fuzz! The demons built nests all over town!" },
    { ID_MAYOR, "Hattie Haha and Grampy Bean breathed the big cloud... they're not themselves anymore." },
    { ID_MAYOR, "Please. Clear the nests. Save our friends. Then... that tower of cloud by the Wall." },
    { NAR, "Walk into a demon nest to fight. Talk to the possessed to exorcise them." },
    SAY_END
};

static const StoryLine L_nest_cleared[] = {
    { NAR, "Nest cleared! The citizens hiding nearby peek out and cheer." },
    SAY_END
};

static const StoryLine L_rivet_call[] = {
    { ID_RIVET, "Psst, @P! Got scrap? Swing by the workshop and I'll beef up your gear!" },
    SAY_END
};

static const StoryLine L_rivet_upgrade[] = {
    { ID_RIVET, "Look at you, all singed and heroic. Let's make that weapon meaner. Or cuter. Or both!" },
    SAY_END
};

static const StoryLine L_all_nests[] = {
    { NAR, "The last nest falls silent. Above Cloud Nine Vapor, the cloud tower pulses angrily." },
    { ID_FUZZ, "That tower is the heart of it, @P. Lord Vapula himself waits at the top. Go with love." },
    SAY_END
};

static const StoryLine L_spire_base[] = {
    { NAR, "The shop has become a tower of churning vapor. Cloud stairs spiral up into the storm." },
    { NAR, "Somewhere far above, a voice like a thunderhead laughs: \"COME UP, LITTLE SNACK.\"" },
    SAY_END
};

static const StoryLine L_spire_won[] = {
    { NAR, "Lord Vapula, Duke of the Ninth Cloud, lets out one last enormous wheeze..." },
    SAY_END
};

static const StoryLine L_possessed_c3[] = {
    { NAR, "@T stares through you. Their eyes glow red, and the voice is not theirs." },
    { NAR, "\"The cloud is SO comfy. Why don't you come up and breathe with us?\"" },
    SAY_END
};

static const StoryLine L_c3_saved[] = {
    { NAR, "@T gasps and blinks. \"Where... where am I? Why is the sky RED?\"" },
    { NAR, "The demon @D fled back toward the Wall. @T hurries off to the church." },
    SAY_END
};

static const StoryLine L_c3_lost[] = {
    { NAR, "The light leaves @T's eyes. The demon took everything with it." },
    { ID_FUZZ, "We tried, @P. We tried everything. Now we can let them rest." },
    SAY_END
};

/* ======================================================================
 * CHAPTER 4 - Hellvile Park
 * ==================================================================== */
static const StoryLine L_c4_open[] = {
    { ID_MAYOR, "You did it! The cloud is GONE! The sky is pink again! I'm going to cry! I'm crying!" },
    { ID_FUZZ, "Vapula is defeated... but the gate in the Wall has cracked open, @P." },
    { ID_FUZZ, "Every demon name we heard belonged to a citizen of Hellvile. Were they ever truly our enemies?" },
    { ID_RIVET, "They weren't. Most of them are tired folks Vapula worked half to death. My mum's over there." },
    { ID_RIVET, "Go on, hero. Walk through the gate in the Wall. Meet the neighbours." },
    SAY_END
};

static const StoryLine L_tally[] = {
    { ID_TALLYWORN, "...You came through the gate. You actually came through the gate." },
    { ID_TALLYWORN, "I've been counting. 3,652 days pumping vapor at the gate for Lord Vapula." },
    { ID_TALLYWORN, "He's gone? Really gone? Then... we're free? We were never allowed to cross." },
    { ID_TALLYWORN, "Please, talk to people. They're scared. They think Hillvile folks are monsters." },
    SAY_END
};

static const StoryLine L_word_spreads[] = {
    { NAR, "Word spreads through Hellvile: a hero from Hillvile is in town, and they're... NICE?" },
    { NAR, "Someone says Mother Nox wants to see you at her chapel, north of the gate plaza." },
    SAY_END
};

static const StoryLine L_nox_hello[] = {
    { ID_MOTHER_NOX, "So you are the one who popped Vapula like a balloon. I am Mother Nox." },
    { ID_MOTHER_NOX, "I host the potluck. And I suppose I lead this sorry, lovely little town." },
    { ID_MOTHER_NOX, "Vapula held us all hostage. The vapor pumps, the gate, the possessions... his orders." },
    { ID_MOTHER_NOX, "Three of mine need help before Hellvile can trust Hillvile again." },
    { ID_MOTHER_NOX, "Sootkin by the gate, who keeps a secret. Lady Smog, still choking on Vapula's smoke." },
    { ID_MOTHER_NOX, "And Anvil at the forge, who misses her daughter so much she forgets to eat." },
    { ID_MOTHER_NOX, "Help them, and I will meet your Mayor at the Wall." },
    SAY_END
};

static const StoryLine L_sootkin_ask[] = {
    { ID_SOOTKIN, "Oh! A Hillvile person! Please don't tell anyone, but... I grow a sunflower under the gate." },
    { ID_SOOTKIN, "It has never seen real sunshine. Could you bring it some Hillvile flowers for company?" },
    { NAR, "Pick some at a flower bed in HaHaland, then bring them to Sootkin." },
    SAY_END
};

static const StoryLine L_sootkin_thanks[] = {
    { ID_SOOTKIN, "Oh! Oh, they're so BRIGHT! Look, sunny, you have friends now!" },
    { ID_SOOTKIN, "Thank you, @P. I'm going to plant them all around the gate. Right where the smoke was." },
    SAY_END
};

static const StoryLine L_smog[] = {
    { NAR, "Lady Smog coughs out puffs of black vapor. Something is still inside her, holding on." },
    { NAR, "\"The Duke... is... ETERNAL... *cough* ...help...\"" },
    SAY_END
};

static const StoryLine L_smog_saved[] = {
    { ID_LADY_SMOG, "*gasp* Air! Real air! I haven't tasted air in eleven years. It's DELICIOUS." },
    { ID_LADY_SMOG, "I served Vapula because I was afraid. I'm not afraid anymore. Thank you, darling." },
    SAY_END
};

static const StoryLine L_smog_lost[] = {
    { NAR, "The smoke takes Lady Smog with it. Mother Nox bows her head." },
    SAY_END
};

static const StoryLine L_anvil[] = {
    { ID_ANVIL, "My Rivet ran off to Hillvile ten years ago. Couldn't stand the smoke. Couldn't stand ME, maybe." },
    { ID_RIVET, "...Mum?" },
    { ID_ANVIL, "...RIVET?!" },
    { NAR, "They hug like two vices closing. Sparks fly. Actual sparks. They are both welders." },
    { ID_RIVET, "I made you something, Mum. It's a spoon. It's a REALLY good spoon." },
    { ID_ANVIL, "It's the best spoon I have ever seen. Thank you, @P. Thank you for bringing her home." },
    SAY_END
};

static const StoryLine L_help_done[] = {
    { ID_MOTHER_NOX, "You did it. Sootkin is singing, Lady Smog is breathing, and Anvil is crying into a spoon." },
    { ID_MOTHER_NOX, "Fetch your Mayor, hero. I will wait at the gate plaza." },
    SAY_END
};

static const StoryLine L_mayor_cross[] = {
    { ID_MAYOR, "M-meet Hellvile's leader? On the OTHER side? Me?" },
    { ID_MAYOR, "...Oh my stars. You walked through demons for us. I can walk through a gate." },
    { ID_MAYOR, "Lead the way, @P! I'll bring cupcakes. Everyone likes cupcakes!" },
    SAY_END
};

static const StoryLine L_summit[] = {
    { ID_MOTHER_NOX, "Mayor Gigglesworth." },
    { ID_MAYOR, "Mother Nox. I... I brought cupcakes. Nana Pudding made them. They're cupcakes." },
    { ID_MOTHER_NOX, "...I brought bone soup." },
    { ID_MAYOR, "...I LOVE soup." },
    { ID_MOTHER_NOX, "Then I believe we have nothing left to build a Wall for." },
    { NAR, "Together they press their hands to the old Wall. It creaks. It groans. It..." },
    SAY_END
};

static const StoryLine L_festival[] = {
    { NAR, "The Wall is rubble. The river sparkles. And for the first time ever, the whole park is ONE park." },
    { NAR, "Hillvile and Hellvile pour in from both sides. Somebody started a festival. Everybody came." },
    SAY_END
};

static const StoryLine L_stage_call[] = {
    { NAR, "Fireworks crackle over the Unity Stage. Everyone is gathering there!" },
    SAY_END
};

static const StoryLine L_stage[] = {
    { ID_MAYOR, "Citizens of Hillvile! Citizens of Hellvile! Citizens of... BOTH!" },
    { ID_MOTHER_NOX, "Tonight there is no Wall. There is only the park. OUR park." },
    { ID_PIP, "And it's all thanks to @P! And @E! And Mr. Flops!" },
    { ID_FUZZ, "Bless this park. Bless every single one of you. Even you, Vex." },
    { ID_VEX, "...Thanks, Father. I'm, uh, selling plain water vapes now. They're terrible." },
    { ID_RIVET, "Enough speeches! Hit it!" },
    SAY_END
};

static const StoryLine L_done[] = {
    { NAR, "THE END... of the beginning! Free play is unlocked: the united park is yours." },
    { NAR, "Keep helping folks, hunting secrets, and making friends on both sides of the river." },
    SAY_END
};

/* ======================================================================
 * beat transitions
 * ==================================================================== */
static void to_mayor(void) { beat(BEAT_C1_MAYOR); }
static void to_key(void) { beat(BEAT_C1_KEY); }
static void to_pip(void) { beat(BEAT_C1_PIP); }
static void to_bunny(void) { beat(BEAT_C1_BUNNY); }
static void to_bunny_return(void) { beat(BEAT_C1_BUNNY_RETURN); }
static void to_wall(void) { beat(BEAT_C1_WALL); }

static void after_nana_key(void)
{
    if (!inv_count(ITEM_HOUSE_KEY)) inv_add(ITEM_HOUSE_KEY, 1);
    if (!inv_count(ITEM_DOOMOGACHI_EGG) && !pet_exists()) inv_add(ITEM_DOOMOGACHI_EGG, 1);
    flag(SF_KEY_GIVEN);
    flag(SF_EGG_GIVEN);
    story_befriend(ID_NANA, 3);
    beat(BEAT_C1_HOME);
}

static void home_hatch(void)
{
    story_push_scene(&SCENE_PET, 0, PEND_PET, CTX_BEAT, BEAT_C1_HOME);
}

static void to_friends(void)
{
    inv_take(ITEM_LOST_BUNNY, 1);
    story_reward(30, ITEM_SKULL_TRINKET, 1, USTORY_NONE);
    story_befriend(ID_PIP, 4);
    beat(BEAT_C1_FRIENDS);
}

static void cupcake_given(void)
{
    flag(SF_CUPCAKE_GIVEN);
    if (!inv_count(ITEM_CUPCAKE)) inv_add(ITEM_CUPCAKE, 1);
}

static void lulla_done(void)
{
    inv_take(ITEM_CUPCAKE, 1);
    story_befriend(ID_LULLA, 4);
    story_reward(40, ITEM_NONE, 0, USTORY_NONE);
    to_wall();
}

static void to_chapter2(void) { beat(BEAT_C2_VAPOR); }

static void vex_choice(int c)
{
    story_befriend(ID_VEX, 1);
    if (c == 1) {
        DoomoResident *p = town_res(ME);
        flag(SF_VAPE_SAMPLE);
        if (p) p->stat[STAT_CORRUPTION] = (uint8_t)DOOMO_MIN(100, p->stat[STAT_CORRUPTION] + 15);
        inv_add(ITEM_VAPE_PEN, 1);
        story_say_lines(L_vex_yes, NULL);
    } else {
        story_say_lines(L_vex_no, NULL);
    }
    beat(BEAT_C2_WEIRD);
}

static void to_fuzz(void) { beat(BEAT_C2_FUZZ); }

static void practice(void)
{
    story_push_exorcism(-1, CTX_BEAT, BEAT_C2_FUZZ);
}

static void to_exorcise(void) { beat(BEAT_C2_EXORCISE); }

static void exorcise_choice(int c)
{
    if (c == 0) story_push_exorcism(ID_STEAMY, CTX_BEAT, BEAT_C2_EXORCISE);
}

static void to_rivet(void) { beat(BEAT_C2_RIVET); }

static void give_weapon(int brutal)
{
    int i, slot = -1;
    DoomoWeapon w;
    for (i = 0; i < DOOMO_MAX_WEAPONS; i++)
        if (!(doomo_town.weapons[i].flags & WF_VALID)) { slot = i; break; }
    if (slot < 0) slot = DOOMO_MAX_WEAPONS - 1;
    memset(&w, 0, sizeof(w));
    weld_default(&w, brutal ? WFRAME_PUMP_SHOTGUN : WFRAME_BUBBLE_WAND,
                 brutal ? WSTYLE_BRUTAL : WSTYLE_CUTE);
    w.quality = 72;
    w.charm = brutal ? WCHARM_SKULL : WCHARM_HEART;
    w.flags = WF_VALID | WF_FAVORITE;
    doomo_town.weapons[slot] = w;
    doomo_town.story.equipped = (uint8_t)slot;
    progress_add(PROG_WEAPONS_WELDED, 1);
    weld_gfx_invalidate();
}

static void after_workshop_c2(void) { story_say_lines(L_rivet_vex, NULL); flag(SF_VEX_GONE); beat(BEAT_C2_CELLAR); }

static void open_workshop_c2(void)
{
    story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_BEAT, BEAT_C2_RIVET);
}

static void weapon_choice(int c)
{
    flag(SF_WEAPON_CHOSEN);
    story_befriend(ID_RIVET, 4);
    if (c == 1) {
        flag(SF_WEAPON_BRUTAL);
        give_weapon(1);
        unlock_grant(USTORY_BRUTAL_START);
        story_say_lines(L_rivet_brutal, open_workshop_c2);
    } else {
        give_weapon(0);
        unlock_grant(USTORY_CUTE_START);
        story_say_lines(L_rivet_cute, open_workshop_c2);
    }
}

static void cellar_choice(int c)
{
    if (c == 0) story_push_combat(MAPID_VAPE_CELLAR, CTX_BEAT, BEAT_C2_CELLAR, -1, -1, -1);
}

static void erupt(void)
{
    story_push_cutscene(CUT_ERUPTION, CTX_BEAT, BEAT_C3_ERUPTION);
}

static void spire_choice(int c)
{
    if (c == 0) story_push_combat(MAPID_CLOUD_NINE_SPIRE, CTX_BEAT, BEAT_C3_SPIRE, -1, -1, -1);
}

static void vapula_down(void)
{
    story_push_cutscene(CUT_VAPULA_DOWN, CTX_BEAT, BEAT_C3_SPIRE);
}

static void to_prisoners(void) { beat(BEAT_C4_PRISONERS); }

static void help_check(void)
{
    if (cur() == BEAT_C4_HELP && has(SF_C4_SOOTKIN) && has(SF_C4_LADY_SMOG) && has(SF_C4_RIVET_HOME))
        story_say_lines(L_help_done, NULL), beat(BEAT_C4_MAYOR);
}

static void smog_choice(int c)
{
    if (c == 0) story_push_exorcism(ID_LADY_SMOG, CTX_C4_SMOG, 0);
}

static void summit_done(void)
{
    story_push_cutscene(CUT_WALL_FALLS, CTX_BEAT, BEAT_C4_MAYOR);
}

static void mayor_came(void)
{
    flag(SF_C4_MAYOR_CAME);
    story_befriend(ID_MAYOR, 3);
    ow_refresh_cast();
}

static void stage_done(void)
{
    story_push_scene(&SCENE_CREDITS, 0, PEND_CREDITS, CTX_BEAT, BEAT_C4_STAGE);
}

static int c3_exo_index(int id)
{
    return id == ID_HATTIE ? 0 : id == ID_GRAMPY ? 1 : -1;
}

static void c3_exo_choice(int c)
{
    int k = c3_exo_index(story_fmt_target);
    if (c == 0 && k >= 0) story_push_exorcism(story_fmt_target, CTX_C3_EXO, k);
}

static int nests_left(void)
{
    int i, n = 0;
    for (i = 0; i < STORY_NESTS; i++) n += !has(SF_NEST0 + i);
    return n;
}

static int exos_left(void)
{
    int i, n = 0;
    for (i = 0; i < STORY_C3_POSSESSED; i++) n += !has(SF_C3_EXO0 + i);
    return n;
}

static void c3_check(void)
{
    if (cur() == BEAT_C3_NESTS && nests_left() == 0 && exos_left() == 0) {
        story_say_lines(L_all_nests, NULL);
        beat(BEAT_C3_SPIRE);
    }
}

/* Called by the overworld when the player walks into a nest. */
void script_nest_encounter(int nest)
{
    int cx, cy;
    if (nest < 0 || nest >= STORY_NESTS || has(SF_NEST0 + nest)) return;
    if (!ow_nest_cell(nest, &cx, &cy)) { cx = 30; cy = 30; }
    ui_toast(STR_ICON_SKULL " DEMONS! " STR_ICON_SKULL);
    audio_sfx(SFX_DEMON_ROAR);
    story_push_combat(MAPID_HAHALAND_INVADED, CTX_NEST, nest, cx, cy + 2, nest);
}

/* ======================================================================
 * script_npc: talking
 * ==================================================================== */
int script_npc(int id)
{
    int b = cur();
    switch (b) {
    case BEAT_C1_ARRIVE:
    case BEAT_C1_MAYOR:
        if (id == ID_MAYOR) {
            story_befriend(ID_MAYOR, 3);
            if (b == BEAT_C1_ARRIVE) beat(BEAT_C1_MAYOR);
            story_say_lines(L_mayor_hello, to_key);
            return 1;
        }
        break;
    case BEAT_C1_KEY:
        if (id == ID_NANA) { story_say_lines(L_nana_key, after_nana_key); return 1; }
        break;
    case BEAT_C1_PIP:
        if (id == ID_PIP) { story_say_lines(L_pip_lost, to_bunny); return 1; }
        break;
    case BEAT_C1_BUNNY_RETURN:
        if (id == ID_PIP) { story_say_lines(L_pip_thanks, to_friends); return 1; }
        break;
    case BEAT_C1_CUPCAKE:
        if (id == ID_NANA && !has(SF_CUPCAKE_GIVEN)) {
            cupcake_given();
            story_say_lines(L_nana_cupcake, NULL);
            return 1;
        }
        if (id == ID_LULLA && has(SF_CUPCAKE_GIVEN)) {
            story_say_lines(L_lulla, lulla_done);
            return 1;
        }
        break;
    case BEAT_C2_VAPOR:
        if (id == ID_VEX) {
            story_say_lines(L_vex_hello, NULL);
            story_choice(ID_VEX, "First puff's free. Go on. Everyone in Hillvile is trying it!", O_vape, 2, vex_choice);
            return 1;
        }
        break;
    case BEAT_C2_SCREAM:
        if (id == ID_STEAMY) { story_fmt_target = ID_STEAMY; story_say_lines(L_steamy_possessed, to_fuzz); return 1; }
        break;
    case BEAT_C2_FUZZ:
        if (id == ID_FUZZ) { story_say_lines(L_fuzz_hello, practice); return 1; }
        break;
    case BEAT_C2_EXORCISE:
        if ((id == ID_STEAMY || id == ID_FUZZ) && possessed(ID_STEAMY)) {
            char q[80];
            story_fmt_target = ID_STEAMY;
            doomo_snprintf(q, sizeof(q), "Ready, @P? The odds look like %d%%. Hold the holy water like you mean it.",
                           exorcism_odds(ID_STEAMY));
            story_choice(ID_FUZZ, q, O_exorcise, 2, exorcise_choice);
            return 1;
        }
        break;
    case BEAT_C2_RIVET:
        if (id == ID_RIVET) {
            story_say_lines(L_rivet_hello, NULL);
            story_choice(ID_RIVET, "I weld. Cute or brutal, I don't judge. What's your style?", O_weapon, 2, weapon_choice);
            return 1;
        }
        break;
    case BEAT_C3_NESTS:
        if ((id == ID_HATTIE || id == ID_GRAMPY) && possessed(id)) {
            story_fmt_target = (int16_t)id;
            story_say_lines(L_possessed_c3, NULL);
            story_choice(NAR, "Perform the exorcism?", O_exorcise, 2, c3_exo_choice);
            return 1;
        }
        if (id == ID_RIVET && !has(SF_RIVET_UPGRADE)) {
            flag(SF_RIVET_UPGRADE);
            story_say_lines(L_rivet_upgrade, NULL);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    case BEAT_C4_PRISONERS:
        if (id == ID_TALLYWORN && !story_flag(SF_MET0 + 8)) {
            story_set_flag(SF_MET0 + 8, 1);
            story_befriend(ID_TALLYWORN, 2);
            story_say_lines(L_tally, NULL);
            return 1;
        }
        break;
    case BEAT_C4_HELP:
        if (id == ID_MOTHER_NOX && !story_flag(SF_MET0 + 9)) {
            story_set_flag(SF_MET0 + 9, 1);
            story_befriend(ID_MOTHER_NOX, 2);
            story_say_lines(L_nox_hello, help_check);
            return 1;
        }
        if (id == ID_SOOTKIN && !has(SF_C4_SOOTKIN)) {
            if (inv_count(ITEM_FLOWERS) > 0) {
                inv_take(ITEM_FLOWERS, 1);
                flag(SF_C4_SOOTKIN);
                story_befriend(ID_SOOTKIN, 4);
                story_say_lines(L_sootkin_thanks, help_check);
            } else {
                story_say_lines(L_sootkin_ask, NULL);
            }
            return 1;
        }
        if (id == ID_LADY_SMOG && !has(SF_C4_LADY_SMOG) && possessed(ID_LADY_SMOG)) {
            story_fmt_target = ID_LADY_SMOG;
            story_say_lines(L_smog, NULL);
            story_choice(NAR, "Free Lady Smog from the smoke?", O_exorcise, 2, smog_choice);
            return 1;
        }
        if ((id == ID_ANVIL || id == ID_RIVET) && !has(SF_C4_RIVET_HOME)) {
            flag(SF_C4_RIVET_HOME);
            story_befriend(ID_ANVIL, 4);
            story_befriend(ID_RIVET, 2);
            story_say_lines(L_anvil, help_check);
            return 1;
        }
        if (id == ID_MOTHER_NOX) { help_check(); if (cur() == BEAT_C4_MAYOR) return 1; }
        break;
    case BEAT_C4_MAYOR:
        if (id == ID_MAYOR && !has(SF_C4_MAYOR_CAME)) {
            story_say_lines(L_mayor_cross, mayor_came);
            return 1;
        }
        if ((id == ID_MAYOR || id == ID_MOTHER_NOX) && has(SF_C4_MAYOR_CAME)) {
            story_say_lines(L_summit, summit_done);
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * script_trigger: walk-on events
 * ==================================================================== */
static const char *const O_fog[] = { "Peek through the fog", "Back away" };

static void fog_choice(int c)
{
    if (c == 0) story_push_cutscene(CUT_FOG, CTX_BEAT, BEAT_C1_WALL);
}

int script_trigger(int tag)
{
    int b = cur();
    switch (tag) {
    case STORY_TAG_EVENT_BEACH:
        if (b == BEAT_C1_BUNNY && !has(SF_BUNNY_FOUND)) {
            flag(SF_BUNNY_FOUND);
            inv_add(ITEM_LOST_BUNNY, 1);
            audio_sfx(SFX_SQUEAK);
            story_say_lines(L_bunny_found, to_bunny_return);
            return 1;
        }
        break;
    case STORY_TAG_EVENT_FOG:
        if (b == BEAT_C1_WALL) {
            story_choice(NAR, "Purple fog seeps through the Wall. It smells like grape candy.", O_fog, 2, fog_choice);
            return 1;
        }
        if (b <= BEAT_C1_CUPCAKE && !has(SF_WALL_HINT)) {
            flag(SF_WALL_HINT);
            story_say(NAR, "The Wall towers over you, cold and rusty. Somewhere beyond it, something giggles.");
            return 1;
        }
        break;
    case STORY_TAG_EVENT_CELLAR:
        if (b == BEAT_C2_CELLAR) {
            story_choice(NAR, "The cellar hatch is open. Purple light pulses below. It smells of grape and sulfur.",
                         O_descend, 2, cellar_choice);
            return 1;
        }
        if (b >= BEAT_C2_VAPOR && b < BEAT_C2_CELLAR) {
            story_say(NAR, "A heavy hatch with a padlock shaped like a little skull. Locked. For now.");
            return 1;
        }
        break;
    case STORY_TAG_EVENT_SPIRE:
        if (b == BEAT_C3_SPIRE) {
            story_say_lines(L_spire_base, NULL);
            story_choice(NAR, "Climb Cloud Nine Spire?", O_descend, 2, spire_choice);
            return 1;
        }
        if (b == BEAT_C3_NESTS) {
            story_say(NAR, "The cloud tower is sealed by a wall of screaming vapor. Clear the nests first!");
            return 1;
        }
        break;
    case STORY_TAG_EVENT_STAGE:
        if (b == BEAT_C4_STAGE) { story_say_lines(L_stage, stage_done); return 1; }
        break;
    case STORY_TAG_EVENT_GATE_SEAL:
        story_say(NAR, b >= BEAT_C4_GATE ?
                  "The old hell gate is sealed with flowers now. Sootkin waters them every morning." :
                  "The sealed gate hums. Something on the other side is very, very bored.");
        return 1;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * script_place: entering buildings
 * ==================================================================== */
static const char *const O_travel_hh[] = { "Go to Hellvile", "Go to the united park", "Stay here" };
static const char *const O_travel_hv[] = { "Go to HaHaland", "Go to the united park", "Stay here" };
static const char *const O_travel_pk[] = { "Go to HaHaland", "Go to Hellvile", "Stay here" };

static int travel_ok_park(void) { return cur() >= BEAT_C4_FESTIVAL; }

static void travel_from_hh(int c)
{
    if (c == 0) {
        ow_enter_map(MAPID_HELLVILE, 3, 27);
        if (cur() == BEAT_C4_GATE) to_prisoners();
    } else if (c == 1 && travel_ok_park()) {
        ow_enter_map(MAPID_PARK_UNITED, 34, 24);
    }
}

static void travel_from_hv(int c)
{
    if (c == 0) ow_enter_map(MAPID_HAHALAND, 55, 30);
    else if (c == 1 && travel_ok_park()) ow_enter_map(MAPID_PARK_UNITED, 34, 24);
}

static void travel_from_pk(int c)
{
    if (c == 0) ow_enter_map(MAPID_HAHALAND, 55, 30);
    else if (c == 1) ow_enter_map(MAPID_HELLVILE, 3, 27);
}

int script_place(int place)
{
    int b = cur();
    int map = story_map();
    switch (place) {
    case PLACE_HOME:
        if (b == BEAT_C1_HOME) {
            inv_take(ITEM_HOUSE_KEY, 0);
            if (!has(SF_HOME_SEEN)) {
                flag(SF_HOME_SEEN);
                story_push_cutscene(CUT_HOME, CTX_BEAT, BEAT_C1_HOME);
            } else {
                story_say_lines(L_home, home_hatch);
            }
            return 1;
        }
        if (b < BEAT_C1_HOME) {
            story_say(NAR, "Your new cottage! The door is locked. Nana Pudding has the key.");
            return 1;
        }
        break;
    case PLACE_CAFE:
        if (b == BEAT_C1_KEY || (b == BEAT_C1_CUPCAKE && !has(SF_CUPCAKE_GIVEN))) return script_npc(ID_NANA);
        break;
    case PLACE_CHURCH:
        if (b == BEAT_C2_FUZZ) return script_npc(ID_FUZZ);
        break;
    case PLACE_WORKSHOP:
        if (b == BEAT_C2_RIVET) return script_npc(ID_RIVET);
        if (b == BEAT_C3_NESTS || b == BEAT_C3_SPIRE) {
            if (!has(SF_RIVET_UPGRADE)) return script_npc(ID_RIVET);
            story_push_scene(&SCENE_WORKSHOP, 0, PEND_WORKSHOP, CTX_FREE, 0);
            return 1;
        }
        break;
    case PLACE_VAPE_SHOP:
        if (b < BEAT_C2_VAPOR) {
            story_say(NAR, "A shop under construction. A sign: CLOUD NINE VAPOR - COMING SOON! Something purple flickers inside.");
            return 1;
        }
        if (b == BEAT_C2_VAPOR) return script_npc(ID_VEX);
        if (b == BEAT_C3_SPIRE) return script_trigger(STORY_TAG_EVENT_SPIRE);
        if (b > BEAT_C2_VAPOR && b < BEAT_C3_ERUPTION) {
            story_say(NAR, has(SF_VEX_GONE) ? "The shop is empty. Purple light leaks from the cellar hatch outside." :
                      "\"BACK IN 5 MINUTES - VEX\". The sign has said that for an hour.");
            return 1;
        }
        break;
    case PLACE_WALL_GATE:
        if (b < BEAT_C4_GATE) {
            story_say(NAR, b >= BEAT_C3_ERUPTION ?
                      "The gate in the Wall is jammed shut by writhing vapor. Not yet." :
                      "A huge iron gate, sealed with seven padlocks and one very stern sign: KEEP OUT. FOREVER.");
            return 1;
        }
        if (map == MAPID_HAHALAND || map == MAPID_HAHALAND_INVADED) {
            if (b == BEAT_C4_GATE) { travel_from_hh(0); return 1; }
            story_choice(NAR, "The gate in the Wall stands open.", O_travel_hh, travel_ok_park() ? 3 : 2, travel_from_hh);
            if (!travel_ok_park()) { /* option 2 = stay */ }
            return 1;
        }
        if (map == MAPID_HELLVILE) {
            story_choice(NAR, "Back through the gate?", O_travel_hv, 3, travel_from_hv);
            return 1;
        }
        if (map == MAPID_PARK_UNITED) {
            story_choice(NAR, "Where to?", O_travel_pk, 3, travel_from_pk);
            return 1;
        }
        break;
    case PLACE_STAGE:
        if (b == BEAT_C4_STAGE) return script_trigger(STORY_TAG_EVENT_STAGE);
        break;
    case PLACE_FORGE:
        if (b == BEAT_C4_HELP && !has(SF_C4_RIVET_HOME)) return script_npc(ID_ANVIL);
        break;
    case PLACE_CHAPEL:
        if (b == BEAT_C4_HELP) return script_npc(ID_MOTHER_NOX);
        break;
    case PLACE_TOWN_HALL:
        if (b == BEAT_C4_MAYOR && !has(SF_C4_MAYOR_CAME)) return script_npc(ID_MAYOR);
        break;
    default:
        break;
    }
    return 0;
}

/* ======================================================================
 * results of sub-scenes
 * ==================================================================== */
static void to_hatch_lines(void) { story_say_lines(L_hatched, to_pip); }

void script_resume(const StoryPending *p, int result)
{
    int b = cur();
    if (p->kind == PEND_CUTSCENE) {
        switch (p->arg) {
        case CUT_ARRIVAL:
            if (b == BEAT_C1_ARRIVE) to_mayor();
            break;
        case CUT_HOME:
            story_say_lines(L_home, home_hatch);
            break;
        case CUT_FOG:
            flag(SF_FOG_SEEN);
            story_say_lines(L_fog_after, to_chapter2);
            break;
        case CUT_ERUPTION:
            beat(BEAT_C3_ERUPTION);
            break;
        case CUT_VAPULA_DOWN:
            beat(BEAT_C4_GATE);
            break;
        case CUT_WALL_FALLS:
            ow_enter_map(MAPID_PARK_UNITED, 16, 26);
            beat(BEAT_C4_FESTIVAL);
            break;
        case CUT_FREEPLAY:
            story_say_lines(L_done, NULL);
            break;
        case CUT_CHAPTER1:
            if (b == BEAT_C1_ARRIVE) story_push_cutscene(CUT_ARRIVAL, CTX_BEAT, BEAT_C1_ARRIVE);
            break;
        case CUT_CHAPTER2:
            if (b == BEAT_C2_VAPOR) story_say_lines(L_ch2_open, NULL);
            break;
        case CUT_CHAPTER3:
            if (b == BEAT_C3_ERUPTION) { story_say_lines(L_invaded, NULL); beat(BEAT_C3_NESTS); }
            break;
        case CUT_CHAPTER4:
            if (b == BEAT_C4_GATE) story_say_lines(L_c4_open, NULL);
            break;
        default:
            break;
        }
        return;
    }
    switch (p->kind) {
    case PEND_PET:
        if (result > 0) {
            inv_take(ITEM_DOOMOGACHI_EGG, 1);
            if (b == BEAT_C1_HOME) { beat(BEAT_C1_HATCH); to_hatch_lines(); }
        } else {
            story_say(NAR, "The egg is still warm. Step inside your cottage again when you're ready to hatch it.");
        }
        break;
    case PEND_EXO_PRACTICE:
        if (b == BEAT_C2_FUZZ) {
            flag(SF_FUZZ_PRACTICE);
            story_befriend(ID_FUZZ, 3);
            story_say_lines(L_fuzz_practised, to_exorcise);
        }
        break;
    case PEND_EXORCISM:
        if (p->ctx == CTX_BEAT && b == BEAT_C2_EXORCISE) {
            story_fmt_target = ID_STEAMY;
            if (result > 0) {
                flag(SF_STEAMY_SAVED);
                story_befriend(ID_STEAMY, 4);
                story_say_lines(L_steamy_saved, to_rivet);
            } else if (result < 0) {
                flag(SF_STEAMY_LOST);
                story_say_lines(L_steamy_lost, to_rivet);
            } else {
                char buf[120];
                doomo_snprintf(buf, sizeof(buf), "The demon clings on! Rest, then try again. %d more failure%s and the soul is lost.",
                               exorcism_tries_left(ID_STEAMY), exorcism_tries_left(ID_STEAMY) == 1 ? "" : "s");
                story_say(ID_FUZZ, buf);
            }
        } else if (p->ctx == CTX_C3_EXO) {
            story_fmt_target = p->arg;
            if (result != 0) {
                story_set_flag(SF_C3_EXO0 + (p->extra & 3), 1);
                if (result > 0) story_befriend(p->arg, 3);
                story_say_lines(result > 0 ? L_c3_saved : L_c3_lost, c3_check);
            } else {
                story_say(NAR, "The demon holds on! Try again when you're ready.");
            }
        } else if (p->ctx == CTX_C4_SMOG) {
            story_fmt_target = ID_LADY_SMOG;
            if (result != 0) {
                flag(SF_C4_LADY_SMOG);
                if (result > 0) story_befriend(ID_LADY_SMOG, 4);
                story_say_lines(result > 0 ? L_smog_saved : L_smog_lost, help_check);
            } else {
                story_say(NAR, "The smoke clings on. Lady Smog coughs, \"...again... please...\"");
            }
        }
        break;
    case PEND_WORKSHOP:
        if (p->ctx == CTX_BEAT && b == BEAT_C2_RIVET) after_workshop_c2();
        break;
    case PEND_COMBAT:
        if (p->ctx == CTX_NEST) {
            if (result > 0) {
                story_set_flag(SF_NEST0 + (p->extra & 7), 1);
                story_reward(40, ITEM_SCRAP, 2, USTORY_NONE);
                story_say_lines(L_nest_cleared, c3_check);
                if (nests_left() == 2 && !has(SF_RIVET_UPGRADE)) story_say_lines(L_rivet_call, c3_check);
            } else {
                story_say(NAR, "You retreat to catch your breath. The nest is still there...");
            }
        } else if (p->extra == BEAT_C2_CELLAR && b == BEAT_C2_CELLAR) {
            if (result > 0) { story_reward(100, ITEM_SCRAP, 3, USTORY_NONE); story_say_lines(L_cellar_won, erupt); }
            else story_say_lines(L_cellar_lost, NULL);
        } else if (p->extra == BEAT_C3_SPIRE && b == BEAT_C3_SPIRE) {
            if (result > 0) { story_reward(300, ITEM_GATE_SIGIL, 1, USTORY_NONE); story_say_lines(L_spire_won, vapula_down); }
            else story_say(NAR, "Lord Vapula's laughter rolls down the cloud stairs. Gather yourself and climb again.");
        }
        break;
    case PEND_CREDITS:
        if (b == BEAT_C4_STAGE) {
            flag(SF_CREDITS_SEEN);
            beat(BEAT_DONE);
            unlock_grant(USTORY_UNITED_PARK);
            unlock_grant(USTORY_NEW_GAME_PLUS);
            unlock_grant(173);                    /* foundation: United Park title theme */
            unlock_grant(174);                    /* foundation: Cute + Gore gore mode */
            story_push_cutscene(CUT_FREEPLAY, CTX_BEAT, BEAT_DONE);
        }
        break;
    default:
        break;
    }
}

/* ======================================================================
 * entering a beat
 * ==================================================================== */
void script_enter_beat(int b)
{
    switch (b) {
    case BEAT_C1_FRIENDS:
        break;
    case BEAT_C1_WALL:
        break;
    case BEAT_C2_VAPOR:
        chapter_card(2);
        break;
    case BEAT_C2_WEIRD:
        story_set_step(0);
        break;
    case BEAT_C2_SCREAM:
        possess(ID_STEAMY);
        ow_refresh_cast();
        break;
    case BEAT_C3_ERUPTION:
        possess(ID_HATTIE);
        possess(ID_GRAMPY);
        ow_enter_map(MAPID_HAHALAND_INVADED, 52, 39);
        chapter_card(3);
        break;
    case BEAT_C3_SPIRE:
        break;
    case BEAT_C4_GATE:
        /* the town heals; anyone still possessed by the cloud is freed */
        unpossess(ID_HATTIE);
        unpossess(ID_GRAMPY);
        possess(ID_LADY_SMOG);
        ow_enter_map(MAPID_HAHALAND, 52, 39);
        chapter_card(4);
        unlock_grant(USTORY_HELLVILE_VISITS);
        break;
    case BEAT_C4_PRISONERS:
        break;
    case BEAT_C4_FESTIVAL:
        story_say_lines(L_festival, NULL);
        break;
    case BEAT_C4_STAGE:
        story_say_lines(L_stage_call, NULL);
        break;
    default:
        break;
    }
    ow_refresh_cast();
}

/* Progress counted by the overworld's small talk (FRIENDS, WEIRD,
 * PRISONERS, FESTIVAL). */
void script_ambient_talked(int id, int new_friend)
{
    int b = cur();
    if (b == BEAT_C1_FRIENDS && new_friend && !RES_DISTRICT(id)) {
        story_set_step(story_step() + 1);
        if (story_step() >= 3) { story_say_lines(L_three_friends, NULL); beat(BEAT_C1_CUPCAKE); }
    } else if (b == BEAT_C2_WEIRD) {
        story_set_step(story_step() + 1);
        if (story_step() >= 3) { story_say_lines(L_scream, NULL); beat(BEAT_C2_SCREAM); }
    } else if (b == BEAT_C4_PRISONERS) {
        if (story_hellvile_friends() >= 5) { story_say_lines(L_word_spreads, NULL); beat(BEAT_C4_HELP); }
    } else if (b == BEAT_C4_FESTIVAL) {
        story_set_step(story_step() + 1);
        if (story_step() >= 4) beat(BEAT_C4_STAGE);
    }
}

/* ======================================================================
 * objectives + targets
 * ==================================================================== */
const char *script_objective(void)
{
    static char buf[72];
    int b = cur();
    switch (b) {
    case BEAT_C1_ARRIVE:
    case BEAT_C1_MAYOR: return "Say hi to Mayor Giggs by the fountain";
    case BEAT_C1_KEY: return "Get your house key from Nana at Brimstone Bites";
    case BEAT_C1_HOME: return "Go home: the pink door on Hillvile Lane (NW)";
    case BEAT_C1_HATCH: return "Your Doomogachi is hatching!";
    case BEAT_C1_PIP: return "Someone is crying at the playground (SW)";
    case BEAT_C1_BUNNY: return "Find Pip's bunny plush on the beach";
    case BEAT_C1_BUNNY_RETURN: return "Bring Mr. Flops back to Pip";
    case BEAT_C1_FRIENDS:
        doomo_snprintf(buf, sizeof(buf), "Make friends: talk to citizens (%d/3)", story_step());
        return buf;
    case BEAT_C1_CUPCAKE:
        return has(SF_CUPCAKE_GIVEN) ? "Bring the cupcake to Miss Lulla in the park" :
               "Visit Nana Pudding at Brimstone Bites";
    case BEAT_C1_WALL: return "Investigate the purple fog by the Wall (E)";
    case BEAT_C2_VAPOR: return "A new shop by the Wall: Cloud Nine Vapor";
    case BEAT_C2_WEIRD:
        doomo_snprintf(buf, sizeof(buf), "Folks are acting weird. Ask around (%d/3)", story_step());
        return buf;
    case BEAT_C2_SCREAM: return "Someone screamed in the plaza!";
    case BEAT_C2_FUZZ: return "Get Father Fuzz at the Church of St. Cuddles";
    case BEAT_C2_EXORCISE: return "Exorcise Steamy in the plaza";
    case BEAT_C2_RIVET: return "Rivet wants to see you at the Workshop (E)";
    case BEAT_C2_CELLAR: return "Investigate the cellar of Cloud Nine Vapor";
    case BEAT_C3_ERUPTION: return "Survive!";
    case BEAT_C3_NESTS:
        doomo_snprintf(buf, sizeof(buf), "Clear demon nests %d/%d, exorcise %d/%d",
                       STORY_NESTS - nests_left(), STORY_NESTS,
                       STORY_C3_POSSESSED - exos_left(), STORY_C3_POSSESSED);
        return buf;
    case BEAT_C3_SPIRE: return "Climb Cloud Nine Spire. Defeat Lord Vapula!";
    case BEAT_C4_GATE: return "Cross into Hellvile through the Wall gate (E)";
    case BEAT_C4_PRISONERS:
        if (!story_flag(SF_MET0 + 8)) return "Talk to Tallyworn at the gate plaza";
        doomo_snprintf(buf, sizeof(buf), "Meet Hellvile's citizens (%d/5)", DOOMO_MIN(5, story_hellvile_friends()));
        return buf;
    case BEAT_C4_HELP:
        if (!story_flag(SF_MET0 + 9)) return "Meet Mother Nox at her chapel (N)";
        doomo_snprintf(buf, sizeof(buf), "Help Sootkin, Lady Smog and Anvil (%d/3)",
                       has(SF_C4_SOOTKIN) + has(SF_C4_LADY_SMOG) + has(SF_C4_RIVET_HOME));
        return buf;
    case BEAT_C4_MAYOR:
        return has(SF_C4_MAYOR_CAME) ? "Bring the Mayor and Mother Nox together" :
               "Fetch Mayor Giggs from HaHaland's Wall gate";
    case BEAT_C4_FESTIVAL:
        doomo_snprintf(buf, sizeof(buf), "The festival! Chat with folks (%d/4)", story_step());
        return buf;
    case BEAT_C4_STAGE: return "Everyone is gathering at the Unity Stage (N)";
    case BEAT_DONE: {
        const char *s = side_objective();
        return (s && s[0]) ? s : "Free play! Explore and help out";
    }
    default: return "";
    }
}

/* Where the main target is: resident id (or -1) and map/cell. */
typedef struct { int16_t id; uint8_t map, x, y; } Target;

static Target target_of(int b)
{
    Target t = { -1, MAPID_HAHALAND, 0, 0 };
    switch (b) {
    case BEAT_C1_ARRIVE: case BEAT_C1_MAYOR: t.id = ID_MAYOR; t.x = 33; t.y = 25; break;
    case BEAT_C1_KEY: t.id = ID_NANA; t.x = 25; t.y = 17; break;
    case BEAT_C1_HOME: t.x = 4; t.y = 7; break;
    case BEAT_C1_PIP: case BEAT_C1_BUNNY_RETURN: t.id = ID_PIP; t.x = 10; t.y = 47; break;
    case BEAT_C1_BUNNY: t.x = 12; t.y = 55; break;
    case BEAT_C1_CUPCAKE:
        if (has(SF_CUPCAKE_GIVEN)) { t.id = ID_LULLA; t.x = 30; t.y = 42; }
        else { t.id = ID_NANA; t.x = 25; t.y = 17; }
        break;
    case BEAT_C1_WALL: t.x = 55; t.y = 24; break;
    case BEAT_C2_VAPOR: t.id = ID_VEX; t.x = 54; t.y = 37; break;
    case BEAT_C2_SCREAM: t.id = ID_STEAMY; t.x = 38; t.y = 24; break;
    case BEAT_C2_FUZZ: t.id = ID_FUZZ; t.x = 7; t.y = 28; break;
    case BEAT_C2_EXORCISE: t.id = ID_STEAMY; t.x = 38; t.y = 24; break;
    case BEAT_C2_RIVET: t.id = ID_RIVET; t.x = 53; t.y = 10; break;
    case BEAT_C2_CELLAR: t.x = 53; t.y = 37; break;
    case BEAT_C3_NESTS: {
        int i;
        t.map = MAPID_HAHALAND_INVADED;
        for (i = 0; i < STORY_NESTS; i++)
            if (!has(SF_NEST0 + i)) { int cx, cy; if (ow_nest_cell(i, &cx, &cy)) { t.x = (uint8_t)cx; t.y = (uint8_t)cy; } return t; }
        if (!has(SF_C3_EXO0)) { t.id = ID_HATTIE; t.x = 40; t.y = 21; }
        else { t.id = ID_GRAMPY; t.x = 27; t.y = 54; }
        break; }
    case BEAT_C3_SPIRE: t.map = MAPID_HAHALAND_INVADED; t.x = 53; t.y = 37; break;
    case BEAT_C4_GATE: t.x = 55; t.y = 30; break;
    case BEAT_C4_PRISONERS:
        t.map = MAPID_HELLVILE;
        if (!story_flag(SF_MET0 + 8)) { t.id = ID_TALLYWORN; t.x = 9; t.y = 30; }
        else { t.x = 20; t.y = 27; }
        break;
    case BEAT_C4_HELP:
        t.map = MAPID_HELLVILE;
        if (!story_flag(SF_MET0 + 9)) { t.id = ID_MOTHER_NOX; t.x = 7; t.y = 13; }
        else if (!has(SF_C4_SOOTKIN)) {
            if (inv_count(ITEM_FLOWERS) > 0) { t.id = ID_SOOTKIN; t.x = 41; t.y = 35; }
            else { t.map = MAPID_HAHALAND; t.x = 31; t.y = 18; }
        }
        else if (!has(SF_C4_LADY_SMOG)) { t.id = ID_LADY_SMOG; t.x = 49; t.y = 17; }
        else { t.id = ID_ANVIL; t.x = 31; t.y = 9; }
        break;
    case BEAT_C4_MAYOR:
        if (!has(SF_C4_MAYOR_CAME)) { t.id = ID_MAYOR; t.x = 54; t.y = 30; }
        else { t.map = MAPID_HELLVILE; t.id = ID_MOTHER_NOX; t.x = 8; t.y = 24; }
        break;
    case BEAT_C4_FESTIVAL: t.map = MAPID_PARK_UNITED; t.x = 16; t.y = 30; break;
    case BEAT_C4_STAGE: t.map = MAPID_PARK_UNITED; t.x = 31; t.y = 9; break;
    default: t.map = 0xFF; break;
    }
    return t;
}

int script_target(int *map_id, int *cx, int *cy)
{
    Target t = target_of(cur());
    if (map_id) *map_id = t.map == 0xFF ? -1 : t.map;
    if (cx) *cx = t.x;
    if (cy) *cy = t.y;
    return t.id;
}

/* ======================================================================
 * the cast on the map
 * ==================================================================== */
static int add(StoryNpcSpot *out, int n, int max, int id, int x, int y, int dir, int flags)
{
    if (n >= max || !alive(id)) return n;
    out[n].id = (int16_t)id;
    out[n].x = (uint8_t)x;
    out[n].y = (uint8_t)y;
    out[n].dir = (uint8_t)dir;
    out[n].flags = (uint8_t)flags;
    return n + 1;
}

int script_cast(int map, StoryNpcSpot *out, int max)
{
    int b = cur(), n = 0;
    Target t = target_of(b);
#define MAINF(who) ((t.id == (who)) ? NPCF_MAIN | NPCF_FIXED : NPCF_FIXED)
    if (map == MAPID_HAHALAND) {
        /* the Mayor */
        if (b <= BEAT_C1_MAYOR) n = add(out, n, max, ID_MAYOR, 33, 25, 0, MAINF(ID_MAYOR));
        else if (b == BEAT_C2_SCREAM || b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_MAYOR, 35, 25, 2, NPCF_FIXED);
        else if (b == BEAT_C4_GATE || (b == BEAT_C4_MAYOR && !has(SF_C4_MAYOR_CAME))) n = add(out, n, max, ID_MAYOR, 54, 30, 1, MAINF(ID_MAYOR));
        else if (b < BEAT_C3_ERUPTION || b >= BEAT_DONE) n = add(out, n, max, ID_MAYOR, 29, 8, 0, NPCF_FIXED);
        /* Nana on her terrace */
        n = add(out, n, max, ID_NANA, 25, 17, 0, MAINF(ID_NANA));
        /* Pip at the playground */
        n = add(out, n, max, ID_PIP, 10, 47, 0, MAINF(ID_PIP));
        /* Miss Lulla on her park bench */
        n = add(out, n, max, ID_LULLA, 30, 42, 0, MAINF(ID_LULLA));
        /* Father Fuzz */
        if (b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_FUZZ, 36, 24, 2, NPCF_MAIN | NPCF_FIXED);
        else n = add(out, n, max, ID_FUZZ, 7, 28, 0, MAINF(ID_FUZZ));
        /* Steamy */
        if (b == BEAT_C2_SCREAM || b == BEAT_C2_EXORCISE) n = add(out, n, max, ID_STEAMY, 38, 24, 0, MAINF(ID_STEAMY));
        /* Dr. Bloom at the hospital */
        n = add(out, n, max, ID_BLOOM, 17, 23, 0, NPCF_FIXED);
        /* Rivet at the workshop (chapter 2 on, until she goes home) */
        if (b >= BEAT_C2_VAPOR && !has(SF_C4_RIVET_HOME)) {
            if (b == BEAT_C4_GATE) n = add(out, n, max, ID_RIVET, 54, 28, 1, NPCF_FIXED);
            else n = add(out, n, max, ID_RIVET, 53, 10, 0, MAINF(ID_RIVET));
        }
        /* Vex outside Cloud Nine */
        if (b >= BEAT_C2_VAPOR && b <= BEAT_C2_RIVET) n = add(out, n, max, ID_VEX, 54, 37, 0, MAINF(ID_VEX));
    } else if (map == MAPID_HAHALAND_INVADED) {
        /* refugees at the church */
        n = add(out, n, max, ID_MAYOR, 4, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_FUZZ, 7, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_NANA, 9, 28, 0, NPCF_FIXED);
        n = add(out, n, max, ID_PIP, 5, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_BLOOM, 10, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_LULLA, 3, 29, 0, NPCF_FIXED);
        n = add(out, n, max, ID_RIVET, 53, 10, 0, (b == BEAT_C3_NESTS && !has(SF_RIVET_UPGRADE)) ? NPCF_FIXED | NPCF_SIDE : NPCF_FIXED);
        if (possessed(ID_HATTIE)) n = add(out, n, max, ID_HATTIE, 40, 21, 0, MAINF(ID_HATTIE) | NPCF_MAIN);
        if (possessed(ID_GRAMPY)) n = add(out, n, max, ID_GRAMPY, 27, 54, 0, MAINF(ID_GRAMPY) | NPCF_MAIN);
    } else if (map == MAPID_HELLVILE) {
        n = add(out, n, max, ID_TALLYWORN, 9, 30, 0, MAINF(ID_TALLYWORN));
        if (b == BEAT_C4_MAYOR && has(SF_C4_MAYOR_CAME)) {
            n = add(out, n, max, ID_MAYOR, 6, 24, 2, NPCF_MAIN | NPCF_FIXED);
            n = add(out, n, max, ID_MOTHER_NOX, 8, 24, 1, NPCF_MAIN | NPCF_FIXED);
        } else {
            n = add(out, n, max, ID_MOTHER_NOX, 7, 13, 0, MAINF(ID_MOTHER_NOX));
        }
        n = add(out, n, max, ID_SOOTKIN, 41, 35, 0, MAINF(ID_SOOTKIN));
        n = add(out, n, max, ID_LADY_SMOG, 49, 17, 0, MAINF(ID_LADY_SMOG));
        n = add(out, n, max, ID_ANVIL, 31, 9, 0, MAINF(ID_ANVIL));
        if (b == BEAT_C4_HELP && !has(SF_C4_RIVET_HOME)) n = add(out, n, max, ID_RIVET, 29, 9, 2, NPCF_FIXED);
        else if (has(SF_C4_RIVET_HOME) && b < BEAT_C4_FESTIVAL) n = add(out, n, max, ID_RIVET, 29, 9, 2, NPCF_FIXED);
        if (b >= BEAT_C4_GATE && b < BEAT_C4_FESTIVAL) n = add(out, n, max, ID_VEX, 47, 15, 0, NPCF_FIXED);
        if (b >= BEAT_C4_GATE) n = add(out, n, max, ID_HAZE, 44, 13, 0, NPCF_FIXED);
    } else if (map == MAPID_PARK_UNITED) {
        n = add(out, n, max, ID_MAYOR, 29, 10, 0, MAINF(ID_MAYOR));
        n = add(out, n, max, ID_MOTHER_NOX, 34, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_FUZZ, 26, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_RIVET, 37, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_ANVIL, 38, 11, 0, NPCF_FIXED);
        n = add(out, n, max, ID_PIP, 31, 10, 0, NPCF_FIXED);
        n = add(out, n, max, ID_NANA, 4, 18, 0, NPCF_FIXED);
        n = add(out, n, max, ID_VEX, 37, 18, 0, NPCF_FIXED);
        n = add(out, n, max, ID_SOOTKIN, 28, 20, 0, NPCF_FIXED);
        n = add(out, n, max, ID_VAPULA, 45, 30, 0, NPCF_FIXED);
        n = add(out, n, max, ID_BLOOM, 12, 26, 0, NPCF_FIXED);
        n = add(out, n, max, ID_TALLYWORN, 35, 20, 0, NPCF_FIXED);
    }
#undef MAINF
    return n;
}

/* ======================================================================
 * debug: jump + step
 * ==================================================================== */
static void ensure_pet(void) { if (!pet_exists()) pet_new("Doomo"); }

static void ensure_weapon(void)
{
    if (!has(SF_WEAPON_CHOSEN)) { flag(SF_WEAPON_CHOSEN); give_weapon(0); }
}

void script_debug_setup(int b)
{
    int i;
    DoomoStoryState *s = &doomo_town.story;
    s->quest = (uint8_t)b;                 /* targets below are the new beat's */
    if (b >= BEAT_C1_HOME) { flag(SF_KEY_GIVEN); flag(SF_EGG_GIVEN); }
    if (b >= BEAT_C1_HATCH) { flag(SF_HOME_SEEN); ensure_pet(); inv_take(ITEM_DOOMOGACHI_EGG, 1); }
    if (b == BEAT_C1_BUNNY_RETURN) { flag(SF_BUNNY_FOUND); if (!inv_count(ITEM_LOST_BUNNY)) inv_add(ITEM_LOST_BUNNY, 1); }
    if (b > BEAT_C1_BUNNY_RETURN) { flag(SF_BUNNY_FOUND); inv_take(ITEM_LOST_BUNNY, inv_count(ITEM_LOST_BUNNY)); }
    for (i = 0; i < 4; i++)
        if (b >= BEAT_C1_ARRIVE + 0 && story_beat_chapter(b) > i) flag(SF_TITLE_CH1 + i);
    if (b > BEAT_C1_CUPCAKE) flag(SF_CUPCAKE_GIVEN);
    if (b > BEAT_C1_WALL) flag(SF_FOG_SEEN);
    if (b == BEAT_C2_SCREAM || b == BEAT_C2_FUZZ || b == BEAT_C2_EXORCISE) possess(ID_STEAMY);
    if (b > BEAT_C2_FUZZ) flag(SF_FUZZ_PRACTICE);
    if (b > BEAT_C2_EXORCISE) { unpossess(ID_STEAMY); flag(SF_STEAMY_SAVED); }
    if (b > BEAT_C2_RIVET) { ensure_weapon(); flag(SF_VEX_GONE); }
    if (b >= BEAT_C3_ERUPTION && b < BEAT_C4_GATE) { s->map = MAPID_HAHALAND_INVADED; }
    if (b == BEAT_C3_NESTS) { possess(ID_HATTIE); possess(ID_GRAMPY); }
    if (b > BEAT_C3_NESTS) {
        for (i = 0; i < STORY_NESTS; i++) flag(SF_NEST0 + i);
        for (i = 0; i < STORY_C3_POSSESSED; i++) flag(SF_C3_EXO0 + i);
        unpossess(ID_HATTIE);
        unpossess(ID_GRAMPY);
    }
    if (b >= BEAT_C4_GATE) s->map = MAPID_HAHALAND;
    if (b >= BEAT_C4_PRISONERS) s->map = MAPID_HELLVILE;
    if (b > BEAT_C4_PRISONERS) {
        static const int16_t hv[5] = { ID_TALLYWORN, 507, 509, 531, 537 };
        story_set_flag(SF_MET0 + 8, 1);
        for (i = 0; i < 5; i++) story_befriend(hv[i], STORY_FRIEND_LEVEL);
    }
    if (b == BEAT_C4_HELP) possess(ID_LADY_SMOG);
    if (b > BEAT_C4_HELP) {
        story_set_flag(SF_MET0 + 9, 1);
        flag(SF_C4_SOOTKIN); flag(SF_C4_LADY_SMOG); flag(SF_C4_RIVET_HOME);
        unpossess(ID_LADY_SMOG);
    }
    if (b >= BEAT_C4_FESTIVAL) { flag(SF_C4_MAYOR_CAME); s->map = MAPID_PARK_UNITED; }
    if (b >= BEAT_DONE) flag(SF_CREDITS_SEEN);
    {
        int m, cx, cy;
        script_target(&m, &cx, &cy);
        if (m < 0) { cx = 16; cy = 26; }
        if (b <= BEAT_C1_MAYOR) { cx = 34; cy = 26; }
        s->x = (int16_t)(cx * DOOMO_CELL_UNITS + 32);
        s->y = (int16_t)((cy + 1) * DOOMO_CELL_UNITS + 32);
    }
}

/* A citizen of a district the player has not befriended yet. */
static int fresh_citizen(int district)
{
    int i, lo = district ? 500 : 100;
    for (i = lo; i < lo + 400; i++) {
        const DoomoResident *r = town_res(i);
        if (r && r->status == RES_ALIVE && r->level < STORY_FRIEND_LEVEL && roster_npc_of(i) < 0 &&
            !(r->flags & RF_STORY_NPC))
            return i;
    }
    return lo;
}

int script_debug_step(void)
{
    int b = cur(), i;
    switch (b) {
    case BEAT_C1_ARRIVE: beat(BEAT_C1_MAYOR); return 1;
    case BEAT_C1_MAYOR: story_interact_npc(ID_MAYOR); return 1;
    case BEAT_C1_KEY: story_interact_npc(ID_NANA); return 1;
    case BEAT_C1_HOME: story_enter_place(PLACE_HOME); return 1;
    case BEAT_C1_HATCH: beat(BEAT_C1_PIP); return 1;
    case BEAT_C1_PIP: story_interact_npc(ID_PIP); return 1;
    case BEAT_C1_BUNNY: story_trigger(STORY_TAG_EVENT_BEACH); return 1;
    case BEAT_C1_BUNNY_RETURN: story_interact_npc(ID_PIP); return 1;
    case BEAT_C1_FRIENDS: story_interact_npc(fresh_citizen(0)); return 1;
    case BEAT_C1_CUPCAKE: story_interact_npc(has(SF_CUPCAKE_GIVEN) ? ID_LULLA : ID_NANA); return 1;
    case BEAT_C1_WALL: story_trigger(STORY_TAG_EVENT_FOG); return 1;
    case BEAT_C2_VAPOR: story_interact_npc(ID_VEX); return 1;
    case BEAT_C2_WEIRD: story_interact_npc(fresh_citizen(0)); return 1;
    case BEAT_C2_SCREAM: story_interact_npc(ID_STEAMY); return 1;
    case BEAT_C2_FUZZ: story_interact_npc(ID_FUZZ); return 1;
    case BEAT_C2_EXORCISE: story_interact_npc(ID_STEAMY); return 1;
    case BEAT_C2_RIVET: story_interact_npc(ID_RIVET); return 1;
    case BEAT_C2_CELLAR: story_trigger(STORY_TAG_EVENT_CELLAR); return 1;
    case BEAT_C3_ERUPTION:
        beat(BEAT_C3_NESTS);
        return 1;
    case BEAT_C3_NESTS:
        for (i = 0; i < STORY_NESTS; i++)
            if (!has(SF_NEST0 + i)) { script_nest_encounter(i); return 1; }
        if (possessed(ID_HATTIE)) { story_interact_npc(ID_HATTIE); return 1; }
        if (possessed(ID_GRAMPY)) { story_interact_npc(ID_GRAMPY); return 1; }
        for (i = 0; i < STORY_C3_POSSESSED; i++) flag(SF_C3_EXO0 + i);
        c3_check();
        return 1;
    case BEAT_C3_SPIRE: story_trigger(STORY_TAG_EVENT_SPIRE); return 1;
    case BEAT_C4_GATE: story_enter_place(PLACE_WALL_GATE); return 1;
    case BEAT_C4_PRISONERS:
        if (!story_flag(SF_MET0 + 8)) story_interact_npc(ID_TALLYWORN);
        else story_interact_npc(fresh_citizen(1));
        return 1;
    case BEAT_C4_HELP:
        if (!story_flag(SF_MET0 + 9)) { story_interact_npc(ID_MOTHER_NOX); return 1; }
        if (!has(SF_C4_SOOTKIN)) {
            if (!inv_count(ITEM_FLOWERS)) inv_add(ITEM_FLOWERS, 1);
            story_interact_npc(ID_SOOTKIN);
            return 1;
        }
        if (!has(SF_C4_LADY_SMOG)) { story_interact_npc(ID_LADY_SMOG); return 1; }
        if (!has(SF_C4_RIVET_HOME)) { story_interact_npc(ID_ANVIL); return 1; }
        help_check();
        return 1;
    case BEAT_C4_MAYOR: story_interact_npc(ID_MAYOR); return 1;
    case BEAT_C4_FESTIVAL: story_interact_npc(fresh_citizen(story_step() & 1)); return 1;
    case BEAT_C4_STAGE: story_trigger(STORY_TAG_EVENT_STAGE); return 1;
    default: return 0;
    }
}
