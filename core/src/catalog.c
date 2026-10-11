/* catalog.c - every name table behind the creator, plus species defaults
 * (owner: foundation). Index order matches the enums in catalog.h. Names
 * that become resident names fit DOOMO_NAME_LEN - 1 (11) characters.
 *
 * First names:  0..159 cute (Hillvile)   160..255 spooky (Hellvile)
 * Last names:   0..79  cute              80..127  spooky
 * (resident.c picks from these ranges.)
 */
#include "doomo/catalog.h"
#include "doomo/palette.h"

const char *const doomo_species_names[SPECIES_COUNT] = {
    "Human", "Imp", "Cherub", "Ghoul", "Skeleton", "Slime", "Hellhound",
    "Cacoblob", "Batling", "Robot", "Plushie", "Ghost", "Mummy", "Mothkin",
    "Frogling", "Bunnykin", "Catfolk", "Foxkin", "Bearkin", "Pupkin",
    "Mousekin", "Sheepkin", "Raccoonkin", "Owlkin"
};

const char *const doomo_body_names[BODY_COUNT] = {
    "Average", "Round", "Lanky", "Stocky", "Tiny", "Hunched", "Buff",
    "Noodle", "Pear", "Top-Heavy"
};

const char *const doomo_head_names[HEAD_COUNT] = {
    "Round", "Oval", "Square", "Heart", "Triangle", "Pear", "Bean", "Skull",
    "Star", "Blob", "Diamond", "Mochi"
};

const char *const doomo_eye_names[EYE_COUNT] = {
    "Dot", "Round", "Anime", "Sleepy", "Angry", "Happy Arc", "Cat", "Goat",
    "Spiral", "Star", "Heart", "X", "Button", "Void", "Lashes", "Droopy",
    "Wide", "Squint", "Sparkle", "Demon Slit", "Cross", "Hollow", "Glow",
    "Compound"
};

const char *const doomo_brow_names[BROW_COUNT] = {
    "None", "Thin", "Thick", "Angry", "Worried", "Unibrow", "Arched",
    "Bushy", "Dots", "Scarred", "Flat", "Raised"
};

const char *const doomo_nose_names[NOSE_COUNT] = {
    "None", "Dot", "Button", "Pointy", "Round", "Piggy", "Skull", "Hook",
    "Wide", "Long", "Kitty", "Clown"
};

const char *const doomo_mouth_names[MOUTH_COUNT] = {
    "Smile", "Grin", "Flat", "Frown", "Open", "Kitty", "Teeth", "Shark",
    "Tongue", "Smirk", "Pout", "O", "Wavy", "Stitched", "Beak", "Fangy",
    "Drool", "Lipstick", "Big Grin", "Void"
};

const char *const doomo_ear_names[EARS_COUNT] = {
    "Normal", "None", "Pointy", "Elf", "Cat", "Dog", "Bat", "Fin", "Bunny",
    "Fox", "Bear", "Mouse", "Sheep", "Floppy Pup", "Owl Tufts", "Raccoon"
};

const char *const doomo_cheek_names[8] = {
    "Blush", "Freckles", "Scar", "Bandage", "Beauty Mark", "Tear Tattoo",
    "Dimples", "Sparkles"
};

const char *const doomo_hair_names[HAIR_COUNT] = {
    "Bald", "Buzz", "Short", "Spiky", "Mohawk", "Bob", "Long", "Ponytail",
    "Twintails", "Bun", "Afro", "Curly", "Mullet", "Emo Fringe",
    "Pompadour", "Braids", "Side Shave", "Wild", "Locs", "Pigtails", "Hime",
    "Wavy", "Bowl", "Liberty Spikes", "Slicked", "Messy", "Odango",
    "Drills", "Flame", "Tentacles", "Slime Drip", "Crown Braid"
};

const char *const doomo_fhair_names[FHAIR_COUNT] = {
    "None", "Stubble", "Mustache", "Goatee", "Full Beard", "Handlebar",
    "Mutton Chops", "Wizard"
};

const char *const doomo_horn_names[HORNS_COUNT] = {
    "None", "Nubs", "Ram", "Bull", "Devil", "Unicorn", "Antlers", "Goat",
    "Oni", "Spiral", "Broken", "Crown Spikes", "Antennae", "Tall Devil",
    "Many", "Halo Horns"
};

const char *const doomo_tail_names[TAIL_COUNT] = {
    "None", "Devil", "Cat", "Fox", "Lizard", "Bunny", "Spiked", "Tentacle",
    "Skeletal", "Flame", "Bear Nub", "Waggy Pup", "Mouse", "Sheep Puff",
    "Ringed Raccoon", "Feather Fan"
};

const char *const doomo_wing_names[WINGS_COUNT] = {
    "None", "Bat", "Angel", "Butterfly", "Bone", "Fly", "Dragon",
    "Tiny Bat", "Mech", "Flame"
};

const char *const doomo_halo_names[HALO_COUNT] = {
    "None", "Halo", "Broken Halo", "Flame Ring", "Stars", "Thorns"
};

const char *const doomo_mark_names[MARK_COUNT] = {
    "None", "Stripes", "Spots", "Tribal", "Skull Paint", "Cheek Hearts",
    "Stitches", "Cracks", "Star Freckles", "Clown", "Eyeliner", "Third Eye",
    "Pentagram", "Tears", "Patches", "Scales", "Fur Muzzle", "Fur Belly",
    "Fur Socks", "Fur Mask", "Tabby", "Calico", "Tipped Ears"
};

const char *const doomo_top_names[TOP_COUNT] = {
    "None", "T-Shirt", "Hoodie", "Tank Top", "Sweater", "Suit", "Dress",
    "Robe", "Armor", "Leather Jacket", "Overalls", "Kimono", "Lab Coat",
    "Crop Top", "Sweater Vest", "Cloak", "Band Tee", "Priest", "Nurse",
    "Maid", "Spacesuit", "Plate Armor", "Hazmat", "Pajamas"
};

const char *const doomo_bottom_names[BOTTOM_COUNT] = {
    "None", "Pants", "Shorts", "Skirt", "Long Skirt", "Ripped Jeans",
    "Cargo", "Leggings", "Kilt", "Tutu", "Sweats", "Robe"
};

const char *const doomo_shoes_names[SHOES_COUNT] = {
    "Barefoot", "Sneakers", "Boots", "Sandals", "Heels", "Clogs",
    "Combat Boots", "Slippers", "Hooves", "Rollerskates", "Platforms",
    "Claws"
};

const char *const doomo_hat_names[HAT_COUNT] = {
    "None", "Cap", "Beanie", "Top Hat", "Witch Hat", "Crown", "Propeller",
    "Cowboy", "Beret", "Marine Helmet", "Horned Helmet", "Bunny Ears",
    "Cat Ears", "Headphones", "Bandana", "Chef Hat", "Party Hat", "Nun Veil",
    "Fez", "Flower Crown", "Tiara", "Skull Cap", "Traffic Cone", "Bucket"
};

const char *const doomo_acc_names[ACC_COUNT] = {
    "None", "Glasses", "Sunglasses", "Monocle", "Eyepatch", "Mask",
    "Bowtie", "Cross Necklace", "Skull Necklace", "Scarf", "Bandages",
    "Earrings", "Nose Ring", "Pacifier", "Gas Mask", "Cape", "Backpack",
    "Rosary", "Chains", "Teddy"
};

const char *const doomo_aura_names[AURA_COUNT] = {
    "None", "Sparkles", "Flames", "Flies", "Hearts", "Smoke", "Bubbles",
    "Glitch"
};

const char *const doomo_pattern_names[PATTERN_COUNT] = {
    "Solid", "Stripes", "Polka Dots", "Plaid", "Flames", "Skulls", "Hearts",
    "Camo"
};

const char *const doomo_pronoun_names[PRONOUN_COUNT] = {
    "they/them", "she/her", "he/him", "it/its", "xe/xem"
};

const char *const doomo_romance_names[ROMANCE_COUNT] = {
    "Anyone", "She/her folks", "He/him folks", "They/them folks",
    "Not looking"
};

const char *const doomo_persona_axis_names[PERS_COUNT] = {
    "Movement", "Speech", "Expression", "Attitude", "Quirk"
};

/* index = (movement>=8)<<3 | (expression>=8)<<2 | (speech>=8)<<1 |
 *         (attitude>=8)   - see persona_type() */
const char *const doomo_persona_type_names[PERSONA_TYPE_COUNT] = {
    "Quiet Bookworm", "Cozy Napper", "Grumpy Gremlin", "Deadpan Drifter",
    "Gentle Dreamer", "Sweet Sunbeam", "Dramatic Poet", "Cheeky Charmer",
    "Diligent Helper", "Chill Adventurer", "Stern Captain", "Wild Card",
    "Eager Go-Getter", "Bubbly Sparkler", "Fiery Leader", "Chaos Goblin"
};

const char *const doomo_voice_names[VOICE_COUNT] = {
    "Pitch", "Speed", "Quality", "Intonation", "Accent"
};

/* The first 12 match the edible items (items.h) one to one. */
const char *const doomo_food_names[FOOD_COUNT] = {
    "Cupcakes", "Onigiri", "Soul Soup", "Brimstone Bagels",
    "Candy Eyeballs", "Pudding", "Hell Peppers", "Angel Cake", "Meat Slabs",
    "Salad", "Milk", "Coffee",
    "Pancakes", "Waffles", "Ramen", "Mochi", "Dumplings", "Pizza", "Tacos",
    "Sushi", "Curry", "Donuts", "Macarons", "Ice Cream", "Bubble Tea",
    "Honey Toast", "Melon Bread", "Takoyaki", "Omurice", "Cotton Candy",
    "Carrot Cake", "Popcorn", "Fried Chicken", "Spaghetti", "Cereal",
    "Gumdrops", "Lava Cake", "Bat Wing Chips", "Worm Gummies",
    "Black Licorice", "Burnt Toast", "Sulfur Soda", "Eyeball Jelly",
    "Grave Dirt Cake", "Blood Oranges", "Pickles", "Brussels Sprouts",
    "Bone Broth"
};

const char *const doomo_hobby_names[HOBBY_COUNT] = {
    "Baking", "Knitting", "Gardening", "Stargazing", "Skateboarding",
    "Video Games", "Painting", "Singing", "Dancing", "Reading", "Fishing",
    "Napping", "Cosplay", "Karaoke", "Board Games", "Bug Collecting",
    "Birdwatching", "Yoga", "Pottery", "Origami", "Plush Making",
    "Cloud Watching", "Bubble Blowing", "Poetry", "Tarot Reading",
    "Seances", "Ghost Hunting", "Metal Drumming", "Pranks", "Graffiti",
    "Welding", "Skull Collecting"
};

const char *const doomo_music_names[MUSIC_COUNT] = {
    "Bubblegum Pop", "Lo-fi Beats", "City Pop", "Chiptune", "Lullabies",
    "Jazz", "Classical", "Folk", "Disco", "Sea Shanties", "Hymns",
    "Synthwave", "Goth Rock", "Hardcore Punk", "Doom Metal", "Industrial"
};

const char *const doomo_fear_names[FEAR_COUNT] = {
    "Spiders", "The Dark", "Thunder", "Clowns", "Ghosts", "Heights",
    "Deep Water", "Vacuum Cleaners", "Bees", "Needles", "Mirrors",
    "Being Alone", "Public Speaking", "Bath Time", "Dentists",
    "Popping Balloons", "Silence", "Holy Water", "Sunlight", "Church Bells",
    "Exorcisms", "Hugs", "Cute Things", "Tax Season"
};

const char *const doomo_catchphrases[CATCHPHRASE_COUNT] = {
    /* sweet */
    "Snuggle up!", "Boop!", "Yippee!", "Oh my stars!", "Hug o'clock!",
    "Nyaa~", "Wiggle wiggle!", "Sweet as pie!", "Cozy vibes only!", "Blep!",
    "Hehehe~", "Toot toot!", "Paws up!", "Bun-believable!", "Sparkle on!",
    "Mochi mochi!", "So fluffy!", "Okie dokie!", "Tail wags!", "Purr-fect!",
    "Hoo hoo!", "Baa-rilliant!", "Squeak squeak!", "Fluff yeah!",
    "Let's be friends!", "Smooch!", "Yummy yummy!", "Teehee!",
    "Sunny side up!", "Oopsie daisy!", "Cuddle puddle!", "Bless your heart!",
    "Ribbit!", "Beep boop!", "Wheee!", "Nom nom nom!",
    /* sassy */
    "Whatever.", "As if!", "Ugh, fine.", "No thank you!", "Rude!",
    "Big mood.", "Yikes.", "Slay!",
    /* spooky */
    "Doom and gloom!", "Ugh, mortals.", "Feed me souls.", "Bleh.", "Boo!",
    "Mwahaha!", "Stay cursed.", "Gloom on!", "Sulfur kisses!",
    "Rattle rattle!", "Go to heck!", "Rip and tear... gently.", "Hail me!",
    "I bite. Cutely.", "The void says hi.", "Spooky time!",
    "Hiss!", "Dark and fluffy.", "Born to haunt.", "Hellooo!"
};

const char *const doomo_first_names[FIRST_NAME_COUNT] = {
    /* 0..159 cute */
    "Mochi", "Pudding", "Biscuit", "Clover", "Pip", "Bun", "Muffin",
    "Peaches", "Honey", "Maple", "Sprinkle", "Cocoa", "Toffee", "Waffles",
    "Bean", "Button", "Daisy", "Poppy", "Pebble", "Nugget", "Tofu",
    "Dumpling", "Bonbon", "Cupcake", "Marzipan", "Taffy", "Truffle",
    "Noodle", "Pickle", "Juniper", "Willow", "Hazel", "Fern", "Sunny",
    "Lulu", "Momo", "Kiki", "Coco", "Fifi", "Gigi", "Mimi", "Nana", "Lolo",
    "Pippa", "Posy", "Tilly", "Milo", "Otis", "Ziggy", "Bubbles",
    "Squeaky", "Fluffy", "Snuggles", "Wiggles", "Patches", "Pumpkin",
    "Marshmallow", "Bluebell", "Buttercup", "Sugarplum", "Gumdrop",
    "Jellybean", "Lollipop", "Cinnamon", "Nutmeg", "Ginger", "Pepper",
    "Sage", "Basil", "Olive", "Plum", "Cherry", "Apricot", "Kiwi", "Mango",
    "Lychee", "Yuzu", "Matcha", "Boba", "Sesame", "Azuki", "Kinako",
    "Sakura", "Hana", "Yuki", "Hoshi", "Sora", "Riri", "Nori", "Ume",
    "Fuzzy", "Puff", "Fizz", "Bloop", "Doodle", "Scoot", "Hops", "Thistle",
    "Bramble", "Acorn", "Chestnut", "Hickory", "Pinecone", "Tumble",
    "Pompom", "Tuft", "Whiskers", "Socks", "Mittens", "Nibbles", "Scamper",
    "Skipper", "Dandelion", "Marigold", "Primrose", "Lilac", "Rosie",
    "Opal", "Pearl", "Goldie", "Biscotti", "Brioche", "Crumpet", "Scone",
    "Eclair", "Macaron", "Churro", "Mocha", "Latte", "Chai", "Sorbet",
    "Gelato", "Custard", "Fudge", "Praline", "Nougat", "Meringue", "Sundae",
    "Pancake", "Flapjack", "Butterbean", "Chickpea", "Sprout", "Twig",
    "Puddle", "Drizzle", "Breeze", "Comet", "Twinkle", "Luna", "Nova",
    "Stella", "Cosmo", "Pixel", "Beep", "Boop", "Zippy", "Jojo",
    "Lala", "Wren",
    /* 160..255 spooky */
    "Grimsby", "Gloom", "Mortis", "Cinder", "Ash", "Soot", "Ember",
    "Brimley", "Hex", "Grub", "Gristle", "Bones", "Skully", "Crypt", "Fang",
    "Talon", "Thorn", "Lurk", "Sludge", "Gunk", "Blight", "Scorch",
    "Smolder", "Rasp", "Croak", "Wraith", "Shade", "Murk", "Dusk", "Raven",
    "Crow", "Nyx", "Lilith", "Damien", "Abaddon", "Asmo", "Belial",
    "Malachar", "Grimm", "Morbid", "Ghastly", "Ichor", "Bile", "Snarl",
    "Gnash", "Gnaw", "Chomp", "Gorgo", "Krampy", "Spite", "Grudge",
    "Malice", "Jinx", "Hexley", "Wormwood", "Nightshade", "Belladonna",
    "Hemlock", "Mandrake", "Wolfsbane", "Cobweb", "Spindle", "Widow",
    "Moth", "Scab", "Wart", "Pox", "Clank", "Sprocket", "Smog", "Fume",
    "Reek", "Igor", "Boris", "Vlad", "Morgana", "Salem", "Hellga", "Brim",
    "Flint", "Obsidian", "Onyx", "Sable", "Inky", "Pitch", "Gravel", "Grit",
    "Toxie", "Venom", "Viper", "Scorpia", "Spike", "Razor", "Grunt",
    "Grog", "Ugg"
};

const char *const doomo_last_names[LAST_NAME_COUNT] = {
    /* 0..79 cute */
    "Pudding", "Sprinkles", "Buttercream", "Honeydew", "Cuddlesworth",
    "Snugglebum", "Fluffington", "Bunbury", "Puffleton", "Mapleleaf",
    "Clovermead", "Applebottom", "Tealeaf", "Sugarbean", "Biscuitson",
    "Gumdrop", "Dewdrop", "Rainbow", "Sunbeam", "Moonbeam", "Starshine",
    "Pebbleton", "Bramblewood", "Willowby", "Hazelnut", "Acornfield",
    "Thistledown", "Featherby", "Whiskerton", "Pawsworth", "Mittens",
    "Tufts", "Bouncewell", "Wigglesby", "Giggles", "Chuckles",
    "Twinkletoe", "Bubbleton", "Fizzlewick", "Doodlebug", "Puddleby",
    "Cupcake", "Shortcake", "Crumble", "Toffeenose", "Jellyroll",
    "Creampuff", "Cocoabean", "Mochimochi", "Dumplings", "Sweetpea",
    "Snapdragon", "Bluebell", "Marigold", "Primrose", "Daisychain",
    "Lilypad", "Mosswood", "Fernsby", "Kettleby", "Teacup", "Saucer",
    "Pillowby", "Quilt", "Hugsworth", "Beanbag", "Gumball", "Lollipop",
    "Sugarplum", "Peppermint", "Spearmint", "Fudgeworth", "Honeycomb",
    "Sunnyside", "Meadowlark", "Brightwater", "Snickerdoo", "Marshmello",
    "Tumbleweed", "Bumblebee",
    /* 80..127 spooky */
    "Grimsby", "Gravesend", "Bonechill", "Cryptkeeper", "Ashgrave",
    "Hexwood", "Duskmoor", "Blackthorn", "Nightshade", "Gloomwood",
    "Mournington", "Ravenscroft", "Vilehollow", "Brimstone", "Sulfurwick",
    "Cindervale", "Coffinby", "Tombstone", "Wormwood", "Rotworth",
    "Bilewater", "Gutterson", "Smogmire", "Fumewick", "Vaporwell",
    "Hellbent", "Pitchfork", "Horns", "Fangsworth", "Clawhammer",
    "Spookington", "Creepwood", "Shadowmere", "Murkwater", "Sludgemire",
    "Grimtooth", "Snarlsby", "Gnashwell", "Spitefire", "Grudgebury",
    "Morbidia", "Deathridge", "Hauntley", "Wraithe", "Banshee", "Ghoulsby",
    "Batwing", "Cobwebb"
};

const char *const doomo_month_names[12] = {
    "January", "February", "March", "April", "May", "June", "July",
    "August", "September", "October", "November", "December"
};

static const char *const pron_subj[PRONOUN_COUNT] = { "they", "she", "he", "it", "xe" };
static const char *const pron_obj[PRONOUN_COUNT]  = { "them", "her", "him", "it", "xem" };
static const char *const pron_poss[PRONOUN_COUNT] = { "their", "her", "his", "its", "xyr" };

const char *pronoun_subj(int p)
{
    return (p >= 0 && p < PRONOUN_COUNT) ? pron_subj[p] : pron_subj[0];
}
const char *pronoun_obj(int p)
{
    return (p >= 0 && p < PRONOUN_COUNT) ? pron_obj[p] : pron_obj[0];
}
const char *pronoun_poss(int p)
{
    return (p >= 0 && p < PRONOUN_COUNT) ? pron_poss[p] : pron_poss[0];
}

/* skin: 0..7 natural (PAL_SKIN), 8..39 paint (value - 8). */
#define PSKIN(p) (8 + (p))
const DoomoSpeciesInfo doomo_species_info[SPECIES_COUNT] = {
    /* skin                 hair horn tail wing corr holy flags */
    { SKIN_FAIR,              95,   3,   2,   2,   5,  50, 0 },              /* HUMAN */
    { PSKIN(PAINT_CHERRY),    60,  95,  90,  30,  40,  15, 0 },              /* IMP */
    { SKIN_PEACH,             90,   0,   0,  95,   0,  95, SPF_FLOATS | SPF_HOLY }, /* CHERUB */
    { PSKIN(PAINT_MINT),      70,  10,   5,   5,  35,  20, SPF_UNDEAD },     /* GHOUL */
    { PSKIN(PAINT_BONE),      10,   5,   5,   5,  30,  25, SPF_UNDEAD | SPF_NO_SKIN_SHADE }, /* SKELETON */
    { PSKIN(PAINT_SLIME),     20,   5,   5,   5,  20,  30, SPF_SQUISHY },    /* SLIME */
    { PSKIN(PAINT_CHARCOAL),  50,  40,  95,   5,  45,  10, 0 },              /* HELLHOUND */
    { PSKIN(PAINT_CHERRY),     5,  60,  10,  10,  50,  10, SPF_FLOATS },     /* CACOBLOB */
    { PSKIN(PAINT_GRAPE),     60,  10,  10,  95,  30,  20, SPF_FLOATS },     /* BATLING */
    { PSKIN(PAINT_SILVER),    10,  60,   5,   5,  10,  40, SPF_MECHANICAL | SPF_NO_SKIN_SHADE }, /* ROBOT */
    { PSKIN(PAINT_BUBBLEGUM), 50,   5,  30,  10,   0,  80, SPF_SQUISHY | SPF_HOLY }, /* PLUSHIE */
    { PSKIN(PAINT_WHITE),     30,   2,   0,   5,  20,  40, SPF_FLOATS | SPF_UNDEAD }, /* GHOST */
    { PSKIN(PAINT_BONE),      30,   5,   5,   5,  25,  30, SPF_UNDEAD },     /* MUMMY */
    { PSKIN(PAINT_LAVENDER),  60,  90,   0,  95,  10,  50, SPF_FLOATS },     /* MOTHKIN */
    { PSKIN(PAINT_LIME),      30,   5,   5,   5,   5,  55, SPF_SQUISHY },    /* FROGLING */
    { PSKIN(PAINT_WHITE),     70,   2, 100,   2,   3,  65, 0 },              /* BUNNYKIN */
    { PSKIN(PAINT_CARAMEL),   70,   2, 100,   2,   8,  55, 0 },              /* CATFOLK */
    { PSKIN(PAINT_TANGERINE), 70,   2, 100,   2,  10,  50, 0 },              /* FOXKIN */
    { PSKIN(PAINT_CHOCOLATE), 50,   2, 100,   2,   5,  60, 0 },              /* BEARKIN */
    { PSKIN(PAINT_HONEY),     60,   2, 100,   2,   3,  65, 0 },              /* PUPKIN */
    { PSKIN(PAINT_SILVER),    60,   2, 100,   2,   5,  55, 0 },              /* MOUSEKIN */
    { PSKIN(PAINT_WHITE),     85,   2, 100,   2,   2,  75, 0 },              /* SHEEPKIN */
    { PSKIN(PAINT_SILVER),    60,   2, 100,   2,  15,  40, 0 },              /* RACCOONKIN */
    { PSKIN(PAINT_CARAMEL),   30,   2, 100,  40,   5,  60, 0 }               /* OWLKIN */
};
