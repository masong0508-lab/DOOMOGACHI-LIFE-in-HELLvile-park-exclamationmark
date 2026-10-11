"""Hillvile archetypes, part C: households and teenagers.
(owner: roster; see seed.py)"""
from seedlib import A

ARCHETYPES = [

A("homebodies", "hill", "Stay-home Parent|Homemaker|Remote Worker|Retiree",
  sp="BUNNYKIN:2 CATFOLK:2 PUPKIN BEARKIN FOXKIN MOUSEKIN HUMAN",
  top="SWEATER:3 HOODIE:2 TSHIRT:2 DRESS", bottom="PANTS:2 SWEATS:2 SKIRT", shoes="SLIPPERS:4 SNEAKERS",
  hat="NONE:5 BEANIE", acc="NONE:4 GLASSES SCARF",
  pat="SOLID:3 PLAID POLKA", scheme="PEACH/SKY/WHITE MINT/LAVENDER/WHITE LEMON/TEAL/WHITE ROSE/CHOCOLATE/WHITE",
  pers="3-10 0-6 6-13 9-15 2-9", act="HOME:5 PARK",
  food="Pancakes|Spaghetti|Curry|Omurice|Cereal", hobby="Baking|Gardening|Knitting|Board Games|Video Games",
  music="City Pop|Lo-fi Beats|Folk|Bubblegum Pop", pet="35",
  cast="""
Pa Puff | h | Makes pancake art. Mostly lopsided bunnies. | species=BUNNYKIN; fam=puffleton; wed=Ma Puff
Ma Puff | s | Can find a lost sock from three rooms away. | species=BUNNYKIN; fam=puffleton
Puff Jr | t | Asks "why?" about 400 times a day. | job=Student; species=BUNNYKIN; fam=puffleton; trait=KID
Puffling | s | The baby. Has opinions about every vegetable. | job=Toddler; species=BUNNYKIN; fam=puffleton; trait=KID; accessory=PACIFIER
Hazel Nut | s | Works from home in pyjamas. Very productive. | job=Remote Worker; species=CATFOLK; fam=nutley; wed=Walnut
Walnut | h | Fixes everything with tape. It mostly holds. | species=CATFOLK; fam=nutley
Peanutella | s | Spreads joy and hazelnut spread equally. | job=Student; species=CATFOLK; fam=nutley; trait=KID
Biscuit Bo | h | Retired. Spends his days rating park benches. | job=Retiree; species=PUPKIN; trait=ELDER
Cosy Clem | t | Owns 41 blankets. Lends them to cold neighbours. | species=PLUSHIE
Domino | t | Lines up dominoes across the living room. | job=Remote Worker; species=HUMAN; skin=SKIN_BROWN
""" ),

A("teens", "hill", "High Schooler|Part-timer|Paper Delivery",
  traits="TEEN",
  sp="FOXKIN:2 CATFOLK RACCOONKIN PUPKIN BUNNYKIN HUMAN",
  top="HOODIE:3 TSHIRT:2 BAND_TEE CROP_TOP", bottom="SHORTS RIPPED_JEANS:2 SKIRT SWEATS", shoes="SNEAKERS:4 ROLLERSKATES",
  hat="NONE:3 CAP:2 HEADPHONES BEANIE", acc="NONE:2 BACKPACK:3",
  pat="SOLID:3 STRIPES", scheme="LIME/NAVY/WHITE CYAN/GRAPE/WHITE TANGERINE/NAVY/LEMON BUBBLEGUM/TEAL/WHITE",
  pers="8-15 4-11 7-14 7-14 5-12", act="PARK:3 SHOP CAFE", height="4-8",
  food="Pizza|Ramen|Bubble Tea|Tacos|Fried Chicken", hobby="Video Games|Skateboarding|Karaoke|Cosplay|Graffiti",
  music="Hardcore Punk|City Pop|Chiptune|Synthwave|Bubblegum Pop", phrase="Big mood.|Slay!|Whatever.|Yikes.|As if!",
  romance="ANY:6 SHE HE THEY NONE",
  cast="""
Scrunchie | s | Has a scrunchie for every day of the year. | species=FOXKIN
Skipper | h | Delivers newspapers by skateboard, mostly on time. | job=Paper Delivery; species=PUPKIN
Teacup Tia | s | Part-timer at the tea house. Spills, then laughs. | job=Part-timer; species=CATFOLK; bff=Matcha Mimi
Homework | t | Always says "I'll do it tomorrow". Never does. | species=HUMAN
Juniper Jay | h | Writes love songs about the girl next door. | species=FOXKIN
Freckles | s | Counts her freckles each summer. 312 and rising. | species=BUNNYKIN; cheeks=FRECKLES,BLUSH
Grindpaw | h | Skates everywhere. Has never owned a car. | species=HUMAN; skin=SKIN_TAN; shoes=ROLLERSKATES
""" ),

]
