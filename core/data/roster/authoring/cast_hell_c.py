"""Hellvile archetypes, part C: the gate, the army, music, households,
fallen angels and things from beyond. (owner: roster; see seed.py)"""
from seedlib import A

ARCHETYPES = [

A("rivet_family", "hell", "Blacksmith|Gear Cutter|Imp Student|Retired Riveter",
  sp="IMP:4",
  skin="TANGERINE CHERRY ROSE HELLFIRE",
  top="OVERALLS:3 TANK", bottom="NONE:2 CARGO", shoes="COMBAT:2 BOOTS",
  hat="BANDANA:3 NONE", acc="GAS_MASK NONE:2",
  pat="SOLID:3", scheme="NAVY/TANGERINE/LEMON CHARCOAL/HELLFIRE/SILVER RUST/NAVY/LEMON",
  pers="5-11 9-15 6-12 5-12 6-13", act="WORKSHOP:3 HOME",
  food="Hell Peppers|Meat Slabs|Brimstone Bagels", hobby="Welding:5", music="Doom Metal|Industrial|Folk",
  phrase="Rip and tear... gently.|Big mood.|Fluff yeah!",
  cast="""
Anvil | s | Rivet's mum. Still sets a place for her at dinner. | fam=rivetfam; wed=Sprocket; trait=SOFTIE
Sprocket | h | Rivet's dad. Pretends not to read her letters. | job=Gear Cutter; fam=rivetfam; bio_pronoun_ok=1
Bolt | t | Rivet's little sibling. Wants to weld like her. | job=Imp Student; fam=rivetfam; trait=KID; bio_pronoun_ok=1
Nana Clank | s | Taught Rivet to weld. Brags about her to everyone. | job=Retired Riveter; fam=rivetfam; trait=ELDER
""" ),

A("gate_laborers", "hell", "Gate Laborer|Vapor Pumper|Cloud Hauler|Chain Gang Cook",
  sp="IMP:3 GHOUL:2 SKELETON:2 HELLHOUND BATLING RACCOONKIN MOUSEKIN",
  fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL", fur_MOUSEKIN="CHARCOAL/SILVER SILVER/WHITE",
  top="OVERALLS:4 TANK:2 TSHIRT", bottom="NONE:2 CARGO:2", shoes="BOOTS:3 BARE",
  hat="CAP:2 BANDANA:2 NONE", acc="GAS_MASK:3 CHAINS NONE",
  pat="SOLID:4", scheme="CHARCOAL/VAPE_PURPLE/BONE RUST/CHARCOAL/LEMON JET/GRAPE/BONE",
  pers="1-7 2-8 2-8 4-11 2-9", act="WORKSHOP:2 HOME:2", stats="happy=35-60 energy=40-65",
  aura="SMOKE:2 NONE:4", cheeks="BLUSH:30 BANDAGE:30 SCAR:15",
  food="Bone Broth|Burnt Toast|Soul Soup|Pancakes", hobby="Napping:3|Singing|Board Games|Knitting", music="Sea Shanties:3|Folk|Hymns",
  phrase="Big mood.|Ugh, fine.|Bless your heart!|Doom and gloom!", fear="Being Alone|Silence|Thunder",
  cast="""
Shackles | h | Pumps vapor at the gate. Dreams of a garden. | job=Vapor Pumper; trait=SOFTIE
Ploddy | t | Hauls clouds all night. Hums lullabies to them. | job=Cloud Hauler; trait=SOFTIE
Gruel Grace | s | Chain gang cook. Sneaks extra sugar in the gruel. | job=Chain Gang Cook; trait=SOFTIE
Tallyworn | h | Counts the days until the Wall comes down. | trait=SOFTIE
Sootkin | x | Hides a sunflower under the gate. Waters it daily. | species=MOUSEKIN; trait=SOFTIE
Weary Wil | h | Has shovelled smoke for 300 years. Still polite. | species=SKELETON; trait=SOFTIE
Grindle | s | Works double shifts so the young ones can rest. | species=HELLHOUND; trait=SOFTIE
Bindle | t | Keeps a bindle packed for the day they can leave. | species=RACCOONKIN; trait=SOFTIE
Wrenchley | h | Grumbles at everything. Shares his lunch anyway. | species=GHOUL
Fumebottom | h | Coughs a lot. Says he is fine. He is not fine. | species=BATLING; trait=VAPER
Clinker | s | Gate laborer. Writes poems on the gate in chalk. | trait=SOFTIE
Drudge | t | Works, sleeps, works. Wants to see a beach. | species=GHOUL
""" ),

A("vapor_knights", "hell", "Vapor Knight|Smoke Lancer|Cloud Captain",
  sp="IMP:2 HELLHOUND SKELETON GHOUL",
  traits="LOYALIST", top="PLATE_ARMOR:5 ARMOR", bottom="NONE:3 PANTS", shoes="BOOTS:3 COMBAT",
  hat="HORNED_HELMET:3 NONE:2", acc="CAPE:4 NONE",
  pat="SOLID:4 FLAMES", scheme="JET/VAPE_PURPLE/SILVER CHARCOAL/HELLFIRE/SILVER JET/MAGENTA/LEMON",
  wings="FLAME:3 BAT:2 DRAGON", aura="FLAMES:3 SMOKE:2", halo="FLAME_RING:1 NONE:4",
  pers="5-11 9-15 1-7 0-5 6-13", act="WANDERING:3", body="BUFF:3 TOP_HEAVY:2", height="9-15",
  food="Meat Slabs|Soul Soup|Hell Peppers", hobby="Board Games|Welding", music="Doom Metal:5",
  phrase="Hail me!|Rip and tear... gently.|Doom and gloom!", fear="Holy Water|Exorcisms|Church Bells",
  cast="""
Smogsalot | h | Vapor knight. Charges into battle coughing. |
Dame Scorch | s | Smoke lancer. Has jousted a thundercloud. | job=Smoke Lancer
Capt. Cumul | t | Cloud captain. Commands a fleet of grumpy clouds. | job=Cloud Captain
Blazeguard | h | Guards Vapula's door. Nobody knocks. | wings=FLAME; aura=FLAMES
Ashknight | s | Wears armour to bed. Very uncomfortable. | species=SKELETON
Brimhelm | s | Polishes her horned helmet every hour. |
""" ),

A("gate_guards", "hell", "Gate Guard|Watchtower Imp|Wall Sentry",
  sp="HELLHOUND:2 SKELETON:2 IMP GHOUL",
  top="ARMOR:4 LEATHER_JACKET", bottom="CARGO:2 PANTS", shoes="COMBAT:4",
  hat="MARINE_HELMET:2 HORNED_HELMET:2 NONE", acc="NONE:3 SUNGLASSES",
  pat="CAMO:3 SOLID:2", scheme="CHARCOAL/BLOOD/BONE FOREST/CHARCOAL/LEMON JET/RUST/BONE",
  pers="2-8 8-14 1-6 2-8 3-10", act="WANDERING:4",
  food="Meat Slabs|Coffee|Bone Broth", hobby="Board Games|Napping", music="Doom Metal|Industrial",
  phrase="No thank you!|Go to heck!|Ugh, mortals.",
  cast="""
Halt | h | Says "halt". Says nothing else. Ever. |
Watchtower | s | Watches Hillvile through a spyglass. Jealous. | job=Watchtower Imp; species=IMP
Sentrina | t | Wall sentry. Waves back at Hillvile kids. Quietly. | job=Wall Sentry; trait=SOFTIE; bff=Lookout Lou
Gatebone | h | Rattles the gate key to scare pigeons. | species=SKELETON
Spearmint | s | Polishes her spear with mint for fresh breath. |
Bunkerton | h | Lives in a bunker. Comes out for parades. | species=GHOUL
""" ),

A("hell_nobles", "hell", "Hell Baroness|Duke of Smog|Marquis of Mire|Viscount",
  sp="IMP:2 CACOBLOB GHOUL BATLING",
  traits="LOYALIST", top="SUIT:2 DRESS:2 CLOAK", bottom="PANTS:2 LONG_SKIRT", shoes="HEELS:2 BOOTS",
  hat="CROWN:3 TIARA TOP_HAT", acc="CAPE:3 MONOCLE",
  pat="SOLID:3 STRIPES", scheme="BLOOD/LEMON/JET GRAPE/LEMON/BLOOD JET/LEMON/MAGENTA",
  pers="2-8 8-14 4-11 0-6 6-13", act="HOME:4",
  food="Lava Cake|Blood Oranges|Soul Soup", hobby="Board Games|Tarot Reading|Painting", music="Classical:4",
  phrase="Hail me!|Ugh, mortals.|No thank you!",
  cast="""
Lady Bile | s | Owns half of Hellvile. Wants the other half. | job=Hell Baroness
Duke Smog | h | Duke of Smog. Wears a monocle on each eye. | job=Duke of Smog
Marquis | h | Lives in a swamp castle. Smells royal. | job=Marquis of Mire
Viscountess | s | Never touches the ground. Carried everywhere. | job=Viscount; species=BATLING
Lord Lurk | h | Lurks at parties. Is the party. | species=CACOBLOB
""" ),

A("telemarketers", "hell", "Telemarketer Imp|Cold Caller|Survey Demon",
  sp="IMP:3 GHOUL BATLING",
  top="SWEATER_VEST:2 SUIT TSHIRT", bottom="PANTS:3", shoes="SNEAKERS:2",
  hat="HEADPHONES:5", acc="NONE:3 GLASSES",
  pat="SOLID:3 STRIPES", scheme="NAVY/LIME/WHITE CHARCOAL/MAGENTA/WHITE JET/LEMON/WHITE",
  pers="10-15 4-10 9-15 6-13 7-14", act="HOME:3",
  food="Coffee:3|Popcorn", hobby="Karaoke|Board Games", music="City Pop|Synthwave",
  phrase="Hellooo!|No thank you!|Big mood.",
  cast="""
Callbacky | h | Calls during dinner. Every dinner. Everyone's. |
Holdmusic | s | Puts you on hold, then sings the hold music. | job=Cold Caller
Survia | t | Survey demon. "On a scale of one to doom..." | job=Survey Demon
Dialtone | t | Hangs up first. Every time. Proudly. |
Ringaling | s | Has called every phone in Hillvile. Twice. | species=BATLING
""" ),

A("parking", "hell", "Parking Warden|Tow Truck Imp|Ticket Writer",
  sp="IMP:2 GHOUL HELLHOUND",
  top="SWEATER_VEST:2 LEATHER_JACKET", bottom="PANTS:3", shoes="BOOTS:3",
  hat="TRAFFIC_CONE:2 CAP:3", acc="SUNGLASSES:2 NONE",
  pat="STRIPES:3 SOLID", scheme="LEMON/JET/WHITE TANGERINE/CHARCOAL/LEMON CHARCOAL/LEMON/BLOOD",
  pers="6-12 10-15 2-8 0-6 6-13", act="WANDERING:4",
  food="Coffee|Brimstone Bagels|Donuts", hobby="Pranks|Board Games", music="Industrial|Classical",
  phrase="Go to heck!|Rude!|No thank you!",
  cast="""
Ticketina | s | Writes parking tickets for parked clouds. | job=Ticket Writer
Towzer | h | Tows anything. Towed a house once. | job=Tow Truck Imp
Meterman | t | Feeds the meters with other people's coins. |
Conehat | x | Wears a traffic cone. Directs traffic wrongly. | hat=TRAFFIC_CONE
""" ),

A("bullies", "hell", "Schoolyard Bully|Lunch Money Imp|Spitball Ace",
  sp="IMP:2 HELLHOUND GHOUL RACCOONKIN",
  fur_RACCOONKIN="CHARCOAL/WHITE",
  traits="KID", body="STOCKY:2 TINY ROUND", height="1-4",
  top="HOODIE:2 BAND_TEE TSHIRT", bottom="SHORTS:2 RIPPED_JEANS", shoes="SNEAKERS:3",
  hat="CAP:2 NONE:2", acc="NONE:3 BACKPACK",
  pat="SOLID:3 FLAMES", scheme="BLOOD/JET/WHITE CHARCOAL/LIME/WHITE NAVY/CHERRY/LEMON",
  cheeks="BANDAGE:40 BLUSH:40", pers="10-15 12-15 8-14 6-13 9-15", act="PARK:3",
  food="Pizza|Candy Eyeballs|Fried Chicken", hobby="Pranks:4|Skateboarding", music="Hardcore Punk:3",
  phrase="Go to heck!|As if!|Hiss!",
  cast="""
Knucklehead | h | Pushes kids in puddles. Then fishes them out. | job=Schoolyard Bully; trait=SOFTIE
Spitwad | t | Spitball ace. Never misses. Always apologises. | job=Spitball Ace
Lunchbox | h | Takes lunch money. Buys everyone ice cream. | job=Lunch Money Imp
Wanda Wedge | s | Gives wedgies. Gets them back. Fair is fair. |
Noogie Noel | t | Hides homework, then helps you find it. | species=RACCOONKIN
""" ),

A("skate_punks", "hell", "Skate Punk|Ramp Builder|Grind Gremlin",
  sp="RACCOONKIN:2 IMP CATFOLK HELLHOUND",
  fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL", fur_CATFOLK="JET/WHITE",
  top="BAND_TEE:3 HOODIE:2 TANK", bottom="RIPPED_JEANS:3 SHORTS", shoes="SNEAKERS:4",
  hat="BEANIE:2 CAP:2 NONE", acc="NONE:3 CHAINS",
  pat="FLAMES:2 SKULLS SOLID", scheme="JET/LIME/WHITE CHARCOAL/TANGERINE/WHITE GRAPE/CYAN/WHITE",
  cheeks="BANDAGE:50 SCAR:20", pers="13-15 8-14 7-14 7-14 9-15", act="PARK:4",
  food="Pizza|Tacos|Sulfur Soda", hobby="Skateboarding:6|Graffiti", music="Hardcore Punk:5",
  phrase="Big mood.|As if!|Whatever.",
  cast="""
Ollie Grind | h | Skates down the cathedral steps. Every Sunday. |
Rampage | s | Ramp builder. Built a ramp to the moon. Almost. | job=Ramp Builder
Gremgrind | t | Grind gremlin. Grinds rails, teeth and coffee. | job=Grind Gremlin; species=IMP
Kickflop | h | Fails every trick. Gets up laughing. | species=CATFOLK; trait=SOFTIE
Bailey Bail | s | Bails so well it counts as a trick. | species=HELLHOUND
""" ),

A("hell_parents", "hell", "Stay-home Ghoul|Night Shift Worker|Crypt Keeper|Imp Student",
  sp="GHOUL:2 IMP:2 SKELETON BATLING HELLHOUND",
  top="SWEATER:2 HOODIE TSHIRT DRESS", bottom="PANTS:2 SKIRT SWEATS", shoes="SLIPPERS:3 BOOTS",
  hat="NONE:5", acc="NONE:4 GLASSES",
  pat="SOLID:3 SKULLS PLAID", scheme="GRAPE/BONE/WHITE FOREST/ROSE/BONE CHARCOAL/PEACH/WHITE JET/LAVENDER/WHITE",
  pers="3-10 6-13 4-11 5-12 5-12", act="HOME:5",
  food="Soul Soup|Spaghetti|Bone Broth|Pizza|Curry", hobby="Knitting|Board Games|Gardening|Video Games",
  music="Goth Rock|Jazz|Classical|Folk",
  cast="""
Mama Morgue | s | Runs a tidy crypt. Ten coffins, all made. | fam=morgue; wed=Papa Morgue
Papa Morgue | h | Tells the worst dad jokes in all of Hellvile. | fam=morgue
Morguelet | t | Wants to be a Hillvile florist. Keeps it secret. | job=Imp Student; fam=morgue; trait=KID,SOFTIE
Ma Gloom | s | Works nights at the gate. Packs everyone lunch. | job=Night Shift Worker; fam=gloomwood; wed=Pa Gloom; trait=SOFTIE
Pa Gloom | h | Crypt keeper. Keeps the crypt and the snacks. | job=Crypt Keeper; fam=gloomwood
Wisp | t | Floats around the house making ghost noises. | job=Imp Student; fam=gloomwood; trait=KID; species=BATLING
Auntie Ash | s | Visits every weekend, unannounced, with stew. | fam=gloomwood; species=HELLHOUND
Bonesworth | h | Retired bone setter. Sets bones, sets tables. | job=Retiree; trait=ELDER; species=SKELETON; fam=bones
Little Ulna | s | Collects other people's lost teeth. Polite. | job=Imp Student; trait=KID; species=SKELETON; fam=bones
Gristlegran | h | Cooks for the whole crypt block on Sundays. | trait=ELDER,SOFTIE; fam=bones
""" ),

A("thorn_farmers", "hell", "Thorn Farmer|Pepper Grower|Nightshade Picker",
  sp="IMP:2 GHOUL FROGLING HELLHOUND",
  skin_FROGLING="FOREST TEAL",
  top="OVERALLS:4 TANK", bottom="NONE:2 CARGO", shoes="BOOTS:4",
  hat="BUCKET:2 COWBOY:2 NONE", acc="NONE:3",
  pat="PLAID:3 SOLID", scheme="FOREST/BLOOD/BONE RUST/LIME/BONE CHARCOAL/HELLFIRE/BONE",
  pers="2-8 6-12 2-9 6-13 4-11", act="PARK:3",
  food="Hell Peppers:3|Blood Oranges|Pickles", hobby="Gardening:5", music="Folk:3|Doom Metal",
  phrase="Sulfur kisses!|Big mood.|Okie dokie!",
  cast="""
Thornton | h | Grows roses with thorns on the petals too. |
Pepperella | s | Pepper grower. Her peppers glow at night. | job=Pepper Grower
Nightshade | t | Nightshade picker. Wears thick gloves. Smart. | job=Nightshade Picker
Bramblefang | h | Grows a hedge maze. Gets lost in it weekly. | species=HELLHOUND
Mossmouth | t | Swamp farmer. Grows lilies for the Hillvile pond. | species=FROGLING; trait=SOFTIE
""" ),

A("bone_ball", "hell", "Bone Ball Player|Skull Kicker|Ref Demon",
  sp="SKELETON:2 HELLHOUND IMP",
  top="TSHIRT:3 TANK", bottom="SHORTS:4", shoes="SNEAKERS:3",
  hat="CAP:2 NONE:2", acc="NONE:3",
  pat="STRIPES:4", scheme="BLOOD/WHITE/JET JET/LIME/WHITE CHARCOAL/TANGERINE/WHITE",
  pers="12-15 9-15 10-15 7-14 8-15", act="PARK:4", body="BUFF:2 LANKY:2",
  food="Meat Slabs|Fried Chicken|Sulfur Soda", hobby="Skateboarding|Board Games", music="Hardcore Punk|Doom Metal",
  phrase="Rattle rattle!|Slay!|Hail me!",
  cast="""
Kickskull | h | Kicks skulls into goals. Asks the skulls first. | job=Skull Kicker
Femurfly | s | Throws her own femur for the team. | species=SKELETON
Whistlewick | t | Ref demon. Gives red cards for bad vibes. | job=Ref Demon; species=IMP
Goaliath | x | Biggest goalie in Hellvile. Never moves. | species=HELLHOUND; build=15
""" ),

A("smoke_sirens", "hell", "Smoke Siren|Torch Singer|Cabaret Ghoul",
  sp="GHOUL BATLING CATFOLK GHOST",
  fur_CATFOLK="JET/WHITE CHARCOAL/WHITE",
  top="DRESS:4", bottom="LONG_SKIRT:2 NONE", shoes="HEELS:4",
  hat="NONE:3", acc="EARRINGS:3",
  pat="SOLID:3", scheme="BLOOD/JET/LEMON VAPE_PURPLE/JET/SILVER JET/MAGENTA/SILVER",
  mouth="LIPSTICK:4 SMIRK", aura="SMOKE:3 NONE", pers="2-8 4-10 9-15 4-11 4-11", act="CONCERT:4",
  food="Blood Oranges|Macarons|Coffee", hobby="Singing:5", music="Jazz:5|Goth Rock",
  phrase="Sulfur kisses!|Smooch!|Slay!",
  cast="""
Smokevelvet | s | Sings so smoky the fire alarm applauds. |
Torchlight | t | Torch singer. Sings while holding a torch. Hot. | job=Torch Singer
Cabaretta | s | Cabaret ghoul. Kicks so high she floats. | job=Cabaret Ghoul; species=GHOUL
Lullabane | t | Sings dark lullabies that help ghosts sleep. | species=GHOST; trait=SOFTIE
""" ),

A("fallen_angels", "hell", "Fallen Angel|Halo Pawnbroker|Cloud Dropout",
  sp="CHERUB:3 HUMAN MOTHKIN",
  skin="SKIN_PORCELAIN SKIN_FAIR SKIN_BROWN SKIN_UMBER", skin_MOTHKIN="CHARCOAL BONE",
  top="ROBE:2 LEATHER_JACKET:2 CLOAK", bottom="NONE:2 RIPPED_JEANS ROBE", shoes="BARE:2 BOOTS",
  hat="NONE:5", acc="NONE:2 CHAINS ROSARY",
  pat="SOLID:4", scheme="JET/WHITE/LEMON CHARCOAL/LEMON/BLOOD BONE/JET/LEMON",
  horns="HALO_HORNS:4 BROKEN", halo="BROKEN:4 THORNS", wings="BONE:3 ANGEL:2 BAT",
  pers="1-7 6-12 3-10 3-10 6-13", act="WANDERING:3",
  food="Angel Cake:2|Black Licorice|Lava Cake", hobby="Poetry|Cloud Watching|Singing", music="Hymns|Goth Rock|Doom Metal",
  phrase="Doom and gloom!|Big mood.|Bless your heart!", fear="Heights|Church Bells|Holy Water",
  cast="""
Fallon | s | Fell from Cloud Nine. Literally. Still dizzy. |
Haloh No | h | Pawned his halo for a guitar. Regrets nothing. | job=Halo Pawnbroker
Cherubim | t | Cloud dropout. Kept the wings. Lost the manners. | job=Cloud Dropout
Seraphail | s | Sings hymns backwards. Still sounds lovely. | trait=SOFTIE
Lucinda | s | Glows a little when she is happy. Hides it. | species=MOTHKIN; trait=SOFTIE
""" ),

A("eldritch", "hell", "Thing From Beyond|Void Gazer|Tentacle Masseur",
  sp="GHOUL CACOBLOB SLIME",
  skin="GRAPE TEAL ECTOPLASM MAGENTA",
  top="ROBE:2 CLOAK NONE", bottom="NONE:3 ROBE", shoes="BARE:3",
  hat="NONE:4", acc="NONE:3",
  pat="SOLID:4", scheme="GRAPE/ECTOPLASM/JET TEAL/MAGENTA/JET JET/ECTOPLASM/GRAPE",
  hair="TENTACLES:5", hair_force="1", tail="TENTACLE:4 NONE", eye_count="3:3 1",
  mark="THIRD_EYE:3 CRACKS SCALES", eyes="VOID:3 GLOW:2 COMPOUND", mouth="VOID:3 WAVY",
  aura="GLITCH:2 SMOKE NONE", pers="0-6 4-10 0-6 4-11 13-15", act="WANDERING:3",
  food="Eyeball Jelly|Soul Soup|Sushi", hobby="Stargazing|Seances|Tarot Reading", music="Industrial|Classical|Doom Metal",
  phrase="The void says hi.|Feed me souls.|Spooky time!",
  cast="""
Glorbex | h | From beyond the stars. Here for the pastries. |
Voidra | s | Void gazer. The void gazes back. They are pals. | job=Void Gazer
Squidley | h | Tentacle masseur. Eight hands of comfort. | job=Tentacle Masseur; trait=SOFTIE
Nyarlo | t | Whispers in a language of wet clicks. Sweetly. | species=CACOBLOB
""" ),

A("fly_demons", "hell", "Fly Demon|Swamp Buzzer|Rot Sniffer",
  sp="IMP:2 MOTHKIN GHOUL",
  skin_MOTHKIN="CHARCOAL BONE",
  top="TSHIRT:2 HOODIE", bottom="SHORTS:2 CARGO", shoes="SNEAKERS BARE",
  hat="NONE:4", acc="NONE:3 GAS_MASK",
  pat="SOLID:3 CAMO", scheme="FOREST/SLIME/JET CHARCOAL/LIME/BONE JET/CHARTREUSE/WHITE",
  wings="FLY:5", aura="FLIES:4 NONE", eyes="COMPOUND:4 GLOW",
  pers="12-15 8-14 6-12 5-12 9-15", act="WANDERING:3", stats="hygiene=25-55",
  food="Worm Gummies|Grave Dirt Cake|Pickles", hobby="Bug Collecting:4|Pranks", music="Industrial|Hardcore Punk",
  phrase="Hiss!|Bleh.|Spooky time!",
  cast="""
Buzzlebub | h | Buzzes in your ear at 3am. Means well. |
Swatsy | s | Has been swatted 400 times. Undefeated. |
Maggotina | t | Rot sniffer. Finds the bad apple every time. | job=Rot Sniffer
Droney | t | Swamp buzzer. Hums one note, forever. | job=Swamp Buzzer; species=MOTHKIN
""" ),

A("doom_band", "hell", "Doom Guitarist|Shriek Vocalist|Bassist|Doom Drummer",
  sp="IMP:2 SKELETON GHOUL HELLHOUND BATLING",
  top="BAND_TEE:4 LEATHER_JACKET:2", bottom="RIPPED_JEANS:3 KILT", shoes="COMBAT:3 PLATFORM",
  hat="NONE:4 SKULL_CAP", acc="CHAINS:3 SKULL_NECKLACE",
  pat="SOLID:2 SKULLS:2 FLAMES", scheme="JET/HELLFIRE/BONE JET/BLOOD/SILVER CHARCOAL/SLIME/BONE",
  hair="LONG:5 WILD:2", mark="SKULL_PAINT:3 NONE:2 CRACKS", pers="7-13 9-15 9-15 5-12 10-15", act="CONCERT:4",
  food="Meat Slabs|Hell Peppers|Sulfur Soda", hobby="Metal Drumming:4|Singing", music="Doom Metal:8",
  phrase="Rip and tear... gently.|Hail me!|Doom and gloom!",
  cast="""
Riffmaw | h | Plays guitar so loud it tunes nearby church bells. | job=Doom Guitarist
Shriekra | s | Shriek vocalist. Gargles lava to warm up. | job=Shriek Vocalist
Bassment | t | Bassist. Lives in a basement. Plays in it too. | job=Bassist
Blastbeat | h | Drums so fast his arms are a blur. Literally. | job=Doom Drummer; species=SKELETON
Headbang | x | Headbangs so hard the skull came loose. Twice. | species=SKELETON
Moshpit | s | Starts a mosh pit at every lullaby recital. | species=HELLHOUND
""" ),

A("punk_band", "hell", "Punk Guitarist|Zine Screamer|Safety Pin Smith",
  sp="RACCOONKIN IMP GHOUL BATLING",
  fur_RACCOONKIN="CHARCOAL/WHITE",
  top="LEATHER_JACKET:3 BAND_TEE:2", bottom="RIPPED_JEANS:3 KILT", shoes="COMBAT:3",
  hat="NONE:4", acc="CHAINS:2 NOSE_RING:2",
  pat="PLAID:3 SOLID", scheme="JET/LIME/MAGENTA CHARCOAL/CHERRY/LEMON JET/CYAN/MAGENTA",
  hair="LIBERTY_SPIKES:4 MOHAWK:4", hair_force="1", hair_col="LIME:2 MAGENTA:2 CYAN HELLFIRE",
  pers="12-15 12-15 10-15 6-13 12-15", act="CONCERT:3 WANDERING",
  food="Pizza|Sulfur Soda|Burnt Toast", hobby="Graffiti:3|Metal Drumming|Skateboarding", music="Hardcore Punk:8",
  phrase="Go to heck!|As if!|Whatever.",
  cast="""
Spike McGee | h | Has 40 spikes of hair. Counts them nightly. |
Anarkitty | s | Punk guitarist. Plays three chords. Loudly. | job=Punk Guitarist; species=CATFOLK; fur=JET/WHITE
Safetypin | t | Safety pin smith. Holds the band together. | job=Safety Pin Smith
Screamo | t | Screams lyrics about recycling and kindness. | job=Zine Screamer; trait=SOFTIE
Mohawkins | h | His mohawk is taller than he is. | hair=MOHAWK
""" ),

A("shock_radio", "hell", "Shock Jock|Radio Imp|Static Ghost",
  sp="IMP GHOST GHOUL",
  top="HOODIE:2 LEATHER_JACKET", bottom="PANTS:2", shoes="SNEAKERS:2",
  hat="HEADPHONES:5", acc="NONE:2 SUNGLASSES",
  pat="SOLID:3", scheme="JET/HELLFIRE/WHITE CHARCOAL/CYAN/WHITE GRAPE/LIME/WHITE",
  pers="9-15 13-15 12-15 6-13 11-15", act="HOME:3", traits="NIGHT_OWL",
  food="Coffee:3|Sulfur Soda", hobby="Karaoke|Pranks", music="Industrial|Synthwave|Hardcore Punk",
  phrase="Hellooo!|Big mood.|Go to heck!",
  cast="""
DJ Doomsday | h | Shock jock. Predicts the end of the world daily. |
Static | t | Static ghost. Lives between radio stations. | job=Static Ghost; species=GHOST
Feedbacka | s | Radio imp. Screeches the weather report. | job=Radio Imp
Callerline | s | Takes listener calls. Every caller is her mum. |
""" ),

A("graffiti", "hell", "Graffiti Imp|Wall Tagger|Stencil Artist",
  sp="IMP:2 RACCOONKIN BATLING",
  fur_RACCOONKIN="CHARCOAL/WHITE SILVER/CHARCOAL",
  top="HOODIE:4 BAND_TEE", bottom="CARGO:2 RIPPED_JEANS", shoes="SNEAKERS:4",
  hat="BEANIE:2 CAP:2", acc="MASK:2 BACKPACK",
  pat="SOLID:2 CAMO STRIPES", scheme="JET/LIME/MAGENTA CHARCOAL/CYAN/LEMON NAVY/MAGENTA/LIME",
  pers="10-15 6-12 7-14 7-14 8-15", act="WANDERING:4", traits="NIGHT_OWL",
  food="Pizza|Popcorn|Sulfur Soda", hobby="Graffiti:6|Painting", music="Hardcore Punk|Synthwave",
  phrase="Big mood.|Slay!|Whatever.",
  cast="""
Spraycan | h | Tags every wall with a tiny smiling skull. |
Tagalong | s | Wall tagger. Signs her name in bubble letters. | job=Wall Tagger
Stencilla | t | Stencil artist. Paints hearts on the Wall at night. | job=Stencil Artist; trait=SOFTIE
Drippy | h | Paints drippy letters. Also drips. |
Muralfang | h | Painted a 100-foot dragon. Mostly tail. | species=RACCOONKIN
""" ),

A("guard_dogs", "hell", "Guard Dog|Yard Hound|Fence Patroller",
  sp="PUPKIN:2 HELLHOUND:2",
  fur_PUPKIN="JET/CARAMEL CHOCOLATE/HONEY CHARCOAL/WHITE",
  top="LEATHER_JACKET:2 TANK TSHIRT", bottom="CARGO:2 PANTS", shoes="BOOTS:2 BARE",
  hat="NONE:4", acc="CHAINS:3 NONE",
  pat="SOLID:4", scheme="JET/HONEY/SILVER CHARCOAL/BLOOD/WHITE CHOCOLATE/LIME/WHITE",
  pers="8-14 9-15 6-13 4-11 5-12", act="WANDERING:3",
  food="Meat Slabs:3|Bone Broth", hobby="Napping:3|Pranks", music="Hardcore Punk|Doom Metal",
  phrase="Go to heck!|Tail wags!|Hiss!",
  cast="""
Fido Fiend | h | Guard dog. Barks at the wind, the moon, himself. |
Yardley | s | Yard hound. Digs holes to hide her feelings. | job=Yard Hound
Fencepost | t | Patrols the fence. Wags at everyone who passes. | job=Fence Patroller; trait=SOFTIE
Grrrnest | h | Growls in his sleep. Snores in his growls. | species=HELLHOUND
""" ),

A("creepy_clowns", "hell", "Creepy Clown|Balloon Popper|Funhouse Owner",
  sp="HUMAN:2 IMP GHOUL SKELETON",
  skin="SKIN_PORCELAIN SKIN_FAIR",
  top="OVERALLS:2 SUIT TSHIRT", bottom="PANTS:3 TUTU", shoes="BOOTS PLATFORM:2",
  hat="PARTY:2 TOP_HAT NONE", acc="BOWTIE:3 NONE",
  pat="POLKA:3 STRIPES:2", scheme="JET/BLOOD/WHITE GRAPE/LIME/WHITE BLOOD/LEMON/JET",
  mark="CLOWN:6", nose="CLOWN:5 ROUND", mark_col="BLOOD JET WHITE",
  eyes="WIDE:3 SPIRAL:2 GLOW", mouth="BIG_GRIN:4 TEETH:2",
  pers="8-14 9-15 13-15 6-13 14-15", act="WANDERING:2 PARK",
  food="Cotton Candy|Popcorn|Candy Eyeballs", hobby="Pranks:4|Bubble Blowing", music="Disco|Industrial",
  phrase="Hehehe~|Mwahaha!|Spooky time!",
  cast="""
Giggleguts | h | Laughs at nothing, for hours, in the dark. |
Poppit | s | Balloon popper. Pops balloons, apologises. | job=Balloon Popper
Funhouse | t | Owns the funhouse. Every mirror is mean. | job=Funhouse Owner
Honkers | h | Honks his nose at funerals. Not invited back. |
Mr. Jinglez | h | Jingles when he walks. You always hear him. | species=SKELETON
""" ),

A("hell_dentists", "hell", "Fang Dentist|Tooth Collector|Drill Imp",
  sp="IMP:2 GHOUL",
  top="LAB_COAT:3", bottom="PANTS:2", shoes="CLOGS:2",
  hat="NONE:3", acc="MASK:2 GLASSES",
  pat="SOLID:3", scheme="WHITE/BLOOD/JET WHITE/SLIME/JET",
  pers="6-12 7-13 5-11 3-10 9-15", act="HOSPITAL:3",
  food="Candy Eyeballs|Gumdrops", hobby="Skull Collecting|Bug Collecting", music="Industrial|Classical",
  phrase="I bite. Cutely.|Mwahaha!|Yikes.",
  cast="""
Drillsby | h | Fang dentist. Sharpens fangs. Brightens smiles. | job=Fang Dentist
Toothina | s | Tooth collector. Pays in nightmares. Small ones. | job=Tooth Collector
Molarbane | t | Drill imp. Hums the drill song. Nobody likes it. | job=Drill Imp
Flossie | t | Flosses everyone. Without asking. | species=GHOUL
""" ),

A("hell_hospital", "hell", "Hell Surgeon|Blood Bank Bat|Bone Setter|Plague Doctor",
  sp="GHOUL:2 BATLING SKELETON HUMAN",
  top="LAB_COAT:3 CLOAK NURSE", bottom="PANTS:2 SKIRT", shoes="BOOTS:2 CLOGS",
  hat="NONE:3 TOP_HAT", acc="MASK:3 GLASSES",
  pat="SOLID:4", scheme="WHITE/BLOOD/CHARCOAL JET/BONE/BLOOD CHARCOAL/SLIME/WHITE",
  pers="3-9 6-12 2-8 3-10 7-14", act="HOSPITAL:4",
  food="Blood Oranges|Bone Broth|Coffee", hobby="Reading|Bug Collecting", music="Classical|Industrial",
  phrase="Bleh.|Stay cursed.|Yikes.",
  cast="""
Scalpelia | s | Hell surgeon. Stitches in cursive. Elegant. | job=Hell Surgeon
Bloodbank | h | Blood bank bat. Very strict about withdrawals. | job=Blood Bank Bat; species=BATLING
Setbone | t | Bone setter. Sets bones in festive shapes. | job=Bone Setter; species=SKELETON
Plaguey | s | Plague doctor. Beak mask, kind eyes. | job=Plague Doctor; species=HUMAN; skin=SKIN_FAIR; accessory=MASK; trait=SOFTIE
Dr. Gurney | h | Wheels patients around for fun. Gets reported. |
""" ),

A("sin_bin", "hell", "Sin Bin Janitor|Brimstone Sweeper|Ash Collector",
  sp="GHOUL:2 IMP SLIME SKELETON HELLHOUND",
  skin_SLIME="SLIME CHARTREUSE",
  top="OVERALLS:4 TSHIRT", bottom="NONE:2 CARGO", shoes="BOOTS:3 BARE",
  hat="CAP:3 BANDANA", acc="GAS_MASK:2 NONE:2",
  pat="SOLID:4", scheme="CHARCOAL/LEMON/WHITE RUST/CHARCOAL/BONE FOREST/LEMON/CHARCOAL",
  pers="2-8 4-10 2-8 6-13 4-11", act="WANDERING:3",
  food="Burnt Toast|Bone Broth|Popcorn", hobby="Napping|Board Games|Singing", music="Sea Shanties|Folk|Industrial",
  phrase="Bleh.|Ugh, fine.|Big mood.",
  cast="""
Mopsworth | h | Sin bin janitor. Mops up sins. Wrings them out. |
Brushfire | s | Sweeps brimstone into piles shaped like hearts. | job=Brimstone Sweeper; trait=SOFTIE
Ashpan | t | Ash collector. Keeps the best ash in a jar. | job=Ash Collector
Gunkle | h | Unclogs the lava drains. Hums while he works. | species=SLIME
Binny | h | Empties the bins. Reads the letters in them. | species=SKELETON
Dustpan Dot | s | Has swept the same street for 600 years. | species=HELLHOUND
""" ),

]
