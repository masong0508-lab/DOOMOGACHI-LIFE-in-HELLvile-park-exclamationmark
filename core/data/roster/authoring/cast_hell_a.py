"""Hellvile archetypes, part A: Vapula's court, the vape trade, bureaucracy,
muscle and mischief. (owner: roster; see seed.py)

Hellvile folk are demonic and edgy but still drawn cute. About 15% are
secretly lovely (trait=SOFTIE): their bios hint at it, and Chapter 4
redeems them first."""
from seedlib import A

ARCHETYPES = [

A("vapula_court", "hell", "Smoke Herald|Cloud Courtier|Vapor Chancellor|Royal Taster",
  sp="CACOBLOB:3 IMP:2 BATLING GHOUL",
  traits="LOYALIST", top="CLOAK:3 ROBE:2 SUIT", bottom="ROBE:2 LONG_SKIRT PANTS", shoes="PLATFORM:2 BOOTS HEELS",
  hat="CROWN:2 TOP_HAT:2 NONE", acc="CAPE:3 MONOCLE SKULL_NECKLACE",
  pat="SOLID:3 SKULLS FLAMES", scheme="VAPE_PURPLE/LEMON/JET GRAPE/MAGENTA/SILVER JET/VAPE_PURPLE/LEMON",
  aura="SMOKE:5 NONE", pers="2-9 8-15 3-10 0-6 6-13", act="HOME:3 WANDERING",
  eyes="DEMON_SLIT:3 GLOW:2 GOAT SQUINT", eye_col="VAPE_PURPLE MAGENTA LEMON HELLFIRE",
  food="Soul Soup|Lava Cake|Sulfur Soda|Grave Dirt Cake", hobby="Tarot Reading|Seances|Board Games",
  music="Doom Metal|Industrial|Classical", phrase="Hail me!|Ugh, mortals.|Feed me souls.|Mwahaha!",
  fear="Holy Water|Exorcisms|Church Bells|Hugs",
  cast="""
Lord Vapula | h | Duke of the Ninth Cloud. Exhales thunderheads. | id=999; story=LORD_VAPULA; job=Duke of Ninth Cloud; species=CACOBLOB; skin=GRAPE; body=TOP_HEAVY; height=15; build=15; head=BLOB; head_size=15; eyes=GLOW; eye_color=VAPE_PURPLE; eye_size=13; eye_count=1; eye_tilt=4; brows=ANGRY; brow_height=4; nose=NONE; mouth=SHARK; mouth_size=12; fangs=3; hair=BALD; hair_color=JET; highlight=NONE; facial_hair=NONE; ears=NONE; horns=CROWN_SPIKES; horn_color=LEMON; horn_size=14; tail=TENTACLE; wings=DRAGON; halo=FLAME_RING; marking=CRACKS; marking_color=VAPE_PURPLE; cheeks=SCAR; top=CLOAK; top_color=VAPE_PURPLE; pattern=FLAMES; pattern_color=MAGENTA; bottom=ROBE; bottom_color=JET; shoes=BARE; shoes_color=JET; hat=CROWN; hat_color=LEMON; accessory=CAPE; accessory_color=JET; aura=SMOKE; birthday=SEP-09; food=Soul Soup; hates=Angel Cake; hobby=Tarot Reading; music=Doom Metal; color=VAPE_PURPLE; fear=Hugs; phrase=Hail me!; movement=4; speech=15; expression=12; attitude=1; quirk=14; romance=NONE; activity=HOME; corruption=96; happy=50; sanity=45; pitch=0; speed=6; quality=31; intonation=26; accent=20; trait=LOYALIST,VAPER
Hazey | t | Announces Vapula's arrival with a foghorn. | job=Smoke Herald; species=IMP
Chancellor | h | Counts every puff of vapor in the Ninth Cloud. | job=Vapor Chancellor; species=GHOUL; fhair=GOATEE
Lady Smog | s | Wears a gown woven entirely from smog. | job=Cloud Courtier; species=CACOBLOB; hat=CROWN
Coughlotte | s | Coughs politely, constantly, in perfect rhythm. | job=Cloud Courtier; species=BATLING
Tastebud | t | Tastes Vapula's soup for curses. Has hiccups. | job=Royal Taster; species=IMP; trait=SOFTIE,LOYALIST
""" ),

A("cloud_nine", "hell", "Vape Clerk|Flavor Barista|Cloud Promoter|Shop Owner",
  sp="FOXKIN:2 BATLING:2 IMP CATFOLK",
  fur_FOXKIN="RUST/BONE TANGERINE/WHITE", fur_CATFOLK="JET/WHITE CHARCOAL/WHITE",
  traits="VAPER INFILTRATOR", top="HOODIE:2 CROP_TOP:2 SUIT LEATHER_JACKET", bottom="SKIRT:2 RIPPED_JEANS LEGGINGS",
  shoes="PLATFORM:3 SNEAKERS", hat="NONE:3 HEADPHONES BEANIE", acc="SUNGLASSES:3 EARRINGS:2 NOSE_RING",
  pat="SOLID:3 STRIPES", scheme="VAPE_PURPLE/CYAN/WHITE MAGENTA/VAPE_PURPLE/LEMON GRAPE/MINT/WHITE",
  aura="SMOKE:4 NONE:2", pers="8-14 4-10 9-15 4-11 7-14", act="SHOP:3 WANDERING",
  eyes="LASHES:3 SLEEPY:2 CAT DEMON_SLIT", eye_col="VAPE_PURPLE MAGENTA CYAN LEMON",
  food="Sulfur Soda:3|Cotton Candy|Candy Eyeballs|Bubble Tea", hobby="Karaoke|Cosplay|Pranks|Dancing",
  music="Synthwave:4|City Pop|Industrial", phrase="Sulfur kisses!|Slay!|Big mood.|Hellooo!",
  cast="""
Vex | h | Owns Cloud Nine Vapor. Smiles too wide, too long. | id=666; story=VEX; job=Cloud Nine Owner; species=FOXKIN; fur=RUST/BONE; body=LANKY; height=10; build=5; head=TRIANGLE; head_size=11; eyes=LASHES; eye_color=VAPE_PURPLE; eye_size=10; eye_tilt=6; eye_count=2; brows=ARCHED; brow_height=8; nose=CAT; nose_size=5; mouth=SMIRK; mouth_size=8; fangs=2; hair=SLICKED; hair_color=JET; highlight=VAPE_PURPLE; facial_hair=NONE; ears=FOX; horns=DEVIL; horn_color=JET; horn_size=4; tail=FOX; wings=NONE; halo=NONE; marking=FUR_TIPPED_EARS; marking_color=JET; cheeks=BEAUTY_MARK; top=SUIT; top_color=VAPE_PURPLE; pattern=STRIPES; pattern_color=MAGENTA; bottom=PANTS; bottom_color=JET; shoes=PLATFORM; shoes_color=JET; hat=NONE; hat_color=VAPE_PURPLE; accessory=SUNGLASSES; accessory_color=MAGENTA; aura=SMOKE; birthday=JUN-06; food=Cotton Candy; hates=Angel Cake; hobby=Pranks; music=Synthwave; color=VAPE_PURPLE; fear=Exorcisms; phrase=Sulfur kisses!; movement=12; speech=7; expression=14; attitude=5; quirk=13; romance=ANY; activity=SHOP; corruption=92; happy=88; trait=VAPER,INFILTRATOR,LOYALIST
Puffina | s | Vape clerk. Blows smoke rings shaped like hearts. | species=BATLING
Brimbrew | t | Mixes flavours like Brimstone Blast and Unholy Grape. | job=Flavor Barista; species=IMP
Hypebat | h | Promoter. Hands out free samples to children. Ugh. | job=Cloud Promoter; species=BATLING
Mistral | s | Runs the counter. Remembers every customer's weakness. | species=CATFOLK
Haze | s | Secretly hates the smoke. Waters the shop's plants. | species=FOXKIN; trait=SOFTIE,VAPER
""" ),

A("vape_lords", "hell", "Vape Lord|Cloud Chaser|Smoke Trickster",
  sp="IMP:2 BATLING:2 GHOUL HELLHOUND",
  traits="VAPER", top="HOODIE:3 LEATHER_JACKET:2 TANK", bottom="CARGO:2 RIPPED_JEANS:2 SWEATS", shoes="SNEAKERS:3 COMBAT",
  hat="BEANIE:2 CAP:2 NONE", acc="SUNGLASSES:3 CHAINS NOSE_RING",
  pat="SOLID:3 FLAMES CAMO", scheme="JET/LIME/VAPE_PURPLE CHARCOAL/VAPE_PURPLE/LIME JET/MAGENTA/CYAN GRAPE/LIME/WHITE",
  aura="SMOKE:5 NONE", pers="3-10 7-14 4-11 6-13 8-15", act="WANDERING:3 HOME",
  eyes="SLEEPY:4 SQUINT:2 GLOW", food="Sulfur Soda|Bat Wing Chips|Pizza|Popcorn", hobby="Video Games|Pranks|Skateboarding",
  music="Synthwave|Industrial|Hardcore Punk", phrase="Big mood.|Whatever.|Bleh.|Doom and gloom!",
  cast="""
Cloudzilla | h | Blows clouds so big they have their own weather. | species=IMP
Vapor Vinny | h | Has not seen his own feet since spring. | species=GHOUL
Puffcake | t | Calls every cloud "a vibe". Every one. | species=BATLING
Smogette | s | Chases clouds on a skateboard. Catches none. | job=Cloud Chaser; species=HELLHOUND
Hookah Hex | h | Vapes in a rocking chair. Rocks out. | species=IMP
Wheezle | h | Coughs out little ghosts. Names them. Sad. | job=Smoke Trickster; species=BATLING; trait=SOFTIE,VAPER
""" ),

A("vape_lab", "hell", "Flavor Chemist|Cloud Engineer|Lab Imp",
  sp="IMP:2 GHOUL ROBOT MOUSEKIN",
  fur_MOUSEKIN="CHARCOAL/SILVER SILVER/WHITE",
  traits="VAPER", top="HAZMAT:4 LAB_COAT:2", bottom="NONE:2 CARGO", shoes="BOOTS:2 COMBAT",
  hat="NONE:4 SKULL_CAP", acc="GAS_MASK:5 GLASSES",
  pat="SOLID:4", scheme="LEMON/JET/LIME LIME/CHARCOAL/LEMON SLIME/JET/WHITE",
  pers="4-10 5-12 3-9 3-10 9-15", act="WORKSHOP:3 HOME", aura="SMOKE:2 GLITCH NONE:2",
  eyes="WIDE:2 SPIRAL:2 GLOW", food="Sulfur Soda|Coffee|Eyeball Jelly", hobby="Bug Collecting|Welding|Seances",
  music="Industrial:4|Synthwave", phrase="Mwahaha!|Yikes.|Beep boop!",
  cast="""
Dr. Fumes | h | Invented Unholy Grape. Regrets nothing, mostly. | species=GHOUL
Beakerella | s | Measures nothing. Pours everything. Boom. | species=IMP
Condenser | i | Cloud engineer bot. Pressurises the big tanks. | job=Cloud Engineer; species=ROBOT
Squeakvent | t | Lab imp who sniffs every new flavour first. | job=Lab Rat; species=MOUSEKIN
Ventra | s | Secretly swaps the flavours for plain water. | species=IMP; trait=SOFTIE,VAPER
""" ),

A("imp_accountants", "hell", "Imp Accountant|Tax Imp|Auditor|Bookkeeper",
  sp="IMP:6 GHOUL",
  top="SUIT:4 SWEATER_VEST:2", bottom="PANTS:4 SKIRT", shoes="BOOTS:2 HEELS",
  hat="NONE:4 TOP_HAT", acc="GLASSES:4 MONOCLE:2",
  pat="STRIPES:3 SOLID:2 PLAID", scheme="CHARCOAL/CHERRY/WHITE JET/HELLFIRE/BONE NAVY/BLOOD/SILVER",
  pers="5-11 9-15 1-7 0-5 4-11", act="SHOP:2 HOME", body="TINY:2 AVERAGE ROUND",
  eyes="SQUINT:4 ANGRY:2 GOAT", food="Coffee:3|Brimstone Bagels|Black Licorice", hobby="Board Games:3|Reading",
  music="Classical|Industrial", phrase="Ugh, mortals.|No thank you!|Go to heck!|Feed me souls.",
  fear="Tax Season|Hugs|Cute Things",
  cast="""
Taxfang | h | Charges interest on high-fives. Compounding. | job=Tax Imp; crush=Penny Oink
Ledgerella | s | Balances books with one claw, eats with the other. |
Auditrix | t | Audits your soul for missing receipts. | job=Auditor
Debitt | h | Owes everyone money. Is the bank. |
Pennyworm | t | Bookkeeper. Keeps books. Never returns them. | job=Bookkeeper
Fiscalo | h | Rounds every number down, except his own. | job=Ghoul Accountant; species=GHOUL
Spreadsheet | s | Dreams in columns. Secretly sneaks tips to waiters. | trait=SOFTIE
Comma Krak | t | Has never used a decimal correctly. Proudly. |
""" ),

A("ghoul_landlords", "hell", "Ghoul Landlord|Rent Agent|Property Ghoul",
  sp="GHOUL:6",
  top="SUIT:4 CLOAK", bottom="PANTS:4", shoes="BOOTS:2 HEELS",
  hat="TOP_HAT:3 NONE:2", acc="MONOCLE:3 SKULL_NECKLACE NONE",
  pat="STRIPES:3 SOLID:2", scheme="JET/ECTOPLASM/SILVER FOREST/BONE/JET CHARCOAL/SLIME/WHITE",
  pers="1-7 10-15 2-8 0-5 5-12", act="WANDERING:2 HOME", body="LANKY:3 HUNCHED:2",
  food="Bone Broth|Grave Dirt Cake|Black Licorice", hobby="Board Games|Seances|Skull Collecting", music="Classical|Goth Rock",
  phrase="Ugh, mortals.|Stay cursed.|No thank you!",
  cast="""
Mr. Moldry | h | Raises the rent every full moon. And every new one. |
Evicta | s | Has never fixed a leak. Calls it a water feature. |
Gutterworth | h | Owns 300 crypts. Lives in the worst one. |
Lease Liz | s | Charges extra for windows. And for looking out them. | job=Rent Agent
Damp Dorian | t | Collects mould. Calls it "character". | job=Property Ghoul
Graveley | x | Secretly lowers the rent for families with kids. | trait=SOFTIE
""" ),

A("skeleton_clerks", "hell", "Skeleton Clerk|Form Stamper|Permit Denier|Archivist of Dread",
  sp="SKELETON:6",
  top="SWEATER_VEST:3 SUIT:2", bottom="PANTS:3 SKIRT", shoes="BOOTS:2 SNEAKERS",
  hat="NONE:4 CAP", acc="BOWTIE:3 GLASSES:2",
  pat="PLAID:3 SOLID:2", scheme="CHARCOAL/BONE/CHERRY NAVY/SILVER/BONE FOREST/BONE/HONEY",
  pers="0-5 6-12 0-5 0-6 3-10", act="SHOP:3", eyes="HOLLOW:4 SQUINT:2 SLEEPY",
  food="Bone Broth:3|Milk|Black Licorice", hobby="Board Games|Skull Collecting|Reading", music="Industrial|Classical",
  phrase="Rattle rattle!|No thank you!|Bleh.",
  cast="""
Form 27B | t | Stamps "DENIED" on every form. Including its own. | job=Form Stamper; bio_pronoun_ok=1
Ribsley | h | Takes coffee breaks that last a century. |
Femura | s | Files paperwork in alphabetical order by smell. | job=Archivist of Dread
Clavicle | h | Has asked for a promotion since 1802. |
Patella Pat | t | Rattles while typing. Very loud office. |
Tibia Tess | s | Denies permits for fun. Approved one for a nest. | job=Permit Denier; trait=SOFTIE
Mandible | h | Clacks his jaw at people who are late. |
Vertebrian | h | Never bends the rules. Cannot bend at all. |
""" ),

A("hellhound_bouncers", "hell", "Hellhound Bouncer|Doorhound|Security Pup",
  sp="HELLHOUND:6",
  top="LEATHER_JACKET:4 TANK:2", bottom="CARGO:2 PANTS:2 RIPPED_JEANS", shoes="COMBAT:4 CLAWS:2",
  hat="NONE:3 SKULL_CAP CAP", acc="SUNGLASSES:4 CHAINS:3",
  pat="SOLID:4 FLAMES", scheme="JET/HELLFIRE/SILVER CHARCOAL/BLOOD/BONE RUST/JET/HELLFIRE",
  pers="2-8 10-15 1-6 0-6 2-9", act="WANDERING:2 CONCERT", body="BUFF:4 STOCKY:2 TOP_HEAVY",
  height="9-15", build="10-15", tail="FLAME:3 SPIKED",
  food="Meat Slabs:4|Bone Broth|Hell Peppers", hobby="Metal Drumming|Board Games|Napping", music="Doom Metal:4|Hardcore Punk",
  phrase="Go to heck!|No thank you!|Hiss!|Ugh, mortals.",
  cast="""
Brutus | h | Bouncer. Not on the list? Not getting in. |
Fangela | s | Checks IDs by sniffing them. | species=HELLHOUND
Chomper | h | Chews through velvet ropes when bored. |
Big Snarl | h | Lets in anyone who brings him a squeaky toy. | trait=SOFTIE
Muzzle | h | Doorhound. Has growled at the same door for years. | job=Doorhound
Cerbie | s | Three moods: grumpy, grumpier, nap. | eye_count=3
Ruffjustice | s | Security pup. Takes her job far too seriously. | job=Security Pup
""" ),

A("caco_bookies", "hell", "Cacoblob Bookie|Odds Maker|Loan Shark",
  sp="CACOBLOB:6",
  top="SUIT:3 TSHIRT:2 TANK", bottom="NONE:3 PANTS", shoes="BARE:3",
  hat="CAP:3 TOP_HAT NONE", acc="NONE:2 SUNGLASSES CHAINS",
  pat="STRIPES:3 SOLID:2 PLAID", scheme="CHERRY/LIME/LEMON MAGENTA/LEMON/JET ROSE/FOREST/WHITE",
  pers="4-10 9-15 6-13 2-9 6-13", act="SHOP:3 WANDERING",
  food="Lava Cake|Candy Eyeballs|Popcorn", hobby="Board Games:3|Pranks", music="Jazz|Disco|Synthwave",
  phrase="Mwahaha!|Feed me souls.|Big mood.|Hail me!",
  cast="""
Oddsball | h | Takes bets on everything. Even weather. Loses. |
Bettsy | s | Odds maker. Says the odds are always in her eye. | job=Odds Maker
Loanie | t | Loan shark. Charges in teeth. | job=Loan Shark; mouth=SHARK
Eyeballer | h | Watches the races with one big eye. Blinks rarely. |
Gamblob | h | Bets on the underdog every time. Cheers loudly. | trait=SOFTIE
Wagerine | s | Has never lost a bet she was allowed to rig. |
""" ),

A("edgy_teens", "hell", "Edgy Teen|Mall Goth|Skate Rat",
  sp="IMP:2 BATLING CATFOLK RACCOONKIN GHOUL HUMAN",
  fur_CATFOLK="JET/WHITE CHARCOAL/WHITE", fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL",
  top="BAND_TEE:4 HOODIE:3 CROP_TOP", bottom="RIPPED_JEANS:4 SKIRT LEGGINGS", shoes="PLATFORM:2 COMBAT:2 SNEAKERS",
  hat="NONE:3 BEANIE", acc="CHAINS:2 NOSE_RING EARRINGS SKULL_NECKLACE",
  pat="SKULLS:2 STRIPES SOLID", scheme="JET/BUBBLEGUM/WHITE CHARCOAL/LIME/JET GRAPE/MAGENTA/WHITE",
  hair="EMO_FRINGE:4 SIDE_SHAVE:2 SPIKY TWINTAILS", mark="EYELINER:4 FUR NONE",
  pers="4-12 9-15 3-10 2-9 7-14", act="WANDERING:2 PARK CONCERT", height="4-8",
  food="Black Licorice|Bat Wing Chips|Pizza|Sulfur Soda", hobby="Skateboarding|Graffiti|Video Games|Poetry",
  music="Goth Rock:3|Hardcore Punk:3", phrase="Whatever.|Ugh, fine.|As if!|Bleh.|Stay cursed.",
  cast="""
Grimbert | h | Writes dark poetry. All of it rhymes with "doom". | species=IMP; crush=Muffin
Sulkie | s | Rolls her eyes so hard they once got stuck. | species=BATLING
Edgelord | t | Wears 14 belts. None hold anything up. | species=GHOUL
Kitty Hex | s | Black cat. Crosses paths on purpose. | species=CATFOLK
Spikeback | h | Skate rat. Breaks boards, never bones. | job=Skate Rat; species=RACCOONKIN
Mallgoth | t | Hangs at the mall fountain. Wishes for coins. | job=Mall Goth; species=HUMAN; skin=SKIN_PORCELAIN
Moodring | s | Mood ring always says "dark". Is actually fine. | species=IMP; trait=SOFTIE
Scowlie | h | Practises scowls in the mirror. Smiles at dogs. | species=BATLING
""" ),

A("cultists", "hell", "Cultist|Candle Lighter|High Priestess|Chant Leader",
  sp="GHOUL:2 IMP:2 HUMAN SKELETON MOTHKIN",
  top="ROBE:5 CLOAK:2", bottom="ROBE:4 NONE", shoes="SANDALS:2 BARE BOOTS",
  hat="NONE:4 WITCH", acc="NONE:2 SKULL_NECKLACE ROSARY",
  pat="SOLID:5", scheme="BLOOD/JET/LEMON JET/BLOOD/BONE GRAPE/JET/HELLFIRE",
  halo="THORNS:2 NONE:3", mark="PENTAGRAM:2 SKULL_PAINT THIRD_EYE NONE:2", mark_col="BLOOD HELLFIRE JET",
  pers="1-6 6-12 0-6 0-5 9-15", act="CHURCH:2 HOME:2", eyes="HOLLOW:2 GLOW:2 SLEEPY SPIRAL",
  food="Soul Soup|Candy Eyeballs|Grave Dirt Cake", hobby="Seances:4|Tarot Reading:2|Singing", music="Doom Metal|Hymns|Industrial",
  phrase="Hail me!|Feed me souls.|The void says hi.|Doom and gloom!", fear="Church Bells|Sunlight|Holy Water",
  cast="""
Bro. Ashe | h | Chants backwards. Gets the words wrong forwards. |
Candlewick | s | Has lit 10,000 candles. Burned 9,000 fingers. | job=Candle Lighter
Mother Nox | s | High priestess. Hosts a lovely potluck, actually. | job=High Priestess; trait=SOFTIE
Hoodsworth | t | Has never once taken off the hood. Shy. |
Chanty | x | Leads chants. Mostly about snacks. | job=Chant Leader
Vesper | h | Wears thorns as a hat. Says it is cosy. | halo=THORNS
Moonchild | s | Howls at the moon. The moon howls back, she says. | species=MOTHKIN
""" ),

A("goth_gremlins", "hell", "Goth Gremlin|Gloom Merchant|Spider Keeper",
  sp="BATLING:2 IMP:2 GHOUL MOTHKIN",
  top="DRESS:3 CLOAK:2 HOODIE", bottom="SKIRT:3 LEGGINGS LONG_SKIRT", shoes="PLATFORM:4 BOOTS",
  hat="WITCH:2 NONE:2", acc="SKULL_NECKLACE:3 CHAINS",
  pat="SKULLS:3 STRIPES:2", scheme="JET/GRAPE/WHITE CHARCOAL/MAGENTA/BONE JET/SLIME/WHITE",
  mark="EYELINER:4 TEARS:2", pers="2-9 6-13 2-9 2-9 8-15", act="WANDERING:2 HOME", body="TINY:3 AVERAGE",
  food="Black Licorice|Candy Eyeballs|Worm Gummies", hobby="Seances|Poetry|Bug Collecting:3", music="Goth Rock:5",
  phrase="Gloom on!|Dark and fluffy.|Bleh.|Hiss!",
  cast="""
Gloomlet | s | Tiny. Grumpy. Bites ankles, then apologises. |
Dreadie | h | Sells umbrellas that keep the sun out. | job=Gloom Merchant
Arachna | t | Spider keeper. Knits each spider a little web. | job=Spider Keeper; trait=SOFTIE
Murkle | t | Hisses at sunshine and at cheerful songs. |
Sombra | s | Sighs dramatically every eleven minutes. | species=MOTHKIN
Grumplet | h | Rated every sunset one star. |
""" ),

A("petty_villains", "hell", "Petty Villain|Sandwich Thief|Line Cutter|Spoiler",
  sp="IMP:3 GHOUL RACCOONKIN HUMAN",
  top="SUIT:2 CLOAK:2 HOODIE", bottom="PANTS:3 SKIRT", shoes="BOOTS:2 COMBAT",
  hat="TOP_HAT:2 NONE:2", acc="CAPE:2 MASK MONOCLE",
  pat="STRIPES:2 SOLID:2", scheme="JET/CHERRY/WHITE GRAPE/LIME/JET NAVY/MAGENTA/WHITE",
  pers="7-13 10-15 9-15 3-10 10-15", act="WANDERING:3", eyes="ANGRY:2 SQUINT:2 DEMON_SLIT",
  fhair="HANDLEBAR:2 MUSTACHE NONE:3", mouth="SMIRK:4 GRIN:2 TEETH",
  food="Pizza|Burnt Toast|Lava Cake", hobby="Pranks:5", music="Industrial|Disco|Classical",
  phrase="Mwahaha!|Go to heck!|Rude!",
  cast="""
Snatchworth | h | Steals the last biscuit. From every packet. | job=Petty Villain; fhair=HANDLEBAR
Sandwichula | s | Sandwich thief. Leaves a thank-you note. | job=Sandwich Thief
Cutsworth | t | Line cutter. Cuts lines, then holds the door. | job=Line Cutter; trait=SOFTIE
Spoilio | h | Spoils the ending of every book. Even cookbooks. | job=Spoiler
Lady Litter | s | Drops crumbs on purpose. Feeds the pigeons. | species=RACCOONKIN
Dr. Meanie | s | Plans evil schemes. Forgets them at lunch. | species=HUMAN; skin=SKIN_FAIR
Squeakthief | h | Steals squeakers from dog toys. Hoards them. | species=IMP
""" ),

A("raccoon_bandits", "hell", "Trash Bandit|Raccoon Burglar|Lookout|Fence",
  sp="RACCOONKIN:6",
  fur="SILVER/CHARCOAL CHARCOAL/WHITE CARAMEL/CHOCOLATE",
  top="HOODIE:3 TSHIRT:2 LEATHER_JACKET", bottom="CARGO:3 SWEATS", shoes="SNEAKERS:3 BARE",
  hat="BEANIE:4 BANDANA:2", acc="MASK:3 BACKPACK:2 NONE",
  pat="STRIPES:4 SOLID", scheme="CHARCOAL/WHITE/LIME JET/SILVER/CHERRY NAVY/CHARCOAL/LEMON",
  pers="11-15 6-12 6-13 6-13 8-15", act="WANDERING:4", traits="NIGHT_OWL",
  food="Pizza:2|Burnt Toast|Popcorn|Fried Chicken", hobby="Pranks:3|Bug Collecting", music="Hardcore Punk|Chiptune|Jazz",
  phrase="Hehehe~|Mwahaha!|Hiss!",
  cast="""
Bandito | h | Steals from bins. Tidier than the bins were. | job=Trash Bandit; fam=bandit
Rummage | s | Burgles only spare buttons. Has 9,000. | job=Raccoon Burglar; fam=bandit
Masky | t | Takes off the mask. Underneath, another mask. | fam=bandit
Binbin | h | Lookout. Falls asleep on lookout duty. | job=Lookout
Swipey | s | Fence. Sells stolen spoons back to their owners. | job=Fence
Scraps | h | Saves the best trash for hungry kittens. | trait=SOFTIE; fam=bandit
""" ),

A("black_cats", "hell", "Bad Luck Charmer|Hex Hissler|Alley Cat",
  sp="CATFOLK:6",
  fur="JET/WHITE CHARCOAL/WHITE JET/BUBBLEGUM",
  top="DRESS:2 LEATHER_JACKET:2 CROP_TOP HOODIE", bottom="SKIRT:2 LEGGINGS RIPPED_JEANS", shoes="BOOTS:2 PLATFORM HEELS",
  hat="WITCH:2 NONE:3", acc="EARRINGS:2 SKULL_NECKLACE CHAINS",
  pat="SOLID:3 SKULLS", scheme="JET/LEMON/GRAPE JET/HELLFIRE/WHITE CHARCOAL/SLIME/JET",
  eyes="CAT:5 DEMON_SLIT:2", eye_col="LEMON:3 SLIME HELLFIRE",
  pers="6-12 7-13 4-11 4-11 7-14", act="WANDERING:3", traits="NIGHT_OWL",
  food="Sushi|Bone Broth|Meat Slabs|Milk", hobby="Pranks|Tarot Reading|Napping", music="Jazz|Goth Rock",
  phrase="Hiss!|Nyaa~|Stay cursed.|I bite. Cutely.",
  cast="""
Hexley Purr | s | Crosses your path, then crosses it again. | job=Bad Luck Charmer
Moggy | h | Knocks your cup off the table. Watches. |
Unlucky 13 | t | Thirteen toes. Thirteen siblings. Thirteen naps. |
Omenia Claw | s | Hisses at mirrors. Breaks seven a year. | job=Hex Hissler
Smudge | s | Alley cat. Shares her sardines with strays. | job=Alley Cat; trait=SOFTIE
Salem Snoot | h | Pretends to curse people. Mostly naps. |
""" ),

A("sewer_mice", "hell", "Sewer Merchant|Cheese Smuggler|Pipe Crawler",
  sp="MOUSEKIN:6",
  fur="CHARCOAL/SILVER SILVER/WHITE CARAMEL/WHITE CHARCOAL/BUBBLEGUM",
  top="HOODIE:2 OVERALLS:2 CLOAK TSHIRT", bottom="CARGO:3 PANTS", shoes="BOOTS:3 BARE",
  hat="BEANIE:2 BANDANA CAP NONE", acc="BACKPACK:2 MASK NONE",
  pat="SOLID:3 PLAID", scheme="FOREST/CHARCOAL/LEMON RUST/SLIME/BONE CHARCOAL/HONEY/LIME",
  mouth="TEETH:3 GRIN SMIRK", fangs="1:3 2", pers="11-15 6-13 5-12 5-12 7-14", act="WANDERING:3 SHOP",
  food="Pickles|Burnt Toast|Bone Broth|Worm Gummies", hobby="Bug Collecting|Pranks|Board Games", music="Jazz|Sea Shanties|Industrial",
  phrase="Squeak squeak!|Hehehe~|Hiss!",
  cast="""
Cheddarlock | h | Smuggles cheese across the Wall. Mostly brie. | job=Cheese Smuggler
Gnawbert | h | Chews holes in everything. Calls it a door. |
Sewerella | s | Sells shiny things she found in the pipes. | job=Sewer Merchant
Pipsqueak | t | Pipe crawler. Fits through any crack. Mostly. | job=Pipe Crawler
Whiskerton | x | Hoards crumbs for winter. Shares them in spring. | trait=SOFTIE
Ratatat | h | Drums on pipes at night. Neighbours hate it. |
""" ),

A("con_foxes", "hell", "Con Fox|Card Sharp|Snake Oil Seller",
  sp="FOXKIN:6",
  fur="RUST/BONE TANGERINE/WHITE CHARCOAL/WHITE",
  top="SUIT:4 LEATHER_JACKET", bottom="PANTS:4", shoes="BOOTS:2 HEELS",
  hat="TOP_HAT:2 NONE:2 FEZ", acc="MONOCLE:2 EARRINGS SUNGLASSES",
  pat="STRIPES:3 PLAID SOLID", scheme="BLOOD/JET/LEMON RUST/NAVY/LEMON CHARCOAL/HELLFIRE/BONE",
  pers="8-14 4-10 10-15 6-13 8-15", act="WANDERING:3 SHOP", eyes="LASHES:2 SQUINT:2 CAT",
  mouth="SMIRK:5 GRIN", food="Macarons|Coffee|Blood Oranges", hobby="Board Games:3|Pranks|Tarot Reading",
  music="Jazz:3|Disco", phrase="Hellooo!|Sulfur kisses!|Mwahaha!",
  cast="""
Swindlefox | h | Sells bridges. Has never owned a bridge. | job=Con Fox
Shuffles | s | Card sharp. Every card in the deck is an ace. | job=Card Sharp
Slickpaw | t | Smiles with all teeth and both eyebrows. |
Tonic Todd | h | Sells potions that are just juice. Good juice. | job=Snake Oil Seller
Foxglove | s | Picks your pocket, then returns it, polished. | trait=SOFTIE
Chameleo | t | Changes the story every time it is told. |
""" ),

A("bat_club", "hell", "Nightclub DJ|Bat Bartender|Club Promoter|Dancer",
  sp="BATLING:6",
  top="CROP_TOP:3 LEATHER_JACKET:2 BAND_TEE", bottom="SKIRT:2 LEGGINGS:2 RIPPED_JEANS", shoes="PLATFORM:4",
  hat="HEADPHONES:3 NONE:2", acc="SUNGLASSES:3 EARRINGS",
  pat="SOLID:3 FLAMES", scheme="MAGENTA/CYAN/JET JET/LIME/MAGENTA GRAPE/CYAN/WHITE",
  pers="10-15 6-12 10-15 5-12 8-15", act="CONCERT:4", traits="NIGHT_OWL", aura="NONE:3 GLITCH",
  food="Sulfur Soda|Bat Wing Chips|Popcorn", hobby="Dancing:4|Karaoke", music="Synthwave:3|Industrial:2|Disco",
  phrase="Slay!|Hellooo!|Big mood.",
  cast="""
DJ Echo | h | Spins records upside down. While hanging. | job=Nightclub DJ
Batrina | s | Bartender. Mixes drinks that glow in the dark. | job=Bat Bartender
Nightwing | t | Promoter. Hands out flyers that fly away. | job=Club Promoter
Flutterby | s | Dancer. Dances with her eyes closed. | job=Dancer
Squeakbeat | s | Makes beats from bat squeaks. Heavy bass. |
Wingding | h | Throws parties every night. Cleans up every day. | trait=SOFTIE
""" ),

A("bone_jesters", "hell", "Bone Jester|Heckler|Prank Caller",
  sp="SKELETON:4 IMP",
  top="SUIT TSHIRT:2 DRESS", bottom="PANTS:2 KILT SHORTS", shoes="BOOTS SNEAKERS",
  hat="PARTY:3 TOP_HAT NONE", acc="BOWTIE:2 NONE",
  pat="POLKA:2 STRIPES:2 SKULLS:2", scheme="JET/LIME/WHITE BLOOD/LEMON/JET GRAPE/TANGERINE/BONE",
  eyes="X:3 CROSS:2 SPIRAL HOLLOW", mark="CLOWN:2 SKULL_PAINT:2 NONE", nose="CLOWN:2 SKULL:2",
  pers="10-15 10-15 13-15 8-15 12-15", act="CONCERT:2 WANDERING:2",
  food="Popcorn|Candy Eyeballs|Bone Broth", hobby="Pranks:5|Karaoke", music="Disco|Hardcore Punk|Chiptune",
  phrase="Rattle rattle!|Mwahaha!|Hehehe~",
  cast="""
Funny Bone | h | Jester. Every joke ends with his jaw falling off. | species=SKELETON; eyes=X
Heckla | s | Heckles performers. Then claps the loudest. | job=Heckler; eyes=CROSS
Ringring | t | Prank caller. "Is your fridge running?" Daily. | job=Prank Caller
Knick Knack | h | Plays his own bones like a xylophone. Off-key. |
Ticklebone | h | Tickles people with a feather. Unhinged. | species=IMP
""" ),

A("hell_chefs", "hell", "Brimstone Cook|Hot Sauce Chef|Grill Imp",
  sp="IMP:3 HELLHOUND GHOUL",
  top="TSHIRT:2 TANK:2 OVERALLS", bottom="PANTS:3", shoes="CLOGS:2 COMBAT",
  hat="CHEF:4 BANDANA:2", acc="NONE:4",
  pat="FLAMES:4 SOLID", scheme="BLOOD/HELLFIRE/WHITE JET/HELLFIRE/LEMON CHARCOAL/CHERRY/WHITE",
  pers="11-15 9-15 11-15 4-11 9-15", act="CAFE:4",
  food="Hell Peppers:4|Lava Cake:2|Meat Slabs|Brimstone Bagels", hobby="Baking|Pranks", music="Doom Metal|Hardcore Punk|Industrial",
  phrase="Sulfur kisses!|Mwahaha!|Feed me souls.",
  cast="""
Chef Scorch | h | His hot sauce melted a spoon. He was thrilled. |
Sizzlefang | s | Grills meat with her breath. Saves on gas. | species=IMP; job=Grill Imp
Ghostpepper | t | Hot sauce chef. Rates sauces in screams. | job=Hot Sauce Chef; species=GHOUL
Lavalina | s | Bakes lava cakes with real lava. Warns no one. |
Charbroil | h | Burns everything. Calls it "the Hellvile way". | species=HELLHOUND
Smokehouse | t | Smokes cheese, fish and occasionally eyebrows. |
""" ),

A("lava_miners", "hell", "Lava Miner|Sulfur Digger|Ember Hauler|Foreman",
  sp="HELLHOUND:2 IMP:2 BEARKIN SKELETON",
  fur_BEARKIN="JET/WHITE CHOCOLATE/CARAMEL",
  top="OVERALLS:3 TANK:2", bottom="NONE:2 CARGO:2", shoes="COMBAT:3 BOOTS",
  hat="MARINE_HELMET:2 CAP:2 BANDANA", acc="GAS_MASK:2 NONE:3",
  pat="SOLID:3 CAMO", scheme="RUST/CHARCOAL/HONEY CHARCOAL/HELLFIRE/LEMON JET/TANGERINE/BONE",
  pers="4-10 8-14 2-8 4-11 3-10", act="WORKSHOP:2 WANDERING", body="BUFF:3 STOCKY:3",
  stats="hygiene=40-60", food="Meat Slabs|Bone Broth|Burnt Toast", hobby="Napping|Welding|Board Games",
  music="Sea Shanties|Doom Metal|Industrial", phrase="Big mood.|Doom and gloom!|Ugh, fine.",
  cast="""
Cindersmith | h | Mines lava with a teaspoon. Very patient. |
Pickaxe Pam | s | Sulfur digger. Smells like eggs. Proud of it. | job=Sulfur Digger
Ember Ed | h | Hauls embers in a wheelbarrow. Sings shanties. | job=Ember Hauler
Magma Mo | t | Foreman. Gives everyone the afternoon off. Secretly. | job=Foreman; trait=SOFTIE
Slagbear | h | A grizzly with soot on every inch of fur. | species=BEARKIN
Obsidia | s | Polishes obsidian until she can see her mood. | species=SKELETON
""" ),

A("torment_eng", "hell", "Torment Engineer|Chain Polisher|Pitchfork Smith",
  sp="IMP:2 ROBOT GHOUL HELLHOUND",
  top="HAZMAT:2 OVERALLS:2 LAB_COAT", bottom="CARGO:2 NONE", shoes="COMBAT:3",
  hat="NONE:3 SKULL_CAP", acc="GAS_MASK:3 CHAINS:2",
  pat="SOLID:4", scheme="CHARCOAL/LEMON/JET SILVER/BLOOD/JET RUST/LEMON/CHARCOAL",
  pers="4-10 7-13 1-7 2-9 6-13", act="WORKSHOP:4",
  food="Coffee|Meat Slabs|Bat Wing Chips", hobby="Welding:5|Board Games", music="Industrial:5|Doom Metal",
  phrase="Rip and tear... gently.|Doom and gloom!|Big mood.",
  cast="""
Gearfang | h | Builds torment machines. They mostly tickle. |
Chainsley | s | Polishes chains until they jingle like bells. | job=Chain Polisher
Prongs | t | Pitchfork smith. Every fork has three prongs. Always. | job=Pitchfork Smith
Rackenstein | i | Torment bot. Gives terrible, honest feedback. | species=ROBOT; wings=MECH
Weldra | s | Welds spikes onto everything. Even teacups. | species=HELLHOUND
""" ),

A("contract_devils", "hell", "Contract Devil|Fine Print Writer|Soul Lawyer",
  sp="IMP:5 GHOUL",
  top="SUIT:5", bottom="PANTS:3 SKIRT", shoes="HEELS:2 BOOTS",
  hat="NONE:4 TOP_HAT", acc="GLASSES:2 MONOCLE",
  pat="STRIPES:3 SOLID", scheme="BLOOD/JET/LEMON JET/BLOOD/SILVER NAVY/HELLFIRE/WHITE",
  eyes="SPIRAL:2 DEMON_SLIT:3 GOAT", pers="5-11 4-10 6-13 2-8 6-13", act="SHOP:2 WANDERING",
  horns="DEVIL:3 DEVIL_TALL:2 RAM", food="Coffee|Soul Soup|Blood Oranges", hobby="Reading|Board Games", music="Jazz|Classical",
  phrase="Hail me!|Sulfur kisses!|Feed me souls.",
  cast="""
Signhere | h | Contract devil. Every contract is 900 pages. |
Finesse | s | Writes fine print so small ants need glasses. | job=Fine Print Writer
Clausewitz | t | Soul lawyer. Has never lost a case. Or a sock. | job=Soul Lawyer
Dotted Line | s | Carries a quill made of a cursed goose. |
Loophole | x | Finds loopholes, then quietly fixes them. | trait=SOFTIE
Notary Nick | h | Stamps contracts with his own horns. |
""" ),

A("tickle_fiends", "hell", "Tickle Fiend|Wedgie Specialist|Noogie Master",
  sp="IMP:3 BATLING",
  top="TSHIRT:2 HOODIE", bottom="SHORTS:2 PANTS", shoes="SNEAKERS:2 HOOVES",
  hat="NONE:3 PROPELLER", acc="NONE:3 MASK",
  pat="POLKA:2 STRIPES", scheme="CHERRY/LEMON/WHITE MAGENTA/LIME/WHITE BLOOD/BUBBLEGUM/LEMON",
  pers="12-15 10-15 13-15 10-15 13-15", act="WANDERING:4",
  food="Gumdrops|Candy Eyeballs|Worm Gummies", hobby="Pranks:6", music="Bubblegum Pop|Hardcore Punk",
  phrase="Mwahaha!|Hehehe~|I bite. Cutely.",
  cast="""
Ticklefang | t | Tickles until you laugh. Then a bit more. |
Wedgina | s | Wedgie specialist. Certified, framed on the wall. | job=Wedgie Specialist
Noogles | h | Noogie master. Gentle noogies only, he insists. | job=Noogie Master
Gigglefiend | i | Cannot stop laughing at its own pranks. | species=BATLING
""" ),

A("dmv", "hell", "DMV Clerk|Queue Manager|Number Caller",
  sp="GHOUL:3 SKELETON:2 MUMMY",
  top="SWEATER_VEST:3 SWEATER", bottom="PANTS:3 SKIRT", shoes="SNEAKERS CLOGS",
  hat="NONE:4", acc="GLASSES:3 NONE",
  pat="SOLID:3 PLAID", scheme="FOREST/BONE/CHERRY CHARCOAL/ECTOPLASM/WHITE NAVY/BONE/HONEY",
  eyes="SLEEPY:5 HOLLOW DROOPY", pers="0-2 6-12 0-4 1-8 2-9", act="SHOP:4",
  stats="energy=30-60",
  food="Coffee:3|Black Licorice|Burnt Toast", hobby="Napping:3|Board Games", music="Classical|Industrial",
  phrase="Bleh.|Ugh, fine.|No thank you!",
  cast="""
Number Nine | t | Calls "number nine". Has called it for 80 years. | job=Number Caller
Queuella | s | Makes queues out of thin air. A real talent. | job=Queue Manager
Snailpace | h | Moves at one inch per hour. On a good day. |
Formalda | s | Stamps forms in triplicate. Loses two. | species=MUMMY
Lunchbreak | s | Always on lunch. Since Tuesday. | species=SKELETON
Shutterly | h | Shuts the window the moment you reach it. | species=SKELETON
""" ),

A("rent_collect", "hell", "Rent Collector|Debt Imp|Repo Ghoul",
  sp="IMP:2 GHOUL HELLHOUND",
  top="SUIT:2 LEATHER_JACKET:2", bottom="PANTS:3", shoes="BOOTS:3",
  hat="TOP_HAT CAP NONE", acc="SUNGLASSES:2 CHAINS",
  pat="SOLID:3 STRIPES", scheme="CHARCOAL/CHERRY/SILVER JET/LIME/SILVER RUST/JET/LEMON",
  pers="5-11 11-15 3-9 1-7 5-12", act="WANDERING:4",
  food="Coffee|Meat Slabs|Pickles", hobby="Board Games|Pranks", music="Industrial|Jazz",
  phrase="Go to heck!|No thank you!|Ugh, mortals.",
  cast="""
Knuckles | h | Knocks on doors with three knocks and a growl. | species=HELLHOUND
Duesy | s | Debt imp. Remembers every penny ever owed. | job=Debt Imp
Repossessa | t | Repo ghoul. Repossessed a cloud once. | job=Repo Ghoul; species=GHOUL
Overdue | h | Late fees for being early. Somehow. |
""" ),

A("hell_gym", "hell", "Gym Brute|Protein Demon|Personal Trainer",
  sp="HELLHOUND:2 IMP:2 BEARKIN HUMAN",
  fur_BEARKIN="CHOCOLATE/CARAMEL JET/WHITE",
  top="TANK:5 CROP_TOP", bottom="SHORTS:3 SWEATS:2", shoes="SNEAKERS:4",
  hat="NONE:3 BANDANA CAP", acc="NONE:4",
  pat="SOLID:3 FLAMES CAMO", scheme="BLOOD/JET/WHITE CHARCOAL/LIME/WHITE HELLFIRE/JET/SILVER",
  body="BUFF:6 TOP_HEAVY:2", build="12-15", height="8-14",
  pers="10-15 10-15 9-15 4-11 7-14", act="PARK:3",
  food="Meat Slabs:4|Fried Chicken|Milk", hobby="Yoga|Napping", music="Doom Metal:3|Hardcore Punk",
  phrase="Rip and tear... gently.|Hail me!|Big mood.",
  cast="""
Swoleguard | h | Lifts cars to impress nobody in particular. |
Proteina | s | Drinks shakes made of other shakes. | job=Protein Demon
Flexo | t | Flexes at their reflection. Reflection flexes back. |
Spotter | h | Personal trainer. Shouts, then hugs. | job=Personal Trainer; trait=SOFTIE
Gainz | h | Counts reps out loud, in a scream. | species=HUMAN; skin=SKIN_UMBER
Burly Bea | s | Bear-hugged a boulder. Boulder said ow. | species=BEARKIN
""" ),

]
