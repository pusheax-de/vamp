# Vampire — 2D Turn-Based RPG

A city-based vampire RPG with a GURPS-inspired rule system, built in C++14 as a Win32 application.

## Premise

You play as a human who gets infected by a vampire in a modern city. Dive into the underground metro tunnels, navigate vampire clan politics, fight police and mercenaries, and master blood disciplines — all in a turn-based, cover-based tactical system.

## RPG System Overview

### Core Attributes (1–12)
| Attribute | Governs |
|-----------|---------|
| **STR** | Melee damage, carry, grapples |
| **AGI** | Accuracy, dodge, stealth, AP scaling |
| **END** | HP, resist bleed/poison, sprint |
| **PER** | Vision range, overwatch, detecting hidden enemies |
| **INT** | Hacking, crafting, medicine, ritual complexity |
| **CHA** | Dialogue, prices, leadership, mental resistance |

### Derived Stats
- **HP** = 8 + END + ?STR/2?
- **AP per turn** = 6 + ?AGI/2?
- **Initiative** = AGI + PER (tie-break 1d6)
- **Vision radius** = 6 + ?PER/2? tiles

### Skill Check
All checks: **3d6 ? (Attribute + SkillRank + modifiers)**

### Skills (26 total, ranked 0–5)
- **Combat:** Firearms, Melee, Athletics, Tactics, Explosives, Medicine
- **Thief:** Stealth, Lockpicking, Pickpocket, Hacking, Traps, Disguise
- **Social:** Talking, Persuasion, Deception, Intimidation, Empathy, Streetwise
- **Vampire Disciplines:** Blink, Telekinesis, Hemocraft, Blood Mark, Auspex, Domination, Obfuscate, Celerity

### Combat
- Turn-based with Action Points
- Directional cover (half: ?2, full: ?4 to attacker)
- Overwatch, suppression, burst fire
- Wound effects: Bleeding, Stunned, Crippled, Pinned

### Vampire Disciplines
Powered by **Blood Reserve** (max 6 + Blood Potency), replenished by sleep or risky feeding.

| Discipline | Effect |
|------------|--------|
| **Blink** | Teleport up to 3–6 tiles |
| **Telekinesis** | Push/pull enemies, disarm weapons |
| **Hemocraft** | Blood armor, blood spike, seal doors |
| **Blood Mark** | Track target through fog-of-war + debuff |
| **Auspex** | Reveal pulse, pierce stealth and obfuscation |
| **Domination** | Stun or command weak-willed humans |
| **Obfuscate** | Supernatural invisibility |
| **Celerity** | Spend BR to gain bonus AP |

### Vampire Clans
| Clan | Specialty |
|------|-----------|
| **Nosferatu** | Obfuscate, Auspex — metro dwellers, masters of stealth |
| **Tremere** | Hemocraft, Blood Mark — blood sorcerers |
| **Brujah** | Celerity, Blink — street fighters, anarchists |
| **Ventrue** | Domination, Telekinesis — aristocrats, mental control |
| **Malkavian** | Auspex, Domination — seers and madmen |

### Sleep & Safehouses
Sleep at limited safehouses to fully restore Blood Reserve. Each has a security rating, access cost, and territory alignment. Ambush chance = base 10% + heat ? security.

## Project Structure

```
vampire.sln                  Solution file
vampire.vcxproj              Project file
vampire.cpp                  Win32 application entry point
framework.h                  Windows framework includes
game/
  GameSystems.h              Master include header
  Dice.h / .cpp              3d6 rolls, random utilities
  Attributes.h               Core attributes + derived stats
  SkillDefs.h                Skill IDs, categories, metadata table
  Skill.h / .cpp             Skill ranks, checks, opposed rolls
  StatusEffect.h / .cpp      Status effects (Bleed, Stun, Pinned, etc.)
  Weapon.h                   Weapon data table (14 weapon types)
  Armor.h                    Armor data table (6 armor types)
  Inventory.h / .cpp         Items, equipment slots, consumables
  MapTile.h                  Tile data: terrain, directional cover, LoS
  Character.h / .cpp         Full character sheet
  CoverSystem.h / .cpp       Cover queries, LoS (Bresenham), flanking
  GameWorld.h / .cpp          Grid map, territories, clans, heat system
  FogOfWar.h / .cpp          Vision, fog-of-war, reveal mechanics
  CombatSystem.h / .cpp      Turn-based combat resolution
  Discipline.h / .cpp        8 vampire disciplines
  SocialSystem.h / .cpp      Dialogue, persuasion, deception, empathy
  SleepSystem.h / .cpp       Safehouses, sleep, ambush, feeding
```

## Building

Open `vampire.sln` in Visual Studio 2022 and build (Debug/x64). Requires Windows SDK 10.0.

## License

Private project — all rights reserved.
