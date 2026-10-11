/* unlocks_action.c - the action module's unlockables (ids 416..607):
 * weapon frames and parts for Rivet's workshop, the Arena and Boss Rush
 * modes, a crosshair, and the demon gallery (owner: action).
 *
 * Most parts start unlocked (the workshop is big from the first visit);
 * these are earned by fighting: kills (cute / brutal), headshots, secrets,
 * arena waves, welding and a few by story progress (granted by code). */
#include "doomo/unlocks.h"
#include "doomo/weld.h"
#include "doomo/town.h"

#define W(id, list, idx, name, how, prog, need) \
    { id, UL_CAT_WEAPON_PART, list, idx, name, how, prog, need }
#define MODE(id, mode, name, how, prog, need) \
    { id, UL_CAT_MODE, 0, mode, name, how, prog, need }
#define GAL(id, idx, name, how) \
    { id, UL_CAT_GALLERY, GAL_DEMONS, idx, name, how, 0, 0 }

const DoomoUnlock doomo_unlocks_action[] = {
    /* ---- frames ---- */
    W(416, CLIST_WFRAME, WFRAME_DUAL_PISTOLS, "Dual Pistols", "Weld 3 weapons", PROG_WEAPONS_WELDED, 3),
    W(417, CLIST_WFRAME, WFRAME_MACHINE_PISTOL, "Machine Pistol", "Defeat 15 demons", PROG_DEMONS_KILLED, 15),
    W(418, CLIST_WFRAME, WFRAME_ASSAULT_RIFLE, "Assault Rifle", "Defeat 30 demons", PROG_DEMONS_KILLED, 30),
    W(419, CLIST_WFRAME, WFRAME_BATTLE_RIFLE, "Battle Rifle", "Defeat 80 demons", PROG_DEMONS_KILLED, 80),
    W(420, CLIST_WFRAME, WFRAME_BULLPUP, "Bullpup", "Weld 6 weapons", PROG_WEAPONS_WELDED, 6),
    W(421, CLIST_WFRAME, WFRAME_LMG, "LMG", "Defeat 150 demons", PROG_DEMONS_KILLED, 150),
    W(422, CLIST_WFRAME, WFRAME_MINIGUN, "Minigun", "Defeat 300 demons", PROG_DEMONS_KILLED, 300),
    W(423, CLIST_WFRAME, WFRAME_DMR, "DMR", "Land 15 headshots", PROG_HEADSHOTS, 15),
    W(424, CLIST_WFRAME, WFRAME_SNIPER, "Sniper Rifle", "Land 40 headshots", PROG_HEADSHOTS, 40),
    W(425, CLIST_WFRAME, WFRAME_RAILGUN, "Railgun", "Land 100 headshots", PROG_HEADSHOTS, 100),
    W(426, CLIST_WFRAME, WFRAME_CROSSBOW, "Crossbow", "Find 2 secrets", PROG_SECRETS_FOUND, 2),
    W(427, CLIST_WFRAME, WFRAME_BOW, "Bow", "Find a secret", PROG_SECRETS_FOUND, 1),
    W(428, CLIST_WFRAME, WFRAME_SUPER_SHOTGUN, "Super Shotgun", "Defeat 60 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 60),
    W(429, CLIST_WFRAME, WFRAME_AUTO_SHOTGUN, "Auto Shotgun", "Survive 10 arena waves", PROG_ARENA_WAVES, 10),
    W(430, CLIST_WFRAME, WFRAME_LEVER_SHOTGUN, "Lever Shotgun", "Weld 4 weapons", PROG_WEAPONS_WELDED, 4),
    W(431, CLIST_WFRAME, WFRAME_GRENADE_LAUNCHER, "Grenade Launcher", "Survive 5 arena waves", PROG_ARENA_WAVES, 5),
    W(432, CLIST_WFRAME, WFRAME_ROCKET_LAUNCHER, "Rocket Launcher", "Defeat 120 demons", PROG_DEMONS_KILLED, 120),
    W(433, CLIST_WFRAME, WFRAME_FLAMETHROWER, "Flamethrower", "Defeat 50 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 50),
    W(434, CLIST_WFRAME, WFRAME_PLASMA_RIFLE, "Plasma Rifle", "Survive 15 arena waves", PROG_ARENA_WAVES, 15),
    W(435, CLIST_WFRAME, WFRAME_NAILGUN, "Nailgun", "Weld 8 weapons", PROG_WEAPONS_WELDED, 8),
    W(436, CLIST_WFRAME, WFRAME_BFG, "BFG", "Defeat Lord Vapula", 0, 0),
    W(437, CLIST_WFRAME, WFRAME_CHAINSAW, "Chainsaw", "Defeat 25 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 25),
    W(438, CLIST_WFRAME, WFRAME_SLEDGEHAMMER, "Sledgehammer", "Defeat 40 demons", PROG_DEMONS_KILLED, 40),
    W(439, CLIST_WFRAME, WFRAME_PLUSH_LAUNCHER, "Plush Launcher", "Defeat 20 demons cutely", PROG_DEMONS_KILLED_CUTE, 20),
    W(440, CLIST_WFRAME, WFRAME_LOLLIPOP_HAMMER, "Lollipop Hammer", "Defeat 40 demons cutely", PROG_DEMONS_KILLED_CUTE, 40),
    W(441, CLIST_WFRAME, WFRAME_HEART_BOW, "Heart Bow", "Win 3 exorcisms", PROG_EXORCISMS_WON, 3),
    W(442, CLIST_WFRAME, WFRAME_GLITTER_SPRAYER, "Glitter Sprayer", "Defeat 60 demons cutely", PROG_DEMONS_KILLED_CUTE, 60),
    W(443, CLIST_WFRAME, WFRAME_STICKER_GUN, "Sticker Gun", "Weld 5 weapons", PROG_WEAPONS_WELDED, 5),
    W(444, CLIST_WFRAME, WFRAME_YARN_CROSSBOW, "Yarn Crossbow", "Make 5 friendships", PROG_FRIENDSHIPS, 5),
    W(445, CLIST_WFRAME, WFRAME_KATANA, "Katana", "Land 20 headshots", PROG_HEADSHOTS, 20),
    /* ---- barrels ---- */
    W(450, CLIST_WBARREL, WBARREL_DOUBLE, "Double Barrel", "Weld 2 weapons", PROG_WEAPONS_WELDED, 2),
    W(451, CLIST_WBARREL, WBARREL_TRIPLE, "Triple Barrel", "Weld 12 weapons", PROG_WEAPONS_WELDED, 12),
    W(452, CLIST_WBARREL, WBARREL_SPIRAL, "Spiral Barrel", "Land 10 headshots", PROG_HEADSHOTS, 10),
    W(453, CLIST_WBARREL, WBARREL_WIDE, "Wide Barrel", "Defeat 45 demons", PROG_DEMONS_KILLED, 45),
    W(454, CLIST_WBARREL, WBARREL_SAWED, "Sawed-off Barrel", "Defeat 20 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 20),
    W(455, CLIST_WBARREL, WBARREL_SUPPRESSED, "Suppressor", "Find 3 secrets", PROG_SECRETS_FOUND, 3),
    W(456, CLIST_WBARREL, WBARREL_HEART, "Heart Barrel", "Defeat 25 demons cutely", PROG_DEMONS_KILLED_CUTE, 25),
    W(457, CLIST_WBARREL, WBARREL_STAR, "Star Barrel", "Find 6 secrets", PROG_SECRETS_FOUND, 6),
    /* ---- cores ---- */
    W(460, CLIST_WCORE, WCORE_AP_BULLET, "AP Bullets", "Defeat 35 demons", PROG_DEMONS_KILLED, 35),
    W(461, CLIST_WCORE, WCORE_EXPLOSIVE_ROUND, "Explosive Rounds", "Survive 4 arena waves", PROG_ARENA_WAVES, 4),
    W(462, CLIST_WCORE, WCORE_SLUG, "Slugs", "Defeat 25 demons", PROG_DEMONS_KILLED, 25),
    W(463, CLIST_WCORE, WCORE_FLECHETTE, "Flechettes", "Land 30 headshots", PROG_HEADSHOTS, 30),
    W(464, CLIST_WCORE, WCORE_PLASMA, "Plasma Core", "Defeat 90 demons", PROG_DEMONS_KILLED, 90),
    W(465, CLIST_WCORE, WCORE_ROCKET, "Rocket Core", "Defeat 110 demons", PROG_DEMONS_KILLED, 110),
    W(466, CLIST_WCORE, WCORE_GRENADE, "Grenade Core", "Survive 6 arena waves", PROG_ARENA_WAVES, 6),
    W(467, CLIST_WCORE, WCORE_NAIL, "Nail Core", "Weld 3 weapons", PROG_WEAPONS_WELDED, 3),
    W(468, CLIST_WCORE, WCORE_RAIL_SLUG, "Rail Slugs", "Land 75 headshots", PROG_HEADSHOTS, 75),
    W(469, CLIST_WCORE, WCORE_FUEL, "Fuel Core", "Defeat 35 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 35),
    W(470, CLIST_WCORE, WCORE_GLITTER, "Glitter Core", "Defeat 15 demons cutely", PROG_DEMONS_KILLED_CUTE, 15),
    W(471, CLIST_WCORE, WCORE_HOLY_WATER, "Holy Water Core", "Win your first exorcism", PROG_EXORCISMS_WON, 1),
    W(472, CLIST_WCORE, WCORE_HELLFIRE, "Hellfire Core", "Defeat 200 demons", PROG_DEMONS_KILLED, 200),
    W(473, CLIST_WCORE, WCORE_ICE, "Ice Core", "Find 4 secrets", PROG_SECRETS_FOUND, 4),
    W(474, CLIST_WCORE, WCORE_LOVE, "Love Core", "Befriend 3 folks across the wall", PROG_CROSS_BORDER_FRIENDS, 3),
    W(475, CLIST_WCORE, WCORE_SAWBLADE, "Sawblades", "Defeat 80 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 80),
    W(476, CLIST_WCORE, WCORE_SLIME, "Slime Core", "Defeat 70 demons", PROG_DEMONS_KILLED, 70),
    /* ---- mods ---- */
    W(480, CLIST_WMOD, WMOD_BAYONET, "Bayonet", "Defeat 15 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 15),
    W(481, CLIST_WMOD, WMOD_COOLER, "Cooler", "Weld 4 weapons", PROG_WEAPONS_WELDED, 4),
    W(482, CLIST_WMOD, WMOD_OVERCLOCK, "Overclock", "Survive 8 arena waves", PROG_ARENA_WAVES, 8),
    W(483, CLIST_WMOD, WMOD_RICOCHET, "Ricochet", "Find 5 secrets", PROG_SECRETS_FOUND, 5),
    W(484, CLIST_WMOD, WMOD_HOMING, "Homing", "Land 50 headshots", PROG_HEADSHOTS, 50),
    W(485, CLIST_WMOD, WMOD_BURST, "Burst Fire", "Defeat 55 demons", PROG_DEMONS_KILLED, 55),
    W(486, CLIST_WMOD, WMOD_FULL_AUTO, "Full Auto", "Defeat 65 demons", PROG_DEMONS_KILLED, 65),
    W(487, CLIST_WMOD, WMOD_CHARGE_SHOT, "Charge Shot", "Survive 12 arena waves", PROG_ARENA_WAVES, 12),
    W(488, CLIST_WMOD, WMOD_LIFESTEAL, "Lifesteal", "Defeat 100 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 100),
    W(489, CLIST_WMOD, WMOD_SQUEAKER, "Squeaker", "Defeat 10 demons cutely", PROG_DEMONS_KILLED_CUTE, 10),
    /* ---- sights ---- */
    W(492, CLIST_WSIGHT, WSIGHT_HOLO, "Holo Sight", "Weld 2 weapons", PROG_WEAPONS_WELDED, 2),
    W(493, CLIST_WSIGHT, WSIGHT_SCOPE_4X, "4x Scope", "Land 5 headshots", PROG_HEADSHOTS, 5),
    W(494, CLIST_WSIGHT, WSIGHT_SCOPE_8X, "8x Scope", "Land 60 headshots", PROG_HEADSHOTS, 60),
    W(495, CLIST_WSIGHT, WSIGHT_THERMAL, "Thermal Sight", "Find 8 secrets", PROG_SECRETS_FOUND, 8),
    W(496, CLIST_WSIGHT, WSIGHT_HEART_DOT, "Heart Dot", "Defeat 35 demons cutely", PROG_DEMONS_KILLED_CUTE, 35),
    W(497, CLIST_WSIGHT, WSIGHT_LASER, "Laser Sight", "Defeat 10 demons", PROG_DEMONS_KILLED, 10),
    /* ---- stocks ---- */
    W(500, CLIST_WSTOCK, WSTOCK_HEAVY, "Heavy Stock", "Defeat 50 demons", PROG_DEMONS_KILLED, 50),
    W(501, CLIST_WSTOCK, WSTOCK_SKELETON, "Skeleton Stock", "Defeat 30 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 30),
    W(502, CLIST_WSTOCK, WSTOCK_PLUSH, "Plush Stock", "Defeat 20 demons cutely", PROG_DEMONS_KILLED_CUTE, 20),
    /* ---- magazines ---- */
    W(505, CLIST_WMAG, WMAG_DRUM, "Drum Mag", "Defeat 75 demons", PROG_DEMONS_KILLED, 75),
    W(506, CLIST_WMAG, WMAG_QUICK, "Quick Mag", "Weld 7 weapons", PROG_WEAPONS_WELDED, 7),
    W(507, CLIST_WMAG, WMAG_BELT, "Ammo Belt", "Survive 18 arena waves", PROG_ARENA_WAVES, 18),
    W(508, CLIST_WMAG, WMAG_CANDY_TUBE, "Candy Tube", "Defeat 45 demons cutely", PROG_DEMONS_KILLED_CUTE, 45),
    /* ---- underbarrel ---- */
    W(510, CLIST_WUNDER, WUNDER_GRENADE, "Underbarrel Grenade", "Survive 7 arena waves", PROG_ARENA_WAVES, 7),
    W(511, CLIST_WUNDER, WUNDER_SHOTGUN, "Underbarrel Shotgun", "Defeat 85 demons", PROG_DEMONS_KILLED, 85),
    W(512, CLIST_WUNDER, WUNDER_FLASHLIGHT, "Flashlight", "Find 2 secrets", PROG_SECRETS_FOUND, 2),
    W(513, CLIST_WUNDER, WUNDER_CHAINSAW, "Underbarrel Saw", "Defeat 70 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 70),
    W(514, CLIST_WUNDER, WUNDER_BUBBLE, "Bubble Maker", "Defeat 50 demons cutely", PROG_DEMONS_KILLED_CUTE, 50),
    /* ---- charms ---- */
    W(517, CLIST_WCHARM, WCHARM_SKULL, "Skull Charm", "Defeat 10 demons brutally", PROG_DEMONS_KILLED_BRUTAL, 10),
    W(518, CLIST_WCHARM, WCHARM_DICE, "Lucky Dice", "Survive 3 arena waves", PROG_ARENA_WAVES, 3),
    W(519, CLIST_WCHARM, WCHARM_EYEBALL, "Eyeball Charm", "Land 35 headshots", PROG_HEADSHOTS, 35),
    W(520, CLIST_WCHARM, WCHARM_ROSARY, "Rosary Charm", "Win 5 exorcisms", PROG_EXORCISMS_WON, 5),
    /* ---- modes ---- */
    MODE(530, MODE_ARENA, "Demon Arena", "Clear the Vape Cellar or defeat 60 demons", PROG_DEMONS_KILLED, 60),
    MODE(531, MODE_BOSS_RUSH, "Boss Rush", "Defeat Lord Vapula", 0, 0),
    /* ---- a crosshair ---- */
    { 535, UL_CAT_SKIN, OPT_CROSSHAIR, 4, "Star Crosshair", "Find 4 secrets", PROG_SECRETS_FOUND, 4 },
    /* ---- the demon gallery (granted on the first defeat of each) ---- */
    GAL(540, 0, "Impling", "Defeat an Impling"),
    GAL(541, 1, "Vape Wraith", "Defeat a Vape Wraith"),
    GAL(542, 2, "Puff Demon", "Defeat a Puff Demon"),
    GAL(543, 3, "Hellhound Pup", "Defeat a Hellhound Pup"),
    GAL(544, 4, "Bone Jester", "Defeat a Bone Jester"),
    GAL(545, 5, "Slime Belcher", "Defeat a Slime Belcher"),
    GAL(546, 6, "Gargoyle", "Defeat a Gargoyle"),
    GAL(547, 7, "Cyber Piggy", "Defeat a Cyber Piggy"),
    GAL(548, 8, "Brute", "Defeat a Brute"),
    GAL(549, 9, "Cloud Spider", "Defeat a Cloud Spider"),
    GAL(550, 10, "Lord Vapula", "Defeat Lord Vapula"),
    /* ---- secret achievements ---- */
    { 560, UL_CAT_MISC, 0, 0, "Legendary Welder", "Weld a weapon of quality 100", 0, 0 },
    { 561, UL_CAT_MISC, 0, 1, "Plush Pacifist", "Finish a level with only cute kills", 0, 0 },
    { 562, UL_CAT_MISC, 0, 2, "Gibbed!", "Gib a demon with a brutal overkill", 0, 0 },
    { 563, UL_CAT_MISC, 0, 3, "Secret Sniffer", "Find every secret in a level", 0, 0 },
    { 0, 0, 0, 0, NULL, NULL, 0, 0 }
};
