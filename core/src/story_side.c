/* story_side.c - side quests (owner: story).
 *
 * 46 errands from named citizens of both districts, spread over the four
 * chapters and the free roam after the credits: fetch, deliver,
 * matchmaking, lost pets, photos, mini-boss hunts, exorcism requests,
 * cross-border reconciliation and visits. Each quest is one table row;
 * its state is 2 bits in the story flags (64 + 2q): 0 not taken, 1 active,
 * 2 ready to hand in, 3 done.
 */
#include "story_int.h"

typedef struct SideQuest {
    const char *title;
    int16_t  giver;            /* roster id                                  */
    uint8_t  ch_min, ch_max;   /* offered in these chapters (5 = free roam)  */
    uint8_t  gmap, gx, gy;     /* where the giver waits                      */
    uint8_t  kind;             /* SQ_*                                       */
    int16_t  target;           /* FETCH: item; DELIVER / MATCH / RECONCILE /
                                  EXORCISE: resident; VISIT: place; HUNT: map */
    int16_t  other;            /* DELIVER: item; MATCH: who they confess to;
                                  HUNT: demon set (invasion nests)           */
    uint8_t  count;            /* FETCH: how many                            */
    uint8_t  tmap, tx, ty;     /* target spot (find / photo / pet / NPC)     */
    uint16_t money;
    uint8_t  item, item_n;
    uint16_t unlock;
    const char *offer;
    const char *hint;
    const char *thanks;
} SideQuest;

#define HH MAPID_HAHALAND
#define HV MAPID_HELLVILE
#define PK MAPID_PARK_UNITED
#define NONE USTORY_NONE

static const SideQuest quests[] = {
    /* ---------------- chapter 1: pastel errands ---------------------------- */
    { "Mail Mix-Up", 97, 1, 2, HH, 20, 20, SQ_DELIVER, 52, ITEM_LOVE_LETTER, 1, HH, 42, 26,
      60, ITEM_FLOWERS, 1, USTORY_REWARD_FIRST + 1,
      "Ribbit! A love letter for Valentina the matchmaker got stuck to my hat. Could you hop it over to her in the plaza?",
      "Deliver the love letter to Valentina (plaza)",
      "She read it and fainted from joy! Wonderful! Here, a thank-you and a lovebird stamp. Ribbit!" },
    { "Coffee for Bluebell", 29, 1, 2, HH, 30, 16, SQ_FETCH, ITEM_COFFEE, -1, 1, HH, 0, 0,
      45, ITEM_NONE, 0, USTORY_REWARD_FIRST + 2,
      "I've been up since dawn arranging tulips into the shape of a heart. I need coffee. Please. From the cafe.",
      "Bring Bluebell a Coffee (Brimstone Bites)",
      "*slurp* My petals are perking up! Take this tulip pin. I made it from a real tulip. Mostly." },
    { "Lost Kitten", 96, 1, 2, HH, 17, 9, SQ_PET, 0, -1, 1, HH, 18, 15,
      80, ITEM_NONE, 0, NONE,
      "Woof! I was walking Sir Whiskers and he walked ME into a hedge and vanished! He likes high places. Up on the Lane?",
      "Find Sir Whiskers the cat (Hillvile Lane)",
      "SIR WHISKERS! You little rascal! Thank you, thank you! Here's my dog-walking money." },
    { "Say Cheese!", 46, 1, 5, HH, 37, 24, SQ_PHOTO, 0, -1, 1, HH, 34, 24,
      50, ITEM_NONE, 0, NONE,
      "I'm making a postcard of HaHaland! Could you snap the giggling fountain for me? Stand right in front and press A!",
      "Photograph the fountain in the plaza",
      "Oh, the sparkles! The lighting! You have the eye, darling. Here's your model fee." },
    { "Cupid's Little Helper", 117, 1, 2, HH, 44, 28, SQ_MATCH, 11, 25, 1, HH, 45, 36,
      100, ITEM_PERFUME, 1, NONE,
      "Psst! Freckles has a HUGE crush on Skipper the paper kid. Give Freckles a little pep talk? She's in the park.",
      "Encourage Freckles to confess (park)",
      "Love is in the air! Or at least nervous giggling. Thanks, partner. Here, smell nice!" },
    { "Strong Bones", 5, 1, 2, HH, 8, 18, SQ_FETCH, ITEM_MILK, -1, 2, HH, 0, 0,
      70, ITEM_NONE, 0, NONE,
      "Clack clack! I'm a xylophone teacher AND a xylophone. Two Milks keep these bones ringing true. Could you?",
      "Bring Bonejangles 2 Milk",
      "*glug glug* Hear that? Perfect pitch! You've saved my tibia solo." },
    { "Bedtime Stories", 16, 1, 5, HH, 40, 36, SQ_VISIT, PLACE_LIBRARY, -1, 1, HH, 41, 35,
      40, ITEM_HYMN_BOOK, 1, USTORY_REWARD_FIRST + 4,
      "Hoo! The little ones want a new bedtime story. Would you check the Library for 'The Very Hungry Demon'?",
      "Visit the Library in the park",
      "Hoo hoo! The kids will love it. It has a happy ending. Mostly. Take this, and a bookworm bookmark." },
    { "Ribbon Cutting", 83, 1, 2, HH, 8, 43, SQ_VISIT, PLACE_FUN_FAIR, -1, 1, HH, 4, 42,
      60, ITEM_NONE, 0, NONE,
      "I cut ribbons professionally! The Fun Fair's big top needs an official visitor before I can snip. Pop in?",
      "Visit the Fun Fair big top",
      "SNIP! The Fun Fair is officially fun! Here, your official visitor fee." },
    { "Grandma's Glasses", 27, 1, 2, HH, 6, 10, SQ_FIND, 0, -1, 1, HH, 44, 55,
      70, ITEM_PUDDING, 1, USTORY_REWARD_FIRST + 3,
      "Dearie, I've lost my reading glasses! Last I saw them I was napping at the beach. Or was it 1974?",
      "Find Oma Fluff's glasses (beach)",
      "Oh! I can see you now! You're lovely. Have a pudding, and my old locket. There's a cake in it." },
    { "Sandcastle Contest", 112, 1, 5, HH, 36, 53, SQ_PHOTO, 0, -1, 1, HH, 38, 61,
      50, ITEM_NONE, 0, USTORY_REWARD_FIRST + 14,
      "Duuude. I need a photo from the very end of the pier for the surf club wall. Gnarly view out there!",
      "Photograph the view from the end of the pier",
      "Radical shot! You're officially in the surf club now, dude!" },
    { "Clean Sweep", 39, 1, 2, HH, 44, 10, SQ_FETCH, ITEM_SOAP, -1, 3, HH, 0, 0,
      90, ITEM_NONE, 0, NONE,
      "Blorp! I wash every window in Hillvile Towers. I'm out of soap and full of pride. Three Soaps, please!",
      "Bring Squishy 3 Soap (shop)",
      "Sparkle sparkle! You can see your reflection in me now. Thanks, friend!" },
    { "Choir Practice", 142, 1, 5, HH, 23, 36, SQ_VISIT, PLACE_CONCERT_HALL, -1, 1, HH, 25, 35,
      60, ITEM_NONE, 0, USTORY_SONG_FIRST + SONG_CHURCH - 1,
      "HALLELUJAH! I mean, hello. The Concert Hall needs someone to test the echo. Go in and sing! Loudly!",
      "Visit the Concert Hall",
      "I heard you from here! Beautiful! Terrible! Beautiful! Here's a hymn for your jukebox." },

    /* ---------------- chapter 2: something in the vapor -------------------- */
    { "Smoke Signals", 57, 2, 2, HH, 50, 24, SQ_PHOTO, 0, -1, 1, HH, 51, 38,
      80, ITEM_NONE, 0, USTORY_REWARD_FIRST + 8,
      "The weather's all wrong. A purple cloud is sitting over Cloud Nine Vapor and it's NOT on my forecast. Photo, please?",
      "Photograph the purple cloud at Cloud Nine Vapor",
      "Look at that cloud... it's SMILING. That's not meteorology. That's evil. Here, a storm chaser badge." },
    { "Steam Cleaning", 41, 2, 4, HH, 21, 26, SQ_DELIVER, ID_STEAMY, ITEM_SOAP, 1, HH, 38, 26,
      70, ITEM_SOAP, 2, NONE,
      "Poor Steamy smells like grape after his... ordeal. Bring him my lavender soap? It's very calming.",
      "Bring the lavender soap to Steamy (plaza)",
      "He's back at the bath house, humming! Real humming, not demon humming. Bless you!" },
    { "Holy Supplies", 354, 2, 3, HH, 11, 27, SQ_FETCH, ITEM_HOLY_WATER, -1, 2, HH, 0, 0,
      100, ITEM_ROSARY, 1, NONE,
      "The old scrolls warn of more possessions. We need Holy Water, two flasks. The church sells it, child.",
      "Bring Sr. Lumen 2 Holy Water",
      "Blessed be. Take this rosary: it once made a demon sneeze for an hour." },
    { "Scrap Science", 69, 2, 4, HH, 50, 12, SQ_FETCH, ITEM_SCRAP, -1, 3, HH, 0, 0,
      80, ITEM_GEARS, 2, NONE,
      "Squeak! I'm inventing a vapor detector! I just need three bits of scrap. Rivet sells some, or find it in fights!",
      "Bring Prof. Fizz 3 Scrap",
      "EUREKA! It beeps! It beeps at everything! Take some gears for your welding." },
    { "Nervous Nellie", 23, 2, 2, HH, 19, 30, SQ_EXORCISE, 14, -1, 1, HH, 21, 31,
      150, ITEM_CHILL_PILL, 1, USTORY_REWARD_FIRST + 7,
      "My client Lagspike vaped ALL night on stream and now his chat is only screaming. Can you help him? Gently?",
      "Exorcise Lagspike (near the graveyard)",
      "He's streaming cat videos again. Normal cat videos. Have a chill pill and a golden hug token." },
    { "No Vapes at Daycare", 36, 2, 3, HH, 30, 42, SQ_FIND, 0, -1, 1, HH, 5, 49,
      60, ITEM_NONE, 0, NONE,
      "Someone dropped a vape pen at the playground! The kids think it's a magic wand. Please find it before they do!",
      "Find the dropped vape pen (playground)",
      "Got it? Into the bin it goes. Forever. Thank you, @P. The kids are safe." },
    { "A Taste of Home", 13, 2, 3, HH, 53, 10, SQ_FETCH, ITEM_CANDY_EYEBALLS, -1, 2, HH, 0, 0,
      60, ITEM_WELD_ROD, 2, NONE,
      "Don't laugh. I miss Hellvile candy. The shop sells Candy Eyeballs now. Two packs? For morale. Imp morale.",
      "Bring Rivet 2 Candy Eyeballs (shop)",
      "*crunch* ...Just like Mum's. Ahem. Thanks. Have some weld rods. Don't tell anyone I'm sentimental." },
    { "Knightly Hearts", 52, 2, 5, HH, 40, 26, SQ_MATCH, 9, 86, 1, HH, 12, 21,
      120, ITEM_RING, 1, USTORY_REWARD_FIRST + 0,
      "Dame Fluff and Valkyrie have been sparring for YEARS. It's obviously love. Go nudge Dame Fluff, darling!",
      "Nudge Dame Fluff to confess (west lawn)",
      "They're holding paws! With gauntlets on! Take this ring, and my Cupid's Arrow pin." },
    { "Pip's Bravery Badge", 42, 2, 2, HH, 11, 47, SQ_PHOTO, 0, -1, 1, HH, 55, 27,
      50, ITEM_PLUSHIE, 1, USTORY_REWARD_FIRST + 15,
      "Mr. Flops wants a picture of the scary Wall! He's too scared. I'm NOT scared. You do it though.",
      "Photograph the Wall for Pip",
      "Whoa! Spooky! Mr. Flops fainted. Here's a plushie for being brave, and Mr. Flops ears!" },

    /* ---------------- chapter 3: hell invades ------------------------------ */
    { "Fire Drill", 99, 3, 3, HH, 45, 22, SQ_HUNT, MAPID_HAHALAND_INVADED, 4, 1, HH, 45, 24,
      150, ITEM_HELLSTEEL, 1, USTORY_REWARD_FIRST + 5,
      "WOOF! A big brute demon is sitting on my fire hydrant! I'm a fire chief, not a demon chief! Help!",
      "Defeat the brute by the plaza",
      "The hydrant is FREE! You're an honorary firefighter now, kid. Here's your badge." },
    { "Barricade the Bakery", 1, 3, 3, HH, 9, 28, SQ_FETCH, ITEM_SCRAP, -1, 3, HH, 0, 0,
      100, ITEM_CUPCAKE, 3, NONE,
      "Sweet pea! The demons are after my sourdough starter! Three bits of scrap and I can barricade the cafe!",
      "Bring Nana 3 Scrap",
      "Perfect! Nothing gets between Nana and her starter. Have some emergency cupcakes." },
    { "Medic!", 111, 3, 3, HH, 10, 29, SQ_FETCH, ITEM_MEDICINE, -1, 2, HH, 0, 0,
      120, ITEM_ENERGY_DRINK, 2, NONE,
      "So many scrapes and singed fur. I'm running low. Two Medicine, please. My cold hands are steady, at least.",
      "Bring Dr. Bloom 2 Medicine",
      "Thank you. Everyone will be patched up. You look tired. Drink these. Doctor's orders." },
    { "Hymn of Hope", 426, 3, 4, HH, 3, 27, SQ_EXORCISE, 91, -1, 1, HH, 16, 28,
      150, ITEM_BLESSED_SALT, 2, NONE,
      "Bellwether rang the bells too close to the cloud and now they toll BACKWARDS. Free her, please!",
      "Exorcise Bellwether the bell ringer",
      "Listen! The bells ring forward again! Take some blessed salt for the road." },
    { "Mr. Biscuit", 127, 3, 4, HH, 8, 27, SQ_PET, 0, -1, 1, HH, 16, 35,
      100, ITEM_NONE, 0, NONE,
      "*hic* Mr. Biscuit ran away when the sky went red! He's a puppy. He's THIS big. *gestures tiny*",
      "Find Mr. Biscuit the puppy (by the graveyard)",
      "MR. BISCUIT! *hug* Thank you thank you thank you!" },
    { "Graveyard Shift", 354, 3, 3, HH, 11, 27, SQ_HUNT, MAPID_HAHALAND_INVADED, 5, 1, HH, 14, 48,
      200, ITEM_BLESSED_SALT, 1, USTORY_REWARD_FIRST + 6,
      "Something with too many legs crawled out of the Fun Fair. Spiders made of cloud, child. Purge them.",
      "Clear the cloud spiders at the Fun Fair",
      "The children can play again. Take this salt and my old spider swatter medal, child." },
    { "Spire Scout", 17, 3, 3, HH, 46, 40, SQ_PHOTO, 0, -1, 1, HH, 52, 39,
      80, ITEM_NONE, 0, USTORY_REWARD_FIRST + 11,
      "My balloon got sucked toward that cloud tower! Get me a close-up photo so I can plan a rescue flight!",
      "Photograph the foot of Cloud Nine Spire",
      "Oof. It's even uglier up close. Thanks! Here, my sky scout wings pin." },
    { "Stay Awake, Guards!", 86, 3, 3, HH, 24, 23, SQ_FETCH, ITEM_ENERGY_DRINK, -1, 2, HH, 0, 0,
      100, ITEM_HELLSTEEL, 1, NONE,
      "The royal guard hasn't slept since the sky ripped. Two Energy Drinks and we'll hold the plaza till dawn!",
      "Bring Valkyrie 2 Energy Drinks",
      "HUZZAH! The guard stands firm! Take this hellsteel we pried off a demon." },
    { "Comfort Food", 4, 3, 4, HH, 39, 22, SQ_DELIVER, 26, ITEM_ONIGIRI, 1, HH, 22, 20,
      70, ITEM_ONIGIRI, 1, NONE,
      "Hootenanny has been keeping night watch for three days straight. Bring him my onigiri? Extra pickle.",
      "Bring the onigiri to Hootenanny (west lawn)",
      "He ate it in one bite and said 'hoo'. That means thank you. Here's one for you too." },
    { "Save the Mascot", 43, 3, 4, HH, 14, 44, SQ_EXORCISE, 20, -1, 1, HH, 17, 47,
      150, ITEM_PLUSHIE, 1, NONE,
      "Hugsley the town mascot breathed the cloud and now he hugs TOO HARD. Please exorcise him before he hugs a bus.",
      "Exorcise Hugsley the mascot (playground)",
      "He's back to normal hugs! Normal, wonderful hugs. Here, a balloon animal. It's a plushie. Close enough." },

    /* ---------------- chapter 4: hellvile, reconciliation ------------------ */
    { "Postcard from the Fountain", 753, 4, 5, HV, 36, 36, SQ_PHOTO, 0, -1, 1, HH, 34, 24,
      120, ITEM_NONE, 0, NONE,
      "I write poems on the gate in chalk. I've never seen a fountain that giggles. Could you bring me a photo of one?",
      "Photograph HaHaland's fountain for Clinker",
      "A fountain... that laughs. I'm going to write forty poems about this. Thank you." },
    { "Charcoal Cookies", 566, 4, 5, HV, 20, 22, SQ_DELIVER, ID_NANA, ITEM_BRIMSTONE_BAGEL, 1, HH, 25, 17,
      150, ITEM_ANGEL_CAKE, 1, NONE,
      "I baked for Nana Pudding! The famous Nana Pudding! They're only a little bit charcoal. Would you deliver them?",
      "Bring Charlotte's bagels to Nana Pudding",
      "She said they were 'crunchy and full of love'! She wants to swap recipes! Have some angel cake!" },
    { "The Florist's Apprentice", 582, 4, 5, HV, 24, 31, SQ_RECONCILE, 29, -1, 1, HH, 30, 16,
      120, ITEM_FLOWERS, 3, USTORY_REWARD_FIRST + 10,
      "Shh. I want to be a florist. A HILLVILE florist. Could you ask Bluebell if she'd take a ghoul apprentice?",
      "Ask Bluebell to take Morguelet as an apprentice",
      "She said YES?! I'm going to cry black tears of joy. Here, flowers, and a flower friend ribbon!" },
    { "Game Night", 730, 4, 5, HV, 22, 12, SQ_FETCH, ITEM_BOARD_GAME, -1, 1, HH, 0, 0,
      100, ITEM_NONE, 0, NONE,
      "I lowered the rent for the families in my crypts. Now the kids need something fun. A board game, maybe?",
      "Bring Graveley a Board Game",
      "Ha! The little ghouls will play for a century. Thank you. Don't tell anyone I'm nice." },
    { "Lullaby for Clouds", 701, 4, 5, HV, 55, 34, SQ_VISIT, PLACE_CONCERT_HALL, -1, 1, HH, 25, 35,
      100, ITEM_NONE, 0, NONE,
      "I hum lullabies to the clouds I haul. I've heard Hillvile has a Concert Hall. Could you go hear it for me?",
      "Visit HaHaland's Concert Hall for Ploddy",
      "Real music. In a real hall. The clouds will sleep so well tonight. Thank you, friend." },
    { "Squeaky Toy Diplomacy", 855, 4, 5, HV, 54, 31, SQ_FETCH, ITEM_PLUSHIE, -1, 1, HH, 0, 0,
      90, ITEM_MEAT_SLAB, 1, NONE,
      "GRR. Nobody gets into the Arena without a squeaky toy. Rules is rules. ...Can I keep it after?",
      "Bring Big Snarl a Plushie",
      "*squeak squeak squeak* ...Ahem. You're on the list. FOREVER. Have a steak." },
    { "Last Puff", 766, 4, 5, HV, 9, 17, SQ_EXORCISE, 788, -1, 1, HV, 11, 19,
      180, ITEM_SAGE_BUNDLE, 1, NONE,
      "My friend Hazey still heralds Vapula's smoke. He can't stop. Something's still inside him. Please?",
      "Exorcise Hazey the smoke herald",
      "He coughed out the last of it and then he SANG. My friend is back! Thank you!" },
    { "Counting Together", 712, 4, 5, HV, 9, 31, SQ_RECONCILE, 104, -1, 1, HH, 46, 24,
      120, ITEM_NONE, 0, NONE,
      "I counted days for ten years. They say a clockmaker in Hillvile counts seconds. Would he count with me?",
      "Introduce Tallyworn to Tock the clockmaker",
      "He said yes! Every day at noon, we count together. One. Two. Three. Thank you." },
    { "Echo of the Duke", 519, 4, 5, HV, 52, 37, SQ_HUNT, MAPID_CLOUD_NINE_SPIRE, -1, 1, HV, 52, 37,
      250, ITEM_HELLSTEEL, 2, USTORY_REWARD_FIRST + 12,
      "Sniff sniff. There's still an echo of Vapula's stink up in that Spire. A real champion would clear it out.",
      "Clear the echo of Vapula from the Spire",
      "You smell like victory! And smoke. Mostly victory. Take this champion's buckle, champ." },
    { "Rival Bakers", 669, 4, 5, HV, 17, 40, SQ_RECONCILE, 73, -1, 1, HH, 29, 19,
      120, ITEM_CUPCAKE, 2, NONE,
      "Crumbelina the royal baker said my marshmallows were 'a crime'. Could you... make peace for me?",
      "Make peace between Scorchmello and Crumbelina",
      "A bake-off! She challenged me to a FRIENDLY bake-off! Have some of her cupcakes, they're incredible." },
    { "Squeak the Bat", 820, 4, 5, HV, 45, 38, SQ_PET, 0, -1, 1, HV, 44, 41,
      100, ITEM_NONE, 0, USTORY_REWARD_FIRST + 9,
      "*sniffle* My pet bat Squeak flew off when the gate opened! He likes the lake shore. I bit someone. Sorry.",
      "Find Squeak the bat by the Lake of Fire",
      "SQUEAK! *happy chomp* Oops, sorry. Here, you're officially a bat buddy!" },
    { "Wall Art", 565, 4, 5, HV, 11, 24, SQ_PHOTO, 0, -1, 1, HV, 3, 25,
      100, ITEM_NONE, 0, USTORY_REWARD_FIRST + 13,
      "I painted hearts on the Wall every night for years. Before they knock it down, could you photograph them?",
      "Photograph the hearts on the Wall gate",
      "My hearts will live forever in that picture. Here, a wall artist ribbon. You've earned it." },

    /* ---------------- after the credits: the united park -------------------- */
    { "Encore!", 511, 5, 5, PK, 36, 12, SQ_VISIT, PLACE_STAGE, -1, 1, PK, 31, 8,
      200, ITEM_NONE, 0, USTORY_SONG_FIRST + SONG_FESTIVAL - 1,
      "That festival was the best gig of my un-life. Get up on the Unity Stage and give 'em an encore!",
      "Visit the Unity Stage for an encore",
      "ENCORE! ENCORE! You rock, Hillvile! Take the festival anthem for your jukebox." },
    { "Frog Wedding", 62, 5, 5, PK, 20, 12, SQ_MATCH, 912, 82, 1, PK, 45, 40,
      300, ITEM_RING, 1, USTORY_REWARD_FIRST + 16,
      "Warty sings love songs to the moon. Lily Lou grows lilies for the pond. A cross-border frog wedding! Nudge him?",
      "Nudge Warty to confess to Lily Lou",
      "A wedding! A WEDDING! The first cross-border wedding in Hellvile Park! Have a ring and my planner pin!" },
    { "The Big Picture", 46, 5, 5, PK, 22, 15, SQ_PHOTO, 0, -1, 1, PK, 31, 18,
      150, ITEM_NONE, 0, NONE,
      "Darling, I need the shot of the century: the river, both sides, no Wall. Stand on the bridge and snap it!",
      "Photograph the park from the north bridge",
      "No Wall in the frame. Just people. It's the most beautiful thing I've ever developed." },
    { "Potluck Planner", 778, 5, 5, PK, 40, 22, SQ_FETCH, ITEM_SOUL_SOUP, -1, 2, HH, 0, 0,
      150, ITEM_ANGEL_CAKE, 2, NONE,
      "My potluck is open to everyone now. Everyone! I need more soup. Two Soul Soups from the market, dear?",
      "Bring Mother Nox 2 Soul Soup (Hellvile market)",
      "Splendid. Hillvile brought cake and we brought soup. That is diplomacy. Have some cake." },
    { "Vapula's Apology", 999, 5, 5, PK, 45, 31, SQ_DELIVER, ID_MAYOR, ITEM_FLOWERS, 1, PK, 29, 10,
      200, ITEM_NONE, 0, NONE,
      "...*tiny wheeze* I'm... smaller now. Humbler. Could you give these flowers to your Mayor? With my apology?",
      "Bring Vapula's apology flowers to the Mayor (stage)",
      "He forgave me. He HUGGED me. I didn't know a cloud could feel this... dry-eyed. Thank you." },
    { "Explorer's Club", 15, 4, 5, HH, 14, 25, SQ_VISIT, PLACE_MARKET, -1, 1, HV, 19, 17,
      120, ITEM_NONE, 0, NONE,
      "As scout leader I MUST know: what do they sell at the Hellvile imp market? Go look! For science! For badges!",
      "Visit the imp market in Hellvile",
      "Spicy peppers and eyeball candy?! Incredible. You've earned the Cross-Border Explorer badge!" },
};
#define NQ ((int)(sizeof(quests) / sizeof(quests[0])))
DOOMO_STATIC_ASSERT(sizeof(quests) / sizeof(quests[0]) <= STORY_SIDE_MAX, side_quests_fit);

static const char *const kind_names[SQ_KIND_COUNT] = {
    "Fetch", "Deliver", "Find", "Lost pet", "Photo", "Matchmaking", "Hunt",
    "Exorcism", "Reconcile", "Visit"
};

/* ---- state ------------------------------------------------------------------ */
static int raw_state(int q)
{
    int f = STORY_FLAG_SIDE_FIRST + q * 2;
    return story_flag(f) | (story_flag(f + 1) << 1);
}

static void set_state(int q, int s)
{
    int f = STORY_FLAG_SIDE_FIRST + q * 2;
    story_set_flag(f, s & 1);
    story_set_flag(f + 1, (s >> 1) & 1);
}

static int alive_ok(int id)
{
    const DoomoResident *r = town_res(id);
    return r && (r->status == RES_ALIVE || r->status == RES_POSSESSED || r->status == RES_GHOST);
}

static int chapter_now(void)
{
    int ch = story_chapter();
    return ch == STORY_CH_DONE ? 5 : ch;
}

static int available(int q)
{
    const SideQuest *s = &quests[q];
    int ch = chapter_now();
    if (ch < s->ch_min || ch > s->ch_max) return 0;
    if (!alive_ok(s->giver)) return 0;
    {
        const DoomoResident *g = town_res(s->giver);
        if (g->status == RES_POSSESSED) return 0;
    }
    if ((s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_RECONCILE) && !alive_ok(s->target))
        return 0;
    if (s->kind == SQ_EXORCISE) {
        const DoomoResident *t = town_res(s->target);
        if (!t || (t->status != RES_ALIVE && t->status != RES_POSSESSED)) return 0;
    }
    if (s->kind == SQ_DELIVER && s->target == ID_STEAMY && story_flag(SF_STEAMY_LOST)) return 0;
    /* the plot's own big moments come first */
    if (ch == 1 && story_beat() < BEAT_C1_FRIENDS) return 0;
    if (ch == 2 && story_beat() < BEAT_C2_RIVET && s->kind == SQ_EXORCISE) return 0;
    if (s->target == ID_STEAMY && story_beat() < BEAT_C2_RIVET) return 0;
    if (ch == 3 && story_beat() < BEAT_C3_NESTS) return 0;
    if (ch == 4 && s->gmap == HV && story_beat() < BEAT_C4_PRISONERS) return 0;
    return 1;
}

int story_side_count(void) { return NQ; }

int story_side_state(int q)
{
    int s;
    if (q < 0 || q >= NQ) return SIDE_LOCKED;
    s = raw_state(q);
    if (s == 0) return available(q) ? SIDE_OFFERED : SIDE_LOCKED;
    return s == 1 ? SIDE_ACTIVE : s == 2 ? SIDE_READY : SIDE_DONE;
}

const char *story_side_title(int q) { return (q >= 0 && q < NQ) ? quests[q].title : ""; }
int story_side_kind(int q) { return (q >= 0 && q < NQ) ? quests[q].kind : 0; }
int story_side_giver(int q) { return (q >= 0 && q < NQ) ? quests[q].giver : -1; }
int story_side_chapter(int q) { return (q >= 0 && q < NQ) ? quests[q].ch_min : 0; }
const char *story_side_hint(int q) { return (q >= 0 && q < NQ) ? quests[q].hint : ""; }
const char *story_side_kind_name(int k) { return (k >= 0 && k < SQ_KIND_COUNT) ? kind_names[k] : ""; }

void story_side_where(int q, int *map_id, int *cx, int *cy)
{
    if (q < 0 || q >= NQ) return;
    if (map_id) *map_id = quests[q].gmap;
    if (cx) *cx = quests[q].gx;
    if (cy) *cy = quests[q].gy;
}

int story_side_target(int q, int *map_id, int *cx, int *cy)
{
    const SideQuest *s;
    if (q < 0 || q >= NQ) return 0;
    s = &quests[q];
    if (map_id) *map_id = s->tmap;
    if (cx) *cx = s->tx;
    if (cy) *cy = s->ty;
    return s->kind == SQ_FIND || s->kind == SQ_PET || s->kind == SQ_PHOTO ||
           s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_EXORCISE ||
           s->kind == SQ_RECONCILE || s->kind == SQ_HUNT || s->kind == SQ_VISIT;
}

int story_side_done_count(void)
{
    int q, n = 0;
    for (q = 0; q < NQ; q++) n += raw_state(q) == 3;
    return n;
}

int side_active_count(void)
{
    int q, n = 0;
    for (q = 0; q < NQ; q++) n += raw_state(q) == 1 || raw_state(q) == 2;
    return n;
}

const char *side_objective(void)
{
    int q;
    for (q = 0; q < NQ; q++) if (raw_state(q) == 2) return "Hand in a finished side quest!";
    for (q = 0; q < NQ; q++) if (raw_state(q) == 1) return quests[q].hint;
    return "";
}

/* ---- accepting & finishing -------------------------------------------------- */
static int16_t cur_q = -1;

static void on_accept(int q)
{
    const SideQuest *s = &quests[q];
    set_state(q, 1);
    if (s->kind == SQ_DELIVER && s->other > ITEM_NONE && s->other < ITEM_COUNT) inv_add(s->other, 1);
    if (s->kind == SQ_EXORCISE) {
        DoomoResident *t = town_res(s->target);
        if (t && t->status == RES_ALIVE) {
            t->stat[STAT_CORRUPTION] = 100;
            t->status = RES_POSSESSED;
            t->exorcism = 0;
            portrait_invalidate(s->target);
        }
    }
    ui_toast(STR_ICON_HEART " Side quest accepted!");
    audio_sfx(SFX_SELECT);
    ow_refresh_cast();
}

static void finish(int q)
{
    const SideQuest *s = &quests[q];
    int i, all = 1;
    set_state(q, 3);
    story_befriend(s->giver, 4);
    story_reward(s->money, s->item, s->item_n, s->unlock);
    ui_toast(STR_ICON_STAR " Side quest complete!");
    audio_sfx(SFX_HAPPY);
    for (i = 0; i < NQ; i++) if (raw_state(i) != 3) { all = 0; break; }
    if (all) unlock_grant(USTORY_ALL_SIDE);
    ow_refresh_cast();
}

static const char *const O_accept[] = { "Sure, I'll help!", "Maybe later" };

static void accept_choice(int c)
{
    if (cur_q >= 0 && c == 0) on_accept(cur_q);
}

static void say_giver(int q, const char *text)
{
    story_say(quests[q].giver, text);
}

static void hunt_choice(int c)
{
    const SideQuest *s;
    if (cur_q < 0 || c != 0) return;
    s = &quests[cur_q];
    if (s->target == MAPID_HAHALAND_INVADED)
        story_push_combat(s->target, CTX_SIDE, cur_q, s->tx, s->ty, s->other);
    else
        story_push_combat(s->target, CTX_SIDE, cur_q, -1, -1, -1);
}

static const char *const O_hunt[] = { "Let's go!", "Not yet" };

static void exo_choice(int c)
{
    if (cur_q >= 0 && c == 0) story_push_exorcism(quests[cur_q].target, CTX_SIDE, cur_q);
}

/* ---- talking ------------------------------------------------------------------ */
int side_npc(int id)
{
    int q;
    /* 1) someone you are delivering to / matching / reconciling / exorcising */
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) != 1 || s->target != id) continue;
        if (s->kind == SQ_DELIVER) {
            int it = s->other;
            if (inv_count(it) > 0) {
                inv_take(it, 1);
                story_befriend(id, 3);
                story_say(id, "For me? Oh! Thank you so much! Please tell them I said so!");
                set_state(q, 2);
                ui_toast(STR_ICON_CHECK " Delivered! Report back.");
                ow_refresh_cast();
                return 1;
            }
        } else if (s->kind == SQ_MATCH) {
            int ok;
            story_fmt_target = (int16_t)s->other;
            story_say(id, "You think I should tell @T how I feel? ...Okay. Okay! Wish me luck!");
            ok = sim_confess(id, s->other, 45);
            story_say(-1, ok ? "It worked! They're blushing at each other from across the park." :
                               "It didn't go perfectly... but they agreed to have tea. That's a start!");
            set_state(q, 2);
            ow_refresh_cast();
            return 1;
        } else if (s->kind == SQ_RECONCILE) {
            DoomoEdge *e;
            story_fmt_target = (int16_t)s->giver;
            story_say(id, "@T asked about me? From across the river? ...Well. I suppose it's time we talked.");
            rel_adjust(id, s->giver, 45);
            e = rel_find(id, s->giver);
            if (e && (e->kind & REL_KIND_MASK) == REL_ACQUAINTANCE) rel_set_kind(id, s->giver, REL_FRIEND);
            progress_add(PROG_CROSS_BORDER_FRIENDS, 1);
            story_befriend(id, 3);
            set_state(q, 2);
            ow_refresh_cast();
            return 1;
        } else if (s->kind == SQ_EXORCISE) {
            const DoomoResident *t = town_res(id);
            if (t && t->status == RES_POSSESSED) {
                static const char *const O_ex[] = { "Begin the exorcism!", "Not yet" };
                cur_q = (int16_t)q;
                story_fmt_target = (int16_t)id;
                story_say(-1, "@T's eyes glow red. A voice that isn't theirs whispers: \"Go away, little hero.\"");
                story_choice(-1, "Perform the exorcism?", O_ex, 2, exo_choice);
                return 1;
            }
        }
    }
    /* 2) a giver */
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        int st = raw_state(q);
        if (s->giver != id) continue;
        cur_q = (int16_t)q;
        if (st == 0 && available(q)) {
            say_giver(q, s->offer);
            story_choice(id, s->title, O_accept, 2, accept_choice);
            return 1;
        }
        if (st == 2) {
            say_giver(q, s->thanks);
            finish(q);
            return 1;
        }
        if (st == 1) {
            if (s->kind == SQ_FETCH) {
                if (inv_count(s->target) >= s->count) {
                    inv_take(s->target, s->count);
                    say_giver(q, s->thanks);
                    finish(q);
                } else {
                    char buf[96];
                    doomo_snprintf(buf, sizeof(buf), "Any luck? I need %d %s. You have %d.", s->count,
                                   doomo_items[s->target].name, inv_count(s->target));
                    say_giver(q, buf);
                }
                return 1;
            }
            if (s->kind == SQ_HUNT) {
                say_giver(q, "Ready to fight? It's dangerous out there!");
                story_choice(id, s->hint, O_hunt, 2, hunt_choice);
                return 1;
            }
            {
                char buf[96];
                doomo_snprintf(buf, sizeof(buf), "Still on it? %s.", s->hint);
                say_giver(q, buf);
            }
            return 1;
        }
    }
    return 0;
}

/* ---- spots: find / pet / photo ---------------------------------------------------- */
static int same_map(int a, int b)
{
    if (a == b) return 1;
    return (a == MAPID_HAHALAND || a == MAPID_HAHALAND_INVADED) &&
           (b == MAPID_HAHALAND || b == MAPID_HAHALAND_INVADED);
}

int side_spots(int map_id, SideSpot *out, int max)
{
    int q, n = 0;
    for (q = 0; q < NQ && n < max; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) != 1) continue;
        if (s->kind != SQ_FIND && s->kind != SQ_PET && s->kind != SQ_PHOTO) continue;
        if (!same_map(s->tmap, map_id)) continue;
        out[n].x = s->tx;
        out[n].y = s->ty;
        out[n].kind = s->kind;
        out[n].q = (uint8_t)q;
        n++;
    }
    return n;
}

int side_trigger_spot(int map_id, int cx, int cy, int press)
{
    int q;
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        int dx, dy;
        if (raw_state(q) != 1) continue;
        if (s->kind != SQ_FIND && s->kind != SQ_PET && s->kind != SQ_PHOTO) continue;
        if (!same_map(s->tmap, map_id)) continue;
        dx = cx - s->tx; dy = cy - s->ty;
        if (dx < -1 || dx > 1 || dy < -1 || dy > 1) continue;
        if (s->kind == SQ_PHOTO && !press) continue;
        set_state(q, 2);
        if (s->kind == SQ_PHOTO) {
            audio_sfx(SFX_SELECT);
            ow_flash(PAL_WHITE, 6);
            story_say(-1, "*CLICK!* A perfect shot. Bring it back!");
        } else if (s->kind == SQ_PET) {
            audio_sfx(SFX_SQUEAK);
            story_say(-1, "Found them! The little critter wiggles happily in your arms. Bring them home!");
        } else {
            audio_sfx(SFX_SPARKLE);
            story_say(-1, "You found it! Bring it back to its owner.");
        }
        ow_refresh_cast();
        return 1;
    }
    return 0;
}

/* ---- places (visit quests) ---------------------------------------------------------- */
int side_place(int place)
{
    int q;
    for (q = 0; q < NQ; q++) {
        const SideQuest *s = &quests[q];
        if (raw_state(q) == 1 && s->kind == SQ_VISIT && s->target == place) {
            set_state(q, 2);
            ui_toast(STR_ICON_CHECK " Visited! Report back.");
            ow_refresh_cast();
            /* fall through to the place's normal menu */
            return 0;
        }
    }
    return 0;
}

/* ---- sub-scene results ------------------------------------------------------------- */
void side_resume(const StoryPending *p, int result)
{
    int q = p->extra;
    if (q < 0 || q >= NQ) return;
    if (p->kind == PEND_COMBAT) {
        if (result > 0) { set_state(q, 2); story_say(-1, "Victory! Go tell them the good news."); }
        else story_say(-1, "You'll get them next time. Catch your breath first.");
    } else if (p->kind == PEND_EXORCISM) {
        story_fmt_target = quests[q].target;
        if (result > 0) { set_state(q, 2); story_say(-1, "The demon @D is cast out! @T is free. Go tell the good news."); }
        else if (result < 0) { set_state(q, 2); story_say(-1, "@T's soul could not be saved. Their friends will want to know..."); }
        else story_say(-1, "The demon clings on. Try again when you're ready.");
    }
    ow_refresh_cast();
}

/* ---- the cast of side quests on a map ------------------------------------------------ */
int side_cast(int map_id, StoryNpcSpot *out, int max)
{
    int q, n = 0;
    for (q = 0; q < NQ && n < max; q++) {
        const SideQuest *s = &quests[q];
        int st = raw_state(q);
        if (st == 3) continue;
        if (st == 0 && !available(q)) continue;
        if (same_map(s->gmap, map_id) && n < max) {
            out[n].id = s->giver;
            out[n].x = s->gx;
            out[n].y = s->gy;
            out[n].dir = 0;
            out[n].flags = (uint8_t)(NPCF_FIXED | (st == 2 ? NPCF_READY : st == 0 ? NPCF_SIDE : 0));
            n++;
        }
        if (st == 1 && (s->kind == SQ_DELIVER || s->kind == SQ_MATCH || s->kind == SQ_RECONCILE ||
                        s->kind == SQ_EXORCISE) && same_map(s->tmap, map_id) && n < max) {
            out[n].id = s->target;
            out[n].x = s->tx;
            out[n].y = s->ty;
            out[n].dir = 0;
            out[n].flags = NPCF_FIXED | NPCF_SIDE;
            n++;
        }
    }
    return n;
}

/* ---- debug ---------------------------------------------------------------------------- */
void story_side_debug_accept(int q)
{
    if (q < 0 || q >= NQ) return;
    on_accept(q);
}

void story_side_debug_finish(int q)
{
    if (q < 0 || q >= NQ) return;
    finish(q);
}
