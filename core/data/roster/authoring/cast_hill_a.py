"""Hillvile archetypes, part A: civic life, food, care, faith and craft.
(owner: roster; see seed.py)"""
from seedlib import A

ARCHETYPES = [

A("town_hall", "hill", "Town Clerk|Deputy Mayor|Stamp Keeper|Parade Planner",
  sp="CHERUB:3 SHEEPKIN:2 OWLKIN:2 PUPKIN BEARKIN HUMAN",
  top="SUIT:4 SWEATER_VEST:3 DRESS:2", bottom="PANTS:3 SKIRT:2 LONG_SKIRT",
  shoes="SNEAKERS:2 HEELS CLOGS BOOTS", hat="NONE:5 TOP_HAT:2 BERET", acc="BOWTIE:4 GLASSES:3 MONOCLE NONE:2",
  pat="SOLID:4 STRIPES:2 PLAID", scheme="SKY/NAVY/LEMON LEMON/SKY/WHITE ROSE/NAVY/WHITE MINT/NAVY/LEMON",
  pers="4-10 0-5 8-15 7-14 2-9", act="HOME:2 SHOP PARK",
  food="Cupcakes|Angel Cake|Macarons|Coffee|Pancakes", hobby="Board Games|Karaoke|Origami|Reading",
  music="Bubblegum Pop|Classical|Jazz|City Pop", phrase="Oh my stars!|Okie dokie!|Bless your heart!|Toot toot!",
  cast="""
Mayor Giggs | h | Laughs at his own speeches. So does everyone else. | id=0; story=MAYOR; job=Mayor of Hillvile; trait=HOLY; species=CHERUB; body=ROUND; height=4; build=13; skin=SKIN_PEACH; head=MOCHI; head_size=14; eyes=HAPPY_ARC; eye_color=SKY; eye_count=2; brows=RAISED; mouth=BIG_GRIN; nose=BUTTON; hair=POMPADOUR; hair_color=LEMON; highlight=WHITE; facial_hair=HANDLEBAR; wings=ANGEL; halo=HALO; horns=NONE; marking=CHEEK_HEARTS; marking_color=ROSE; cheeks=BLUSH,DIMPLES; top=SUIT; top_color=SKY; pattern=STRIPES; pattern_color=WHITE; bottom=PANTS; bottom_color=NAVY; shoes=SNEAKERS; shoes_color=WHITE; hat=TOP_HAT; hat_color=LEMON; accessory=BOWTIE; accessory_color=ROSE; aura=SPARKLES; birthday=APR-01; food=Cupcakes; hates=Brussels Sprouts; hobby=Karaoke; music=Bubblegum Pop; color=LEMON; fear=Public Speaking; phrase=Oh my stars!; movement=9; speech=3; expression=15; attitude=14; quirk=9; romance=ANY; activity=PARK; corruption=0; happy=96
Featherly | s | Has stamped 40,000 forms. Loved every single one. | job=Stamp Keeper; species=OWLKIN; acc=GLASSES; bff=Mayor Giggs
Ribbonetta | s | Cuts every ribbon in town, even the ones on gifts. | job=Ribbon Cutter
Bixby Maple | h | Deputy Mayor. Practises the Mayor's laugh nightly. | species=HUMAN; job=Deputy Mayor
Parfait | t | Plans parades for things like Tuesday. | species=CHERUB; job=Parade Planner; hat=PARTY
Tallyho | h | Counts the town's hugs for the annual census. | species=HUMAN; job=Census Taker
""" ),

A("bakery", "hill", "Baker|Pastry Chef|Bread Apprentice|Cake Decorator",
  sp="BUNNYKIN:3 BEARKIN:2 SHEEPKIN:2 PUPKIN MOUSEKIN HUMAN",
  top="SWEATER:3 DRESS:2 OVERALLS:3 TSHIRT", bottom="SKIRT:2 PANTS:2 LONG_SKIRT",
  shoes="CLOGS:3 SLIPPERS:2 SNEAKERS", hat="CHEF:5 BANDANA:2 NONE:2", acc="NONE:4 SCARF BOWTIE",
  pat="SOLID:3 POLKA:2 PLAID HEARTS", scheme="PEACH/WHITE/HONEY CARAMEL/WHITE/ROSE BUBBLEGUM/WHITE/LEMON LEMON/PEACH/WHITE CHOCOLATE/PEACH/WHITE",
  pers="3-9 0-5 8-14 9-15 0-6", act="CAFE:3 SHOP", body="ROUND:4 AVERAGE:2 PEAR:2",
  food="Cupcakes|Pancakes|Honey Toast|Melon Bread|Carrot Cake|Angel Cake|Donuts|Waffles",
  hobby="Baking:5|Knitting|Gardening", music="Lullabies|Jazz|Folk|City Pop",
  phrase="Sweet as pie!|Yummy yummy!|Nom nom nom!|Cozy vibes only!", cheeks="BLUSH:85 FRECKLES:25 DIMPLES:20",
  cast="""
Nana Puddin | s | Bakes for the whole town. Hums while the dough rises. | id=1; story=NANA_PUDDING; job=Head Baker; trait=ELDER; species=SHEEPKIN; fur=WHITE/BONE; body=ROUND; height=4; build=12; head=MOCHI; eyes=HAPPY_ARC; eye_color=CHOCOLATE; brows=DOTS; mouth=SMILE; hair=CURLY; hair_color=WHITE; highlight=NONE; marking=FUR_MUZZLE; marking_color=BONE; cheeks=BLUSH,DIMPLES; top=SWEATER; top_color=PEACH; pattern=POLKA; pattern_color=WHITE; bottom=LONG_SKIRT; bottom_color=CARAMEL; shoes=SLIPPERS; shoes_color=ROSE; hat=CHEF; hat_color=WHITE; accessory=GLASSES; accessory_color=CHOCOLATE; aura=HEARTS; birthday=DEC-24; food=Melon Bread; hates=Sulfur Soda; hobby=Baking; music=Lullabies; color=PEACH; fear=Being Alone; phrase=Sweet as pie!; movement=3; speech=1; expression=12; attitude=13; quirk=4; romance=NONE; activity=CAFE; corruption=0
Muffin | s | Bakes muffins shaped like everyone in town. | job=Muffin Artist; species=BUNNYKIN; fam=bunbury
Crumpet | h | Kneads dough to the beat of old lullabies. | species=BEARKIN; wed=Muffin; fam=bunbury
Bunbury Jr | t | Sneaks raisins. Leaves frosting fingerprints. | job=Bread Apprentice; trait=KID; species=BUNNYKIN; fam=bunbury; hat=NONE
Scone | h | Can tell the oven temperature by sniffing it. | species=MOUSEKIN
Fondant | s | Pipes tiny roses onto every cake. Even the meat pies. | species=HUMAN; job=Cake Decorator
Sourdoe | x | Talks to xyr sourdough starter. It talks back, xe says. | job=Bread Whisperer; species=PUPKIN
""" ),

A("cafe", "hill", "Barista|Waiter|Latte Artist|Cafe Manager",
  sp="CATFOLK:4 FOXKIN:3 RACCOONKIN MOUSEKIN HUMAN",
  top="MAID:2 SWEATER:2 TSHIRT:2 SWEATER_VEST:2 CROP_TOP", bottom="SKIRT:2 PANTS:2 SHORTS",
  shoes="SNEAKERS:2 SLIPPERS CLOGS", hat="NONE:5 BERET:2 BANDANA", acc="BOWTIE:3 NONE:3 EARRINGS",
  pat="SOLID:3 STRIPES PLAID", scheme="MINT/CHOCOLATE/CARAMEL CHOCOLATE/CARAMEL/WHITE MINT/CHOCOLATE/PEACH ROSE/CHOCOLATE/LEMON CARAMEL/TEAL/WHITE",
  pers="9-15 1-7 7-14 8-15 2-9", act="CAFE:4 HOME",
  food="Coffee:4|Macarons|Pancakes|Bubble Tea|Waffles", hobby="Painting|Karaoke|Board Games|Reading",
  music="Lo-fi Beats:3|Jazz:2|City Pop", phrase="Purr-fect!|Okie dokie!|Yummy yummy!|Nyaa~",
  cast="""
Mocha Latte | s | Draws a different cat in every latte foam. | job=Latte Artist; species=CATFOLK
Espresso | h | Talks fast. Walks fast. Has never once sat down. | species=ROBOT; movement=15
Cardamom | t | Remembers everyone's order and their birthday. | job=Cafe Manager; species=FOXKIN
Biscotto | h | Dunks everything. Even things that should not be dunked. | species=RACCOONKIN
Frappella | s | Sings the menu as an opera every morning. | species=CATFOLK
Cortado | h | Rates every bean out of ten. Gives everything a ten. | species=HUMAN
""" ),

A("candy", "hill", "Candy Maker|Gumdrop Taster|Taffy Puller|Lollipop Artist",
  sp="MOUSEKIN:3 BUNNYKIN:2 PLUSHIE:2 CATFOLK HUMAN",
  top="OVERALLS:3 TSHIRT:2 DRESS:2 CROP_TOP", bottom="SKIRT:2 SHORTS:2 TUTU",
  shoes="SNEAKERS:2 HEELS ROLLERSKATES", hat="NONE:3 PARTY:2 PROPELLER", acc="NONE:3 BOWTIE EARRINGS",
  pat="STRIPES:4 POLKA:3 HEARTS:2 SOLID", scheme="BUBBLEGUM/MINT/LEMON MAGENTA/LEMON/CYAN CYAN/BUBBLEGUM/WHITE LIME/BUBBLEGUM/LEMON",
  pers="10-15 2-7 10-15 9-15 5-12", act="SHOP:3 HOME", eyes="SPARKLE:3 STAR:2 ANIME:2 ROUND:2",
  food="Gumdrops:3|Cotton Candy:3|Macarons|Donuts|Ice Cream|Candy Eyeballs", hobby="Baking|Bubble Blowing|Dancing",
  music="Bubblegum Pop:4|Disco|Chiptune", phrase="Yippee!|Wheee!|Sparkle on!|Yummy yummy!", cheeks="BLUSH:90 SPARKLES:30",
  cast="""
Gumball | h | Has a sweet tooth. Also a sour one. | species=SLIME
Taffeta | s | Pulls taffy into ribbons longer than the street. | job=Taffy Puller
Jellybean | t | Sorts jellybeans by mood. Blue ones are shy. | species=PLUSHIE
Sprinklepop | s | Sneezes glitter. Doctors say it is fine. | job=Lollipop Artist; aura=SPARKLES
Rocky Road | h | Tried to make a candy house. Ate the door. | species=SLIME
""" ),

A("florist", "hill", "Florist|Seed Seller|Bouquet Artist",
  sp="BUNNYKIN:3 MOTHKIN:2 SHEEPKIN:2 FOXKIN HUMAN",
  top="DRESS:4 OVERALLS:2 SWEATER:2", bottom="LONG_SKIRT:2 SKIRT SHORTS",
  shoes="SANDALS:3 BOOTS:2 BARE", hat="FLOWER_CROWN:5 BUCKET NONE:2", acc="NONE:4 SCARF EARRINGS",
  pat="SOLID:2 POLKA HEARTS:2 STRIPES", scheme="ROSE/MINT/LEMON LAVENDER/LIME/WHITE PEACH/FOREST/ROSE LEMON/MINT/LAVENDER",
  pers="2-8 0-4 8-14 10-15 1-7", act="SHOP:3 PARK:2",
  food="Salad|Honey Toast|Macarons|Angel Cake|Bubble Tea", hobby="Gardening:4|Painting|Poetry|Origami",
  music="Folk:3|Classical|Lullabies", phrase="Oopsie daisy!|Sunny side up!|Snuggle up!|Teehee!",
  fear="Bees|Thunder|Spiders",
  cast="""
Petunia | s | Names every flower. Has 3,112 flower friends. | species=BUNNYKIN; bff=Marigold
Marigold | s | Grows marigolds the size of dinner plates. | species=SHEEPKIN
Bouquet | t | Makes a bouquet for anyone who looks a bit sad. | species=CHERUB; job=Bouquet Artist
Larkspur | h | Sells seeds. Gives most of them away for free. | job=Seed Seller; species=FOXKIN
Bluebell | s | Presses flowers in every book she borrows. | species=BUNNYKIN
Sweetpea | t | Whispers encouragement to slow-growing tulips. | species=MOTHKIN
""" ),

A("gardeners", "hill", "Gardener|Pumpkin Grower|Hedge Sculptor|Groundskeeper",
  sp="FROGLING:3 BEARKIN:3 RACCOONKIN MOUSEKIN HUMAN",
  top="OVERALLS:5 TSHIRT:2 TANK", bottom="SHORTS:2 PANTS:2 CARGO",
  shoes="BOOTS:4 BARE:2 CLOGS", hat="BUCKET:3 CAP:2 NONE:2 COWBOY", acc="NONE:4 SCARF",
  pat="SOLID:3 PLAID:2", scheme="LIME/FOREST/CHARTREUSE FOREST/HONEY/WHITE LIME/CHOCOLATE/LEMON TEAL/CARAMEL/PEACH CHARTREUSE/FOREST/HONEY",
  pers="1-7 2-8 4-11 10-15 0-8", act="PARK:4 HOME",
  food="Salad:3|Carrot Cake|Pickles|Onigiri|Brussels Sprouts", hobby="Gardening:5|Bug Collecting|Birdwatching",
  music="Folk:3|Sea Shanties|Lo-fi Beats", phrase="Ribbit!|Sunny side up!|Okie dokie!|Big mood.",
  cast="""
Gourdon | h | Grew a pumpkin so big it has its own address. | job=Pumpkin Grower; species=BEARKIN; build=14; bio_pronoun_ok=1
Topiary | s | Trims hedges into portraits of her neighbours. | job=Hedge Sculptor; species=FROGLING
Mulch | t | Loves worms. Has introduced the worms to everyone. | species=FROGLING
Sprouty | h | Plants one tree for every nap he takes. | species=BEARKIN; fam=sprout
Clover Moss | s | Finds a four-leaf clover every single morning. | species=MOUSEKIN; fam=sprout
Thatch | h | Groundskeeper of Hellvile Park. Rakes leaves into hearts. | species=HUMAN; job=Groundskeeper
""" ),

A("mail", "hill", "Mail Carrier|Parcel Sorter|Postmaster",
  sp="FROGLING:5 PUPKIN BUNNYKIN",
  top="TSHIRT:2 SWEATER_VEST:2 HOODIE", bottom="SHORTS:3 PANTS",
  shoes="SNEAKERS:4 BOOTS", hat="CAP:5 NONE", acc="BACKPACK:5 NONE",
  pat="SOLID:4 STRIPES", scheme="SKY/NAVY/LEMON COBALT/SKY/WHITE TEAL/NAVY/LEMON",
  pers="11-15 2-7 6-12 7-14 1-7", act="WANDERING:4 SHOP",
  food="Onigiri|Dumplings|Milk|Cereal", hobby="Origami|Birdwatching|Cloud Watching",
  music="City Pop|Chiptune|Folk", phrase="Ribbit!|Toot toot!|Okie dokie!", fear="Thunder|Deep Water",
  cast="""
Hopscotch | h | Delivers mail by hopping. Has never missed a hop. | species=FROGLING
Lilypad Lou | s | Knows every address by heart and every pet by name. | species=FROGLING
Stampton | t | Licks stamps with great ceremony. | job=Parcel Sorter; species=FROGLING
Ribbitson | h | Postmaster. Wears a different cap for each weekday. | job=Postmaster; species=FROGLING; facial_hair=MUSTACHE
Envelopa | s | Writes back to letters addressed to nobody. | species=HUMAN
""" ),

A("crossing", "hill", "Crossing Guard|Traffic Bot|Lost Kid Finder",
  sp="ROBOT:6",
  top="SWEATER_VEST:3 HAZMAT TSHIRT", bottom="PANTS:3 NONE", shoes="BOOTS:3 SNEAKERS ROLLERSKATES",
  hat="TRAFFIC_CONE:3 CAP:2", acc="NONE:3 BACKPACK", pat="STRIPES:3 SOLID",
  scheme="LEMON/TANGERINE/WHITE TANGERINE/LEMON/SILVER LIME/LEMON/SKY",
  pers="4-10 0-3 2-8 3-9 0-6", act="WANDERING:3 PARK", eyes="GLOW:2 DOT:2 ROUND WIDE",
  eye_col="CYAN SKY LIME LEMON", wings="NONE", horns="ANTENNAE:2 NONE",
  food="Milk|Cereal|Popcorn", hobby="Board Games|Cloud Watching|Origami", music="Chiptune:4|Synthwave",
  phrase="Beep boop!|Okie dokie!|Toot toot!", fear="Deep Water|Thunder|Vacuum Cleaners",
  cast="""
Stopsign | i | Holds up traffic with one hand. Waves with the other. | pitch=12
Bolt Buddy | i | Helps ducklings cross first. Always. |
Crosswalk | i | Beeps a lullaby while the light is red. |
Beacon | t | Glows brighter when kids wave at it. |
Gizmo Gary | h | Collects lost mittens and returns them washed. | job=Lost Kid Finder
""" ),

A("library", "hill", "Ghost Librarian|Archivist|Book Mender",
  sp="GHOST:5 OWLKIN MOTHKIN",
  top="SWEATER_VEST:3 SWEATER:2 NONE", bottom="NONE:3 LONG_SKIRT", shoes="BARE",
  hat="NONE:4 BERET", acc="GLASSES:5 SCARF", pat="SOLID:3 PLAID:2",
  scheme="LAVENDER/NAVY/WHITE MINT/TEAL/WHITE SKY/NAVY/LEMON WHITE/GRAPE/LAVENDER",
  pers="0-5 0-3 1-7 3-10 1-8", act="HOME:3 SHOP", eyes="SLEEPY:3 DOT:2 ROUND VOID",
  food="Pudding|Milk|Angel Cake|Honey Toast", hobby="Reading:5|Poetry|Origami", music="Classical:3|Lo-fi Beats:2|Jazz",
  phrase="Hoo hoo!|Boo!|Cozy vibes only!|Snuggle up!", fear="Vacuum Cleaners|Being Alone", traits="NIGHT_OWL",
  cast="""
Shushana | s | Shushes so softly that the books lean in to listen. | species=GHOST
Dewey | h | Has read every book twice. Once while alive. | species=GHOST
Wispy Pages | t | Floats up to the top shelves so you do not have to. | species=GHOST
Inkwell | s | Mends torn books with ghostly thread. | job=Book Mender; species=MOTHKIN
Footnote | h | Archives the town gossip for future historians. | species=GHOST; job=Archivist
Boo Radish | x | Haunts the cookbook aisle. Smells of soup. | species=GHOST
""" ),

A("teachers", "hill", "Teacher|Music Teacher|Art Teacher|Headteacher",
  sp="OWLKIN:4 SHEEPKIN BEARKIN HUMAN FOXKIN",
  top="SWEATER_VEST:3 SWEATER:3 SUIT DRESS", bottom="PANTS:2 LONG_SKIRT:2 SKIRT",
  shoes="CLOGS:2 BOOTS SNEAKERS", hat="NONE:5 BERET", acc="GLASSES:5 SCARF BOWTIE",
  pat="PLAID:3 SOLID:2 STRIPES", scheme="FOREST/HONEY/WHITE NAVY/LEMON/ROSE CHOCOLATE/MINT/PEACH TEAL/PEACH/WHITE",
  pers="3-9 0-6 6-13 6-13 1-7", act="HOME:2 PARK",
  food="Coffee:2|Honey Toast|Carrot Cake|Pudding|Curry", hobby="Reading:3|Board Games|Stargazing|Poetry|Painting",
  music="Classical:3|Jazz|Folk", phrase="Hoo hoo!|Okie dokie!|Let's be friends!|Oh my stars!",
  cast="""
Prof. Hoot | h | Gives gold stars for effort, and silver ones for naps. | job=Headteacher; species=OWLKIN; facial_hair=CHOPS
Miss Tuttle | s | Teaches maths using cupcakes as units. | species=SHEEPKIN
Mr. Thistle | h | Field trip enthusiast. Has lost zero kids, mostly. | species=BEARKIN
Sonata | t | Music teacher. Tunes the school piano by ear. | job=Music Teacher; species=OWLKIN
Crayon Fern | s | Art teacher. Every wall in her house is a mural. | species=HUMAN; job=Art Teacher
Owlbert | h | Teaches night classes in astronomy and owl jokes. | species=OWLKIN; trait=NIGHT_OWL
""" ),

A("kids", "hill", "Student|Little Explorer|Playground Champ",
  sp="BUNNYKIN:3 CATFOLK:2 PUPKIN:2 FOXKIN MOUSEKIN BEARKIN HUMAN CHERUB PLUSHIE",
  traits="KID", body="TINY:5 ROUND:3", build="6-12",
  top="TSHIRT:3 HOODIE:3 OVERALLS:2 PAJAMAS SWEATER", bottom="SHORTS:3 SKIRT:2 PANTS",
  shoes="SNEAKERS:4 BOOTS SLIPPERS ROLLERSKATES", hat="NONE:4 CAP PROPELLER:2 BEANIE PARTY", acc="NONE:3 BACKPACK:3 TEDDY",
  pat="SOLID:2 STRIPES POLKA HEARTS", scheme="SKY/LEMON/BUBBLEGUM LIME/SKY/WHITE BUBBLEGUM/MINT/LEMON TANGERINE/SKY/WHITE",
  pers="10-15 2-9 10-15 8-15 6-14", act="PARK:3 HOME", eyes="ANIME:3 SPARKLE:3 ROUND:2 WIDE",
  food="Ice Cream|Cotton Candy|Pizza|Cereal|Gumdrops|Pancakes|Popcorn", hobby="Video Games|Skateboarding|Bug Collecting|Bubble Blowing|Board Games",
  music="Bubblegum Pop|Chiptune|Disco", phrase="Wheee!|Yippee!|Boop!|Blep!|Hehehe~|Wiggle wiggle!",
  fear="The Dark|Thunder|Bath Time|Dentists|Vacuum Cleaners",
  cast="""
Pip | t | Lost their bunny plush, Mr. Flops. Very brave about it. | id=42; story=PIP; job=Student; species=BUNNYKIN; fur=WHITE/BUBBLEGUM; body=TINY; height=1; build=8; head=MOCHI; head_size=15; eyes=ANIME; eye_color=SKY; eye_size=14; brows=WORRIED; mouth=POUT; hair=MESSY; hair_color=WHITE; highlight=NONE; marking=FUR_TIPPED_EARS; marking_color=BUBBLEGUM; cheeks=BLUSH,FRECKLES; top=OVERALLS; top_color=SKY; pattern=SOLID; bottom=NONE; shoes=SNEAKERS; shoes_color=LEMON; hat=NONE; accessory=BACKPACK; accessory_color=LEMON; aura=NONE; birthday=MAR-21; food=Carrot Cake; hates=Pickles; hobby=Bug Collecting; music=Bubblegum Pop; color=SKY; fear=The Dark; phrase=Bun-believable!; movement=12; speech=2; expression=14; attitude=9; quirk=7; activity=PARK
Scamper | h | Fastest kid in Hillvile. Has no brakes. | species=PUPKIN; movement=15; bff=Pip
Dandy Lion | h | Blows dandelions and makes very specific wishes. | species=CATFOLK
Tadpole | t | Can hold their breath for nine whole seconds. | species=FROGLING
Kaboodle | s | Draws treasure maps to places that do not exist. | species=FOXKIN; fam=kaboodle
Cubby | h | Hugs trees. The trees seem to like it. | species=BEARKIN
Mimsy | s | Has an imaginary friend who is a real ghost. | species=MOUSEKIN
Puddlejump | t | Owns six pairs of rain boots. Prays for rain. | species=BUNNYKIN; shoes=BOOTS
Pippa Pop | s | Collects bottle caps. Knows each one's backstory. | species=CATFOLK; bff=Mimsy
Twinkly | s | Wants to be an astronaut, a baker and a cloud. | species=CHERUB; skin=SKIN_BROWN
Button Nose | h | Buttons everything. Even other people's coats. | species=PLUSHIE; eyes=BUTTON
Kit Tumble | h | Falls down a lot. Gets up even more. | species=FOXKIN; fam=kaboodle
""" ),

A("toddlers", "hill", "Toddler|Nap Champion",
  sp="SHEEPKIN BUNNYKIN PUPKIN CHERUB:2 BEARKIN MOUSEKIN",
  traits="KID", body="TINY:6 ROUND:3", build="9-14", height="0-2",
  top="PAJAMAS:4 TSHIRT:2 OVERALLS", bottom="NONE:3 SHORTS", shoes="SLIPPERS:3 BARE:2",
  hat="NONE:3 BEANIE BUNNY_EARS", acc="PACIFIER:5 TEDDY:3",
  pat="POLKA:3 SOLID:2 HEARTS", scheme="SKY/LEMON/WHITE BUBBLEGUM/WHITE/LEMON MINT/PEACH/WHITE LAVENDER/SKY/WHITE",
  pers="4-12 0-4 12-15 10-15 6-12", act="HOME:4", eyes="WIDE:3 ROUND:2 SPARKLE",
  food="Milk:4|Pudding:2|Cereal|Honey Toast", hobby="Napping:4|Bubble Blowing:3", music="Lullabies:5",
  phrase="Blep!|Boop!|Wheee!|Mochi mochi!", fear="Thunder|The Dark|Bath Time|Popping Balloons",
  cast="""
Bitty Boo | t | Says one word, "boop", with forty meanings. | species=SHEEPKIN; fam=boo
Wobbles | h | Just learned to walk. Will not stop walking. | species=PUPKIN; fam=boo
Peanut | s | Naps in sunbeams like a cat. Is not a cat. | species=BUNNYKIN; fam=peanut
Cherubino | h | Flies three inches off the ground. Proudly. | species=CHERUB; skin=SKIN_TAN
Snugglet | s | Has never once let go of her teddy. | species=BEARKIN; accessory=TEDDY
""" ),

A("grannies", "hill", "Grandma|Retired Knitter|Jam Maker|Quilt Master",
  sp="SHEEPKIN:3 BEARKIN:2 MOUSEKIN BUNNYKIN CATFOLK HUMAN",
  traits="ELDER", body="ROUND:3 HUNCHED:2 PEAR:2 TINY", height="2-7",
  top="SWEATER:5 DRESS:2 PAJAMAS", bottom="LONG_SKIRT:4 PANTS", shoes="SLIPPERS:4 CLOGS",
  hat="NONE:4 BEANIE:2 FLOWER_CROWN", acc="GLASSES:5 SCARF:2 EARRINGS",
  pat="SOLID:2 PLAID:2 POLKA:2 HEARTS", scheme="LAVENDER/ROSE/WHITE PEACH/MINT/WHITE ROSE/LAVENDER/LEMON MINT/PEACH/CHOCOLATE",
  pers="0-5 0-6 6-13 9-15 3-11", act="HOME:4 PARK CHURCH", eyes="HAPPY_ARC:4 SLEEPY:2 DROOPY:2",
  hair="BUN:5 CURLY:3 CROWN_BRAID:2", hair_col="WHITE:4 SILVER:3 LAVENDER", hair_force="1",
  food="Pudding|Honey Toast|Angel Cake|Carrot Cake|Milk", hobby="Knitting:5|Baking|Gardening|Board Games|Plush Making",
  music="Lullabies:2|Hymns:2|Classical|Folk", phrase="Bless your heart!|Snuggle up!|Cuddle puddle!|Oh my stars!",
  fear="Thunder|Vacuum Cleaners|Being Alone|Needles",
  cast="""
Granny Purl | s | Has knitted a scarf for every citizen. Starting on 500. | job=Retired Knitter
Nana Jam | s | Makes jam from fruits nobody else can find. | species=HUMAN; job=Jam Maker
Mabel Quilt | s | Her quilts are so warm they count as a hug. | job=Quilt Master; wed=Gramps Moss
Gran Toffee | s | Remembers when Hellvile Park was one big meadow. | species=MOUSEKIN
Oma Fluff | s | Bakes cookies for the mailfrogs every Thursday. | species=SHEEPKIN; bff=Hopscotch
Gran Tabby | s | Nine lives. Still on her first. Very careful. | species=CATFOLK
""" ),

A("grandpas", "hill", "Grandpa|Retired Sailor|Clockmaker|Chess Champion",
  sp="PUPKIN:2 OWLKIN:2 BEARKIN:2 HUMAN FOXKIN",
  traits="ELDER", body="ROUND:2 HUNCHED:3 AVERAGE STOCKY", height="3-9",
  top="SWEATER:3 SWEATER_VEST:3 SUIT", bottom="PANTS:4 SHORTS", shoes="SLIPPERS:2 CLOGS BOOTS",
  hat="NONE:3 BERET CAP BUCKET", acc="GLASSES:4 MONOCLE BOWTIE SCARF",
  fhair="HANDLEBAR:3 FULL_BEARD:3 MUSTACHE:3 CHOPS:2 NONE",
  pat="PLAID:3 SOLID:2 STRIPES", scheme="CHOCOLATE/HONEY/WHITE NAVY/CARAMEL/WHITE FOREST/BONE/HONEY TEAL/SILVER/WHITE",
  pers="0-6 1-8 4-12 7-15 3-11", act="HOME:3 PARK:2", eyes="SLEEPY:3 HAPPY_ARC:2 DROOPY:2",
  hair_col="WHITE:4 SILVER:4", hair="BALD:2 SHORT:2 MESSY", hair_force="1",
  food="Coffee|Onigiri|Pudding|Curry|Pickles|Milk", hobby="Fishing:3|Board Games:3|Napping|Birdwatching",
  music="Jazz:2|Sea Shanties:2|Classical|Folk", phrase="Oh my stars!|Bless your heart!|Okie dokie!|Big mood.",
  cast="""
Gramps Moss | h | Retired sailor. Every story ends with a whale. | job=Retired Sailor; species=PUPKIN
Tock | h | Fixes every clock in town, then naps through the chimes. | species=HUMAN; job=Clockmaker
Pops Wiggle | h | Undefeated at checkers. Lets kids win anyway. | job=Chess Champion; species=BEARKIN; fam=wiggles
Grampy Bean | h | Grows the town's tomatoes and the town's beard. | species=HUMAN; skin=SKIN_UMBER; facial_hair=FULL_BEARD
Old Hazel | t | Has whittled 300 tiny ducks. Names them all. | job=Duck Whittler; species=FOXKIN
""" ),

A("nurses", "hill", "Nurse|Head Nurse|Night Nurse",
  sp="BUNNYKIN:3 SHEEPKIN:2 PUPKIN CATFOLK",
  top="NURSE:6 SWEATER", bottom="SKIRT:2 PANTS:2 NONE", shoes="SNEAKERS:3 CLOGS:2",
  hat="NONE:5 BANDANA", acc="NONE:3 CROSS_NECKLACE GLASSES",
  pat="SOLID:5 HEARTS", scheme="WHITE/ROSE/SKY MINT/WHITE/ROSE SKY/WHITE/BUBBLEGUM BUBBLEGUM/WHITE/MINT",
  pers="6-12 0-4 6-12 7-14 0-5", act="HOSPITAL:5",
  food="Salad|Milk|Pudding|Bubble Tea|Onigiri", hobby="Yoga|Knitting|Reading|Singing",
  music="Lullabies|City Pop|Classical", phrase="Bless your heart!|Hug o'clock!|Snuggle up!",
  fear="Spiders|The Dark",
  cast="""
Nurse Fluff | s | Prescribes naps. They are very effective. | job=Head Nurse; species=BUNNYKIN
Boo-Boo Bea | t | Has a sticker for every boo-boo. Even emotional ones. | species=PLUSHIE
Patch Plum | s | Takes temperatures with a gentle paw on the brow. | species=PUPKIN
Midnight | h | Night nurse. Hums to the sleepless. | job=Night Nurse; species=CATFOLK; trait=NIGHT_OWL
Tender Tuft | s | Has never been afraid of anything except needles. | species=BUNNYKIN; fear=Needles
""" ),

A("hospital", "hill", "Doctor|Dentist|Pharmacist|Surgeon",
  sp="OWLKIN:2 FOXKIN MOUSEKIN HUMAN GHOUL:2",
  top="LAB_COAT:6 SWEATER", bottom="PANTS:4 SKIRT", shoes="SNEAKERS:2 CLOGS",
  hat="NONE:5", acc="GLASSES:4 MASK:2 NONE",
  pat="SOLID:6", scheme="WHITE/TEAL/MINT WHITE/SKY/NAVY WHITE/MINT/ROSE",
  pers="5-11 2-7 3-10 4-11 1-7", act="HOSPITAL:5",
  food="Salad|Coffee:2|Sushi|Bubble Tea", hobby="Reading|Board Games|Yoga|Gardening",
  music="Classical:3|Jazz", phrase="Okie dokie!|Bless your heart!|Oh my stars!",
  cast="""
Dr. Bloom | s | Ghoul doctor. Calm, gentle and fond of cold hands. | id=111; story=DR_BLOOM; job=Doctor; species=GHOUL; skin=MINT; body=LANKY; height=11; build=5; head=OVAL; eyes=SLEEPY; eye_color=ROSE; brows=THIN; nose=DOT; mouth=SMILE; fangs=1; hair=BOB; hair_color=ROSE; highlight=WHITE; ears=ELF; marking=STITCHES; marking_color=ROSE; cheeks=BLUSH; top=LAB_COAT; top_color=WHITE; pattern=SOLID; bottom=LONG_SKIRT; bottom_color=TEAL; shoes=CLOGS; shoes_color=MINT; hat=NONE; accessory=GLASSES; accessory_color=TEAL; aura=NONE; birthday=OCT-28; food=Bone Broth; hates=Hell Peppers; hobby=Gardening; music=Classical; color=ROSE; fear=Sunlight; phrase=Bless your heart!; movement=5; speech=3; expression=6; attitude=10; quirk=6; activity=HOSPITAL; corruption=12; romance=ANY
Dr. Molar | h | Dentist. Hands out toothbrushes as party favours. | job=Dentist; species=MOUSEKIN
Pillsby | t | Pharmacist. Labels every bottle with a smiley face. | species=ROBOT; job=Pharmacist
Dr. Fennec | s | Surgeon with steady paws and terrible puns. | job=Surgeon; species=FOXKIN; fur=HONEY/WHITE
Dr. Hollow | h | Ghoul physician. Faints at the sight of blood. | species=GHOUL; skin=ECTOPLASM
""" ),

A("choir", "hill", "Choir Singer|Choir Director|Organist",
  sp="CHERUB:5 SHEEPKIN OWLKIN",
  traits="HOLY", top="ROBE:5 PRIEST", bottom="NONE:3 ROBE:2", shoes="SANDALS:3 BARE",
  hat="NONE:5 FLOWER_CROWN", acc="NONE:3 BOWTIE ROSARY",
  pat="SOLID:6", scheme="WHITE/LEMON/SKY WHITE/SKY/LEMON LEMON/WHITE/ROSE",
  pers="4-10 0-3 9-15 9-15 1-6", act="CHURCH:5", aura="NONE:3 SPARKLES:2",
  food="Angel Cake:4|Milk|Pudding|Macarons", hobby="Singing:6|Cloud Watching", music="Hymns:6|Classical:2",
  phrase="Bless your heart!|Sparkle on!|Oh my stars!|Hug o'clock!",
  cast="""
Seraphina | s | Hits notes so high only dogs and angels hear them. | species=CHERUB; job=Choir Director
Hallelujah | h | Sings even when asked not to. Especially then. | species=CHERUB
Bellwether | s | Rings the church bells with her whole body. | job=Bell Ringer; species=SHEEPKIN
Cantor Coo | t | Organist. Plays hymns at the speed of disco. | job=Organist; species=OWLKIN; music=Disco
Angelo | h | The tiniest cherub with the biggest voice. | body=TINY; height=1
""" ),

A("church", "hill", "Priest|Nun|Altar Server|Exorcist",
  sp="PLUSHIE:2 SHEEPKIN BUNNYKIN HUMAN CHERUB",
  traits="HOLY", top="PRIEST:5 ROBE:2", bottom="NONE:3 ROBE:2 LONG_SKIRT", shoes="SANDALS:3 CLOGS",
  hat="NUN_VEIL:3 NONE:3", acc="ROSARY:4 CROSS_NECKLACE:4",
  pat="SOLID:6", scheme="JET/WHITE/LEMON WHITE/JET/LEMON NAVY/WHITE/LEMON",
  pers="2-8 0-4 4-11 6-13 0-6", act="CHURCH:5",
  food="Angel Cake|Bubble Tea|Pudding|Onigiri", hobby="Singing|Reading|Gardening|Plush Making",
  music="Hymns:5|Classical|Folk", phrase="Bless your heart!|Hug o'clock!|Cuddle puddle!",
  fear="The Dark|Ghosts|Spiders",
  cast="""
Father Fuzz | h | Plushie priest of St. Cuddles. Banishes demons with hugs. | id=77; story=FATHER_FUZZ; job=Priest; species=PLUSHIE; skin=CARAMEL; ears=BEAR; tail=BEAR_NUB; body=ROUND; height=6; build=13; head=ROUND; head_size=13; eyes=BUTTON; eye_color=JET; brows=THICK; nose=BUTTON; mouth=STITCHED; hair=BALD; hair_color=CARAMEL; highlight=NONE; facial_hair=NONE; marking=PATCHES; marking_color=PEACH; cheeks=BLUSH; top=PRIEST; top_color=JET; pattern=SOLID; pattern_color=WHITE; bottom=ROBE; bottom_color=JET; shoes=SANDALS; shoes_color=CHOCOLATE; hat=NONE; accessory=ROSARY; accessory_color=LEMON; aura=SPARKLES; halo=NONE; birthday=DEC-25; food=Angel Cake; hates=Sulfur Soda; hobby=Plush Making; music=Hymns; color=LEMON; fear=Exorcisms; phrase=Hug o'clock!; movement=4; speech=2; expression=9; attitude=11; quirk=5; romance=NONE; activity=CHURCH; corruption=0
Sr. Snugs | s | Nun with a black belt in tickling. | job=Nun; species=SHEEPKIN; hat=NUN_VEIL
Sr. Mercy | s | Blesses the soup, the spoons and the soup again. | job=Nun; species=BUNNYKIN; hat=NUN_VEIL
Brother Bun | h | Lights 100 candles before breakfast. | species=HUMAN; job=Altar Server
Wafer | t | Altar server. Has dropped the holy water only twice. | job=Altar Server; species=CHERUB; trait=KID
Sr. Lumen | s | Studies old exorcism scrolls. Hums while reading. | job=Exorcist; species=HUMAN; skin=SKIN_OLIVE; hat=NUN_VEIL
""" ),

A("firefighters", "hill", "Firefighter|Fire Chief|Rescue Swimmer",
  sp="PUPKIN:3 BEARKIN:3 FOXKIN",
  top="OVERALLS:3 TSHIRT:2 TANK", bottom="CARGO:3 PANTS", shoes="BOOTS:5",
  hat="MARINE_HELMET:2 CAP:2 NONE:2", acc="NONE:3 SCARF",
  pat="STRIPES:2 SOLID:3", scheme="CHERRY/LEMON/WHITE TANGERINE/LEMON/CHARCOAL CHERRY/CHARCOAL/LEMON",
  pers="11-15 2-8 8-14 8-15 2-8", act="PARK:2 HOME:2", body="BUFF:3 STOCKY:3 AVERAGE", build="9-14", height="7-13",
  food="Fried Chicken|Pizza|Curry|Tacos|Pancakes", hobby="Board Games|Fishing|Yoga|Karaoke",
  music="Disco|City Pop|Folk", phrase="Paws up!|Tail wags!|Fluff yeah!|Okie dokie!", fear="Heights|Deep Water|Spiders",
  pet="30",
  cast="""
Chief Spark | h | Fire chief. Rescues cats, kites and feelings. | job=Fire Chief; species=PUPKIN
Hosey | s | Waters every flowerbed on the way back from a fire. | species=ROBOT
Ladder Lou | t | Climbs things just to see if anyone needs help. | species=FOXKIN
Sooty Bun | h | Gives fire safety talks with puppets. | species=BEARKIN
Splash | s | Rescue swimmer. Does backstrokes in the fountain. | job=Rescue Swimmer; species=PUPKIN
""" ),

A("rangers", "hill", "Park Ranger|Trail Guide|Owl Spotter",
  sp="FOXKIN:3 RACCOONKIN:2 BEARKIN OWLKIN",
  top="SWEATER_VEST:2 TSHIRT:2 OVERALLS", bottom="CARGO:4 SHORTS:2", shoes="BOOTS:5",
  hat="COWBOY:2 CAP:2 BUCKET:2", acc="BACKPACK:3 NONE:2",
  pat="SOLID:3 CAMO:2 PLAID", scheme="FOREST/CARAMEL/RUST FOREST/CARAMEL/HONEY CHARTREUSE/CHOCOLATE/WHITE TEAL/HONEY/BONE",
  pers="6-12 2-8 4-11 8-15 1-8", act="PARK:5",
  food="Onigiri|Honey Toast|Salad|Fried Chicken", hobby="Birdwatching:3|Stargazing|Bug Collecting|Fishing",
  music="Folk:4|Sea Shanties", phrase="Tail wags!|Hoo hoo!|Sunny side up!",
  cast="""
Rusty | h | Knows every tree in the park by its bark. | species=FOXKIN; fur=RUST/BONE; bio_pronoun_ok=1
Trailblazer | s | Has hiked every path in HaHaland twice. | job=Trail Guide; species=RACCOONKIN
Acorn Annie | s | Leaves acorns on doorsteps as a polite hello. | species=BEARKIN
Hooty Mcgee | h | Owl spotter. Mostly spots himself in puddles. | job=Owl Spotter; species=OWLKIN
Compass | t | Always knows which way is north. And snacks. | job=Trail Guide; species=FOXKIN
""" ),

A("lifeguards", "hill", "Lifeguard|Surf Instructor|Sandcastle Judge",
  sp="PUPKIN:2 FOXKIN:2 CATFOLK HUMAN FROGLING",
  top="TANK:4 CROP_TOP:2 TSHIRT", bottom="SHORTS:5", shoes="SANDALS:3 BARE:3",
  hat="NONE:3 CAP BUCKET", acc="SUNGLASSES:4 NONE:2",
  pat="SOLID:3 STRIPES:2 HEARTS", scheme="CHERRY/WHITE/SKY TANGERINE/SKY/LEMON CYAN/LEMON/WHITE TEAL/TANGERINE/WHITE",
  pers="9-15 3-9 8-14 10-15 3-10", act="BEACH:5",
  food="Ice Cream|Tacos|Sushi|Bubble Tea", hobby="Yoga|Skateboarding|Fishing|Dancing",
  music="City Pop:3|Disco|Synthwave", phrase="Sunny side up!|Wheee!|Fluff yeah!|Slay!", fear="Bees|Thunder",
  body="BUFF:2 AVERAGE:2 LANKY",
  cast="""
Splashley | s | Lifeguard. Has never blinked on duty. | species=PUPKIN
Tidepool | h | Surf instructor who is honestly scared of fish. | job=Surf Instructor; species=FOXKIN; fear=Deep Water
Sandy Paws | t | Judges sandcastles. Gives every one a ribbon. | job=Sandcastle Judge; species=CATFOLK
Coral Reef | s | Can whistle in three octaves underwater. | species=FROGLING
Sunny Dunes | h | Applies sunscreen to strangers, very politely. | species=HUMAN; skin=SKIN_BROWN
""" ),

A("fishers", "hill", "Fisher|Fishmonger|Bait Seller",
  sp="CATFOLK:4 RACCOONKIN BEARKIN",
  top="OVERALLS:3 SWEATER:2 TSHIRT", bottom="PANTS:2 SHORTS NONE", shoes="BOOTS:5",
  hat="BUCKET:4 BEANIE:2", acc="NONE:4",
  pat="SOLID:3 STRIPES:2", scheme="NAVY/LEMON/WHITE TEAL/HONEY/WHITE SKY/NAVY/TANGERINE",
  pers="1-7 2-9 3-10 9-15 2-9", act="BEACH:4 SHOP",
  food="Sushi:4|Onigiri|Takoyaki", hobby="Fishing:6|Napping", music="Sea Shanties:5|Folk",
  phrase="Purr-fect!|Nyaa~|Big mood.", fear="Deep Water|Thunder|Vacuum Cleaners",
  cast="""
Capt. Whisk | h | Catches fish, names them, lets them go. | species=CATFOLK; facial_hair=CHOPS
Gilly | s | Sells fish with a song about each one. | job=Fishmonger; species=CATFOLK
Mackerel | h | Once caught a boot. Still wears it. | species=RACCOONKIN
Wormsby | t | Bait seller. Loves the worms too much to sell them. | job=Bait Seller; species=BEARKIN
""" ),

A("idols", "hill", "Pop Idol|Backup Dancer|Idol Manager",
  sp="CATFOLK:3 BUNNYKIN:3 FOXKIN:2 HUMAN CHERUB",
  top="CROP_TOP:3 DRESS:3 TSHIRT", bottom="TUTU:2 SKIRT:3 SHORTS", shoes="PLATFORM:2 ROLLERSKATES HEELS:2 SNEAKERS",
  hat="NONE:3 TIARA:2 HEADPHONES BUNNY_EARS", acc="EARRINGS:3 NONE BOWTIE",
  pat="HEARTS:3 POLKA:2 STRIPES SOLID", scheme="BUBBLEGUM/CYAN/LEMON MAGENTA/LEMON/WHITE LAVENDER/BUBBLEGUM/SKY CYAN/MAGENTA/WHITE",
  pers="11-15 2-8 13-15 9-15 5-13", act="CONCERT:5", head="STAR:3 ROUND:2 HEART:2 MOCHI",
  eyes="SPARKLE:4 STAR:3 HEART:2 LASHES", aura="SPARKLES:4 HEARTS:3 NONE:2", cheeks="BLUSH:90 SPARKLES:60",
  food="Bubble Tea:3|Macarons|Cotton Candy|Mochi", hobby="Singing:4|Dancing:4|Cosplay", music="Bubblegum Pop:6|City Pop:2|Disco",
  phrase="Sparkle on!|Slay!|Smooch!|Yippee!|Teehee!", fear="Public Speaking|Silence|Popping Balloons",
  cast="""
Starla | s | Pop idol. Waves at her fans and at clouds, equally. | species=HUMAN
Mochi Mochi | s | Her one hit song is just the word "mochi". | species=BUNNYKIN
Glitterbun | t | Backup dancer who keeps accidentally becoming the lead. | job=Backup Dancer; species=BUNNYKIN
Fox Trotter | h | Taught the whole town to dance. Some refused. | job=Backup Dancer; species=FOXKIN
Kawaii Kai | h | Idol manager. Carries snacks for all occasions. | job=Idol Manager; species=HUMAN; skin=SKIN_TAN
Cherie Bomb | s | Sings so sweetly the birds ask for encores. | species=CHERUB; skin=SKIN_UMBER
""" ),

A("orchestra", "hill", "Violinist|Tuba Player|Conductor|Harpist|Drummer",
  sp="OWLKIN:2 MOUSEKIN:2 CATFOLK SHEEPKIN BEARKIN",
  top="SUIT:4 DRESS:2", bottom="PANTS:3 LONG_SKIRT:2", shoes="HEELS:2 BOOTS:2 CLOGS",
  hat="NONE:5 TOP_HAT", acc="BOWTIE:5 MONOCLE NONE",
  pat="SOLID:6", scheme="JET/WHITE/ROSE NAVY/WHITE/LEMON GRAPE/WHITE/LEMON CHARCOAL/ROSE/WHITE",
  pers="3-10 0-4 6-13 4-11 1-8", act="CONCERT:4 HOME",
  food="Macarons|Coffee|Sushi|Angel Cake", hobby="Reading|Poetry|Board Games|Singing",
  music="Classical:6|Jazz:2", phrase="Hoo hoo!|Oh my stars!|Squeak squeak!",
  cast="""
Maestro Mew | h | Conducts with his tail. The orchestra follows. | job=Conductor; species=CATFOLK
Fiddlestick | s | Plays the violin while hopping. Rarely misses. | species=MOUSEKIN
Oompah | h | Tuba player. Every note sounds like a happy cow. | species=HUMAN; job=Tuba Player
Harpsy | s | Plays harp for napping babies. And for herself. | job=Harpist; species=SHEEPKIN
Timpani Tim | h | Drummer. Taps rhythms on everything, even soup. | job=Drummer; species=OWLKIN
Cello Bello | t | Hugs the cello while playing. It looks happy. | job=Cellist; species=GHOST
""" ),

A("lofi", "hill", "Lo-fi Producer|Radio DJ|Sound Engineer",
  sp="RACCOONKIN:3 CATFOLK:2 FOXKIN HUMAN",
  top="HOODIE:5 SWEATER:2", bottom="SWEATS:3 PANTS", shoes="SLIPPERS:3 SNEAKERS:2",
  hat="HEADPHONES:5 BEANIE", acc="NONE:3 GLASSES:2",
  pat="SOLID:4 STRIPES", scheme="LAVENDER/NAVY/PEACH TEAL/ROSE/LEMON NAVY/LAVENDER/PEACH",
  pers="1-6 2-8 2-8 10-15 3-10", act="HOME:4", traits="NIGHT_OWL",
  food="Ramen:3|Bubble Tea:2|Coffee|Popcorn", hobby="Video Games|Napping|Cloud Watching|Painting",
  music="Lo-fi Beats:6|Synthwave|Chiptune", phrase="Cozy vibes only!|Big mood.|Snuggle up!",
  cast="""
Chillwave | h | Makes beats from rain sounds and kettle whistles. | species=RACCOONKIN
DJ Purrple | s | Hosts the late-night radio show "Purrs and Pianos". | job=Radio DJ; species=CATFOLK
Vinyl Vale | t | Owns 2,000 records. Plays the same one daily. | species=HUMAN
Reverb | h | Sound engineer. Can hear a pin drop in Hellvile. | species=ROBOT; job=Sound Engineer
Snoozy | s | Records her own snores and sells them as ASMR. | species=CATFOLK
""" ),

A("painters", "hill", "Painter|Muralist|Sculptor|Illustrator",
  sp="FOXKIN:2 MOTHKIN:2 CATFOLK BUNNYKIN HUMAN",
  top="OVERALLS:3 SWEATER TSHIRT:2 KIMONO", bottom="PANTS:2 SKIRT SHORTS", shoes="SNEAKERS:2 SANDALS BOOTS",
  hat="BERET:5 NONE:2 BANDANA", acc="NONE:3 SCARF GLASSES",
  pat="POLKA:2 SOLID:2 STRIPES PLAID", scheme="TANGERINE/SKY/LEMON TEAL/MAGENTA/LEMON LEMON/COBALT/ROSE PEACH/GRAPE/MINT",
  pers="2-9 1-7 9-15 7-14 6-13", act="PARK:3 HOME:2", cheeks="BLUSH:70 FRECKLES:40",
  food="Macarons|Bubble Tea|Coffee|Melon Bread", hobby="Painting:6|Pottery|Origami|Poetry",
  music="Jazz|Lo-fi Beats|City Pop|Classical", phrase="Sparkle on!|Teehee!|Oopsie daisy!",
  cast="""
Gouache | s | Paints portraits of strangers and gives them away. | species=FOXKIN
Muralina | t | Covered the Wall in murals. Hellvile copies them. | job=Muralist; species=MOTHKIN
Chisel | h | Sculpts the clouds' shapes in marshmallow. | species=HUMAN; job=Sculptor
Doodlebug | s | Illustrates bedtime stories. Every hero is a bug. | job=Illustrator; species=BUNNYKIN
Paintpaw | h | Has paint on every paw. Calls it a style. | species=FOXKIN
Sunset Sam | h | Paints only sunsets. 4,000 so far. | species=HUMAN; skin=SKIN_FAIR
""" ),

A("crafters", "hill", "Plush Maker|Potter|Candle Maker|Weaver",
  sp="PLUSHIE:3 SHEEPKIN:2 MOUSEKIN BEARKIN",
  top="SWEATER:3 OVERALLS:2 DRESS", bottom="SKIRT:2 PANTS:2", shoes="SLIPPERS:3 CLOGS",
  hat="NONE:3 BEANIE BANDANA", acc="NONE:3 TEDDY:2 SCARF",
  pat="PLAID:2 POLKA:2 HEARTS SOLID", scheme="PEACH/MINT/WHITE LAVENDER/LEMON/ROSE CARAMEL/SKY/WHITE ROSE/MINT/LEMON",
  pers="2-7 0-5 6-13 9-15 3-9", act="SHOP:3 HOME:2",
  food="Pudding|Mochi|Honey Toast|Cupcakes", hobby="Plush Making:5|Pottery:3|Knitting:2",
  music="Folk:3|Lullabies|Lo-fi Beats", phrase="So fluffy!|Cuddle puddle!|Snuggle up!",
  cast="""
Stitchwick | h | Sews new plushies from old socks. Nobody knows whose. | job=Plush Maker; species=PLUSHIE
Clayton | h | Makes mugs shaped like his friends' faces. | species=HUMAN; job=Potter
Waxie | s | Candle maker. Her candles smell like hugs. | species=HUMAN; job=Candle Maker
Loomie | t | Weaves rugs from rainbow yarn and old songs. | job=Weaver; species=MOUSEKIN
Fluffernut | s | A plushie who makes plushies. Very meta. | job=Plush Maker; species=PLUSHIE
""" ),

A("tailors", "hill", "Tailor|Fashion Designer|Hat Maker",
  sp="CATFOLK:3 FOXKIN:2 MOUSEKIN HUMAN",
  top="SUIT:3 DRESS:3 KIMONO", bottom="SKIRT:2 PANTS:2 LONG_SKIRT KILT", shoes="HEELS:3 BOOTS PLATFORM",
  hat="NONE:3 BERET TOP_HAT FEZ", acc="MONOCLE:2 GLASSES EARRINGS SCARF",
  pat="PLAID:3 STRIPES:2 POLKA SOLID", scheme="GRAPE/LEMON/WHITE ROSE/CHARCOAL/WHITE COBALT/PEACH/WHITE MAGENTA/MINT/LEMON",
  pers="5-11 3-9 8-15 5-12 4-11", act="SHOP:4",
  food="Macarons|Sushi|Angel Cake|Bubble Tea", hobby="Cosplay:3|Knitting|Painting|Plush Making",
  music="City Pop|Jazz|Disco|Classical", phrase="Slay!|Purr-fect!|Sparkle on!|Rude!",
  cast="""
Mme. Velvet | s | Measures you with one glance. Never wrong. | species=CATFOLK
Hemsley | h | Has sewn 40 suits for a single sock puppet. | species=HUMAN
Couture Kit | t | Fashion designer. Sees a dress in every cloud. | job=Fashion Designer; species=FOXKIN
Millie Moo | s | Makes hats for ears of every shape and size. | job=Hat Maker; species=HUMAN; skin=SKIN_EBONY; hat=FEZ
Tweedle | h | Wears plaid on plaid on plaid. Somehow works. | species=CATFOLK; pattern=PLAID
""" ),

A("groomers", "hill", "Fur Groomer|Hairdresser|Ear Fluffer",
  sp="PUPKIN:3 CATFOLK:2 SHEEPKIN BUNNYKIN",
  top="TSHIRT:2 SWEATER CROP_TOP OVERALLS", bottom="SKIRT:2 PANTS SHORTS", shoes="SNEAKERS:2 CLOGS HEELS",
  hat="NONE:4 BANDANA", acc="NONE:2 EARRINGS:2 SCARF",
  pat="SOLID:2 POLKA HEARTS", scheme="MINT/BUBBLEGUM/WHITE LAVENDER/MINT/LEMON PEACH/SKY/WHITE",
  pers="8-14 3-10 9-15 8-15 3-10", act="SHOP:4",
  food="Bubble Tea|Macarons|Mochi|Cupcakes", hobby="Cosplay|Karaoke|Dancing|Painting",
  music="City Pop|Bubblegum Pop|Disco", phrase="So fluffy!|Fluff yeah!|Slay!|Tail wags!",
  cast="""
Fluffy Fern | s | Can make any tail twice as fluffy. Charges in hugs. | species=PUPKIN
Mane Event | h | Hairdresser. His own hair defies physics. | job=Hairdresser; species=CATFOLK; hair=POMPADOUR
Brushella | t | Ear fluffer. Fluffs ears. Very serious about it. | job=Ear Fluffer; species=BUNNYKIN
Curlicue | s | Turns frizz into art. Loves rainy days. | job=Hairdresser; species=SHEEPKIN
""" ),

A("scientists", "hill", "Scientist|Inventor|Chemist|Robot Doctor",
  sp="MOUSEKIN:3 ROBOT:2 RACCOONKIN OWLKIN HUMAN",
  top="LAB_COAT:5 SWEATER_VEST HAZMAT", bottom="PANTS:3 SKIRT CARGO", shoes="SNEAKERS:3 BOOTS",
  hat="NONE:4 PROPELLER", acc="GLASSES:5 GAS_MASK NONE",
  pat="SOLID:5", scheme="WHITE/LIME/SKY WHITE/CYAN/LEMON WHITE/MAGENTA/MINT",
  pers="5-12 2-9 5-13 4-11 7-14", act="WORKSHOP:2 HOME:2",
  eyes="WIDE:3 SPIRAL ROUND:2 DOT", aura="NONE:5 BUBBLES GLITCH",
  food="Coffee:3|Cereal|Ramen|Popcorn", hobby="Bug Collecting|Stargazing|Board Games|Video Games",
  music="Chiptune:3|Synthwave|Classical", phrase="Oh my stars!|Beep boop!|Yikes.|Squeak squeak!",
  cast="""
Prof. Fizz | h | Invented the self-buttering toast. Twice. | job=Inventor; species=MOUSEKIN; eyes=SPIRAL
Flaskia | s | Chemist. Every experiment ends in bubbles. | job=Chemist; species=MOUSEKIN; aura=BUBBLES
Cogwheel | i | Robot doctor. Fixes robots and pep talks. | job=Robot Doctor; species=ROBOT; wings=MECH
Eureka | t | Shouts "eureka" after every sneeze. | species=HUMAN
Dr. Quark | s | Studies tiny things, like ants and quarks. | species=ROBOT
Gadget Gus | h | Built a robot to hold his umbrella. | species=HUMAN; skin=SKIN_TAN
""" ),

A("astronomers", "hill", "Astronomer|Astronaut|Planetarium Guide",
  sp="OWLKIN:3 ROBOT MOTHKIN HUMAN CATFOLK",
  top="SPACESUIT:3 SWEATER:2 LAB_COAT", bottom="NONE:2 PANTS:2", shoes="BOOTS:3 SNEAKERS",
  hat="NONE:5 BEANIE", acc="NONE:3 GLASSES",
  pat="SOLID:4", scheme="NAVY/LEMON/WHITE WHITE/TANGERINE/SKY COBALT/LEMON/LAVENDER",
  pers="2-8 1-6 5-12 6-13 4-11", act="PARK:2 HOME:3", traits="NIGHT_OWL",
  eyes="STAR:3 SPARKLE:2 ROUND", aura="NONE:4 SPARKLES",
  food="Ice Cream|Cereal|Coffee|Popcorn", hobby="Stargazing:6|Cloud Watching|Reading",
  music="Synthwave:3|Classical:2|Lo-fi Beats", phrase="Oh my stars!|Hoo hoo!|Wheee!",
  cast="""
Cosmo Hoot | h | Named 61 stars after his friends. And one after soup. | species=OWLKIN
Nebula Nell | s | Astronaut. Went to the moon. Says it was very quiet. | job=Astronaut; species=HUMAN; skin=SKIN_BROWN; top=SPACESUIT
Orbit | i | Planetarium guide that projects stars from its eyes. | job=Planetarium Guide; species=ROBOT
Lunette | t | Sleeps all day, counts meteors all night. | species=MOTHKIN
Comet Tail | s | Wishes on shooting stars for other people. | species=CATFOLK
""" ),

A("builders", "hill", "Carpenter|Builder|Bridge Engineer|Roofer",
  sp="BEARKIN:3 RACCOONKIN:2 PUPKIN HUMAN",
  top="OVERALLS:4 TANK:2 TSHIRT", bottom="CARGO:3 PANTS NONE", shoes="BOOTS:5",
  hat="CAP:2 BANDANA:2 TRAFFIC_CONE NONE", acc="NONE:4",
  pat="SOLID:3 PLAID:2", scheme="TANGERINE/NAVY/LEMON LEMON/CHOCOLATE/WHITE RUST/SKY/LEMON",
  pers="5-11 3-10 4-11 8-15 2-8", act="WANDERING:3 WORKSHOP", body="STOCKY:3 BUFF:2 ROUND",
  food="Fried Chicken|Pizza|Tacos|Curry|Onigiri", hobby="Welding|Board Games|Fishing|Napping",
  music="Folk|Sea Shanties|Disco|City Pop", phrase="Okie dokie!|Paws up!|Big mood.",
  cast="""
Hammerbelle | s | Built half the houses in Hillvile. And all the birdhouses. | species=BEARKIN
Plank | h | Carpenter. Sands every edge until it is huggable. | species=BEARKIN; fam=plank
Rafters | t | Roofer. Has seen every chimney's secret. | species=HUMAN; job=Roofer
Archie Beam | h | Bridge engineer. Built a bridge for a snail. | species=HUMAN; job=Bridge Engineer
Shingle | s | Hums while hammering. In perfect time. | species=BEARKIN; fam=plank
""" ),

A("workshop", "hill", "Welder|Welding Apprentice|Gunsmith|Blacksmith",
  sp="RACCOONKIN:2 BEARKIN ROBOT HUMAN PUPKIN",
  top="OVERALLS:3 TANK:2 LEATHER_JACKET", bottom="CARGO:3 PANTS", shoes="BOOTS:3 COMBAT",
  hat="BANDANA:3 CAP NONE", acc="NONE:2 GAS_MASK GLASSES",
  pat="SOLID:4 CAMO", scheme="CHARCOAL/TANGERINE/LEMON NAVY/HELLFIRE/SILVER RUST/CHARCOAL/LEMON",
  pers="6-12 4-11 5-12 6-13 4-11", act="WORKSHOP:5",
  food="Fried Chicken|Coffee|Pizza|Hell Peppers", hobby="Welding:6|Video Games", music="Doom Metal|Industrial|Synthwave|Chiptune",
  phrase="Fluff yeah!|Okie dokie!|Big mood.|Rip and tear... gently.", romance="ANY",
  cast="""
Rivet | s | Imp welder who fled Hellvile. Hugs like a vice. | id=13; story=RIVET; fam=rivetfam; job=Head Welder; species=IMP; skin=TANGERINE; flag=HELLBORN; trait=SOFTIE; body=STOCKY; height=5; build=10; head=ROUND; head_size=12; eyes=ROUND; eye_color=LEMON; eye_size=11; brows=THICK; nose=DOT; mouth=GRIN; fangs=1; hair=SIDE_SHAVE; hair_color=JET; highlight=HELLFIRE; horns=NUBS; horn_color=CHARCOAL; horn_size=6; tail=DEVIL; wings=NONE; halo=NONE; marking=NONE; marking_color=CHARCOAL; cheeks=BLUSH,BANDAGE,FRECKLES; top=OVERALLS; top_color=NAVY; pattern=SOLID; pattern_color=TANGERINE; bottom=NONE; bottom_color=NAVY; shoes=COMBAT; shoes_color=CHARCOAL; hat=BANDANA; hat_color=TANGERINE; accessory=GAS_MASK; accessory_color=CHARCOAL; aura=NONE; birthday=JUL-04; food=Hell Peppers; hates=Salad; hobby=Welding; music=Doom Metal; color=TANGERINE; fear=Being Alone; phrase=Rip and tear... gently.; movement=11; speech=11; expression=10; attitude=9; quirk=10; romance=ANY; activity=WORKSHOP; corruption=22
Sparkplug | h | Rivet's apprentice. Has singed eyebrows, permanently. | job=Welding Apprentice; species=RACCOONKIN; bff=Rivet
Anvilla | s | Blacksmith. Forges bells, swords and cookie cutters. | species=HUMAN; job=Blacksmith
Gunther | i | Gunsmith bot. Polishes every barrel till it sings. | job=Gunsmith; species=ROBOT; wings=MECH
Torchy | t | Welds tiny sculptures from bottle caps. | job=Welding Apprentice; species=PUPKIN
""" ),

A("shopkeepers", "hill", "Shopkeeper|Cashier|Stock Clerk|Grocer",
  sp="MOUSEKIN:2 CATFOLK RACCOONKIN BUNNYKIN PUPKIN HUMAN",
  top="SWEATER_VEST:2 TSHIRT:2 OVERALLS SWEATER", bottom="PANTS:3 SKIRT:2", shoes="SNEAKERS:3 CLOGS",
  hat="CAP:2 NONE:3", acc="NONE:3 GLASSES BOWTIE",
  pat="STRIPES:2 SOLID:3 PLAID", scheme="MINT/TEAL/WHITE LEMON/FOREST/WHITE SKY/CHERRY/WHITE ROSE/NAVY/LEMON",
  pers="5-12 1-7 6-13 7-14 2-9", act="SHOP:5",
  food="Onigiri|Cereal|Pickles|Popcorn|Melon Bread", hobby="Board Games|Origami|Reading|Karaoke",
  music="City Pop:2|Bubblegum Pop|Jazz", phrase="Okie dokie!|Yummy yummy!|Squeak squeak!",
  cast="""
Twopence | h | Runs the general store. Gives change in candy. | job=Shopkeeper; species=MOUSEKIN
Scanna | s | Cashier who beeps along with the scanner. | species=ROBOT; job=Cashier
Shelfie | t | Stacks cans into towers that never fall. | species=ROBOT; job=Stock Clerk
Gerty Grape | s | Knows exactly when every melon is ripe. | species=HUMAN; job=Grocer
Bargain Bun | h | Puts everything on sale, all the time. | species=BUNNYKIN
Coupon Kay | s | Has coupons for things that do not exist yet. | species=HUMAN; skin=SKIN_OLIVE
""" ),

A("toyshop", "hill", "Toymaker|Toy Tester|Kite Maker",
  sp="PLUSHIE:2 MOUSEKIN:2 FOXKIN BEARKIN ROBOT",
  top="OVERALLS:3 SWEATER:2 SWEATER_VEST", bottom="PANTS:2 SHORTS", shoes="SLIPPERS:2 CLOGS",
  hat="NONE:3 PROPELLER:2 BERET", acc="GLASSES:2 NONE:2 TEDDY",
  pat="STRIPES:2 POLKA:2 SOLID", scheme="CHERRY/LEMON/SKY SKY/TANGERINE/WHITE GRAPE/LIME/LEMON",
  pers="6-12 1-7 9-15 9-15 6-13", act="SHOP:4",
  food="Gumdrops|Cotton Candy|Popcorn|Pancakes", hobby="Plush Making|Origami|Board Games|Video Games",
  music="Chiptune|Bubblegum Pop|Folk", phrase="Wheee!|Toot toot!|Wiggle wiggle!",
  cast="""
Tinker Toby | h | Toymaker. Every toy he makes winks at you. | species=HUMAN
Windup Wren | s | Tests toys by playing with them for a very long time. | job=Toy Tester; species=PLUSHIE
Kitey | s | Kite maker. Her kites have names and opinions. | job=Kite Maker; species=FOXKIN
Yoyolanda | s | Can yo-yo with both paws while juggling. | species=BEARKIN
""" ),

A("bookshop", "hill", "Bookseller|Poet|Novelist|Calligrapher",
  sp="OWLKIN:2 MOTHKIN:2 FOXKIN MOUSEKIN HUMAN",
  top="SWEATER:3 SWEATER_VEST:2 KIMONO", bottom="PANTS:2 LONG_SKIRT:2", shoes="CLOGS:2 SLIPPERS BOOTS",
  hat="NONE:4 BERET", acc="GLASSES:4 SCARF:2",
  pat="PLAID:2 SOLID:3", scheme="FOREST/BONE/HONEY GRAPE/PEACH/WHITE CHOCOLATE/SKY/BONE",
  pers="0-6 0-6 3-10 4-12 3-10", act="SHOP:3 HOME:2",
  food="Coffee|Honey Toast|Pudding|Melon Bread", hobby="Reading:4|Poetry:4", music="Classical|Jazz|Lo-fi Beats|Folk",
  phrase="Hoo hoo!|Cozy vibes only!|Big mood.",
  cast="""
Quillby | h | Bookseller. Recommends books by smelling you. | species=OWLKIN
Sonnet | s | Writes a poem for every rainy day. 900 so far. | job=Poet; species=MOTHKIN
Plot Twist | t | Novelist. Every chapter ends with a cliffhanger. | species=HUMAN; job=Novelist
Inkblot | h | Calligrapher. Signs his name in 40 fonts. | species=GHOST; job=Calligrapher
Verse Vera | s | Reads poetry to the ducks at sunrise. | job=Poet; species=HUMAN; skin=SKIN_PORCELAIN
""" ),

A("teahouse", "hill", "Tea Master|Tea Sommelier|Dumpling Chef",
  sp="CATFOLK:2 FOXKIN:2 RACCOONKIN SHEEPKIN",
  top="KIMONO:5 SWEATER", bottom="NONE:2 LONG_SKIRT PANTS", shoes="SANDALS:3 SLIPPERS",
  hat="NONE:5 FLOWER_CROWN", acc="NONE:3 SCARF",
  pat="SOLID:3 STRIPES HEARTS", scheme="TEAL/PEACH/WHITE ROSE/FOREST/WHITE LAVENDER/LEMON/MINT CHERRY/WHITE/LEMON",
  pers="0-6 0-3 3-10 8-15 1-6", act="CAFE:4",
  food="Mochi:3|Dumplings:2|Onigiri|Melon Bread", hobby="Origami|Gardening|Poetry|Yoga",
  music="Classical|Lo-fi Beats|Folk", phrase="Cozy vibes only!|Mochi mochi!|Purr-fect!",
  cast="""
Matcha Mimi | s | Whisks tea so gently the foam forms hearts. | job=Tea Master; species=CATFOLK
Oolong | h | Can name any tea by the sound it makes poured. | species=HUMAN; job=Tea Sommelier
Bao Bao | t | Dumpling chef. Pinches 12 pleats, always. | job=Dumpling Chef; species=RACCOONKIN
Sencha Sue | s | Serves tea to sad people for free. | species=SHEEPKIN
Earl Grey | h | Sips tea with his pinky out and his tail up. | job=Tea Sommelier; species=CATFOLK; fur=SILVER/WHITE
""" ),

A("noodles", "hill", "Ramen Chef|Noodle Puller|Takoyaki Cook",
  sp="PUPKIN:3 BEARKIN FOXKIN HUMAN",
  top="TSHIRT:2 TANK:2 KIMONO", bottom="PANTS:2 SHORTS", shoes="SANDALS:2 CLOGS:2",
  hat="BANDANA:4 CHEF", acc="NONE:4",
  pat="SOLID:4 STRIPES", scheme="CHERRY/WHITE/CHOCOLATE NAVY/WHITE/TANGERINE CHOCOLATE/CHERRY/WHITE",
  pers="7-13 3-10 7-14 8-15 3-10", act="CAFE:4",
  food="Ramen:4|Takoyaki:2|Dumplings|Curry", hobby="Karaoke|Board Games|Fishing", music="City Pop:3|Jazz|Folk",
  phrase="Nom nom nom!|Yummy yummy!|Tail wags!",
  cast="""
Udon Ollie | h | Pulls noodles so long they reach the next street. | species=HUMAN; job=Noodle Puller
Ramenda | s | Her broth recipe is a secret even from her. | species=PUPKIN
Taco Yaki | t | Flips takoyaki with a toothpick and a flourish. | species=HUMAN; job=Takoyaki Cook
Slurpington | h | Rates noodles by slurp volume. | species=BEARKIN
""" ),

A("icecream", "hill", "Ice Cream Seller|Sundae Artist|Popsicle Maker",
  sp="SHEEPKIN:2 BUNNYKIN:2 PUPKIN MOUSEKIN",
  top="TSHIRT:2 DRESS OVERALLS", bottom="SHORTS:2 SKIRT:2", shoes="SNEAKERS:2 ROLLERSKATES SANDALS",
  hat="CAP:2 PARTY NONE:2", acc="BOWTIE:2 NONE:2",
  pat="STRIPES:4 POLKA SOLID", scheme="MINT/BUBBLEGUM/WHITE LEMON/SKY/BUBBLEGUM PEACH/MINT/CHOCOLATE",
  pers="9-15 1-6 10-15 10-15 4-11", act="BEACH:2 PARK:2",
  food="Ice Cream:5|Cotton Candy|Waffles", hobby="Bubble Blowing|Dancing|Cloud Watching", music="Bubblegum Pop:3|Disco",
  phrase="Yummy yummy!|Wheee!|Sweet as pie!",
  cast="""
Scoops | h | Drives the ice cream truck. Plays the jingle himself. | species=SHEEPKIN
Sprinkla | s | Makes sundaes taller than she is. | job=Sundae Artist; species=BUNNYKIN
Brainfreeze | t | Popsicle maker. Freezes anything. Asks first. | species=SLIME; job=Popsicle Maker
Neapolitan | s | Has three favourite flavours. Cannot choose. | species=PUPKIN
""" ),

A("boba", "hill", "Boba Barista|Smoothie Mixer",
  sp="CATFOLK BUNNYKIN FOXKIN MOUSEKIN",
  top="CROP_TOP:2 TSHIRT HOODIE", bottom="SKIRT SHORTS:2", shoes="SNEAKERS:2 PLATFORM",
  hat="NONE:3 BUCKET", acc="NONE:2 EARRINGS",
  pat="POLKA:2 SOLID", scheme="LAVENDER/PEACH/WHITE CARAMEL/MINT/WHITE TEAL/BUBBLEGUM/LEMON",
  pers="9-15 2-7 9-15 10-15 4-11", act="CAFE:4",
  food="Bubble Tea:5|Mochi|Macarons", hobby="Karaoke|Dancing|Video Games", music="City Pop:2|Bubblegum Pop|Lo-fi Beats",
  phrase="Blep!|Teehee!|Mochi mochi!",
  cast="""
Tapioca | s | Counts the pearls in every cup. Exactly 47. | species=CATFOLK
Taro Tim | h | Makes purple drinks only. Purple is a lifestyle. | species=BUNNYKIN
Slushie | t | Smoothie mixer who blends while dancing. | species=SLIME; job=Smoothie Mixer
Lychee Lu | s | Draws cat faces on every lid. | species=MOUSEKIN
""" ),

A("wellness", "hill", "Yoga Teacher|Nap Coach|Massage Therapist",
  sp="CATFOLK:2 SHEEPKIN:2 BEARKIN FROGLING",
  top="TANK:3 CROP_TOP:2 PAJAMAS", bottom="LEGGINGS:4 SWEATS", shoes="BARE:4 SANDALS",
  hat="NONE:5", acc="NONE:4 SCARF",
  pat="SOLID:4 STRIPES", scheme="MINT/LAVENDER/WHITE PEACH/TEAL/WHITE LAVENDER/SKY/WHITE",
  pers="0-5 0-3 3-9 11-15 1-6", act="PARK:3 HOME",
  food="Salad:3|Bubble Tea|Mochi|Milk", hobby="Yoga:5|Napping:2|Cloud Watching", music="Lo-fi Beats:3|Classical|Lullabies",
  phrase="Cozy vibes only!|Snuggle up!|Purr-fect!",
  cast="""
Namaste Nya | s | Yoga teacher. Her downward cat is famous. | species=CATFOLK
Dozy Doe | t | Nap coach. Has trained 200 citizens to nap. | job=Nap Coach; species=SHEEPKIN; eyes=SLEEPY
Kneady Paws | h | Massage therapist. His paw pads are legendary. | job=Massage Therapist; species=BEARKIN
Zen Lily | s | Meditates on a lily pad. Has not sunk yet. | species=FROGLING
""" ),

A("skaters", "hill", "Roller Derby Star|Skate Coach|Skater",
  sp="FOXKIN:2 CATFOLK:2 RACCOONKIN BUNNYKIN HUMAN",
  top="TSHIRT:2 HOODIE:2 CROP_TOP TANK", bottom="SHORTS:3 LEGGINGS RIPPED_JEANS", shoes="ROLLERSKATES:5 SNEAKERS:2",
  hat="CAP:3 NONE:2 BEANIE", acc="NONE:3 BACKPACK",
  pat="STRIPES:2 FLAMES SOLID:2 HEARTS", scheme="CYAN/MAGENTA/LEMON TANGERINE/COBALT/WHITE LIME/GRAPE/WHITE",
  pers="12-15 4-11 9-15 8-15 7-14", act="PARK:5",
  food="Pizza|Tacos|Ice Cream|Popcorn", hobby="Skateboarding:6|Video Games", music="Hardcore Punk|Disco|Synthwave|City Pop",
  phrase="Wheee!|Slay!|Fluff yeah!|Big mood.", cheeks="BLUSH:60 BANDAGE:40 FRECKLES:20",
  cast="""
Rolla Rita | s | Roller derby star. Bumps you, then says sorry. | species=HUMAN
Kickflip | h | Can kickflip over a sleeping cat. Has not tried. | job=Skater; species=CATFOLK
Grindella | t | Skate coach. Teaches falling before skating. | job=Skate Coach; species=RACCOONKIN
Wheelie | h | Skates to work, to dinner and in his dreams. | species=ROBOT; job=Skater
Derby Dot | s | Has more bandages than stickers. Proud of both. | species=HUMAN; skin=SKIN_BROWN
""" ),

A("ballet", "hill", "Ballet Dancer|Dance Teacher|Tap Dancer",
  sp="BUNNYKIN:2 CATFOLK MOUSEKIN SHEEPKIN CHERUB",
  top="DRESS:3 CROP_TOP TANK", bottom="TUTU:5 LEGGINGS", shoes="SLIPPERS:3 HEELS BARE",
  hat="NONE:4 TIARA FLOWER_CROWN", acc="NONE:3 EARRINGS",
  pat="SOLID:3 HEARTS POLKA", scheme="BUBBLEGUM/WHITE/LAVENDER LAVENDER/WHITE/PEACH PEACH/WHITE/LEMON",
  pers="6-12 0-4 9-15 6-13 1-7", act="CONCERT:3 PARK",
  food="Macarons|Angel Cake|Salad|Bubble Tea", hobby="Dancing:6|Yoga", music="Classical:4|Lullabies",
  phrase="Teehee!|Sparkle on!|Oh my stars!",
  cast="""
Pirouette | s | Spins so fast she once drilled into the floor. | species=BUNNYKIN
Mr. Plie | h | Dance teacher. Counts to eight in his sleep. | species=HUMAN; job=Dance Teacher
Tippy Toes | t | Walks on tiptoes everywhere. Even upstairs. | species=MOUSEKIN
Arabesque | s | Balances on one paw while reading. | species=SHEEPKIN
Clickety | h | Tap dancer. His shoes are louder than thunder. | job=Tap Dancer; species=CHERUB; shoes=HEELS
""" ),

A("farmers", "hill", "Farmer|Beekeeper|Dairy Farmer|Orchard Keeper",
  sp="BEARKIN:3 SHEEPKIN:2 PUPKIN HUMAN",
  top="OVERALLS:5 TSHIRT", bottom="NONE:2 PANTS SHORTS", shoes="BOOTS:5",
  hat="COWBOY:2 BUCKET:2 CAP NONE", acc="NONE:4",
  pat="PLAID:3 SOLID:2", scheme="HONEY/CHOCOLATE/WHITE SKY/CHOCOLATE/LEMON CHERRY/HONEY/WHITE FOREST/LEMON/WHITE",
  pers="3-9 2-8 4-11 9-15 1-7", act="PARK:3 SHOP",
  food="Honey Toast:3|Milk:2|Carrot Cake|Salad|Pancakes", hobby="Gardening:4|Birdwatching|Fishing", music="Folk:5|Sea Shanties",
  phrase="Sunny side up!|Okie dokie!|Tail wags!", pet="40",
  cast="""
Farmer Bram | h | Talks to his carrots. They grow out of politeness. | species=BEARKIN; fam=bramble
Honeysuckle | s | Beekeeper. The bees elected her queen. | job=Beekeeper; species=BEARKIN; fam=bramble
Haybale | t | Sleeps in the hayloft. Wakes up as a scarecrow. | species=PLUSHIE
Moo Lissa | s | Dairy farmer. Her milkshakes bring everyone. | job=Dairy Farmer; species=PUPKIN
Appleby | h | Orchard keeper. Names every apple tree after a poet. | job=Orchard Keeper; species=HUMAN; skin=SKIN_OLIVE
Turnip Twin | t | Two siblings in one coat. Insist they are one. | job=Farm Helper; species=BEARKIN; fam=bramble; trait=KID
""" ),

]
