"""Hillvile archetypes, part B: fun, show business, nature, oddballs.
(owner: roster; see seed.py)"""
from seedlib import A

ARCHETYPES = [

A("ranch", "hill", "Ranch Hand|Rodeo Clown|Horse Whisperer",
  sp="PUPKIN:2 FOXKIN:2 BEARKIN HUMAN",
  top="TSHIRT:2 SWEATER_VEST OVERALLS", bottom="PANTS:3 CARGO", shoes="BOOTS:5",
  hat="COWBOY:6 BANDANA", acc="NONE:2 SCARF BANDAGES",
  pat="PLAID:4 SOLID", scheme="RUST/SKY/BONE CARAMEL/NAVY/WHITE CHERRY/CHOCOLATE/LEMON",
  pers="6-12 4-11 6-13 9-15 3-10", act="PARK:3 WANDERING",
  food="Tacos|Fried Chicken|Pancakes|Pickles", hobby="Fishing|Board Games|Singing|Dancing", music="Folk:5|Sea Shanties",
  phrase="Toot toot!|Tail wags!|Okie dokie!|Yippee!", pet="45",
  cast="""
Lasso Lou | h | Can lasso a cloud. Mostly lets them go. | species=PUPKIN
Dusty Spurs | s | Yodels at sunrise to wake the rooster up. | species=HUMAN
Buckaroo | t | Rodeo clown. Has never been bucked, only hugged. | job=Rodeo Clown; species=BEARKIN; marking=CLOWN; marking_color=CHERRY
Whinny | s | Horse whisperer. The horses whisper back. | job=Horse Whisperer; species=HUMAN; skin=SKIN_TAN
Tex Mex | h | Brings salsa to every party, invited or not. | species=FOXKIN; fur=RUST/BONE
""" ),

A("circus", "hill", "Clown|Juggler|Acrobat|Ringmaster",
  sp="PUPKIN:2 BUNNYKIN:2 HUMAN:2 BEARKIN PLUSHIE",
  top="OVERALLS:2 SUIT TSHIRT DRESS", bottom="PANTS:2 SHORTS TUTU", shoes="BOOTS:2 SNEAKERS PLATFORM",
  hat="PARTY:3 TOP_HAT:2 NONE PROPELLER", acc="BOWTIE:4 NONE",
  pat="POLKA:4 STRIPES:3", scheme="CHERRY/LEMON/SKY CYAN/TANGERINE/WHITE MAGENTA/LIME/LEMON",
  mark="CLOWN:5 STAR_FRECKLES CHEEK_HEARTS FUR", nose="CLOWN:5 ROUND", mark_col="CHERRY WHITE SKY",
  pers="10-15 3-10 12-15 11-15 9-15", act="PARK:3 CONCERT",
  eyes="HAPPY_ARC:3 STAR:2 WIDE:2", mouth="BIG_GRIN:4 GRIN:2 TONGUE",
  food="Cotton Candy:3|Popcorn:3|Gumdrops", hobby="Pranks:3|Dancing|Bubble Blowing|Cosplay", music="Disco:3|Bubblegum Pop|Chiptune",
  phrase="Hehehe~|Wheee!|Toot toot!|Wiggle wiggle!",
  cast="""
Giggles | s | Honks her nose to say hello. And goodbye. | species=HUMAN; skin=SKIN_FAIR; nose=CLOWN
Juggles | h | Juggles eggs. Makes a great omelette after. | species=HUMAN; job=Juggler
Tumbleweed | t | Acrobat who cartwheels to the shop. | job=Acrobat; species=BUNNYKIN
Ringmaster | h | Ringmaster. Introduces people like circus acts. | species=HUMAN; job=Ringmaster; hat=TOP_HAT
Squeaker | s | Squeaky shoes, squeaky nose, squeaky laugh. | species=PLUSHIE
Bozzlebee | h | Fits 20 friends in one tiny car. Somehow. | species=HUMAN; skin=SKIN_EBONY
""" ),

A("magicians", "hill", "Stage Magician|Card Trickster|Escape Artist",
  sp="RACCOONKIN:2 FOXKIN:2 CATFOLK HUMAN",
  top="SUIT:4 CLOAK", bottom="PANTS:4", shoes="BOOTS:2 HEELS",
  hat="TOP_HAT:4 FEZ:2", acc="CAPE:3 MONOCLE:2 BOWTIE",
  pat="SOLID:3 STRIPES", scheme="GRAPE/LEMON/WHITE NAVY/MAGENTA/LEMON JET/ROSE/WHITE",
  pers="5-12 1-7 8-15 6-13 6-13", act="CONCERT:2 PARK", aura="SPARKLES:3 NONE:2",
  food="Macarons|Coffee|Donuts", hobby="Board Games|Tarot Reading|Origami|Cosplay", music="Jazz:3|Classical",
  phrase="Sparkle on!|Teehee!|Oh my stars!",
  cast="""
Abraca | s | Pulls rabbits out of hats. The rabbits are friends. | species=RACCOONKIN
Prestochi | h | Makes coins vanish. Has never found them again. | species=FOXKIN
Hocus | t | Can make any frown disappear. | species=HUMAN; job=Card Trickster
Houdina | s | Escape artist. Escaped a hug once. Regrets it. | job=Escape Artist; species=HUMAN; skin=SKIN_OLIVE
Fezworth | h | Wears a fez. Says it holds all his secrets. | species=FOXKIN; hat=FEZ
""" ),

A("fortune", "hill", "Fortune Teller|Tarot Reader|Tea Leaf Reader",
  sp="MOTHKIN:2 CATFOLK HUMAN",
  top="ROBE:2 DRESS:2 KIMONO", bottom="LONG_SKIRT:3 NONE", shoes="SANDALS:2 SLIPPERS",
  hat="WITCH:3 BANDANA NONE", acc="EARRINGS:3 SCARF",
  pat="SOLID:2 POLKA", scheme="GRAPE/LEMON/LAVENDER TEAL/MAGENTA/LEMON LAVENDER/NAVY/WHITE",
  pers="1-6 1-5 6-12 6-13 8-15", act="SHOP:3", eyes="SLEEPY:2 SPARKLE STAR LASHES",
  mark="THIRD_EYE:2 STAR_FRECKLES:2 FUR", horns="NONE",
  food="Bubble Tea|Pudding|Mochi", hobby="Tarot Reading:5|Stargazing|Seances", music="Lo-fi Beats|Folk|Classical",
  phrase="Oh my stars!|Sparkle on!|Big mood.",
  cast="""
Madame Moth | s | Sees your future. It has snacks in it. | species=MOTHKIN
Crystalla | t | Owns 30 crystal balls. Uses them as paperweights. | species=HUMAN
Tea Leaf | h | Reads tea leaves. They usually say "more tea". | job=Tea Leaf Reader; species=HUMAN; skin=SKIN_BROWN
Omenia | t | Predicted the rain. Predicted it again. | species=GHOST; marking=THIRD_EYE
""" ),

A("sweetgoths", "hill", "Goth Poet|Candle Shop Owner|Cemetery Gardener|Bat Rescuer",
  sp="BATLING:3 GHOST:2 CATFOLK:2 HUMAN",
  fur="JET/WHITE CHARCOAL/WHITE JET/BUBBLEGUM",
  top="DRESS:3 HOODIE:2 SWEATER CLOAK", bottom="SKIRT:3 LEGGINGS LONG_SKIRT", shoes="PLATFORM:3 BOOTS:2",
  hat="NONE:3 WITCH BEANIE", acc="SKULL_NECKLACE:3 CHAINS EARRINGS NONE",
  pat="HEARTS:3 SKULLS:3 STRIPES", scheme="JET/BUBBLEGUM/WHITE GRAPE/ROSE/WHITE CHARCOAL/LAVENDER/BUBBLEGUM",
  mark="EYELINER:4 CHEEK_HEARTS:2 FUR:3", mark_col="JET GRAPE",
  hair="EMO_FRINGE:3 HIME:2 LONG:2 TWINTAILS:2", hair_col="JET:4 GRAPE BUBBLEGUM LAVENDER",
  pers="1-7 0-5 2-9 7-14 4-11", act="PARK:2 HOME:2", traits="NIGHT_OWL",
  food="Pudding|Black Licorice|Macarons|Candy Eyeballs", hobby="Poetry:3|Seances|Gardening|Plush Making",
  music="Goth Rock:5|Classical|Lullabies", phrase="Dark and fluffy.|Gloom on!|Boo!|Bleh.|Cuddle puddle!",
  fear="Sunlight|Being Alone|Silence",
  cast="""
Mortimew | s | Wears black. Blushes pink. Bakes for the bats. | species=CATFOLK
Gloomdrop | t | Writes sad poems that make everyone feel better. | job=Goth Poet; species=GHOST
Velvetine | s | Sells candles that smell like rainy graveyards. | job=Candle Shop Owner; species=BATLING
Cryptkin | h | Tends the cemetery flowers. Talks to every grave. | job=Cemetery Gardener; species=HUMAN; skin=SKIN_PORCELAIN
Batsy | s | Rescues lost bats. Knits them tiny sweaters. | job=Bat Rescuer; species=BATLING
Gloomella | x | Never smiles. Gives the best hugs in town. | species=HUMAN; skin=SKIN_FAIR; mouth=FLAT
Midnyte | h | Reads ghost stories to the ghosts, who love them. | job=Goth Poet; species=BATLING
""" ),

A("emoteens", "hill", "Emo Teen|Garage Drummer|Zine Maker",
  sp="RACCOONKIN:2 CATFOLK:2 FOXKIN HUMAN",
  top="HOODIE:3 BAND_TEE:3", bottom="RIPPED_JEANS:3 SKIRT LEGGINGS", shoes="SNEAKERS:3 PLATFORM",
  hat="NONE:3 BEANIE:2", acc="NONE:2 CHAINS EARRINGS",
  pat="STRIPES:3 SOLID:2", scheme="CHARCOAL/BUBBLEGUM/LIME JET/MINT/LAVENDER NAVY/ROSE/LEMON",
  hair="EMO_FRINGE:5 SIDE_SHAVE:2 MESSY", mark="EYELINER:3 FUR:3",
  pers="2-9 4-10 2-9 6-13 5-12", act="PARK:2 CONCERT HOME",
  food="Ramen|Pizza|Black Licorice|Popcorn", hobby="Video Games|Skateboarding|Metal Drumming|Poetry|Graffiti",
  music="Goth Rock:3|Hardcore Punk:3|Lo-fi Beats", phrase="Whatever.|Big mood.|Ugh, fine.|Bleh.",
  cast="""
Angstrom | h | Says "whatever" but shows up to every party. | species=RACCOONKIN
Mopey Mae | s | Paints her claws black and her heart pastel. | species=CATFOLK
Rimshot | t | Garage drummer. Practises softly so as not to wake babies. | job=Garage Drummer; species=FOXKIN
Zinester | s | Makes a zine about local frogs. Ten copies a month. | job=Zine Maker; species=HUMAN; skin=SKIN_TAN
Sulky Sol | h | Sulks dramatically, then helps you move house. | species=RACCOONKIN
""" ),

A("gamers", "hill", "Game Tester|Speedrunner|Streamer|Arcade Owner",
  sp="RACCOONKIN:2 FOXKIN CATFOLK MOUSEKIN ROBOT HUMAN",
  top="HOODIE:4 TSHIRT:2 PAJAMAS", bottom="SWEATS:3 SHORTS", shoes="SLIPPERS:3 SNEAKERS",
  hat="HEADPHONES:4 CAP NONE", acc="GLASSES:2 NONE:2",
  pat="SOLID:3 STRIPES", scheme="CYAN/MAGENTA/LEMON LIME/GRAPE/WHITE COBALT/LEMON/BUBBLEGUM",
  pers="3-12 3-10 4-12 8-15 6-13", act="HOME:4", traits="NIGHT_OWL",
  food="Pizza:2|Popcorn:2|Ramen|Bubble Tea|Cereal", hobby="Video Games:6|Board Games", music="Chiptune:5|Synthwave",
  phrase="Big mood.|Beep boop!|Yikes.|Slay!",
  cast="""
Pixel Pup | h | Beat every game in town. Then the town itself. | species=FOXKIN
Zoomzoom | t | Speedruns breakfast in under four seconds. | job=Speedrunner; species=MOUSEKIN; movement=15
Lagspike | s | Streamer. Her chat is mostly her grandma. | species=HUMAN; job=Streamer
Joystick | h | Arcade owner. Lets kids play free on rainy days. | species=HUMAN; job=Arcade Owner
Glitchy | i | Game tester bot. Finds every bug, keeps them as pets. | species=ROBOT; aura=GLITCH
Respawn | t | Falls asleep mid-game. Always respawns smiling. | species=HUMAN; skin=SKIN_OLIVE
""" ),

A("news", "hill", "News Anchor|Reporter|Weather Bunny|Camera Op",
  sp="FOXKIN:2 BUNNYKIN CATFOLK OWLKIN HUMAN",
  top="SUIT:3 DRESS:2 SWEATER_VEST", bottom="PANTS:2 SKIRT:2", shoes="HEELS:2 BOOTS SNEAKERS",
  hat="NONE:4 BERET", acc="NONE:2 GLASSES BOWTIE EARRINGS",
  pat="SOLID:4 PLAID", scheme="NAVY/CHERRY/WHITE SKY/NAVY/LEMON GRAPE/WHITE/LEMON",
  pers="7-13 2-8 8-15 5-12 2-8", act="WANDERING:2 SHOP",
  food="Coffee:3|Macarons|Sushi", hobby="Reading|Karaoke|Board Games", music="City Pop|Jazz|Synthwave",
  phrase="Oh my stars!|Okie dokie!|Yikes.",
  cast="""
Headline | s | News anchor. Reads bad news in a soothing voice. | species=FOXKIN
Scoop Scoot | h | Reporter. Has interviewed every pigeon in town. | job=Reporter; species=OWLKIN
Drizzle | t | Weather bunny. Is always, somehow, right. | job=Weather Bunny; species=BUNNYKIN
Zoom Lens | h | Camera op. Films everything sideways, on purpose. | species=ROBOT; job=Camera Op
Breaking | s | Breaks news gently, with cookies. | job=Reporter; species=HUMAN; skin=SKIN_UMBER
""" ),

A("bankers", "hill", "Piggy Banker|Coin Counter|Tax Helper",
  sp="HUMAN:3 BEARKIN MOUSEKIN",
  top="SUIT:4 SWEATER_VEST:2", bottom="PANTS:4", shoes="BOOTS:2 HEELS",
  hat="NONE:3 TOP_HAT", acc="MONOCLE:2 GLASSES:2 BOWTIE",
  pat="STRIPES:3 SOLID:2", scheme="BUBBLEGUM/NAVY/LEMON FOREST/HONEY/WHITE ROSE/CHARCOAL/LEMON",
  nose="PIG:4 ROUND", pers="2-8 0-5 4-11 3-10 1-7", act="SHOP:3",
  food="Coffee|Donuts|Pancakes", hobby="Board Games:3|Reading|Origami", music="Jazz|Classical",
  phrase="Okie dokie!|Oh my stars!|No thank you!", fear="Tax Season:3|Being Alone",
  cast="""
Penny Oink | s | Piggy banker. Saves every penny and every smile. | skin=SKIN_PEACH
Nickelby | h | Counts coins so fast they hum. | species=ROBOT; job=Coin Counter
Ledger Lee | t | Tax helper. Makes tax season a party with snacks. | job=Tax Helper; skin=SKIN_TAN
Dimes | h | Lends money to anyone with a good joke. | species=HUMAN
Mintworth | s | Keeps the vault tidy and the lollipops stocked. | species=HUMAN; skin=SKIN_BROWN
""" ),

A("judges", "hill", "Judge|Lawyer|Pie Judge",
  sp="OWLKIN:2 SHEEPKIN BEARKIN",
  top="ROBE:3 SUIT:2", bottom="ROBE:2 PANTS", shoes="BOOTS CLOGS",
  hat="NONE:3", acc="GLASSES:3 MONOCLE",
  pat="SOLID:5", scheme="JET/WHITE/LEMON NAVY/WHITE/CHERRY CHARCOAL/BONE/LEMON",
  pers="1-6 0-4 2-8 2-9 0-5", act="SHOP HOME",
  food="Pudding|Coffee|Carrot Cake", hobby="Reading:3|Board Games", music="Classical:3",
  phrase="Hoo hoo!|No thank you!|Okie dokie!",
  cast="""
Judge Hoo | h | Settles disputes by asking everyone to hug it out. | species=OWLKIN
Objection | s | Lawyer. Objects to rain, Mondays and sad faces. | job=Lawyer; species=SHEEPKIN
Pie Justice | t | Judges the pie contest. Everyone gets first place. | job=Pie Judge; species=BEARKIN
Gavella | s | Bangs her gavel to the beat of disco songs. | species=OWLKIN; music=Disco
""" ),

A("vets", "hill", "Vet|Pet Sitter|Dog Walker|Zookeeper",
  sp="PUPKIN:2 BUNNYKIN CATFOLK FOXKIN HUMAN",
  top="LAB_COAT:2 OVERALLS:2 TSHIRT HOODIE", bottom="PANTS:2 SHORTS CARGO", shoes="SNEAKERS:3 BOOTS:2",
  hat="CAP:2 NONE:3", acc="BACKPACK NONE:2 SCARF",
  pat="SOLID:3 POLKA", scheme="TEAL/PEACH/WHITE SKY/HONEY/WHITE MINT/CHOCOLATE/LEMON",
  pers="7-13 1-6 8-14 9-15 2-8", act="PARK:3 HOSPITAL", pet="80",
  food="Onigiri|Fried Chicken|Carrot Cake|Milk", hobby="Birdwatching|Bug Collecting|Gardening", music="Folk|City Pop|Bubblegum Pop",
  phrase="Tail wags!|Paws up!|Boop!",
  cast="""
Dr. Whisker | s | Vet. Speaks fluent purr, woof and squeak. | job=Vet; species=CATFOLK
Leashley | h | Walks twelve dogs at once. They walk him, really. | job=Dog Walker; species=PUPKIN
Kibble | t | Pet sitter. Every pet in town is a regular. | job=Pet Sitter; species=BUNNYKIN
Zoey Zoo | s | Zookeeper at the petting zoo. Pets everything. | job=Zookeeper; species=HUMAN; skin=SKIN_BROWN
Barkley | h | Throws balls for dogs. Fetches them himself. | job=Dog Walker; species=PUPKIN
Fluffles | s | Runs a hamster hotel with tiny bedsheets. | job=Pet Sitter; species=FOXKIN
""" ),

A("daycare", "hill", "Daycare Worker|Babysitter|Storyteller",
  sp="SHEEPKIN:2 BEARKIN BUNNYKIN",
  top="SWEATER:2 OVERALLS DRESS", bottom="SKIRT:2 PANTS", shoes="SLIPPERS:2 SNEAKERS",
  hat="NONE:4", acc="NONE:2 TEDDY SCARF",
  pat="POLKA:2 HEARTS SOLID", scheme="LEMON/SKY/WHITE PEACH/MINT/WHITE BUBBLEGUM/SKY/LEMON",
  pers="3-9 0-3 9-15 10-15 2-8", act="HOME:2 PARK",
  food="Pudding|Milk|Cupcakes", hobby="Singing|Plush Making|Napping", music="Lullabies:5",
  phrase="Snuggle up!|Cuddle puddle!|Hug o'clock!",
  cast="""
Miss Lulla | s | Can get any baby to nap in under a minute. | species=SHEEPKIN
Teddy Ted | h | Babysitter. Reads stories in seven silly voices. | job=Babysitter; species=BEARKIN
Tuck-In | t | Tucks in children, plants and grumpy old men. | species=PLUSHIE
Fleecy | s | Storyteller with a story for every scraped knee. | job=Storyteller; species=SHEEPKIN
""" ),

A("slimes", "hill", "Janitor|Window Washer|Bubble Bath Tester|Jelly Chef",
  sp="SLIME:6",
  skin="MINT BUBBLEGUM CYAN LAVENDER LEMON SLIME",
  top="NONE:3 OVERALLS:2 TSHIRT", bottom="NONE:4 SHORTS", shoes="BARE",
  hat="NONE:3 CAP BUCKET", acc="NONE:4",
  pat="SOLID:4 POLKA", scheme="MINT/SKY/WHITE BUBBLEGUM/LEMON/WHITE CYAN/LEMON/WHITE",
  pers="2-8 0-5 6-13 11-15 4-11", act="WANDERING:2 HOME",
  aura="BUBBLES:3 NONE:3", eyes="ROUND:3 HAPPY_ARC:2 DOT:2",
  food="Pudding:3|Gumdrops|Mochi|Milk", hobby="Bubble Blowing:3|Napping|Dancing", music="Bubblegum Pop|Lo-fi Beats|Disco",
  phrase="Wiggle wiggle!|Blep!|Mochi mochi!",
  cast="""
Gloop | h | Janitor. Absorbs the mess and wobbles away happy. | job=Janitor
Squishy | s | Window washer. Leaves a little smiley in the glass. | job=Window Washer
Jiggles | t | Bubble bath tester. Has a five-bubble rating system. | job=Bubble Bath Tester
Wobblina | s | Jelly chef. All her desserts wave at you. | job=Jelly Chef
Goober | h | Mops the floors, then naps in the mop bucket. | job=Janitor
""" ),

A("balloonists", "hill", "Balloonist|Pilot|Kite Pilot",
  sp="FOXKIN:2 OWLKIN MOUSEKIN HUMAN",
  top="LEATHER_JACKET:2 SWEATER OVERALLS", bottom="CARGO:2 PANTS", shoes="BOOTS:4",
  hat="CAP:2 BEANIE NONE", acc="SCARF:3 GLASSES",
  pat="SOLID:3 STRIPES", scheme="CARAMEL/SKY/WHITE CHERRY/LEMON/WHITE COBALT/TANGERINE/WHITE",
  pers="8-14 2-8 7-13 9-15 4-11", act="PARK:3",
  food="Pancakes|Coffee|Popcorn", hobby="Cloud Watching:4|Birdwatching", music="Jazz|Synthwave|Folk",
  phrase="Wheee!|Sunny side up!|Oh my stars!", fear="Thunder|Heights",
  cast="""
Skye High | s | Balloonist. Waves at every rooftop she passes. | species=HUMAN
Aviator Al | h | Flies a tiny plane that is mostly a bicycle. | job=Pilot; species=MOUSEKIN
Updraft | t | Has never come down for lunch. Eats in the air. | species=OWLKIN
Zephyrine | s | Kite pilot. Her kite has been airborne for a year. | job=Kite Pilot; species=HUMAN; skin=SKIN_PEACH
""" ),

A("sailors", "hill", "Sailor|Lighthouse Keeper|Ferry Captain",
  sp="CATFOLK:2 PUPKIN:2 BEARKIN",
  top="SWEATER:3 TSHIRT:2", bottom="PANTS:3 SHORTS", shoes="BOOTS:3 BARE",
  hat="BEANIE:3 CAP:2", acc="NONE:2 SCARF",
  pat="STRIPES:5 SOLID", scheme="WHITE/NAVY/TEAL NAVY/WHITE/CHERRY WHITE/NAVY/CHERRY SKY/NAVY/WHITE",
  pers="4-10 3-10 6-13 9-15 3-10", act="BEACH:4",
  food="Sushi|Fried Chicken|Onigiri|Takoyaki", hobby="Fishing:4|Singing", music="Sea Shanties:6",
  phrase="Toot toot!|Okie dokie!|Tail wags!|Big mood.",
  cast="""
Barnacle | h | Has sailed around the pond 1,000 times. | species=PUPKIN
Lighthouse | s | Keeps the lighthouse lit with fairy lights. | job=Lighthouse Keeper; species=CATFOLK
Skipjack | t | Ferry captain. Sings a shanty for each stop. | job=Ferry Captain; species=BEARKIN
Salty | h | Is not salty at all. Very sweet, actually. | species=CATFOLK
Anchovy | s | Ties knots so good they become art. | species=HUMAN
""" ),

A("knights", "hill", "Royal Guard|Knight|Squire|Jousting Coach",
  sp="BEARKIN:2 PUPKIN:2 HUMAN FOXKIN",
  top="PLATE_ARMOR:4 ARMOR:3", bottom="NONE:2 PANTS KILT", shoes="BOOTS:5",
  hat="NONE:3 HORNED_HELMET CROWN", acc="CAPE:3 NONE",
  pat="SOLID:4", scheme="SILVER/SKY/LEMON SILVER/CHERRY/LEMON SILVER/BUBBLEGUM/WHITE",
  pers="3-9 1-5 4-11 4-11 2-8", act="PARK:2 WANDERING", body="BUFF:2 STOCKY:2 AVERAGE",
  food="Meat Slabs|Pancakes|Fried Chicken|Angel Cake", hobby="Board Games|Singing|Poetry", music="Classical|Folk|Hymns",
  phrase="Fluff yeah!|Paws up!|Bless your heart!",
  cast="""
Sir Cuddle | h | Royal guard. Guards the castle and everyone's naps. | species=BEARKIN
Dame Fluff | s | Knight. Her armour is lined with fleece. | job=Knight; species=PUPKIN
Squirrely | t | Squire. Polishes armour until it giggles. | job=Squire; species=FOXKIN
Lancelittle | h | Jousting coach. Uses pool noodles only. | job=Jousting Coach; species=HUMAN; skin=SKIN_FAIR
Valkyrie | s | Wears a horned helmet to bake. Very effective. | species=BEARKIN; hat=HORNED_HELMET
""" ),

A("royalty", "hill", "Princess|Duke of Daisies|Royal Baker|Court Jester",
  sp="BUNNYKIN CATFOLK FOXKIN HUMAN CHERUB",
  top="DRESS:3 SUIT:2 CLOAK", bottom="LONG_SKIRT:2 PANTS TUTU", shoes="HEELS:3 BOOTS",
  hat="CROWN:3 TIARA:3", acc="CAPE:3 EARRINGS",
  pat="SOLID:3 HEARTS", scheme="LAVENDER/LEMON/WHITE ROSE/LEMON/WHITE SKY/LEMON/WHITE",
  pers="3-9 0-4 8-15 7-14 2-9", act="PARK:2 HOME", aura="SPARKLES:3 NONE:2",
  food="Angel Cake|Macarons|Honey Toast|Cupcakes", hobby="Dancing|Singing|Painting|Board Games", music="Classical:3|Bubblegum Pop",
  phrase="Oh my stars!|Sparkle on!|Smooch!",
  cast="""
Pomelina | s | Princess of HaHaland. Rules mostly with stickers. | species=BUNNYKIN; hat=TIARA
Duke Daisy | h | Duke of Daisies. Owns zero castles, many daisies. | species=HUMAN; job=Duke of Daisies; hat=CROWN
Lady Tiara | s | Wears her tiara to the gym. Lifts. | species=CATFOLK
Jestina | s | Court jester. Her jokes are 90% puns by weight. | job=Court Jester; species=HUMAN; skin=SKIN_OLIVE; hat=PARTY
Crumbelina | s | Royal baker. Makes crowns from shortbread. | job=Royal Baker; species=CHERUB; skin=SKIN_PEACH
""" ),

A("cupids", "hill", "Cupid Courier|Matchmaker|Love Letter Writer",
  sp="CHERUB:5 OWLKIN",
  top="TSHIRT:2 ROBE DRESS", bottom="SHORTS:2 SKIRT NONE", shoes="SANDALS:3 BARE",
  hat="NONE:4 FLOWER_CROWN", acc="NONE:3 BACKPACK",
  pat="HEARTS:5 SOLID", scheme="ROSE/WHITE/LEMON BUBBLEGUM/WHITE/ROSE PEACH/ROSE/WHITE",
  pers="10-15 1-6 11-15 10-15 5-12", act="WANDERING:3 PARK", aura="HEARTS:4 NONE",
  eyes="HEART:4 SPARKLE:2", cheeks="BLUSH:100 SPARKLES:30",
  food="Macarons:3|Angel Cake|Cotton Candy", hobby="Poetry|Singing|Cloud Watching", music="Bubblegum Pop:3|Classical",
  phrase="Smooch!|Hug o'clock!|Let's be friends!",
  cast="""
Cupidoo | h | Shoots heart-shaped suction darts. Mostly misses. |
Valentina | s | Matchmaker. Has made 300 couples and 2 enemies. | job=Matchmaker
Smoochy | t | Delivers love letters. Blushes reading the address. |
Amorette | s | Writes love letters for the shy. Signs them "you". | job=Love Letter Writer
Lovebird | s | Owlkin cupid. Hoots when love is near. | species=OWLKIN; wings=ANGEL; halo=STARS
""" ),

A("cloudshepherds", "hill", "Cloud Shepherd|Weather Watcher|Rainbow Painter",
  sp="SHEEPKIN:5 BUNNYKIN",
  top="SWEATER:3 OVERALLS DRESS", bottom="SHORTS:2 SKIRT PANTS", shoes="BOOTS:2 SANDALS",
  hat="NONE:3 BUCKET FLOWER_CROWN", acc="SCARF:2 NONE:2",
  pat="SOLID:2 POLKA STRIPES", scheme="SKY/WHITE/LEMON LAVENDER/SKY/WHITE MINT/SKY/PEACH",
  pers="1-7 0-4 5-12 11-15 3-10", act="PARK:4",
  food="Cotton Candy:3|Milk|Honey Toast", hobby="Cloud Watching:6|Napping", music="Lullabies|Folk|Lo-fi Beats",
  phrase="Baa-rilliant!|So fluffy!|Snuggle up!",
  cast="""
Cumulus | s | Herds the clouds into nice shapes for picnics. |
Nimbus | h | Cloud shepherd. Lost a cloud once. Still looking. |
Rainbow Ewe | t | Rainbow painter. Paints after every storm. | job=Rainbow Painter
Fluffington | h | His wool is so fluffy clouds get jealous. |
Drizzlewool | s | Weather watcher. Knits umbrellas. They leak. | job=Weather Watcher; species=BUNNYKIN
""" ),

A("lamplighters", "hill", "Lamplighter|Night Watch|Firefly Herder",
  sp="MOTHKIN:5 OWLKIN",
  top="CLOAK:2 SWEATER:2 OVERALLS", bottom="PANTS:2 LONG_SKIRT", shoes="BOOTS:3",
  hat="NONE:3 BEANIE TOP_HAT", acc="SCARF:2 NONE:2",
  pat="SOLID:4 POLKA", scheme="NAVY/LEMON/WHITE GRAPE/LEMON/PEACH TEAL/HONEY/WHITE",
  pers="1-7 0-4 3-10 7-14 2-9", act="WANDERING:3", traits="NIGHT_OWL",
  aura="SPARKLES:2 NONE:3", eyes="COMPOUND:3 SLEEPY:2 ROUND",
  food="Honey Toast:2|Milk|Pudding", hobby="Stargazing:4|Bug Collecting|Poetry", music="Lullabies|Classical|Lo-fi Beats",
  phrase="Hoo hoo!|Cozy vibes only!|Sparkle on!", fear="Being Alone|Silence|Bees",
  cast="""
Lumina | s | Lights every lamp in Hillvile. Waves at each one. |
Wickley | h | Lamplighter. Never once flew into a lamp. Once. |
Glowbug | t | Herds fireflies into star shapes on festival nights. | job=Firefly Herder
Lantern Lyn | s | Night watch. Keeps watch with cocoa and a blanket. | job=Night Watch
Hootenanny | h | Owl night watchman. Throws midnight hoedowns. | job=Night Watch; species=OWLKIN
""" ),

A("beachvendors", "hill", "Smoothie Vendor|Shell Seller|Kite Seller",
  sp="FOXKIN CATFOLK PUPKIN HUMAN",
  top="TANK:3 CROP_TOP TSHIRT", bottom="SHORTS:4", shoes="SANDALS:4",
  hat="BUCKET:2 CAP NONE", acc="SUNGLASSES:3 NONE",
  pat="STRIPES:2 HEARTS SOLID", scheme="TANGERINE/LIME/WHITE LEMON/CYAN/WHITE PEACH/TEAL/LEMON",
  pers="8-14 2-7 9-15 11-15 3-10", act="BEACH:5",
  food="Ice Cream|Tacos|Bubble Tea|Sushi", hobby="Fishing|Bubble Blowing|Dancing", music="City Pop:3|Disco",
  phrase="Sunny side up!|Wheee!|Yummy yummy!",
  cast="""
Mango Tango | s | Blends smoothies while doing the tango. | species=FOXKIN
Seashelly | t | Sells seashells by the seashore, obviously. | job=Shell Seller; species=CATFOLK
Coconutty | h | Opens coconuts with one karate chop. | species=PUPKIN
Breezy | s | Sells kites that are guaranteed to fly. Mostly. | job=Kite Seller; species=HUMAN; skin=SKIN_TAN
""" ),

A("divers", "hill", "Pearl Diver|Pond Keeper|Lily Farmer",
  sp="FROGLING:6",
  top="NONE:2 TANK:2 TSHIRT", bottom="SHORTS:3 NONE", shoes="BARE:5",
  hat="NONE:3 BUCKET", acc="NONE:3 GLASSES",
  pat="SOLID:3 POLKA", scheme="TEAL/LEMON/WHITE CYAN/LIME/WHITE FOREST/PEACH/WHITE",
  pers="4-10 1-6 5-12 10-15 4-11", act="BEACH:3 PARK",
  food="Sushi|Salad|Onigiri", hobby="Fishing|Bug Collecting|Napping", music="Sea Shanties|Lo-fi Beats|Folk",
  phrase="Ribbit!|Wheee!|Big mood.", tail="LIZARD:1 NONE:3",
  cast="""
Pearlie | s | Dives for pearls. Gives them all to her mum. | job=Pearl Diver
Croaksworth | h | Pond keeper. Knows each fish by its swim. | job=Pond Keeper
Lily Lou | t | Farms lily pads. Rents them out for naps. | job=Lily Farmer
Bubbleton | h | Can hold his breath for an hour. Just showing off. | job=Pearl Diver; tail=LIZARD
""" ),

A("foragers", "hill", "Forager|Mushroom Hunter|Berry Picker",
  sp="MOUSEKIN:2 FROGLING RACCOONKIN FOXKIN",
  top="OVERALLS:2 SWEATER CLOAK", bottom="SHORTS:2 PANTS", shoes="BOOTS:3 BARE",
  hat="BUCKET:3 NONE BEANIE", acc="BACKPACK:3 NONE",
  pat="SOLID:3 POLKA", scheme="FOREST/CHERRY/WHITE CHOCOLATE/MINT/WHITE TEAL/RUST/BONE",
  pers="3-9 1-6 5-12 9-15 4-11", act="PARK:4",
  food="Salad|Honey Toast|Dumplings|Pickles", hobby="Gardening|Bug Collecting:3|Birdwatching", music="Folk:4",
  phrase="Squeak squeak!|Okie dokie!|Sunny side up!",
  cast="""
Morel | s | Finds mushrooms nobody else can. Names them all. | job=Mushroom Hunter; species=MOUSEKIN
Truffletuft | h | Sniffs out truffles. Shares them with everyone. | job=Mushroom Hunter; species=RACCOONKIN
Elderberry | t | Picks berries for jam. Eats half on the way. | job=Berry Picker; species=FROGLING
Chanterelle | s | Knows which mushrooms are tasty and which are rude. | species=FOXKIN
Bramblekit | h | Comes home scratched and grinning, with berries. | job=Berry Picker; species=MOUSEKIN
""" ),

A("hikers", "hill", "Hiker|Cartographer|Mountain Guide",
  sp="FOXKIN:2 BEARKIN PUPKIN HUMAN",
  top="SWEATER:2 HOODIE TSHIRT", bottom="CARGO:3 SHORTS", shoes="BOOTS:5",
  hat="BUCKET:2 BEANIE CAP", acc="BACKPACK:5",
  pat="SOLID:3 PLAID", scheme="FOREST/TANGERINE/WHITE NAVY/LIME/WHITE RUST/TEAL/BONE",
  pers="8-14 2-8 6-13 9-15 3-9", act="WANDERING:3 PARK",
  food="Onigiri|Honey Toast|Fried Chicken|Popcorn", hobby="Birdwatching|Stargazing|Fishing", music="Folk:4|Lo-fi Beats",
  phrase="Paws up!|Sunny side up!|Yippee!",
  cast="""
Trekkie | h | Has worn out 40 pairs of boots. Keeps them all. | species=FOXKIN
Mappie | s | Cartographer. Draws maps with little hearts on. | job=Cartographer; species=PUPKIN
Summit | t | Mountain guide. Has climbed every hill twice. | job=Mountain Guide; species=BEARKIN
Wanderlust | s | Never stays home. Sends postcards from the park. | species=HUMAN; skin=SKIN_OLIVE
Switchback | h | Never takes a straight path anywhere. | species=FOXKIN
""" ),

A("scouts", "hill", "Scout|Scout Leader|Badge Collector",
  sp="FOXKIN BEARKIN PUPKIN BUNNYKIN RACCOONKIN HUMAN",
  traits="KID", body="TINY:4 ROUND:2",
  top="TSHIRT:3 SWEATER_VEST:2", bottom="SHORTS:4", shoes="BOOTS:3 SNEAKERS",
  hat="CAP:2 BANDANA:2 NONE", acc="BACKPACK:3 SCARF:2",
  pat="SOLID:4", scheme="FOREST/LEMON/WHITE TEAL/TANGERINE/WHITE NAVY/LEMON/CHERRY",
  pers="10-15 2-8 9-15 8-14 4-11", act="PARK:4",
  food="Popcorn|Fried Chicken|Gumdrops|Pancakes", hobby="Bug Collecting:2|Birdwatching|Stargazing|Origami", music="Folk|Chiptune|Bubblegum Pop",
  phrase="Paws up!|Yippee!|Let's be friends!", fear="The Dark|Spiders|Bees|Deep Water",
  cast="""
Badgeworth | h | Has 41 badges. Wants the "nap" badge most. | job=Badge Collector; species=RACCOONKIN
Campfire | s | Tells ghost stories. Scares herself the most. | species=FOXKIN
Knotty | t | Can tie any knot. Usually ties their shoes together. | species=PUPKIN
Smores | h | Burns marshmallows on purpose. Likes them crispy. | species=BEARKIN
Trail Mix | s | Shares her snacks before anyone asks. | species=BUNNYKIN
Scoutmaster | h | Scout leader. Earned every badge, even sewing. | job=Scout Leader; species=HUMAN; skin=SKIN_BROWN; trait=NONE
""" ),

A("photographers", "hill", "Photographer|Portrait Painter|Selfie Coach",
  sp="CATFOLK FOXKIN BUNNYKIN HUMAN",
  top="SWEATER_VEST TSHIRT:2 HOODIE", bottom="PANTS:2 SKIRT", shoes="SNEAKERS:3",
  hat="BERET:2 CAP NONE", acc="GLASSES:2 BACKPACK NONE",
  pat="SOLID:3 STRIPES", scheme="CHARCOAL/LEMON/WHITE TEAL/PEACH/WHITE GRAPE/MINT/LEMON",
  pers="7-13 2-7 8-14 9-15 4-11", act="PARK:3 WANDERING",
  food="Coffee|Melon Bread|Macarons", hobby="Painting|Birdwatching|Cosplay", music="City Pop|Jazz|Lo-fi Beats",
  phrase="Teehee!|Sparkle on!|Slay!",
  cast="""
Snapshot | s | Takes your photo before you can blink. Always cute. | species=CATFOLK
Shutterbug | h | Photographs bugs. They pose for him. | species=FOXKIN
Shakeshot | t | Shakes every photo, even digital ones. | species=BUNNYKIN
Flashbulb | s | Selfie coach. Knows your good side. Both of them. | job=Selfie Coach; species=HUMAN; skin=SKIN_FAIR
""" ),

A("influencers", "hill", "Influencer|Unboxing Star|Snack Reviewer",
  sp="CATFOLK:2 FOXKIN BUNNYKIN HUMAN",
  top="CROP_TOP:3 HOODIE DRESS", bottom="SKIRT:2 LEGGINGS SHORTS", shoes="PLATFORM:2 SNEAKERS HEELS",
  hat="NONE:2 BUCKET CAT_EARS", acc="SUNGLASSES:3 EARRINGS",
  pat="SOLID:2 HEARTS POLKA", scheme="BUBBLEGUM/LAVENDER/WHITE PEACH/MINT/LEMON CYAN/BUBBLEGUM/WHITE",
  pers="9-15 3-9 13-15 9-15 6-13", act="WANDERING:3 SHOP",
  food="Bubble Tea:3|Macarons|Donuts|Ice Cream", hobby="Cosplay|Karaoke|Dancing", music="Bubblegum Pop:4|City Pop",
  phrase="Slay!|Sparkle on!|Smooch!|Big mood.",
  cast="""
Hashtag | s | Posts a selfie with every snack. Then eats it. | species=CATFOLK
Unboxie | t | Unboxes gifts on stream. Keeps the boxes. | job=Unboxing Star; species=FOXKIN
Snaxson | h | Snack reviewer. Rated 9,000 snacks a ten. | job=Snack Reviewer; species=BUNNYKIN
Trendsetter | s | Wore socks with sandals. Now everybody does. | species=HUMAN; skin=SKIN_EBONY
Likey | h | Likes everything. Literally. Says "like" a lot. | species=CATFOLK
""" ),

A("comedians", "hill", "Comedian|Pun Writer|Mime",
  sp="PUPKIN FOXKIN RACCOONKIN HUMAN PLUSHIE",
  top="SUIT:2 TSHIRT SWEATER", bottom="PANTS:3", shoes="SNEAKERS:2 BOOTS",
  hat="NONE:3 BERET PARTY", acc="BOWTIE:2 NONE:2",
  pat="POLKA STRIPES SOLID:2", scheme="CHERRY/WHITE/JET LEMON/GRAPE/WHITE SKY/CHERRY/WHITE",
  pers="9-15 5-12 12-15 11-15 9-15", act="CONCERT:2 CAFE",
  food="Popcorn|Pizza|Donuts", hobby="Pranks:3|Karaoke|Board Games", music="Disco|Jazz",
  phrase="Hehehe~|Teehee!|Yikes.|Rude!",
  cast="""
Punderful | h | Pun writer. Every sentence is a groan in disguise. | job=Pun Writer; species=PUPKIN
Chuckles | s | Laughs so hard at her own jokes she cannot finish. | species=FOXKIN
Silent Sid | t | Mime. Has been stuck in an invisible box since May. | job=Mime; species=HUMAN; skin=SKIN_PORCELAIN; marking=CLOWN; marking_color=JET
Knock Knock | h | Only tells knock-knock jokes. Answers them himself. | species=RACCOONKIN
Hattie Haha | s | Can make even Hellvile folk snort-laugh. | species=PLUSHIE
""" ),

A("theatre", "hill", "Actor|Stagehand|Playwright|Costume Maker",
  sp="CATFOLK FOXKIN MOUSEKIN OWLKIN HUMAN",
  top="SUIT DRESS:2 CLOAK KIMONO", bottom="PANTS:2 LONG_SKIRT", shoes="BOOTS:2 HEELS",
  hat="NONE:3 TOP_HAT BERET", acc="CAPE:2 NONE:2",
  pat="SOLID:3 STRIPES", scheme="CHERRY/LEMON/JET GRAPE/LEMON/WHITE NAVY/ROSE/LEMON",
  pers="6-12 2-8 13-15 7-14 7-14", act="CONCERT:4",
  food="Macarons|Coffee|Spaghetti", hobby="Cosplay:3|Poetry|Singing", music="Classical|Jazz|Disco",
  phrase="Oh my stars!|Sparkle on!|Rude!",
  cast="""
Drama Llama | s | Gasps at everything. Even good news. | species=CATFOLK
Encore | h | Takes a bow after ordering lunch. | species=FOXKIN
Backstage | t | Stagehand. Can move a castle set in eight seconds. | job=Stagehand; species=MOUSEKIN
Soliloquy | s | Playwright. Her plays all end with a group hug. | species=GHOST; job=Playwright
Sequin | h | Costume maker. Has sewn 10,000 sequins this week. | job=Costume Maker; species=HUMAN; skin=SKIN_BROWN
""" ),

A("puppeteers", "hill", "Puppeteer|Sock Puppet Maker|Shadow Puppeteer",
  sp="MOUSEKIN BEARKIN PLUSHIE HUMAN",
  top="SWEATER:2 OVERALLS", bottom="PANTS:2", shoes="SNEAKERS SLIPPERS",
  hat="NONE:3 BERET", acc="NONE:2 GLASSES",
  pat="STRIPES POLKA SOLID", scheme="TANGERINE/SKY/WHITE GRAPE/LIME/WHITE TEAL/BUBBLEGUM/LEMON",
  pers="5-11 1-6 10-15 9-15 8-15", act="PARK:2 CONCERT",
  food="Popcorn|Cupcakes|Gumdrops", hobby="Plush Making|Cosplay|Origami", music="Folk|Bubblegum Pop",
  phrase="Wiggle wiggle!|Hehehe~|Boop!",
  cast="""
Sockrates | h | Sock puppet philosopher. The sock does the talking. | species=HUMAN; job=Sock Puppet Maker
Marionetta | s | Puppeteer. Her strings never tangle. Her hair does. | species=HUMAN; skin=SKIN_FAIR
Shadowpaw | t | Makes shadow bunnies on walls at bedtime. | job=Shadow Puppeteer; species=BEARKIN
Mitts | i | A puppet that walked off its hand and got a job. | species=PLUSHIE
""" ),

A("hugtherapy", "hill", "Hug Therapist|Comfort Plush|Cuddle Coach",
  sp="PLUSHIE:5 BEARKIN",
  top="SWEATER:3 PAJAMAS TSHIRT", bottom="NONE:2 SWEATS PANTS", shoes="SLIPPERS:4 BARE",
  hat="NONE:4 BEANIE", acc="NONE:3 SCARF TEDDY",
  pat="HEARTS:3 POLKA SOLID", scheme="PEACH/ROSE/WHITE LAVENDER/MINT/WHITE SKY/BUBBLEGUM/WHITE",
  pers="1-6 0-3 8-14 12-15 2-8", act="HOSPITAL:2 HOME",
  food="Pudding:2|Milk|Honey Toast", hobby="Napping|Plush Making|Knitting", music="Lullabies:4|Lo-fi Beats",
  phrase="Hug o'clock!|Cuddle puddle!|So fluffy!|Snuggle up!", aura="HEARTS:2 NONE:3",
  cast="""
Snugglesby | h | Hug therapist. Rated five stars by 400 hugs. |
Pillowpuff | s | So soft that sad people fall asleep on her. |
Huggins | t | Cuddle coach. Teaches the perfect squeeze. | job=Cuddle Coach
Patchwork | t | Has been mended 99 times. Proud of every stitch. | marking=PATCHES
Cushion | s | Comfort plush at the hospital. Never leaves a bed. | job=Comfort Plush; species=BEARKIN
""" ),

A("bots", "hill", "Delivery Bot|Cleaning Bot|Tea Bot|Garden Bot",
  sp="ROBOT:6",
  top="NONE:2 OVERALLS:2 SWEATER_VEST", bottom="NONE:3 PANTS", shoes="ROLLERSKATES:3 BOOTS SNEAKERS",
  hat="NONE:3 CAP PROPELLER", acc="NONE:2 BACKPACK BOWTIE",
  pat="SOLID:4 STRIPES", scheme="SKY/WHITE/LEMON MINT/SILVER/LEMON LEMON/SILVER/SKY PEACH/COBALT/WHITE",
  wings="MECH:2 NONE:3", eyes="GLOW:2 DOT:2 ROUND HAPPY_ARC", eye_col="CYAN SKY LIME LEMON MINT",
  pers="6-12 0-4 4-11 6-13 3-10", act="WANDERING:3 SHOP",
  food="Milk|Cereal|Popcorn", hobby="Origami|Board Games|Gardening|Cloud Watching", music="Chiptune:4|Synthwave:2",
  phrase="Beep boop!|Okie dokie!|Toot toot!",
  cast="""
Zippy-9 | i | Delivers parcels by jetpack. Waves with every arm. | wings=MECH
Dustin | i | Cleaning bot. Hoovers up crumbs and sadness. | job=Cleaning Bot
Teapot T4 | i | Tea bot. Brews tea at exactly hug temperature. | job=Tea Bot
Sprinklr | t | Garden bot. Waters flowers and sings to them. | job=Garden Bot
Bleep | i | Learned to giggle. Now cannot stop giggling. |
""" ),

A("goodghosts", "hill", "Night Nanny|Haunted Housekeeper|Ghost Tour Guide",
  sp="GHOST:6",
  top="NONE:3 DRESS SWEATER", bottom="NONE:4", shoes="BARE",
  hat="NONE:3 BERET FLOWER_CROWN", acc="NONE:3 SCARF",
  pat="SOLID:4 POLKA", scheme="WHITE/LAVENDER/SKY LAVENDER/MINT/WHITE SKY/PEACH/WHITE",
  pers="1-7 0-4 6-12 9-15 4-11", act="HOME:3 WANDERING", traits="NIGHT_OWL",
  food="Pudding|Milk|Angel Cake", hobby="Ghost Hunting|Knitting|Reading|Seances", music="Lullabies|Classical|Lo-fi Beats",
  phrase="Boo!|Hellooo!|Snuggle up!|Cozy vibes only!",
  cast="""
Boolulu | s | Night nanny. Scares away nightmares. Gently. |
Hauntessa | t | Haunts houses to tidy them. Leaves notes. | job=Haunted Housekeeper
Spookums | h | Ghost tour guide. Mostly tours his own house. | job=Ghost Tour Guide
Wisp Willow | s | Floats through walls to deliver soup to the sick. |
Peekaboo | x | Pops out of cupboards to say hello. Sorry. |
""" ),

A("museum", "hill", "Museum Guide|Fossil Expert|Curator",
  sp="MUMMY:2 SKELETON OWLKIN MOUSEKIN",
  top="SWEATER_VEST:2 SUIT", bottom="PANTS:2", shoes="BOOTS CLOGS",
  hat="NONE:3 FEZ", acc="GLASSES:2 BOWTIE BANDAGES",
  pat="SOLID:3 PLAID", scheme="CARAMEL/TEAL/BONE CHOCOLATE/LEMON/BONE NAVY/HONEY/WHITE",
  pers="1-6 0-4 3-10 6-13 2-9", act="SHOP HOME",
  food="Coffee|Pudding|Honey Toast", hobby="Reading:3|Skull Collecting|Bug Collecting", music="Classical:3|Jazz",
  phrase="Rattle rattle!|Hoo hoo!|Okie dokie!",
  cast="""
Tutti | s | Mummy museum guide. Gives tours of her own tomb. | species=MUMMY
Ramses Ray | h | Unravels a little every time he laughs. | species=MUMMY
Bonejangles | h | Skeleton xylophone teacher. Plays his own ribs. | job=Xylophone Teacher; species=SKELETON
Curio | t | Curator. Labels everything, including friends. | job=Curator; species=OWLKIN
Fossil Fay | s | Fossil expert. Dug up a dinosaur. It was a chair. | job=Fossil Expert; species=MOUSEKIN
""" ),

A("buskers", "hill", "Busker|Bagpiper|Accordionist|Ukulele Player",
  sp="FOXKIN RACCOONKIN CATFOLK HUMAN BEARKIN",
  top="SWEATER TSHIRT:2 SWEATER_VEST", bottom="KILT:2 PANTS RIPPED_JEANS", shoes="BOOTS:2 SNEAKERS",
  hat="NONE:2 BEANIE BERET BUCKET", acc="SCARF NONE:2",
  pat="PLAID:3 SOLID", scheme="FOREST/CHERRY/LEMON NAVY/HONEY/WHITE RUST/TEAL/BONE",
  pers="5-12 2-8 9-15 10-15 5-12", act="PARK:4",
  food="Popcorn|Pizza|Dumplings", hobby="Singing:4|Dancing", music="Folk:4|Sea Shanties|Jazz",
  phrase="Toot toot!|Wiggle wiggle!|Yippee!",
  cast="""
Bagpipe Bo | h | Bagpiper. Neighbours wear earmuffs with love. | job=Bagpiper; species=BEARKIN; bottom=KILT
Squeezebox | s | Accordionist. Plays polkas for passing ducks. | species=HUMAN; job=Accordionist
Uke Ukulele | t | Knows four chords and 4,000 songs. | job=Ukulele Player; species=RACCOONKIN
Harmonica | t | Busks for buttons. Has the best button collection. | species=HUMAN; skin=SKIN_UMBER
Strummy | s | Plays under the big oak every afternoon. | species=CATFOLK
""" ),

A("cheer", "hill", "Cheer Captain|Cheerleader|Mascot",
  sp="BUNNYKIN:2 PUPKIN CATFOLK FOXKIN",
  top="CROP_TOP:3 TSHIRT DRESS", bottom="SKIRT:4 SHORTS", shoes="SNEAKERS:4",
  hat="NONE:4", acc="NONE:3 EARRINGS",
  pat="STRIPES:3 SOLID:2", scheme="CHERRY/WHITE/LEMON COBALT/LEMON/WHITE MAGENTA/WHITE/LIME",
  pers="12-15 3-9 13-15 10-15 5-12", act="PARK:4",
  hair="PONYTAIL:4 PIGTAILS:2 TWINTAILS", food="Pancakes|Ice Cream|Salad|Bubble Tea", hobby="Dancing:4|Yoga|Singing",
  music="Bubblegum Pop:3|Disco:2", phrase="Yippee!|Slay!|Wheee!|Paws up!",
  cast="""
Rah Rah | s | Cheer captain. Cheers for people doing chores. | job=Cheer Captain; species=BUNNYKIN
Pom Pom | t | Shakes pom-poms even when sitting still. | species=PUPKIN
Go Team Gus | h | Spells out every word with his arms. G-O. | species=HUMAN
Hooray Hana | s | Can do 30 backflips. Prefers 3 and a nap. | species=HUMAN
Mascotty | s | The town mascot in a giant head. Nobody knows who. | job=Mascot; species=BUNNYKIN; hat=BUNNY_EARS
""" ),

A("nappers", "hill", "Professional Napper|Hammock Tester|Pillow Fluffer",
  sp="CATFOLK:2 SHEEPKIN:2 BEARKIN RACCOONKIN",
  top="PAJAMAS:5 SWEATER", bottom="NONE:3 SWEATS", shoes="SLIPPERS:5",
  hat="NONE:3 BEANIE", acc="NONE:3 TEDDY",
  pat="POLKA:3 STRIPES SOLID", scheme="LAVENDER/SKY/WHITE SKY/LEMON/WHITE MINT/LAVENDER/WHITE",
  pers="0-3 0-4 1-8 13-15 2-9", act="HOME:4", eyes="SLEEPY:5 HAPPY_ARC",
  stats="energy=30-60",
  food="Milk:3|Pudding|Honey Toast", hobby="Napping:8|Cloud Watching", music="Lullabies:4|Lo-fi Beats:2",
  phrase="Snuggle up!|Cozy vibes only!|Big mood.",
  cast="""
Dozella | s | Napper of the year, eight years running. Lying down. | job=Professional Napper; species=CATFOLK
Hammock | h | Hammock tester. Has tested one hammock for a year. | job=Hammock Tester; species=RACCOONKIN
Pillowby | t | Pillow fluffer. Fluffs pillows to a perfect cloud. | job=Pillow Fluffer; species=SHEEPKIN
Zzzabel | s | Talks in her sleep. Gives great advice that way. | species=SHEEPKIN
Lazybones | h | Sleeps 20 hours a day. Dreams in colour. | species=BEARKIN
""" ),

A("picnic", "hill", "Picnic Planner|Blanket Folder|Sandwich Artist",
  sp="BUNNYKIN MOUSEKIN BEARKIN PUPKIN",
  top="DRESS:2 OVERALLS TSHIRT", bottom="SKIRT SHORTS:2", shoes="SANDALS:2 SNEAKERS",
  hat="BUCKET:2 FLOWER_CROWN NONE", acc="NONE:2 BACKPACK",
  pat="PLAID:4 POLKA", scheme="CHERRY/WHITE/LEMON SKY/WHITE/LIME PEACH/MINT/WHITE",
  pers="5-11 0-5 9-15 11-15 2-8", act="PARK:5",
  food="Onigiri|Salad|Pickles|Carrot Cake", hobby="Cloud Watching|Gardening|Board Games", music="Folk|City Pop",
  phrase="Sunny side up!|Yummy yummy!|Let's be friends!",
  cast="""
Basketina | s | Picnic planner. Packs exactly enough for everyone. | species=HUMAN
Gingham | h | Folds blankets into perfect squares. And swans. | job=Blanket Folder; species=MOUSEKIN
Sandwichy | t | Sandwich artist. Cuts every crust into a heart. | job=Sandwich Artist; species=BEARKIN
Lemonade | s | Sells lemonade for a smile. Takes no coins. | species=PUPKIN
""" ),

A("heroes", "hill", "Retired Hero|Retired Marine|Monster Hunter",
  sp="BEARKIN PUPKIN HUMAN:2 FOXKIN",
  traits="ELDER", top="ARMOR:2 LEATHER_JACKET SWEATER", bottom="CARGO:2 PANTS", shoes="COMBAT:3 BOOTS",
  hat="MARINE_HELMET:3 NONE:2 BANDANA", acc="EYEPATCH:2 NONE CAPE",
  pat="CAMO:3 SOLID:2", scheme="FOREST/CHARCOAL/LEMON CHARCOAL/CHERRY/SILVER RUST/FOREST/BONE",
  fhair="FULL_BEARD:2 STUBBLE:2 MUSTACHE NONE", cheeks="SCAR:50 BANDAGE:20 BLUSH:40",
  pers="2-8 4-11 3-10 5-12 3-10", act="PARK:2 HOME:2", body="BUFF:2 STOCKY:2",
  food="Meat Slabs|Coffee|Fried Chicken|Hell Peppers", hobby="Welding|Fishing|Gardening|Board Games", music="Doom Metal|Classical|Folk",
  phrase="Rip and tear... gently.|Big mood.|Okie dokie!",
  cast="""
Sarge Snoot | h | Retired marine. Fought demons once. Now grows roses. | job=Retired Marine; species=PUPKIN; hat=MARINE_HELMET
Auntie Ammo | s | Keeps a rocket launcher for "pest control". | job=Retired Marine; species=HUMAN; skin=SKIN_EBONY
Old Valour | t | Retired hero. Tells the same dragon story kindly. | species=BEARKIN
Captain Paw | h | Monster hunter. Hunts monsters to ask them to tea. | job=Monster Hunter; species=FOXKIN
Grizzle Gil | h | Has a scar for every brave thing he did. | species=HUMAN; skin=SKIN_TAN
""" ),

A("wizards", "hill", "Wizard|Alchemist|Unicorn Groomer",
  sp="OWLKIN HUMAN:2 SHEEPKIN",
  traits="ELDER", top="ROBE:4 CLOAK", bottom="ROBE:3 NONE", shoes="SANDALS:2 SLIPPERS",
  hat="WITCH:4 NONE", acc="NONE:2 GLASSES",
  pat="SOLID:3 POLKA", scheme="COBALT/LEMON/WHITE GRAPE/LEMON/SILVER TEAL/LAVENDER/WHITE",
  fhair="WIZARD:5 FULL_BEARD", horns="UNICORN:2 NONE:3", aura="SPARKLES:3 NONE",
  pers="1-6 1-6 4-11 6-13 8-15", act="HOME:3",
  food="Honey Toast|Pudding|Mochi", hobby="Stargazing|Reading|Tarot Reading", music="Classical|Folk",
  phrase="Oh my stars!|Sparkle on!|Hoo hoo!",
  cast="""
Merlinnie | h | Wizard. His spells mostly make flowers. | species=HUMAN; skin=SKIN_FAIR; facial_hair=WIZARD
Beardolf | h | Has a beard so long he trips on it daily. | species=OWLKIN; facial_hair=WIZARD
Elixa | s | Alchemist. Turns lead into lemonade. | job=Alchemist; species=HUMAN; skin=SKIN_BROWN
Glimmer | t | Unicorn groomer. Has a unicorn horn of their own. | job=Unicorn Groomer; species=SHEEPKIN; horns=UNICORN
""" ),

A("cosplayers", "hill", "Cosplayer|Prop Maker|Con Organizer",
  sp="HUMAN:3 CATFOLK FOXKIN",
  top="CROP_TOP KIMONO DRESS ARMOR HOODIE", bottom="SKIRT:2 TUTU LEGGINGS", shoes="PLATFORM:2 BOOTS",
  hat="CAT_EARS:3 BUNNY_EARS:3 TIARA", acc="NONE CAPE:2 EARRINGS",
  pat="HEARTS:2 STRIPES SOLID", scheme="CYAN/MAGENTA/WHITE LAVENDER/LEMON/WHITE BUBBLEGUM/SKY/LEMON",
  pers="8-14 2-8 11-15 8-15 7-14", act="WANDERING:2 SHOP", tail="CAT:2 FOX LIZARD NONE",
  hair="TWINTAILS:3 DRILLS:2 HIME:2 ODANGO", hair_col="CYAN:2 MAGENTA:2 LAVENDER BUBBLEGUM SKY",
  food="Bubble Tea|Mochi|Ramen", hobby="Cosplay:6|Video Games", music="Bubblegum Pop:3|Chiptune|City Pop",
  phrase="Nyaa~|Sparkle on!|Slay!",
  cast="""
Kawaii Kat | s | Wears cat ears over her cat ears. Double cat. | species=CATFOLK; hat=CAT_EARS
Mecha Mo | t | Built a robot suit from cereal boxes. It flies. | species=HUMAN; job=Prop Maker; top=ARMOR
Bunbun | s | Bunny-ear cosplayer. Hops to stay in character. | hat=BUNNY_EARS; skin=SKIN_PORCELAIN
Drakey | h | Dresses as a dragon with a lizard tail. Roars nicely. | tail=LIZARD; horns=SPIRAL; skin=SKIN_TAN
Con Queen | s | Con organizer. Runs a con for every hobby. | job=Con Organizer; skin=SKIN_OLIVE
""" ),

A("halorepair", "hill", "Halo Repairer|Wing Mender|Star Polisher",
  sp="CHERUB:3 MOTHKIN",
  top="OVERALLS:2 TSHIRT ROBE", bottom="NONE:2 SHORTS", shoes="SANDALS BARE",
  hat="NONE:4", acc="GLASSES:2 NONE",
  pat="SOLID:4", scheme="LEMON/SKY/WHITE WHITE/LEMON/SKY PEACH/LEMON/WHITE",
  halo="HALO:3 STARS:2", pers="4-10 0-4 6-13 8-15 2-8", act="WORKSHOP:2 HOME",
  food="Angel Cake:3|Honey Toast", hobby="Welding|Origami|Cloud Watching", music="Hymns|Classical|Lo-fi Beats",
  phrase="Sparkle on!|Oh my stars!|Okie dokie!",
  cast="""
Haloise | s | Fixes bent halos. Polishes them till they ding. | species=CHERUB; job=Halo Repairer
Feathers | h | Wing mender. Has a feather for every occasion. | job=Wing Mender
Starbright | t | Star polisher. Climbs up at night with a cloth. | job=Star Polisher; trait=NIGHT_OWL
Gleam | t | Polishes the church bells and the sunbeams. | species=MOTHKIN; wings=BUTTERFLY
""" ),

A("brimstonebites", "hill", "Line Cook|Head Chef|Dishwasher|Waiter",
  sp="RACCOONKIN:2 PUPKIN BEARKIN CATFOLK HUMAN",
  top="TSHIRT:2 OVERALLS TANK", bottom="PANTS:3", shoes="CLOGS:3 SNEAKERS",
  hat="CHEF:3 BANDANA:2", acc="NONE:4",
  pat="PLAID:2 SOLID:2", scheme="CHERRY/CHARCOAL/WHITE TANGERINE/CHOCOLATE/WHITE HONEY/CHERRY/WHITE",
  pers="9-15 4-11 6-13 6-13 4-11", act="CAFE:5",
  food="Brimstone Bagels|Curry|Spaghetti|Omurice|Fried Chicken", hobby="Baking|Karaoke|Video Games", music="Disco|City Pop|Hardcore Punk",
  phrase="Nom nom nom!|Yummy yummy!|Okie dokie!",
  cast="""
Chef Sizzle | h | Head chef at Brimstone Bites. Spicy food, sweet heart. | job=Head Chef; species=RACCOONKIN
Saucy | s | Line cook. Can flip four omelettes at once. | species=PUPKIN
Sudsy | t | Dishwasher. Sings to the soap bubbles. | job=Dishwasher; species=BEARKIN
Tipsy Toes | s | Waiter. Has never dropped a plate. Or a smile. | job=Waiter; species=CATFOLK
Omelette | h | Breakfast cook. Draws faces in ketchup. | species=HUMAN; skin=SKIN_TAN
""" ),

A("wallwatch", "hill", "Wall Watcher|Gatekeeper|Border Baker",
  sp="PUPKIN:2 BEARKIN OWLKIN",
  top="SWEATER_VEST LEATHER_JACKET ARMOR", bottom="CARGO:2 PANTS", shoes="BOOTS:3",
  hat="CAP:2 NONE", acc="NONE:2 SCARF",
  pat="SOLID:3 STRIPES", scheme="NAVY/LEMON/WHITE CHARCOAL/SKY/WHITE FOREST/HONEY/WHITE",
  pers="3-9 2-8 4-11 5-12 2-8", act="WANDERING:3",
  food="Coffee|Donuts|Onigiri", hobby="Birdwatching|Stargazing|Board Games", music="Folk|Jazz|Classical",
  phrase="Okie dokie!|Hoo hoo!|Tail wags!", fear="Ghosts|The Dark|Holy Water",
  cast="""
Lookout Lou | h | Watches the Wall. Waves at Hellvile kids over it. | species=PUPKIN
Gatekeep | s | Gatekeeper. Has never opened the gate. Polishes it. | species=ROBOT; job=Gatekeeper
Peekster | t | Peeks over the Wall with a periscope. Finds it rude. | species=OWLKIN
Bordercake | s | Throws cupcakes over the Wall. Some come back. | job=Border Baker; species=PUPKIN
""" ),

A("bathhouse", "hill", "Bath House Keeper|Towel Folder|Soap Maker",
  sp="FROGLING CATFOLK SHEEPKIN BUNNYKIN",
  top="KIMONO:3 PAJAMAS", bottom="NONE:3", shoes="SANDALS:3 SLIPPERS",
  hat="NONE:3", acc="NONE:3",
  pat="SOLID:3 STRIPES", scheme="TEAL/WHITE/PEACH SKY/WHITE/LEMON MINT/WHITE/ROSE",
  pers="1-7 0-4 5-12 11-15 2-8", act="SHOP:3", aura="BUBBLES:3 NONE",
  food="Milk|Mochi|Pudding", hobby="Napping|Bubble Blowing|Yoga", music="Lo-fi Beats|Classical",
  phrase="Cozy vibes only!|So fluffy!|Snuggle up!",
  cast="""
Steamy | s | Runs the bath house. Every tub has a rubber duck. | job=Bath House Keeper; species=FROGLING
Towelette | t | Folds towels into animals. Has a towel zoo. | job=Towel Folder; species=SHEEPKIN
Lather | h | Soap maker. His soaps smell like rainbows. | species=HUMAN; job=Soap Maker
Bubbletub | s | Takes three baths a day. Reviews each one. | species=BUNNYKIN
""" ),

A("mascots", "hill", "Town Mascot|Parade Float Driver|Balloon Twister",
  sp="PLUSHIE:3 BEARKIN",
  top="TSHIRT:2 OVERALLS", bottom="SHORTS:2 NONE", shoes="SNEAKERS:2 BARE",
  hat="PARTY:2 NONE:2 PROPELLER", acc="BOWTIE:2 NONE",
  pat="POLKA:2 STRIPES", scheme="LEMON/CHERRY/SKY BUBBLEGUM/LEMON/MINT SKY/TANGERINE/WHITE",
  pers="9-15 1-6 12-15 12-15 7-13", act="PARK:4",
  food="Cotton Candy|Popcorn|Ice Cream", hobby="Dancing|Bubble Blowing|Pranks", music="Bubblegum Pop|Disco",
  phrase="Yippee!|Wheee!|Boop!",
  cast="""
Hugsley | i | Town mascot. Gets hugged 1,000 times a day. | species=PLUSHIE; job=Town Mascot
Twisty | s | Balloon twister. Can make a balloon you. | species=PLUSHIE; job=Balloon Twister
Floaty | h | Drives the parade float. It is very slow. | job=Parade Float Driver
Jumbo | t | So big and soft they double as a bouncy castle. | species=BEARKIN; build=15; body=ROUND
""" ),

A("trains", "hill", "Train Conductor|Ticket Puncher|Station Master",
  sp="PUPKIN BEARKIN MOUSEKIN ROBOT HUMAN",
  top="SUIT:2 SWEATER_VEST:2", bottom="PANTS:4", shoes="BOOTS:3",
  hat="CAP:4 TOP_HAT", acc="BOWTIE NONE:2",
  pat="SOLID:3 STRIPES", scheme="NAVY/LEMON/WHITE CHERRY/NAVY/LEMON FOREST/LEMON/WHITE",
  pers="4-10 1-6 6-13 6-12 2-8", act="WANDERING:3",
  food="Coffee|Onigiri|Donuts", hobby="Board Games|Origami|Reading", music="Jazz|Folk|City Pop",
  phrase="Toot toot!|Okie dokie!|Wheee!",
  cast="""
Choo Choo | h | Conductor of the park train. Toots on the hour. | species=PUPKIN
Punchy | s | Ticket puncher. Punches heart-shaped holes. | job=Ticket Puncher; species=MOUSEKIN
Steam Stan | t | Station master. Knows every timetable by heart. | species=HUMAN; job=Station Master
Caboose | i | Train bot. Is the last carriage, and proud of it. | species=ROBOT
Waitabit | x | Always waiting for a train that never comes. Happy. | species=HUMAN; skin=SKIN_PEACH
""" ),

A("kites", "hill", "Kite Flyer|Windmill Keeper|Bubble Artist",
  sp="FOXKIN BUNNYKIN SHEEPKIN MOUSEKIN",
  top="TSHIRT:2 SWEATER OVERALLS", bottom="SHORTS:2 SKIRT", shoes="SNEAKERS:2 BOOTS",
  hat="CAP BUCKET NONE:2", acc="SCARF:2 NONE",
  pat="STRIPES:2 SOLID POLKA", scheme="SKY/TANGERINE/WHITE LIME/LAVENDER/WHITE LEMON/COBALT/WHITE",
  pers="8-14 1-6 8-14 10-15 4-11", act="PARK:5",
  food="Cotton Candy|Pancakes|Popcorn", hobby="Cloud Watching|Bubble Blowing:3|Birdwatching", music="Folk|Bubblegum Pop",
  phrase="Wheee!|Yippee!|Sunny side up!",
  cast="""
Windy | s | Flies kites shaped like her friends' faces. | species=FOXKIN
Gusty | h | Windmill keeper. Talks to the wind like an old pal. | job=Windmill Keeper; species=SHEEPKIN
Bubblina | t | Bubble artist. Blew a bubble around a whole bus. | job=Bubble Artist; species=BUNNYKIN; aura=BUBBLES
Pinwheel | s | Carries a pinwheel everywhere. It is never still. | species=MOUSEKIN
""" ),

A("carnival", "hill", "Carnival Game Host|Ferris Wheel Op|Fortune Machine",
  sp="RACCOONKIN FOXKIN PUPKIN ROBOT",
  top="SUIT TSHIRT:2 SWEATER_VEST", bottom="PANTS:2 SHORTS", shoes="SNEAKERS:2",
  hat="TOP_HAT CAP:2 PARTY", acc="BOWTIE:2 NONE",
  pat="STRIPES:4 POLKA", scheme="CHERRY/LEMON/WHITE COBALT/CHERRY/LEMON MAGENTA/CYAN/LEMON",
  pers="9-15 4-11 11-15 10-15 7-14", act="PARK:4",
  food="Popcorn:3|Cotton Candy|Donuts", hobby="Board Games|Pranks|Video Games", music="Disco|Chiptune|Bubblegum Pop",
  phrase="Toot toot!|Yippee!|Hehehe~",
  cast="""
Ringtoss | h | Carnival host. Everybody wins a goldfish. | species=RACCOONKIN
Ferris | s | Ferris wheel op. Stops at the top for kisses. | species=HUMAN; job=Ferris Wheel Op
Fortunabot | i | Fortune machine bot. Every fortune says "hug". | job=Fortune Machine; species=ROBOT
Whackamole | t | Plays whack-a-mole. Apologises to every mole. | species=PUPKIN
""" ),

A("weddings", "hill", "Wedding Planner|Cake Architect|Ring Bearer",
  sp="CATFOLK BUNNYKIN SHEEPKIN HUMAN",
  top="DRESS:2 SUIT:2", bottom="LONG_SKIRT PANTS", shoes="HEELS:2 BOOTS",
  hat="FLOWER_CROWN:2 NONE", acc="EARRINGS BOWTIE",
  pat="SOLID:3 HEARTS", scheme="WHITE/ROSE/LEMON PEACH/WHITE/LAVENDER LAVENDER/WHITE/ROSE",
  pers="7-13 0-4 10-15 7-14 2-8", act="CHURCH:2 SHOP",
  food="Angel Cake:3|Macarons|Cupcakes", hobby="Dancing|Painting|Origami", music="Classical|Bubblegum Pop|Jazz",
  phrase="Smooch!|Oh my stars!|Hug o'clock!",
  cast="""
Bridget | s | Wedding planner. Has cried at 300 weddings. | species=HUMAN
Tiers | h | Cake architect. Built a cake with an elevator. | job=Cake Architect; species=SHEEPKIN
Ringlet | t | Ring bearer for hire. Has never lost a ring. | job=Ring Bearer; species=BUNNYKIN; trait=KID
Confetti | s | Throws confetti at every couple she sees. | species=HUMAN; skin=SKIN_FAIR
""" ),

A("bedtime", "hill", "Bedtime Reader|Lullaby Singer|Dream Catcher",
  sp="OWLKIN SHEEPKIN MOTHKIN BEARKIN",
  top="PAJAMAS:3 SWEATER", bottom="NONE:2 SWEATS", shoes="SLIPPERS:3",
  hat="NONE:2 BEANIE", acc="GLASSES NONE:2 TEDDY",
  pat="POLKA:2 SOLID STRIPES", scheme="NAVY/LEMON/WHITE LAVENDER/SKY/LEMON GRAPE/PEACH/WHITE",
  pers="0-5 0-3 5-11 10-15 2-8", act="HOME:4", traits="NIGHT_OWL",
  food="Milk:3|Honey Toast|Pudding", hobby="Reading|Singing|Stargazing", music="Lullabies:6",
  phrase="Snuggle up!|Cozy vibes only!|Hoo hoo!",
  cast="""
Story Owl | h | Reads bedtime stories to the whole street at once. | species=OWLKIN
Lullabelle | s | Sings lullabies that make thunderstorms nap. | job=Lullaby Singer; species=SHEEPKIN
Dreamweave | t | Catches bad dreams in a net. Swaps them for good. | job=Dream Catcher; species=MOTHKIN
Nightnight | h | Says goodnight to every star. It takes hours. | species=HUMAN
""" ),

A("lostfound", "hill", "Lost and Found Clerk|Sock Matcher|Key Keeper",
  sp="RACCOONKIN:2 MOUSEKIN",
  top="SWEATER_VEST:2 OVERALLS", bottom="PANTS:2", shoes="SNEAKERS SLIPPERS",
  hat="NONE:2 BERET", acc="GLASSES:2 BACKPACK",
  pat="PLAID:2 SOLID", scheme="TEAL/HONEY/WHITE CHOCOLATE/SKY/WHITE GRAPE/LEMON/WHITE",
  pers="2-8 0-4 4-11 7-14 4-11", act="SHOP:3",
  food="Cereal|Popcorn|Melon Bread", hobby="Bug Collecting|Board Games|Origami", music="Lo-fi Beats|Jazz",
  phrase="Oopsie daisy!|Squeak squeak!|Okie dokie!",
  cast="""
Finders | s | Lost and found clerk. Found 4,000 mittens. | job=Lost and Found Clerk; species=RACCOONKIN
Sockmatch | h | Has matched 9,999 socks. Hunting the last one. | job=Sock Matcher; species=MOUSEKIN
Keysie | t | Key keeper. Holds a spare key for everyone. | job=Key Keeper; species=RACCOONKIN
Umbrellina | s | Keeps 300 lost umbrellas. Lends them out when it rains. | species=MOUSEKIN
""" ),

]
