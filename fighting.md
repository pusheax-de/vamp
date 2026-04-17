# Fighting Scenarios and NPC Combat Doctrine

This document defines practical combat expectations for the current RPG system in `game/CombatSystem.*`, `game/Character.*`, `game/Weapon.h`, `game/Armor.h`, and `game/Discipline.*`. It is intended as a design reference for later NPC-vs-NPC and player-vs-NPC simulation.

## 1. System Summary

Combat is a turn-based attrition system built around:

- `3d6 <= target number` checks.
- Effective combat score = `primary attribute + skill rank + modifiers`.
- Derived action points per turn = `6 + floor(AGI / 2)`.
- Derived HP = `8 + END + floor(STR / 2)`.
- Initiative = `AGI + PER + 1d6`.
- Flat damage reduction from armor and Hemocraft armor.

This creates a system where:

- Small score gaps matter a lot.
- Cover matters because it directly reduces hit target numbers.
- AP economy matters as much as raw damage.
- Wound effects rapidly snowball fights after the first solid hit.

## 2. Practical Meaning of "Level"

The code does not define a single character level. For combat analysis, use three bands:

- `Low-tier`: primary combat attribute `4-5`, key combat skill rank `0-1`, light armor or none, basic weapon.
- `Mid-tier`: primary combat attribute `6-8`, key combat skill rank `2-3`, kevlar or tactical armor, competent weapon loadout.
- `High-tier`: primary combat attribute `9-12`, key combat skill rank `4-5`, optimized armor, strong weapon or discipline package.

For hit checks, the most important value is:

- `Combat score = AGI + Firearms` for most ranged attackers.
- `Combat score = STR + Melee` for melee attackers.
- `Combat score = INT + Hemocraft`, `CHA + Domination`, `AGI + Obfuscate`, etc. for vampires.

Useful benchmark target numbers before cover/range:

- `5-7`: poor, unreliable in combat.
- `8-10`: workable only at short range or with aim/cover advantage.
- `11-13`: competent.
- `14-16`: strong.
- `17+`: elite and very consistent.

On 3d6, the curve is steep. A difference of `+2` to `+4` in target number is a major swing, not a minor one.

## 3. Baseline Archetypes for Simulation

Use these as anchors when designing NPC rosters.

### 3.1 Street Civilian

- Attributes: AGI 4, END 4, PER 4, STR 4.
- Skills: Firearms 0, Melee 0, Athletics 0, Tactics 0.
- Gear: knife, bat, or cheap pistol; no armor or light clothing.
- Role: panic, flee, miss often, collapse quickly.

### 3.2 Gang Enforcer

- Attributes: AGI 6, END 6, PER 5, STR 6.
- Skills: Firearms 2, Melee 2, Athletics 1.
- Gear: pistol, shotgun, machete; light clothing or kevlar.
- Role: short-range aggression, basic flanking, limited discipline.

### 3.3 Police / Mercenary

- Attributes: AGI 7, END 7, PER 7, STR 6.
- Skills: Firearms 3, Tactics 2, Athletics 2, Medicine 1.
- Gear: SMG or assault rifle, kevlar or tactical armor.
- Role: cover use, overwatch, focus fire, suppression.

### 3.4 Elite Hunter / Clan Veteran

- Attributes: AGI 9, END 8, PER 8, STR 7, or discipline-focused alternatives.
- Skills: Firearms 4-5 or Melee 4-5, Tactics 3-4, plus discipline rank 3-5.
- Gear: tactical armor, strong weapon, consumables.
- Role: wins through AP efficiency, positioning, and status snowball.

### 3.5 Vampire Skirmisher

- Attributes: AGI 8, END 7, PER 7, CHA 6, INT 6.
- Skills: Firearms 2-3 or Melee 3, plus Blink/Obfuscate/Celerity/Hemocraft 2-4.
- Gear: light armor or kevlar, pistol/SMG/blade.
- Role: reposition, tempo spikes, stealth, selective burst damage.

## 4. Combat Mechanics That Drive Outcomes

These are the main reasons one side wins.

### 4.1 AP Economy

Most common AP costs:

- Move: `1` or tile move cost.
- Aim: `1` for `+2`, stacking to `+6`.
- Single shot: `3`.
- Burst: `4`.
- Reload: `2`.
- Overwatch: `2`.
- Melee: `2-3`.
- Bandage/stim: `2`.

Typical consequences:

- A character with `8 AP` can move once, aim once, and fire twice only if using very cheap actions efficiently.
- A character with `10-12 AP` from high AGI or Celerity can create decisive tempo turns.
- `Pinned` and `Stunned` are brutal because each removes `2 AP` next turn.

### 4.2 Cover and Range

Cover imposes direct hit penalties:

- Half cover effectively costs the attacker `-2`.
- Full cover effectively costs `-4`.
- Diagonal/flanked attacks remove directional cover.

Implications:

- Two equally skilled shooters can differ by an entire combat tier if only one is in cover.
- Fights should be understood as contests over lane control and flank creation, not just damage races.

### 4.3 Armor and Damage

Weapons deal mostly `2d6 + bonus` at range, `1d6-3d6 + bonus` in melee.
Armor gives flat DR:

- Light clothing: `1`.
- Kevlar: `3`.
- Tactical armor: `5`.
- Heavy armor: `7`.
- Hemocraft armor: `+3 to +8` temporary DR depending on rank.

Implications:

- Against unarmored targets, any solid hit is dangerous.
- Against tactical armor or Hemocraft armor, low-damage weapons lose efficiency quickly.
- Shotguns, sniper rifles, machetes, swords, and Blood Spike matter because they cross DR thresholds more reliably.

### 4.4 Wound Snowball

The first meaningful hit often changes the rest of the fight:

- `4+` damage causes Bleeding.
- `6+` damage causes Stunned.
- Critical hits cripple.

This means combat is not linear. Once one side lands a strong hit:

- AP falls on the next turn.
- accuracy falls from Stunned or Crippled Arm.
- movement degrades from Crippled Leg.
- untreated Bleeding accelerates collapse.

## 5. Matchup Expectations by Tier

These are practical, not exact, odds.

### 5.1 Equal Tier

If both sides are same tier, same weapon class, and similar cover:

- `1v1`: roughly even, with initiative and first clean hit deciding the fight.
- `1v2`: the solo fighter is usually disadvantaged unless the pair enters bad lanes or bunches into suppression/disciplines.
- `1v3+`: action economy dominates unless the solo fighter has strong stealth, alpha strike, or area denial.

### 5.2 One Tier Difference

A one-tier advantage is meaningful but not absolute.

- In `1v1`, the stronger combatant is favored.
- In `1v2`, the stronger solo fighter can still win if they deny line of sight and fight serially.
- In `1v3`, the stronger solo fighter needs terrain, stealth, or vampire powers.
- In `1v5`, one tier is not enough; numbers usually win.

### 5.3 Two Tier Difference

A two-tier advantage is decisive in small engagements.

- In `1v1`, the stronger fighter should win consistently unless ambushed.
- In `1v2`, the stronger fighter is still favored.
- In `1v3`, the stronger fighter becomes coin-flip to favored if the enemies are low-tier and poorly coordinated.
- In `1v5`, even a two-tier advantage is fragile without area effects, stealth cycling, or hard cover.

## 6. Scenario Analysis

## 6.1 One vs One

This is the cleanest skill test.

Winning strategy for ranged fighters:

- Take cover immediately if available.
- Aim on turn one if no good shot exists.
- Prefer single shots unless burst will finish or suppress an exposed target.
- Reload early, not after going dry under pressure.

Winning strategy for melee fighters:

- Use cover and movement to approach without taking free shots.
- Force corner fights and adjacency.
- Exploit Crippled/Stunned opponents immediately.

Winning strategy for vampires:

- Blink to cover or flank.
- Use Celerity to create a decisive alpha turn.
- Use Hemocraft armor before entering a damage race.
- Use Obfuscate only if the map supports breaking sightlines after the reveal.

What levels are possible:

- Equal low-tier vs low-tier: messy, swingy, often decided by weapon quality.
- Equal mid-tier vs mid-tier: very tactical, cover and first stun matter more than raw HP.
- Equal high-tier vs high-tier: often decided by AP spikes, disciplines, and status sequencing.
- Mid-tier reliably beats low-tier in open ground.
- High-tier reliably beats mid-tier unless caught in a bad opening.

Likely outcomes:

- Short firefight if one side is exposed.
- Longer duel if both use cover and aimed single shots.
- Fast resolution if shotgun/melee/discipline closes the distance.

## 6.2 One vs Two

This is the first scenario where action economy becomes structural.

The solo combatant can win if:

- the two enemies cannot both fire effectively on the same turn.
- one enemy can be removed or pinned immediately.
- the terrain allows serial engagement.
- the solo side has vampire mobility or suppression tools.

Best solo strategies:

- Fight from a corner or choke.
- Break line of sight after every attack if possible.
- Focus fire one target to zero instead of spreading damage.
- Use overwatch to punish the second enemy's reposition.
- Use burst/suppression if it prevents both enemies from acting efficiently next turn.

Best duo strategies:

- Split into shooter plus flanker.
- One holds overwatch or suppression while the other repositions.
- Force the solo fighter to spend AP on movement instead of aim.

What levels are possible:

- Equal-tier solo usually loses.
- Solo with one-tier advantage can win if terrain is favorable.
- Solo with two-tier advantage is viable, especially against low-tier enemies.
- Solo vampire with Blink/Celerity/Hemocraft performs above nominal tier.

Likely outcomes:

- If the duo starts in good lanes, they grind the solo down.
- If the solo deletes one enemy in the first two rounds, the fight often collapses into a favorable `1v1`.

## 6.3 One vs Three

This is the threshold where basic competence on all three enemies is usually enough to beat a lone equal opponent.

Best solo strategies:

- Ambush the outermost enemy and force `1v1`, then `1v2`, then `1v1`.
- Abuse stealth, Obfuscate, Blink, and corners.
- Use burst to pin one target and hard-focus another.
- Retreat when multiple enemies gain simultaneous line of sight.

Best trio strategies:

- One anchor in cover.
- One flanker.
- One reserve/support for overwatch, reload coverage, or consumables.
- Rotate wounded members backward instead of letting one die in place.

What levels are possible:

- Equal-tier solo normally loses.
- Solo with one-tier advantage still usually loses in open ground.
- Solo with two-tier advantage can win only with terrain, stealth, or strong vampire kit.
- Elite vampire versus three low-tier humans is a valid showcase encounter.

Likely outcomes:

- Without terrain separation, the solo side gets AP-starved and loses.
- With sightline control, the solo side can turn it into repeated `1v1` exchanges.

## 6.4 One vs Five

This should be treated as an asymmetric encounter, not a fair duel.

The solo combatant needs one or more of:

- extreme tier advantage.
- repeated stealth resets.
- hard choke geometry.
- area control through suppression, flashbangs, or door sealing.
- vampire tempo tools like Blink, Celerity, Hemocraft armor, and Domination.

Best solo strategies:

- Never fight all five at once.
- Kill isolated targets and disengage.
- Use doors, corners, and noise to split the group.
- Force enemies to enter overwatch arcs one at a time.
- Prioritize enemies with high firearms skill or tactical rifles first.

Best five-man strategies:

- Maintain two firing lanes and one flank lane.
- Do not stack in burst or flashbang range.
- Cycle wounded units backward.
- Use suppression to deny solo movement.
- Keep at least one unit covering likely Blink exits or choke exits.

What levels are possible:

- Equal-tier solo should lose almost always.
- Solo one-tier above still loses in most cases.
- Solo two-tier above may still lose unless the five are low-tier and stupid.
- Only elite vampires or boss-class NPCs should be expected to beat five competent enemies.

Likely outcomes:

- Open terrain: numbers win.
- Corridor/choke terrain: the solo side can produce a dramatic but still risky victory.

## 6.5 Two vs Five Cooperative

This is the first scenario where coordination becomes more important than raw stats.

Two coordinated mid/high-tier characters can beat five weaker enemies because:

- one can anchor and suppress.
- one can flank or finish.
- one can spend AP on utility while the other spends AP on damage.
- status and focus fire can reduce enemy effective actions very quickly.

Strong cooperative pair compositions:

- `Rifle + SMG`: anchor plus flanker.
- `Shooter + melee bruiser`: one fixes lanes, one punishes cover breaches.
- `Shooter + vampire controller`: one deals steady damage, one disrupts with Blink/Domination/Hemocraft.
- `Double marksmen`: works only with superior cover and overwatch discipline.

Best two-person strategies:

- Designate roles before simulation: anchor and maneuver.
- Focus fire until one enemy drops; never spread early damage.
- Use overlapping overwatch arcs.
- Rotate whoever is bleeding or stunned out of the front.
- Spend resources early enough to preserve action advantage.

Best five-person strategies:

- Collapse on one member of the pair.
- Force the pair to separate.
- Use one expendable flanker to break overwatch.
- Keep pressure so the pair cannot spend turns aiming or healing safely.

What levels are possible:

- Two equal-tier fighters can beat five if the five are low-tier or tactically weak.
- Two mid-tier fighters versus five mid-tier fighters is usually losing unless terrain is excellent.
- Two high-tier fighters or one high-tier plus one strong vampire can plausibly beat five mid-tier enemies.

Likely outcomes:

- Good pair coordination turns `2v5` into a sequence of `2v1` or `2v2` local fights.
- Poor coordination turns it into two separate losses.

## 7. Recommended Encounter Balance Rules

Use these heuristics when building test fights.

- `1v1 fair fight`: same tier, similar weapon class, similar cover access.
- `1v2 fair challenge`: solo should be one tier stronger or have superior opening position.
- `1v3 boss fight`: solo should be two tiers stronger or have meaningful discipline advantages.
- `1v5 boss showcase`: only for elite vampire or scripted ambush terrain.
- `2v5 heroic but plausible`: pair should have either one tier advantage, strong map control, or a complementary discipline kit.

## 8. Trivial NPC AI for First Simulations

The first AI should be simple, deterministic enough to debug, and cheap to run.

### 8.1 Rule-Based Turn Skeleton

For each NPC turn:

1. Evaluate threat.
2. Pick a target.
3. Pick a posture.
4. Spend AP according to posture.
5. Re-evaluate after each major action.

### 8.2 Threat Score

Score visible enemies by:

- can kill me soon.
- closest exposed target.
- lowest current HP.
- enemy with strongest weapon.
- enemy currently stunned, pinned, or flanked.
- enemy already targeted by allies.

This should make NPCs naturally focus fire without complicated planning.

### 8.3 Postures

Each NPC can operate in one of a few postures:

- `Aggressive`: close distance, flank, burst, melee if favorable.
- `Cautious`: stay in cover, aim, single-shot, overwatch.
- `Defensive`: bandage, reload, retreat to better cover.
- `Hunter`: pursue hidden or fleeing targets using Tactics/Auspex.
- `Controller`: use disciplines, suppression, or overwatch to deny movement.

The posture can be chosen from simple conditions:

- low HP or bleeding -> Defensive.
- target exposed and close -> Aggressive.
- no good shot but good lane -> Cautious.
- vampire with enough BR and high-value power available -> Controller.

### 8.4 Basic Action Priorities

For mundane shooter NPCs:

1. If bleeding and safe, bandage.
2. If no ammo and safe, reload.
3. If target kill chance is high, shoot.
4. If burst can pin multiple plans or finish target, burst.
5. If no good shot, move to cover.
6. If lane is strong, overwatch.
7. If AP remains, aim.

For melee NPCs:

1. If adjacent, attack.
2. If one move from adjacency and not exposed to multiple shooters, advance.
3. If crossing open ground under fire, wait for support or use corners.
4. If target stunned or pinned, commit hard.

For vampire NPCs:

1. Use Hemocraft armor before entering sustained exposure.
2. Use Celerity on decisive turns, not randomly.
3. Use Blink to flank, escape, or reach cover.
4. Use Domination on dangerous humans, not on weak cleanup targets.
5. Use Obfuscate only if the next move benefits from broken sight.

## 9. Best NPC Behavior Systems

For this project, the best progression is not full utility AI immediately. Start simple and scale.

### 9.1 Phase 1: Weighted Rule System

Best first implementation.

Why it fits:

- easy to debug.
- deterministic enough for balancing.
- maps directly onto AP-based action lists.
- supports archetypes by changing weights.

Recommended data:

- aggression.
- bravery.
- preferred range.
- discipline usage bias.
- cover bias.
- focus-fire bias.

### 9.2 Phase 2: Utility Scoring Per Action

Best medium-term implementation.

Each possible action gets a score:

- expected hit chance.
- expected damage after DR.
- chance to inflict stun/pin.
- cover gained or lost.
- AP left after action.
- exposure to enemy overwatch.
- synergy with ally targeting.

This gives better combat without hardcoding every case.

### 9.3 Phase 3: Squad Roles With Shared Intent

Best for `3+` NPC encounters.

Add lightweight team coordination:

- assign one anchor.
- assign one flanker.
- assign one suppressor/controller.
- reserve one unit to finish wounded targets.

This avoids the common failure mode where all NPCs independently choose the same mediocre action.

## 10. Recommended AI Heuristics by Encounter Size

### 10.1 Duels

- prioritize survival and cover.
- value aim more highly.
- avoid burst unless finish chance is real.

### 10.2 Small Groups

- prioritize focus fire.
- one actor suppresses, one actor flanks.
- overwatch should guard the likely escape tile, not random directions.

### 10.3 Large Groups

- cap the number of attackers who expose themselves at once.
- keep some units in reserve.
- rotate wounded units backward.
- avoid bunching into doorways or cone-like kill zones.

## 11. Expected Failure Modes in Early Simulation

Watch for these when NPC combat goes live:

- all units choosing nearest target instead of best target.
- units breaking cover for tiny hit chance gains.
- vampires wasting BR on low-impact powers.
- melee NPCs suiciding across open lanes.
- groups overcommitting through chokepoints.
- units refusing to bandage or reload until too late.

## 12. Recommendation for First Simulation Pass

Use the following first-pass test ladder:

1. `1v1`: low vs low, mid vs mid, high vs high.
2. `1v1`: one-tier mismatch and two-tier mismatch.
3. `1v2`: solo mid vs two low, solo high vs two mid.
4. `1v3`: elite vampire vs three low or mid humans.
5. `1v5`: boss vampire in choke terrain only.
6. `2v5`: coordinated pair vs five low, then five mid.

Log the following for every simulation:

- initiative order.
- AP spent per action.
- hit target numbers and rolls.
- damage after DR.
- status applications.
- turns survived.
- whether victory came from raw damage, status snowball, or positioning collapse.

## 13. Final Design Conclusions

- This combat system strongly rewards cover, focus fire, and AP denial.
- Equal numbers matter more than small stat leads.
- Status effects are force multipliers because they convert one hit into future AP and accuracy loss.
- Vampires should outperform mundane NPCs not just through damage, but through tempo, repositioning, and control.
- Early NPC AI should be weighted rule-based with archetype-specific biases.
- For larger encounters, squad coordination and role assignment will matter more than individual tactical perfection.

For simulation purposes, assume:

- `1v1` measures build quality.
- `1v2` measures tempo and positioning.
- `1v3` measures control and terrain abuse.
- `1v5` measures boss design.
- `2v5` measures cooperation quality.
