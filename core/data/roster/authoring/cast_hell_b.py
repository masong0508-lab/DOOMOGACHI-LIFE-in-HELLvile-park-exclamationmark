"""Hellvile archetypes, part B: the spooky trades, kids and elders, night
life, crooks and edgy critter-folk. (owner: roster; see seed.py)"""
from seedlib import A

ARCHETYPES = [

A("mummies", "hell", "Tomb Realtor|Curse Notary|Bandage Merchant",
  sp="MUMMY:5",
  top="ROBE:2 NONE:2 SUIT", bottom="NONE:3 ROBE", shoes="SANDALS:3 BARE",
  hat="NONE:3 CROWN", acc="BANDAGES:5 SKULL_NECKLACE",
  pat="SOLID:4", scheme="HONEY/COBALT/BONE CARAMEL/TEAL/LEMON BONE/LEMON/COBALT",
  pers="0-4 5-11 1-6 2-9 4-11", act="HOME:3 SHOP", eyes="GLOW:2 HOLLOW:2 SLEEPY",
  food="Bone Broth|Grave Dirt Cake|Honey Toast", hobby="Napping:3|Board Games|Skull Collecting", music="Classical|Doom Metal",
  phrase="Bleh.|Ugh, mortals.|Stay cursed.",
  cast="""
Pharaohnie | s | Tomb realtor. "Cosy, dark, eternal. Must see." | job=Tomb Realtor; hat=CROWN
Unwrapt | h | Unravels when nervous. Is always nervous. |
Cursula | s | Curse notary. Stamps curses with a scarab. | job=Curse Notary
Wrappy | h | Sells bandages. Uses most of the stock himself. | job=Bandage Merchant
Sarcophagus | t | Naps in a box for 3,000 years at a time. |
""" ),

A("night_nurses", "hell", "Night Nurse|Leech Doctor|Stitcher",
  sp="GHOUL:3 BATLING IMP",
  top="NURSE:3 LAB_COAT", bottom="SKIRT:2 NONE", shoes="HEELS:2 CLOGS",
  hat="NONE:3", acc="MASK:2 NONE",
  pat="SOLID:4", scheme="WHITE/BLOOD/JET BONE/SLIME/JET WHITE/GRAPE/BLOOD",
  mark="STITCHES:3 NONE:2", pers="1-7 6-12 1-7 2-9 8-15", act="HOSPITAL:4", traits="NIGHT_OWL",
  food="Bone Broth|Blood Oranges|Eyeball Jelly", hobby="Bug Collecting|Plush Making|Reading", music="Lullabies|Goth Rock",
  phrase="Stay cursed.|I bite. Cutely.|Bleh.",
  cast="""
Nurse Leech | s | Prescribes leeches for everything. Even sadness. | job=Leech Doctor
Stitchella | s | Stitcher. Sews on lost limbs. Backwards, sometimes. | job=Stitcher
Midnyx | t | Takes your pulse while you sleep. Unsettling. |
Sutura | x | Stitches up the plushies of crying kids. Free. | trait=SOFTIE; species=BATLING
""" ),

A("mad_science", "hell", "Mad Scientist|Gene Splicer|Lab Assistant|Death Ray Tech",
  sp="GHOUL:2 IMP HUMAN ROBOT MOUSEKIN",
  fur_MOUSEKIN="CHARCOAL/SILVER WHITE/BUBBLEGUM",
  top="LAB_COAT:5", bottom="PANTS:2 CARGO", shoes="BOOTS:2 COMBAT",
  hat="NONE:3", acc="GLASSES:2 GAS_MASK:2",
  pat="SOLID:4", scheme="WHITE/LIME/JET WHITE/MAGENTA/JET WHITE/CYAN/BLOOD",
  eyes="SPIRAL:3 WIDE:2 GLOW", hair="WILD:4 MESSY:2", aura="GLITCH:2 NONE:2 SMOKE",
  pers="9-15 7-13 10-15 3-10 13-15", act="WORKSHOP:3 HOME",
  food="Coffee:3|Eyeball Jelly|Sulfur Soda", hobby="Bug Collecting|Welding|Video Games", music="Industrial:3|Synthwave",
  phrase="Mwahaha!|Yikes.|The void says hi.",
  cast="""
Dr. Frankly | h | Builds monsters. They all want to be florists. |
Splicey | s | Gene splicer. Made a frog with a moustache. | job=Gene Splicer
Igorina | t | Lab assistant. Pulls the big lever on cue. | job=Lab Assistant; body=HUNCHED
Zappo | i | Death ray tech bot. The ray makes popcorn. | job=Death Ray Tech; species=ROBOT
Prof. Ick | h | Studies slime. Is slowly becoming slime. | species=GHOUL
Mousetrap | t | Builds mazes for mice. The mice win. | species=MOUSEKIN
""" ),

A("sludge_plumbers", "hell", "Sludge Plumber|Drain Diver|Pipe Fitter",
  sp="SLIME:4 FROGLING",
  skin="SLIME LIME CHARTREUSE MAGENTA", skin_FROGLING="FOREST TEAL CHARTREUSE",
  top="OVERALLS:4 NONE", bottom="NONE:3", shoes="BOOTS:3 BARE",
  hat="CAP:3 NONE", acc="NONE:3 GAS_MASK",
  pat="SOLID:3", scheme="CHARCOAL/LIME/LEMON NAVY/SLIME/WHITE RUST/LIME/BONE",
  pers="3-9 6-12 3-9 6-13 6-13", act="WANDERING:3", aura="FLIES:2 NONE:3",
  stats="hygiene=20-50", food="Worm Gummies|Pickles|Bone Broth", hobby="Napping|Fishing", music="Sea Shanties|Industrial",
  phrase="Bleh.|Big mood.|Ribbit!",
  cast="""
Plungerella | s | Unclogs drains by climbing into them. |
Drainy | h | Drain diver. Found a whole sofa once. | job=Drain Diver
Gunkface | t | Pipe fitter. Fits into the pipes, mostly. | job=Pipe Fitter
Sludgy | s | Leaves slime trails. Calls them signatures. |
Bog Bert | h | Swamp toad plumber. Croaks at clogs. | species=FROGLING
""" ),

A("slime_gang", "hell", "Slime Gangster|Goo Enforcer|Jelly Boss",
  sp="SLIME:5",
  skin="SLIME MAGENTA LIME CYAN GRAPE",
  top="LEATHER_JACKET:3 SUIT TANK", bottom="NONE:3 PANTS", shoes="BARE",
  hat="CAP:2 TOP_HAT NONE:2", acc="SUNGLASSES:3 CHAINS:2",
  pat="SOLID:3 FLAMES", scheme="JET/SLIME/SILVER CHARCOAL/MAGENTA/LEMON JET/LIME/WHITE",
  pers="5-11 10-15 4-10 4-11 6-13", act="WANDERING:4",
  food="Eyeball Jelly:2|Gumdrops|Lava Cake", hobby="Pranks|Board Games", music="Hardcore Punk|Jazz|Synthwave",
  phrase="Go to heck!|Mwahaha!|Big mood.",
  cast="""
Jellybone | h | Jelly boss. Quivers menacingly. Mostly quivers. | job=Jelly Boss
Oozo | t | Goo enforcer. Engulfs problems. And snacks. | job=Goo Enforcer
Splatz | s | Leaves sticky handprints as a calling card. |
Gelatina | s | Secretly runs a jelly bakery for orphan blobs. | trait=SOFTIE
Glorp | h | Says only "glorp". Everyone understands. |
""" ),

A("burnt_bakers", "hell", "Burnt Toast Baker|Charcoal Pastry Chef|Oven Imp",
  sp="IMP:2 HELLHOUND BEARKIN SHEEPKIN",
  fur_BEARKIN="CHOCOLATE/CARAMEL JET/WHITE", fur_SHEEPKIN="CHARCOAL/WHITE BONE/CHOCOLATE",
  top="OVERALLS:3 TSHIRT:2", bottom="PANTS:2 SKIRT", shoes="CLOGS:3 BOOTS",
  hat="CHEF:5", acc="NONE:3 SCARF",
  pat="SOLID:2 PLAID:2", scheme="CHARCOAL/PEACH/WHITE CHOCOLATE/HELLFIRE/BONE JET/ROSE/WHITE",
  pers="4-10 3-9 8-14 7-14 4-11", act="CAFE:4", cheeks="BLUSH:70 FRECKLES:30",
  food="Burnt Toast:4|Lava Cake|Brimstone Bagels|Cupcakes", hobby="Baking:6", music="Folk|Lullabies|Goth Rock",
  phrase="Sweet as pie!|Bleh.|Dark and fluffy.|Yummy yummy!",
  cast="""
Crispin | h | Burns all his bread. Gives it away anyway. | trait=SOFTIE; bff=Nana Puddin
Charlotte | s | Bakes cookies that are 90% charcoal. Loves you. | job=Charcoal Pastry Chef; trait=SOFTIE
Toastie | t | Oven imp. Hugs the oven to keep it warm. | job=Oven Imp; trait=SOFTIE
Scorchmello | h | Toasts marshmallows to black. Shares them all. | species=SHEEPKIN; trait=SOFTIE
Ashcake | s | Wants to bake for Hillvile. Practises nightly. | species=BEARKIN; trait=SOFTIE; crush=Crumpet
""" ),

A("hex_witches", "hell", "Hex Witch|Potion Brewer|Broom Mechanic|Toad Keeper",
  sp="GHOUL:2 BATLING:2 HUMAN IMP",
  top="DRESS:3 ROBE:2 CLOAK", bottom="LONG_SKIRT:3 SKIRT", shoes="BOOTS:3 HEELS",
  hat="WITCH:6", acc="NONE:2 SKULL_NECKLACE EARRINGS",
  pat="STRIPES:2 SOLID:2 POLKA", scheme="GRAPE/SLIME/JET JET/HELLFIRE/LIME FOREST/MAGENTA/JET",
  nose="LONG:3 HOOK:3 POINTY", mark="NONE:3 SPOTS EYELINER",
  pers="4-10 6-13 7-14 4-11 9-15", act="HOME:3 SHOP",
  food="Soul Soup|Worm Gummies|Eyeball Jelly|Black Licorice", hobby="Tarot Reading|Gardening|Seances", music="Folk|Goth Rock",
  phrase="Mwahaha!|Stay cursed.|Spooky time!",
  cast="""
Hexagonia | s | Casts hexes in perfect six-sided shapes. |
Cauldra | s | Potion brewer. Her potions all taste of pear. | job=Potion Brewer
Broomella | t | Broom mechanic. Tunes brooms for speed. | job=Broom Mechanic
Wartsworth | h | Toad keeper. Kisses toads. Hoping. | job=Toad Keeper
Newtlie | h | Turns people into newts, then back, if asked. |
Grizelda | s | Brews soup for sick neighbours. Calls it a hex. | trait=SOFTIE; species=HUMAN; skin=SKIN_OLIVE
""" ),

A("fake_psychics", "hell", "Fake Psychic|Spirit Board Seller|Curse Remover",
  sp="MOTHKIN:2 IMP GHOST",
  skin_MOTHKIN="GRAPE CHARCOAL LAVENDER",
  top="ROBE:2 DRESS CLOAK", bottom="LONG_SKIRT:2", shoes="SANDALS:2",
  hat="BANDANA:2 WITCH", acc="EARRINGS:3",
  pat="SOLID:2 STRIPES", scheme="GRAPE/LEMON/MAGENTA JET/MAGENTA/LEMON NAVY/LEMON/GRAPE",
  pers="4-10 5-11 9-15 5-12 8-15", act="SHOP:3", mark="THIRD_EYE:3 NONE",
  food="Candy Eyeballs|Bubble Tea|Pudding", hobby="Tarot Reading:4|Seances", music="Synthwave|Lo-fi Beats",
  phrase="The void says hi.|Oh my stars!|Spooky time!",
  cast="""
Mme. Fraud | s | Sees your future. Charges extra if it is good. |
Planchette | t | Sells spirit boards that only ever spell "lol". | job=Spirit Board Seller
Curselift | h | Curse remover. Removes curses he put on you. | job=Curse Remover
Phantasma | t | Pretends to be a ghost. Is, in fact, a ghost. | species=GHOST
""" ),

A("grave_diggers", "hell", "Grave Digger|Tombstone Carver|Crypt Sweeper",
  sp="SKELETON:2 GHOUL:2 HELLHOUND",
  top="OVERALLS:3 SWEATER", bottom="NONE:2 CARGO", shoes="BOOTS:4",
  hat="BEANIE:2 BUCKET NONE", acc="NONE:3 SCARF",
  pat="SOLID:3 PLAID", scheme="CHARCOAL/BONE/FOREST JET/RUST/BONE FOREST/CHARCOAL/HONEY",
  pers="1-6 3-9 1-6 5-12 3-10", act="WANDERING:3", traits="NIGHT_OWL",
  food="Grave Dirt Cake:3|Bone Broth", hobby="Gardening|Skull Collecting|Napping", music="Folk|Doom Metal",
  phrase="Doom and gloom!|Rattle rattle!|Bleh.",
  cast="""
Shovelle | s | Digs graves. Plants daisies on every single one. | trait=SOFTIE
Tombstoner | h | Carves tombstones with jokes on them. | job=Tombstone Carver
Diggsby | h | Digs holes for fun. Has reached the hell gate. |
Sweepy | t | Crypt sweeper. The dust bunnies are friends. | job=Crypt Sweeper
Plotsworth | x | Plans the cemetery layout like a garden. | species=HELLHOUND
""" ),

A("undertakers", "hell", "Undertaker|Coffin Maker|Eulogist",
  sp="GHOUL:2 SKELETON HUMAN",
  top="SUIT:5", bottom="PANTS:4", shoes="BOOTS:2 HEELS",
  hat="TOP_HAT:4", acc="NONE:2 BOWTIE ROSARY",
  pat="SOLID:4 STRIPES", scheme="JET/WHITE/GRAPE JET/BLOOD/BONE CHARCOAL/LAVENDER/WHITE",
  pers="0-4 2-7 0-4 2-8 3-10", act="CHURCH:2 HOME",
  food="Black Licorice|Bone Broth|Coffee", hobby="Reading|Poetry|Skull Collecting", music="Classical:3|Hymns",
  phrase="Doom and gloom!|Bleh.|Gloom on!",
  cast="""
Mortimer | h | Undertaker. Measures everyone, just in case. |
Coffinella | s | Coffin maker. Lines every coffin with velvet. | job=Coffin Maker
Eulogia | t | Eulogist. Makes every funeral sound like a party. | job=Eulogist
Mr. Solemn | h | Has not smiled in 200 years. Close, once. | species=HUMAN; skin=SKIN_PORCELAIN; mouth=FLAT
""" ),

A("scare_actors", "hell", "Jump Scarer|Haunted House Actor|Fog Machine Op",
  sp="GHOST:3 GHOUL SKELETON",
  skin_GHOST="WHITE ECTOPLASM GRAPE",
  top="NONE:2 CLOAK ROBE", bottom="NONE:4", shoes="BARE",
  hat="NONE:4", acc="NONE:2 MASK CHAINS",
  pat="SOLID:4", scheme="WHITE/JET/ECTOPLASM GRAPE/WHITE/SLIME JET/ECTOPLASM/WHITE",
  pers="11-15 7-13 13-15 6-13 11-15", act="WANDERING:3", traits="NIGHT_OWL",
  eyes="VOID:3 WIDE:2 HOLLOW", mouth="O:3 OPEN:2 VOID",
  food="Candy Eyeballs|Popcorn|Eyeball Jelly", hobby="Pranks:3|Ghost Hunting|Cosplay", music="Goth Rock|Industrial",
  phrase="Boo!|Spooky time!|Born to haunt.|Hellooo!",
  cast="""
Boolander | h | Jumps out of cupboards. Then says sorry. | job=Jump Scarer
Shriekie | s | Screams for a living. Very professional. |
Fogwell | t | Fog machine op. Fogs the whole street. | job=Fog Machine Op
Creakster | h | Makes floors creak on cue. A true artist. | species=GHOUL
Rattleby | s | Haunted house actor. Rattles chains, very tenderly. | species=SKELETON
""" ),

A("poltergeists", "hell", "Poltergeist|Door Slammer|Plate Smasher",
  sp="GHOST:5",
  skin="LAVENDER ECTOPLASM WHITE GRAPE",
  top="NONE:4", bottom="NONE:4", shoes="BARE",
  hat="NONE:4", acc="NONE:3 CHAINS",
  pat="SOLID:4", scheme="GRAPE/ECTOPLASM/WHITE JET/LAVENDER/SLIME ECTOPLASM/JET/WHITE",
  aura="GLITCH:3 SMOKE NONE", eyes="VOID:3 GLOW HOLLOW", mouth="VOID:3 O GRIN",
  pers="13-15 12-15 10-15 6-13 13-15", act="WANDERING:4", traits="NIGHT_OWL",
  food="Popcorn|Candy Eyeballs|Sulfur Soda", hobby="Pranks:5|Ghost Hunting", music="Industrial|Hardcore Punk",
  phrase="Boo!|Born to haunt.|The void says hi.",
  cast="""
Slamdoor | h | Slams doors. Has never once closed one gently. | job=Door Slammer
Smashley | s | Breaks plates. Glues them back. Breaks them again. | job=Plate Smasher
Flickers | t | Turns lights on and off. And on. And off. |
Wobblewisp | h | Moves your keys two inches to the left. |
Rattlecup | s | Rattles teacups at midnight. Leaves biscuits. | trait=SOFTIE
""" ),

A("soul_brokers", "hell", "Soul Broker|Debt Imp|Bargain Demon",
  sp="IMP:3 CACOBLOB",
  top="SUIT:4", bottom="PANTS:3", shoes="BOOTS:2 HEELS",
  hat="TOP_HAT:2 NONE:2", acc="MONOCLE:2 SUNGLASSES",
  pat="STRIPES:3 SOLID", scheme="BLOOD/LEMON/JET JET/LEMON/BLOOD GRAPE/LEMON/BLOOD",
  pers="6-12 5-11 9-15 3-9 7-14", act="SHOP:2 WANDERING:2",
  food="Soul Soup:3|Lava Cake", hobby="Board Games:3|Tarot Reading", music="Jazz:3|Classical",
  phrase="Feed me souls.|Hail me!|Sulfur kisses!",
  cast="""
Soulsworth | h | Trades souls for snacks. Bad exchange rate. |
Bargainia | s | "Sign here for eternal glory." It is a coupon. | job=Bargain Demon
Dealbreaker | t | Breaks every deal. Twice. | species=CACOBLOB
Pennysoul | t | Buys souls, keeps them in jars, waters them. | trait=SOFTIE
""" ),

A("hellpups", "hell", "Hellpup|Puppy Student|Fetch Champion",
  sp="HELLHOUND:5 PUPKIN",
  fur_PUPKIN="JET/CARAMEL CHOCOLATE/HONEY",
  traits="KID", body="TINY:5 ROUND:2", height="0-3",
  top="TSHIRT:2 HOODIE:2 BAND_TEE", bottom="SHORTS:3", shoes="SNEAKERS:2 BARE CLAWS",
  hat="NONE:4 CAP", acc="TEDDY:2 PACIFIER:2 NONE:2",
  pat="FLAMES:2 SOLID:2 SKULLS", scheme="CHARCOAL/HELLFIRE/LEMON JET/CHERRY/WHITE RUST/LIME/LEMON",
  pers="12-15 8-14 12-15 8-15 10-15", act="PARK:3 HOME", tail="PUP_WAGGY:3 FLAME",
  food="Meat Slabs|Bone Broth|Popcorn|Hell Peppers", hobby="Pranks|Skateboarding|Video Games|Bug Collecting",
  music="Hardcore Punk|Chiptune|Doom Metal", phrase="I bite. Cutely.|Hiss!|Wheee!|Tail wags!",
  fear="Bath Time|Vacuum Cleaners|Thunder",
  cast="""
Nibbler | h | Chews shoes. Only left ones. | fam=growl
Sparky Jr | h | Breathes tiny flames when he hiccups. Many hiccups. | fam=growl
Puplet | s | Wags so hard her whole body wags. |
Grrrtrude | s | Growls at the mailman. Then licks him. | species=PUPKIN; bio_pronoun_ok=1
Cindy Chew | t | Fetch champion. Fetches things nobody threw. | job=Fetch Champion
Biscuitfang | h | Bites, then cries because he bit. | trait=SOFTIE
""" ),

A("imp_brats", "hell", "Imp Brat|Gremlin Student|Troublemaker",
  sp="IMP:6",
  traits="KID", body="TINY:4 ROUND:2", height="0-3",
  top="TSHIRT:2 HOODIE OVERALLS PAJAMAS", bottom="SHORTS:3 NONE", shoes="SNEAKERS HOOVES:2 BARE",
  hat="PROPELLER:2 NONE:3 BEANIE", acc="NONE:3 PACIFIER BACKPACK",
  pat="STRIPES:2 FLAMES SOLID", scheme="CHERRY/LEMON/WHITE MAGENTA/LIME/WHITE TANGERINE/JET/LEMON",
  pers="13-15 9-15 13-15 8-15 12-15", act="PARK:3", eyes="WIDE:2 ANGRY:2 SPARKLE",
  food="Candy Eyeballs:2|Gumdrops|Worm Gummies|Pizza", hobby="Pranks:4|Video Games|Graffiti", music="Hardcore Punk|Chiptune|Bubblegum Pop",
  phrase="Mwahaha!|Hehehe~|Go to heck!", fear="Bath Time|Dentists|Hugs",
  cast="""
Impy | h | Draws moustaches on every poster in town. | fam=impish
Gremlina | s | Puts glitter in the vents. Every vent. | fam=impish
Snitchet | t | Tattles on everyone, including themself. |
Firecracker | h | Lights firecrackers. Mostly sparklers. |
Tantrum | x | Throws tantrums. Very small ones. |
Pokefork | s | Pokes people with a foam pitchfork, then giggles. |
""" ),

A("crypt_grannies", "hell", "Crypt Granny|Curse Knitter|Bone Soup Cook",
  sp="GHOUL:2 SKELETON MUMMY BATLING",
  traits="ELDER", body="HUNCHED:3 TINY ROUND", height="2-6",
  top="SWEATER:3 DRESS CLOAK", bottom="LONG_SKIRT:4", shoes="SLIPPERS:3 BOOTS",
  hat="NONE:3 WITCH", acc="GLASSES:3 SKULL_NECKLACE",
  pat="PLAID:2 SOLID:2 SKULLS", scheme="GRAPE/BONE/WHITE CHARCOAL/ROSE/WHITE FOREST/LAVENDER/BONE",
  hair="BUN:4 CURLY", hair_col="WHITE:3 SILVER:2 GRAPE", hair_force="1",
  pers="0-5 8-15 4-11 4-11 6-13", act="HOME:4", eyes="SLEEPY:3 SQUINT:2 HOLLOW",
  food="Bone Broth:3|Grave Dirt Cake|Soul Soup", hobby="Knitting:4|Seances", music="Classical|Doom Metal|Hymns",
  phrase="Stay cursed.|Ugh, mortals.|Bless your heart!",
  cast="""
Granny Grit | s | Knits curses into scarves. They itch forever. | job=Curse Knitter
Mother Rot | s | Cooks bone soup for every new ghost in town. | job=Bone Soup Cook; trait=SOFTIE
Nana Necro | s | Raises the dead. Mostly for bingo night. |
Grandmummy | s | Wraps presents in her own bandages. | species=MUMMY
Batty Betty | s | Hangs upside down to nap. Calls it yoga. | species=BATLING
""" ),

A("fossil_elders", "hell", "Fossil Elder|Bone Historian|Retired Reaper",
  sp="SKELETON:5",
  traits="ELDER", body="HUNCHED:3 LANKY", height="6-14",
  top="CLOAK:3 ROBE SWEATER", bottom="ROBE:2 PANTS", shoes="SANDALS:2 SLIPPERS",
  hat="NONE:3", acc="NONE:2 MONOCLE",
  pat="SOLID:4", scheme="JET/BONE/SILVER CHARCOAL/BONE/LEMON NAVY/BONE/WHITE",
  fhair="WIZARD:2 NONE:3", pers="0-3 4-10 0-5 4-11 3-10", act="HOME:4",
  food="Bone Broth:4|Milk", hobby="Napping:3|Reading|Skull Collecting", music="Classical:3|Hymns",
  phrase="Rattle rattle!|Doom and gloom!|Bleh.",
  cast="""
Old Bones | h | Has been dead so long he forgot why. |
Grimgramps | h | Retired reaper. Still carries the scythe for walks. | job=Retired Reaper; facial_hair=WIZARD
Ossia | s | Bone historian. Can date any bone by licking it. | job=Bone Historian
Rickets | h | Creaks when he walks. Uses it as music. |
Marrowmere | t | Sits on the same bench for a century. Waves. | trait=SOFTIE
""" ),

A("doom_choir", "hell", "Doom Choir Singer|Organ Grinder|Bell Tolling",
  sp="GHOUL:2 SKELETON IMP BATLING",
  top="ROBE:4 CLOAK", bottom="ROBE:3", shoes="SANDALS:2 BOOTS",
  hat="NUN_VEIL:2 NONE:3", acc="ROSARY:2 SKULL_NECKLACE:2",
  pat="SOLID:4", scheme="JET/BLOOD/BONE BLOOD/JET/LEMON GRAPE/JET/BONE",
  pers="2-8 4-10 4-11 4-11 5-12", act="CHURCH:3",
  food="Soul Soup|Black Licorice|Bone Broth", hobby="Singing:5", music="Hymns:3|Doom Metal:2",
  phrase="Doom and gloom!|Gloom on!|Hail me!",
  cast="""
Requiem | s | Sings so low the floor shakes. Lovely tone. |
Sr. Sorrow | s | Hums dirges at weddings. Gets invited anyway. | hat=NUN_VEIL
Bellbane | h | Tolls the bell for every cancelled bus. | job=Bell Tolling
Grindorgan | t | Organ grinder. Plays polkas in minor key. | job=Organ Grinder
Dirgelet | s | Small choir imp. Big lungs. Bigger feelings. | species=IMP; trait=SOFTIE
""" ),

A("bat_couriers", "hell", "Bat Courier|Night Mail|Message Bat",
  sp="BATLING:5",
  top="HOODIE:2 TSHIRT LEATHER_JACKET", bottom="SHORTS:2 CARGO", shoes="SNEAKERS:2 BARE",
  hat="CAP:3 NONE", acc="BACKPACK:5",
  pat="SOLID:3 STRIPES", scheme="JET/LIME/WHITE GRAPE/LEMON/WHITE CHARCOAL/CYAN/WHITE",
  pers="12-15 6-12 6-12 6-13 6-13", act="WANDERING:5", traits="NIGHT_OWL",
  food="Bat Wing Chips|Blood Oranges|Popcorn", hobby="Cloud Watching|Pranks", music="Synthwave|Chiptune",
  phrase="Hellooo!|Big mood.|Wheee!",
  cast="""
Swoopsy | s | Delivers letters by dropping them on heads. |
Flapjack | h | Night mail. Flies upside down to read addresses. | job=Night Mail
Sonar | t | Finds any address by shouting at it. |
Parcelbat | h | Carries parcels twice its size. Proudly. | job=Message Bat
Wingnut | h | Loops through the clock tower every delivery. |
""" ),

A("hell_bikers", "hell", "Hell Biker|Chopper Mechanic|Road Captain",
  sp="HELLHOUND:2 SKELETON IMP GHOUL BEARKIN",
  fur_BEARKIN="JET/WHITE CHOCOLATE/CARAMEL",
  top="LEATHER_JACKET:5 TANK", bottom="RIPPED_JEANS:3 PANTS", shoes="BOOTS:3 COMBAT",
  hat="BANDANA:3 SKULL_CAP NONE", acc="SUNGLASSES:3 CHAINS:2",
  pat="FLAMES:3 SOLID:2", scheme="JET/HELLFIRE/SILVER CHARCOAL/BLOOD/BONE JET/RUST/LEMON",
  fhair="FULL_BEARD:2 CHOPS:2 GOATEE NONE:2", pers="6-12 9-15 3-9 4-11 6-13", act="WANDERING:4",
  aura="FLAMES:2 NONE:3", food="Meat Slabs:3|Hell Peppers|Coffee", hobby="Welding:3|Metal Drumming", music="Doom Metal:3|Hardcore Punk",
  phrase="Rip and tear... gently.|Go to heck!|Hail me!",
  cast="""
Throttle | h | Revs his engine to say hello and goodbye. |
Chopperella | s | Mechanic. Fixes bikes and broken hearts. | job=Chopper Mechanic; trait=SOFTIE
Gravelrash | t | Has fallen off every bike in Hellvile. |
Skidmark | h | Road captain. Leads the ride. Gets lost. | job=Road Captain
Exhaustia | s | Smells like oil and roses. Mostly oil. |
Grizzlebolt | h | Bear biker with a sidecar for his teddy. | species=BEARKIN
""" ),

A("sulfur_pirates", "hell", "Sulfur Pirate|Lava Boat Captain|Deckhand|Parrot Keeper",
  sp="IMP:2 SKELETON:2 FOXKIN RACCOONKIN",
  fur_FOXKIN="RUST/BONE CHARCOAL/WHITE",
  top="LEATHER_JACKET:2 TSHIRT:2 SUIT", bottom="PANTS:2 SHORTS RIPPED_JEANS", shoes="BOOTS:4",
  hat="BANDANA:3 TOP_HAT", acc="EYEPATCH:5 EARRINGS",
  pat="STRIPES:4 SOLID", scheme="BLOOD/BONE/LEMON NAVY/BLOOD/BONE JET/LEMON/WHITE",
  fhair="FULL_BEARD:2 GOATEE MUSTACHE NONE:2", pers="8-14 9-15 10-15 6-13 8-15", act="BEACH:3 WANDERING",
  food="Meat Slabs|Blood Oranges|Sulfur Soda", hobby="Fishing:3|Singing", music="Sea Shanties:6",
  phrase="Mwahaha!|Hail me!|Big mood.",
  cast="""
Capt. Hooky | h | Has a hook. Uses it to hang up coats. | job=Lava Boat Captain
Scallywag | s | Steals treasure. Mostly bottle caps. |
Bonebeard | h | Skeleton pirate with a beard of seaweed. | species=SKELETON; facial_hair=FULL_BEARD
Swabbie | t | Deckhand. Mops the deck in lava. | job=Deckhand
Polly Gone | s | Parrot keeper. The parrot only says "doom". | job=Parrot Keeper
Barnaby | x | Sings shanties so sad the fish weep. | trait=SOFTIE; species=RACCOONKIN
""" ),

A("shadow_ninjas", "hell", "Shadow Ninja|Smoke Bomber|Night Thief",
  sp="BATLING:2 CATFOLK IMP",
  fur_CATFOLK="JET/WHITE CHARCOAL/WHITE",
  top="HOODIE:2 KIMONO:2", bottom="PANTS:3", shoes="BARE:2 SNEAKERS",
  hat="BANDANA:3 NONE", acc="MASK:5",
  pat="SOLID:4", scheme="JET/CHARCOAL/CHERRY CHARCOAL/JET/SILVER NAVY/JET/WHITE",
  pers="14-15 3-9 0-4 3-10 6-13", act="WANDERING:4", traits="NIGHT_OWL", aura="SMOKE:2 NONE:3",
  food="Sushi|Onigiri|Bat Wing Chips", hobby="Origami|Pranks", music="Synthwave|Industrial",
  phrase="Hiss!|Stay cursed.|Bleh.",
  cast="""
Shadowstep | t | Appears behind you. Always says "boo", softly. |
Smokeblind | h | Throws smoke bombs, then coughs a lot. | job=Smoke Bomber
Hushpaw | s | Sneaks into houses to leave anonymous gifts. | species=CATFOLK; trait=SOFTIE
Ninjette | t | Can vanish. Cannot reappear on time. | job=Night Thief
""" ),

A("dice_fiends", "hell", "Dice Fiend|Slot Imp|Croupier|Card Dealer",
  sp="IMP:2 CACOBLOB GHOUL FOXKIN",
  fur_FOXKIN="RUST/BONE",
  top="SUIT:3 SWEATER_VEST:2", bottom="PANTS:3", shoes="BOOTS:2 HEELS",
  hat="NONE:3 TOP_HAT", acc="BOWTIE:3 SUNGLASSES",
  pat="SOLID:3 STRIPES", scheme="FOREST/LEMON/BLOOD JET/BLOOD/LEMON BLOOD/FOREST/WHITE",
  pers="7-13 6-12 6-13 5-12 9-15", act="SHOP:3 WANDERING",
  food="Coffee|Popcorn|Lava Cake", hobby="Board Games:5", music="Jazz:3|Disco",
  phrase="Mwahaha!|Big mood.|Hail me!",
  cast="""
Boxcars | h | Rolls double sixes every time. Every single time. |
Jackpotty | s | Slot imp. Pulls the lever for luck. Loses. | job=Slot Imp
Croupette | s | Spins the roulette wheel with her tail. | job=Croupier
Deuces | s | Deals cards faster than the eye. Drops them. | job=Card Dealer; species=FOXKIN
Bluffalo | h | Bluffs with a huge grin. Everyone knows. | species=CACOBLOB
""" ),

A("arcade_goblins", "hell", "Arcade Goblin|High Score Hoarder|Claw Machine Champ",
  sp="IMP:2 RACCOONKIN GHOUL BATLING",
  fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL",
  top="HOODIE:3 BAND_TEE:2", bottom="SWEATS:2 SHORTS", shoes="SNEAKERS:4",
  hat="HEADPHONES:2 CAP:2 NONE", acc="NONE:3 GLASSES",
  pat="SOLID:2 STRIPES", scheme="JET/CYAN/MAGENTA CHARCOAL/LIME/MAGENTA NAVY/LEMON/CYAN",
  pers="8-14 8-14 6-13 6-13 8-15", act="SHOP:2 HOME:2", traits="NIGHT_OWL",
  food="Pizza:3|Sulfur Soda|Popcorn", hobby="Video Games:6", music="Chiptune:5|Synthwave",
  phrase="Big mood.|Mwahaha!|Yikes.",
  cast="""
Highscore | h | Owns every high score. Initials always "BUM". | job=High Score Hoarder
Clawdia | s | Claw machine champ. Wins every plush. Keeps them. | job=Claw Machine Champ; trait=SOFTIE
Buttonmash | t | Button masher. Has broken 30 joysticks. |
Pixelfang | h | Never blinks while gaming. Eyes are square now. |
Tokenrat | h | Hoards tokens in his cheeks. | species=RACCOONKIN
""" ),

A("glitch_hackers", "hell", "Glitch Hacker|Virus Writer|Firewall Imp",
  sp="IMP ROBOT:2 GHOUL HUMAN",
  top="HOODIE:5", bottom="SWEATS:2 CARGO", shoes="SNEAKERS:3 SLIPPERS",
  hat="NONE:3 BEANIE", acc="GLASSES:3 NONE",
  pat="SOLID:4", scheme="JET/LIME/CYAN CHARCOAL/SLIME/WHITE NAVY/CYAN/LIME",
  aura="GLITCH:5 NONE", eyes="GLOW:3 SQUINT", pers="2-8 6-13 1-7 3-10 8-15", act="HOME:4", traits="NIGHT_OWL",
  food="Sulfur Soda:2|Pizza|Ramen", hobby="Video Games:4|Pranks", music="Synthwave:3|Industrial:2",
  phrase="The void says hi.|Beep boop!|Yikes.",
  cast="""
Glitchbit | t | Hacks streetlights to blink in morse jokes. |
Malwarina | s | Writes viruses that only make cursors dance. | job=Virus Writer
Firewalla | i | Firewall imp bot. Blocks everything. Even hugs. | job=Firewall Imp; species=ROBOT
Root Access | h | Has the password to everything. It is "password". | species=HUMAN; skin=SKIN_PORCELAIN
Bitrot | t | Corrupts files, then lovingly restores them. | species=ROBOT; trait=SOFTIE
""" ),

A("doom_influencers", "hell", "Doom Influencer|Hate Reviewer|Gloom Vlogger",
  sp="BATLING IMP CATFOLK GHOUL",
  fur_CATFOLK="JET/WHITE",
  top="CROP_TOP:3 LEATHER_JACKET", bottom="SKIRT:2 RIPPED_JEANS", shoes="PLATFORM:3",
  hat="NONE:3 BEANIE", acc="SUNGLASSES:3 NOSE_RING",
  pat="SOLID:2 SKULLS", scheme="JET/MAGENTA/WHITE GRAPE/CYAN/JET BLOOD/BUBBLEGUM/JET",
  pers="9-15 11-15 12-15 5-12 9-15", act="WANDERING:3",
  food="Sulfur Soda|Lava Cake|Bat Wing Chips", hobby="Cosplay|Karaoke|Graffiti", music="Industrial|Goth Rock|Synthwave",
  phrase="Slay!|Ugh, mortals.|Rude!|Stay cursed.",
  cast="""
Doomscroll | s | Rates sunsets one star. 2 million followers. |
Grumblogger | h | Reviews restaurants by yelling at the menu. | job=Hate Reviewer
Vlogoth | s | Vlogs her gloom. Ends with a sparkly outro. | job=Gloom Vlogger
Unfollow | x | Unfollows everyone hourly. Refollows at night. |
""" ),

A("goth_stylists", "hell", "Goth Stylist|Spike Tailor|Corset Maker",
  sp="BATLING GHOUL IMP CATFOLK",
  fur_CATFOLK="JET/WHITE CHARCOAL/WHITE",
  top="DRESS:2 SUIT LEATHER_JACKET", bottom="SKIRT:2 LONG_SKIRT KILT", shoes="PLATFORM:2 HEELS",
  hat="NONE:2 TOP_HAT WITCH", acc="CHAINS SKULL_NECKLACE:2",
  pat="SKULLS:2 STRIPES:2 PLAID", scheme="JET/BLOOD/SILVER GRAPE/JET/LEMON JET/MAGENTA/SILVER",
  pers="6-12 8-14 8-14 4-11 7-14", act="SHOP:3",
  food="Macarons|Black Licorice|Blood Oranges", hobby="Cosplay:3|Painting", music="Goth Rock:4",
  phrase="Slay!|Dark and fluffy.|Rude!",
  cast="""
Corsetta | s | Corset maker. Laces so tight you float. | job=Corset Maker
Spikeworth | h | Spike tailor. Adds spikes to baby clothes. Cute. | job=Spike Tailor
Velvetta | t | Styles goth hair. Every look is "dramatic". |
Lacey Grave | t | Sews lace onto anything that holds still. |
""" ),

A("tattoo_imps", "hell", "Tattoo Imp|Tattoo Artist|Flash Painter",
  sp="IMP:3 HELLHOUND GHOUL",
  top="TANK:3 LEATHER_JACKET BAND_TEE", bottom="RIPPED_JEANS:3 CARGO", shoes="COMBAT:3 BOOTS",
  hat="NONE:3 BANDANA", acc="NOSE_RING:2 EARRINGS CHAINS",
  pat="SOLID:3 FLAMES", scheme="JET/HELLFIRE/BONE CHARCOAL/CYAN/BONE BLOOD/JET/LEMON",
  mark="TRIBAL:4 PENTAGRAM SCALES SKULL_PAINT", mark_col="JET BLOOD HELLFIRE CYAN",
  pers="5-11 7-13 5-12 6-13 6-13", act="SHOP:3",
  food="Coffee|Hell Peppers|Tacos", hobby="Painting:3|Graffiti", music="Hardcore Punk:3|Doom Metal",
  phrase="Rip and tear... gently.|Big mood.|Hail me!",
  cast="""
Inkfang | h | Tattoos tiny hearts on big tough demons. | trait=SOFTIE
Needlepoint | s | Tattoo artist. Steadiest claw in Hellvile. | job=Tattoo Artist
Flashfire | t | Paints flash art of skulls wearing bows. | job=Flash Painter
Tribalina | s | Covered in tribal ink. Each line is a story. | species=HELLHOUND
Mama Ink | s | Has tattooed half of Hellvile. Kept score. | species=GHOUL
""" ),

A("piercers", "hell", "Piercer|Horn Jeweller",
  sp="IMP GHOUL BATLING",
  top="TANK CROP_TOP BAND_TEE", bottom="RIPPED_JEANS:2", shoes="PLATFORM COMBAT",
  hat="NONE:3", acc="NOSE_RING:4 EARRINGS:2",
  pat="SOLID:3", scheme="JET/SILVER/MAGENTA CHARCOAL/SILVER/CYAN",
  pers="5-11 6-12 6-12 6-13 7-14", act="SHOP:3",
  food="Candy Eyeballs|Sulfur Soda", hobby="Cosplay|Skull Collecting", music="Industrial|Hardcore Punk",
  phrase="Hiss!|Slay!|I bite. Cutely.",
  cast="""
Pokey | s | Piercer. Has 40 piercings. Collects more. |
Studsley | h | Horn jeweller. Puts rings on horns. | job=Horn Jeweller
Hoopla | t | Wears hoops so big birds perch on them. | species=BATLING
""" ),

A("horn_polishers", "hell", "Horn Polisher|Hoof Trimmer|Tail Groomer",
  sp="IMP:3 HELLHOUND",
  top="TSHIRT:2 SWEATER_VEST", bottom="PANTS:2", shoes="HOOVES:3 BOOTS",
  hat="NONE:4", acc="NONE:3 GLASSES",
  pat="SOLID:3 STRIPES", scheme="CHARCOAL/LEMON/BONE BLOOD/WHITE/LEMON JET/BONE/CHERRY",
  horns="DEVIL:2 RAM:2 BULL GOAT ONI", pers="4-10 3-9 6-13 7-14 3-10", act="SHOP:4",
  food="Coffee|Brimstone Bagels|Gumdrops", hobby="Painting|Napping", music="Jazz|City Pop",
  phrase="Sulfur kisses!|Okie dokie!|Hellooo!",
  cast="""
Shinehorn | h | Polishes horns until you can see your sins. |
Hoofsworth | s | Hoof trimmer. Hums while she files. | job=Hoof Trimmer; shoes=HOOVES
Tailory | t | Tail groomer. Fluffs tails. Even scaly ones. | job=Tail Groomer; trait=SOFTIE
Oni Onie | h | Polishes oni horns. Has oni horns. Polishes own. | horns=ONI
""" ),

A("lava_spa", "hell", "Lava Spa Attendant|Mud Masseuse|Sauna Imp",
  sp="IMP:2 SLIME HELLHOUND",
  skin_SLIME="MAGENTA BUBBLEGUM",
  top="KIMONO:2 TANK", bottom="NONE:2 SHORTS", shoes="SANDALS:2 BARE",
  hat="NONE:3", acc="NONE:3",
  pat="SOLID:3", scheme="HELLFIRE/BONE/WHITE TANGERINE/JET/WHITE BLOOD/PEACH/WHITE",
  aura="BUBBLES:3 SMOKE", pers="1-7 4-10 4-11 9-15 3-10", act="SHOP:3",
  food="Lava Cake|Sulfur Soda|Blood Oranges", hobby="Yoga|Napping", music="Lo-fi Beats|Synthwave",
  phrase="Sulfur kisses!|Cozy vibes only!|Big mood.",
  cast="""
Magmassage | s | Massages with hot stones. Very hot. Too hot. | job=Mud Masseuse
Steamer | h | Sauna imp. Turns the heat up when you leave. | job=Sauna Imp
Bubbly Brim | t | Runs the lava tub. Adds bubbles for free. |
Spaella | t | Lava spa attendant. Gives the best foot rubs. | species=SLIME; trait=SOFTIE
""" ),

A("cafe_inferno", "hell", "Cafe Inferno Cook|Grim Barista|Sulfur Soda Jerk",
  sp="IMP:2 GHOUL HELLHOUND RACCOONKIN",
  fur_RACCOONKIN="CHARCOAL/WHITE",
  top="TSHIRT:2 SWEATER_VEST MAID", bottom="SKIRT:2 PANTS", shoes="SNEAKERS:2 PLATFORM",
  hat="NONE:3 BANDANA", acc="BOWTIE:2 NONE",
  pat="SOLID:2 STRIPES SKULLS", scheme="BLOOD/BONE/JET JET/CHERRY/BONE CHARCOAL/HELLFIRE/WHITE",
  pers="8-14 7-13 6-13 5-12 6-13", act="CAFE:4",
  food="Coffee:3|Brimstone Bagels|Burnt Toast|Sulfur Soda", hobby="Baking|Karaoke", music="Goth Rock|Jazz",
  phrase="Feed me souls.|Bleh.|Big mood.",
  cast="""
Grimbrew | h | Grim barista. Draws skulls in your latte foam. | job=Grim Barista
Fizzlebone | t | Soda jerk. Pours sulfur soda with flair. | job=Sulfur Soda Jerk
Scaldie | s | Serves everything scalding hot. On purpose. |
Espressoul | x | Brews coffee so strong it wakes the dead. | species=GHOUL
Wafflewight | h | Makes waffles with skulls pressed in. Tasty. |
""" ),

A("batwing_fryers", "hell", "Bat Wing Fryer|Chip Shop Ghoul|Deep Fry Imp",
  sp="GHOUL:2 IMP HELLHOUND",
  top="TSHIRT:2 TANK", bottom="PANTS:2", shoes="CLOGS:2 SNEAKERS",
  hat="CAP:2 BANDANA", acc="NONE:3",
  pat="SOLID:3 PLAID", scheme="RUST/LEMON/WHITE CHARCOAL/TANGERINE/WHITE BLOOD/LEMON/BONE",
  pers="9-15 7-13 6-12 5-12 6-13", act="CAFE:4", stats="hygiene=40-65",
  food="Bat Wing Chips:4|Fried Chicken", hobby="Board Games|Napping", music="Hardcore Punk|Disco",
  phrase="Nom nom nom!|Bleh.|Big mood.",
  cast="""
Fryday | h | Fries everything, including the menu. |
Greasy Gail | s | Chip shop ghoul. Wraps chips in old curses. | job=Chip Shop Ghoul
Crispwing | t | Deep fry imp. Fries ice cream. Somehow works. | job=Deep Fry Imp
Battercup | t | Batters bats. The bats have complained. |
""" ),

A("sour_candy", "hell", "Sour Candy Maker|Worm Gummy Chef|Jawbreaker Smith",
  sp="IMP:2 GHOUL SLIME",
  top="OVERALLS:2 TSHIRT", bottom="SKIRT SHORTS", shoes="SNEAKERS:2 PLATFORM",
  hat="NONE:2 PARTY", acc="NONE:3",
  pat="STRIPES:3 SKULLS", scheme="LIME/MAGENTA/JET SLIME/GRAPE/WHITE MAGENTA/LIME/JET",
  pers="10-15 8-14 11-15 7-14 10-15", act="SHOP:3",
  food="Worm Gummies:3|Candy Eyeballs:2|Gumdrops", hobby="Baking|Pranks", music="Bubblegum Pop|Hardcore Punk",
  phrase="Mwahaha!|Yummy yummy!|Hiss!",
  cast="""
Sourpuss | s | Makes candy so sour your ears fold. |
Wormwood | h | Worm gummy chef. Some are real. Ask first. | job=Worm Gummy Chef
Jawbreaker | t | Makes jawbreakers. Has broken several jaws. | job=Jawbreaker Smith
Fizzbitter | s | Pop rocks inventor. Pops constantly. | species=SLIME
""" ),

A("butchers", "hell", "Meat Slab Butcher|Sausage Maker|Cleaver Twirler",
  sp="HELLHOUND:2 GHOUL IMP",
  top="OVERALLS:2 TSHIRT", bottom="PANTS:2", shoes="BOOTS:3",
  hat="CAP BANDANA NONE", acc="NONE:3",
  pat="STRIPES:3 SOLID", scheme="BLOOD/WHITE/CHARCOAL WHITE/BLOOD/JET CHARCOAL/BLOOD/BONE",
  pers="5-11 8-14 4-11 4-11 6-13", act="SHOP:4", body="BUFF:2 STOCKY:2",
  food="Meat Slabs:5|Bone Broth", hobby="Napping|Board Games", music="Doom Metal|Industrial",
  phrase="Rip and tear... gently.|Feed me souls.|Big mood.",
  cast="""
Slabsworth | h | Butcher. Cuts meat into heart shapes for dates. | trait=SOFTIE
Sausagina | s | Sausage maker. Every link has a name. | job=Sausage Maker
Cleaverly | t | Twirls cleavers. Has all fingers. Somehow. | job=Cleaver Twirler
Brisket | h | Smokes brisket for 40 hours. Sits with it. | species=GHOUL
""" ),

A("junkyard", "hell", "Junkyard Ghoul|Scrap Robot|Salvager|Tinker Rat",
  sp="ROBOT:2 GHOUL RACCOONKIN MOUSEKIN",
  fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL", fur_MOUSEKIN="CHARCOAL/SILVER",
  top="OVERALLS:3 HOODIE", bottom="CARGO:2 NONE", shoes="BOOTS:3",
  hat="CAP:2 TRAFFIC_CONE NONE", acc="GAS_MASK:2 NONE:2",
  pat="SOLID:3 CAMO", scheme="RUST/SILVER/LEMON CHARCOAL/RUST/LIME FOREST/RUST/SILVER",
  pers="4-10 6-12 3-10 5-12 7-14", act="WORKSHOP:2 WANDERING:2", wings="MECH:1 NONE:4",
  food="Burnt Toast|Pickles|Coffee", hobby="Welding:4|Bug Collecting", music="Industrial:3|Doom Metal",
  phrase="Beep boop!|Bleh.|Big mood.",
  cast="""
Scrapheap | i | Scrap robot made of 40 toasters. Toasts gently. | species=ROBOT
Rustbucket | h | Junkyard ghoul. Rates junk out of ten. All tens. |
Salvagia | s | Salvager. Can rebuild anything but a bridge. | job=Salvager
Tinkerat | t | Tinker rat. Builds wind-up mice. They escape. | job=Tinker Rat; species=MOUSEKIN
Boltsy | i | Small scrap bot. Sorts screws by personality. | species=ROBOT; trait=SOFTIE
""" ),

A("doom_bots", "hell", "Doom Bot|Malware Bot|Siege Drone",
  sp="ROBOT:5",
  top="ARMOR:3 NONE:2", bottom="NONE:3 CARGO", shoes="BOOTS:2 COMBAT",
  hat="NONE:4 HORNED_HELMET", acc="NONE:3 GAS_MASK",
  pat="SOLID:3 CAMO", scheme="JET/BLOOD/SILVER CHARCOAL/HELLFIRE/SILVER JET/SLIME/CHARCOAL",
  wings="MECH:4 NONE", eyes="GLOW:4 DEMON_SLIT", eye_col="BLOOD HELLFIRE SLIME",
  horns="ANTENNAE:2 DEVIL CROWN_SPIKES", pers="5-11 9-15 0-5 0-6 5-12", act="WANDERING:3",
  food="Sulfur Soda|Bat Wing Chips", hobby="Video Games|Welding", music="Industrial:4|Doom Metal",
  phrase="Beep boop!|Rip and tear... gently.|Hail me!",
  cast="""
Ruin-3000 | i | Built to destroy. Prefers making toast. | trait=SOFTIE
Malbot | i | Malware bot. Installs dad jokes on phones. | job=Malware Bot
Dronezilla | t | Siege drone. Circles the Wall, sulking. | job=Siege Drone; wings=MECH
Crushbot | i | Crushes cans for recycling. Very virtuous. |
""" ),

A("moth_goths", "hell", "Lamp Stalker|Moth Poet|Candle Thief",
  sp="MOTHKIN:5",
  skin="CHARCOAL GRAPE BONE JET",
  top="CLOAK:3 DRESS SWEATER", bottom="LONG_SKIRT:2 PANTS", shoes="BOOTS:2 BARE",
  hat="NONE:4", acc="SCARF:2 NONE:2",
  pat="SOLID:3 STRIPES", scheme="JET/BONE/LEMON CHARCOAL/LAVENDER/LEMON GRAPE/BONE/HELLFIRE",
  eyes="COMPOUND:4 GLOW", wings="BUTTERFLY:3 BAT", pers="1-7 4-10 4-11 4-11 8-15", act="WANDERING:3",
  traits="NIGHT_OWL", food="Honey Toast|Black Licorice|Milk", hobby="Poetry:3|Stargazing", music="Goth Rock:3|Lo-fi Beats",
  phrase="Gloom on!|Dark and fluffy.|The void says hi.",
  cast="""
Lampwick | h | Stalks lamps. Hugs every lightbulb he meets. | job=Lamp Stalker
Flutterdusk | s | Moth poet. Every poem is about a porch light. | job=Moth Poet
Candlenab | t | Steals candles. Lights them for lost ghosts. | job=Candle Thief; trait=SOFTIE
Dustwing | t | Leaves powder on everything. Calls it glitter. |
Mothilda | s | Huge wings, tiny voice. Whispers threats. |
""" ),

A("owl_insomniacs", "hell", "Insomniac Poet|Night Shift Guard|Owl Bouncer",
  sp="OWLKIN:4",
  fur="CHARCOAL/WHITE CHOCOLATE/HONEY SILVER/WHITE",
  top="LEATHER_JACKET:2 SWEATER HOODIE", bottom="PANTS:2 CARGO", shoes="BOOTS:3",
  hat="NONE:3 BEANIE", acc="GLASSES:2 NONE",
  pat="SOLID:3 PLAID", scheme="JET/LEMON/WHITE NAVY/HELLFIRE/BONE CHARCOAL/LIME/WHITE",
  eyes="WIDE:3 GLOW:2 SQUINT", pers="1-7 6-12 3-10 3-10 7-14", act="WANDERING:3", traits="NIGHT_OWL",
  stats="energy=35-60", food="Coffee:4|Black Licorice", hobby="Poetry:3|Stargazing", music="Jazz|Goth Rock|Lo-fi Beats",
  phrase="Hoo hoo!|Bleh.|Gloom on!",
  cast="""
Hootsomnia | h | Has not slept since 1999. Very alert. Too alert. | job=Insomniac Poet
Nightshift | s | Night guard. Guards the night itself. | job=Night Shift Guard
Talonsworth | t | Owl bouncer. Turns head 270 degrees at you. | job=Owl Bouncer
Wakewing | x | Reads to sleepy chicks until dawn. Hoots softly. | trait=SOFTIE
""" ),

A("black_sheep", "hell", "Black Sheep|Wool Rustler|Baa-d Influence",
  sp="SHEEPKIN:5",
  fur="CHARCOAL/WHITE JET/BONE CHARCOAL/BUBBLEGUM",
  top="LEATHER_JACKET:3 HOODIE BAND_TEE", bottom="RIPPED_JEANS:3 SKIRT", shoes="COMBAT:2 BOOTS HOOVES",
  hat="NONE:3 BEANIE", acc="CHAINS:2 EARRINGS NOSE_RING",
  pat="SOLID:3 SKULLS", scheme="JET/CHERRY/WHITE CHARCOAL/LIME/BONE JET/MAGENTA/WHITE",
  hair="CURLY:3 AFRO:2 MOHAWK", pers="6-12 9-15 6-13 4-11 8-15", act="WANDERING:3",
  food="Black Licorice|Burnt Toast|Pizza", hobby="Pranks|Graffiti|Metal Drumming", music="Hardcore Punk:3|Goth Rock",
  phrase="Baa-rilliant!|Whatever.|Go to heck!",
  cast="""
Baa-rbara | s | The black sheep of a very white family. Thrilled. |
Ramsey | h | Rams doors open. Has not tried a handle. |
Woolrustle | t | Rustles wool from clouds. Knits jumpers. | job=Wool Rustler
Lamblet | t | Baa-d influence. Talks lambs into staying up late. | job=Baa-d Influence
Fleecebane | s | Shears her own wool into spikes. Very fluffy spikes. |
""" ),

A("grizzlies", "hell", "Grizzly Enforcer|Honey Smuggler|Den Boss",
  sp="BEARKIN:5",
  fur="JET/WHITE CHOCOLATE/CARAMEL CHARCOAL/WHITE",
  top="LEATHER_JACKET:2 SUIT TANK", bottom="PANTS:2 CARGO", shoes="BOOTS:3",
  hat="NONE:3 TOP_HAT", acc="SUNGLASSES:2 CHAINS",
  pat="SOLID:3 STRIPES", scheme="JET/HONEY/WHITE CHARCOAL/BLOOD/BONE CHOCOLATE/HELLFIRE/BONE",
  body="BUFF:3 STOCKY:2", build="11-15", height="9-14", pers="3-9 9-15 2-9 3-10 5-12", act="WANDERING:3",
  food="Meat Slabs|Honey Toast:3|Fried Chicken", hobby="Napping:3|Fishing", music="Jazz|Doom Metal",
  phrase="Go to heck!|Big mood.|I bite. Cutely.",
  cast="""
Ursa Major | s | Den boss. Runs the honey racket with a velvet paw. | job=Den Boss
Growlston | h | Enforcer. Enforces nap time across Hellvile. |
Honeygrab | t | Smuggles honey over the Wall in a hollow log. | job=Honey Smuggler
Mauler | h | Big, scary, collects tiny porcelain kittens. | trait=SOFTIE
Kodiak | s | Stares at fish for hours. Never catches one. |
""" ),

A("killer_bunnies", "hell", "Killer Bunny|Carrot Bandit|Hop Fighter",
  sp="BUNNYKIN:5",
  fur="JET/WHITE CHARCOAL/WHITE WHITE/BUBBLEGUM",
  top="HOODIE:2 LEATHER_JACKET TANK", bottom="SHORTS:2 RIPPED_JEANS", shoes="COMBAT:2 SNEAKERS",
  hat="NONE:4 BANDANA", acc="NONE:2 CHAINS EYEPATCH",
  pat="SOLID:3 SKULLS", scheme="JET/BLOOD/WHITE WHITE/BLOOD/JET CHARCOAL/BUBBLEGUM/BLOOD",
  fangs="2:3 3", mouth="FANGY:3 SHARK", eyes="ANGRY:3 DEMON_SLIT GLOW", eye_col="BLOOD:3 HELLFIRE",
  pers="12-15 10-15 6-13 4-11 12-15", act="WANDERING:3",
  food="Carrot Cake|Meat Slabs|Hell Peppers", hobby="Pranks|Skateboarding", music="Hardcore Punk:3|Doom Metal",
  phrase="I bite. Cutely.|Hiss!|Rip and tear... gently.",
  cast="""
Fluffbite | s | Looks fluffy. Bites. Then looks fluffy again. |
Carrotslash | h | Carrot bandit. Steals carrots, leaves receipts. | job=Carrot Bandit
Hopkick | t | Hop fighter. Kicks with both feet. Twice. | job=Hop Fighter
Thumpkin | h | Thumps the ground until the neighbours move. |
Bunbun Bane | h | Wears a tiny eyepatch. Has both eyes. Style. | accessory=EYEPATCH
""" ),

A("toads", "hell", "Toad Bookmaker|Swamp Crooner|Fly Trader",
  sp="FROGLING:4",
  skin="FOREST TEAL CHARTREUSE CHARCOAL",
  top="SUIT TANK TSHIRT", bottom="PANTS SHORTS NONE", shoes="BARE:3",
  hat="TOP_HAT CAP NONE", acc="NONE:2 SUNGLASSES",
  pat="SOLID:3 STRIPES", scheme="TEAL/CHARTREUSE/CARAMEL FOREST/LEMON/JET TEAL/BLOOD/BONE CHARCOAL/LIME/LEMON",
  aura="FLIES:2 NONE:2", pers="2-8 6-12 4-11 6-13 6-13", act="BEACH:2 WANDERING",
  food="Worm Gummies|Pickles|Bat Wing Chips", hobby="Singing|Fishing|Board Games", music="Jazz:3|Sea Shanties",
  phrase="Ribbit!|Bleh.|Big mood.",
  cast="""
Croakley | h | Toad bookmaker. Takes bets on fly races. | job=Toad Bookmaker
Warty | s | Swamp crooner. Sings love songs to the moon. | job=Swamp Crooner; trait=SOFTIE
Flytrader | t | Trades flies. The currency of the swamp. | job=Fly Trader
Bogsworth | t | Sits on a log. Judges everyone. Silently. |
""" ),

]
