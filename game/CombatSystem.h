#pragma once
// CombatSystem.h - Turn-based combat: attacks, overwatch, suppression, movement

#include "Character.h"
#include "GameWorld.h"
#include "FogOfWar.h"
#include "CoverSystem.h"
#include <vector>
#include <functional>

namespace vamp
{

// ---------------------------------------------------------------------------
// Action types a character can perform
// ---------------------------------------------------------------------------
enum class ActionType : uint8_t
{
    Move,               // Move 1 tile (1 AP, or 2 for difficult terrain)
    CrouchMove,         // Move quietly (2 tiles per 3 AP)
    TakeCover,          // Enter/switch cover side (1 AP)
    ShootSingle,        // Single shot (3 AP)
    ShootBurst,         // Burst / suppression fire (4 AP)
    Reload,             // Reload weapon (2 AP)
    Aim,                // +2 to hit, stackable to +6 (1 AP each)
    Overwatch,          // Set overwatch trigger (2 AP)
    Hide,               // Enter stealth if eligible (2 AP)
    MeleeAttack,        // Melee strike (2-3 AP depending on weapon)
    UseConsumable,      // Bandage, stim, etc. (2 AP typically)
    UseDiscipline,      // Vampire power (variable AP + BR)
    SwapWeapon,         // Switch primary/secondary (1 AP)
    Interact,           // Open door, use terminal, etc. (1-2 AP)
    Wait,               // End turn early
};

// ---------------------------------------------------------------------------
// Attack result
// ---------------------------------------------------------------------------
struct AttackResult
{
    bool    hit             = false;
    bool    critSuccess     = false;
    bool    critFailure     = false;
    int     roll            = 0;
    int     targetNumber    = 0;
    int     damageDealt     = 0;
    bool    targetKilled    = false;
    bool    causedBleeding  = false;
    bool    causedStun      = false;
    bool    causedCripple   = false;
    bool    weaponJammed    = false;
};

// ---------------------------------------------------------------------------
// Overwatch entry (a character watching a zone)
// ---------------------------------------------------------------------------
struct OverwatchEntry
{
    Character*  watcher     = nullptr;
    int         dirX        = 0;    // General facing direction
    int         dirY        = 0;
    bool        active      = true;
    bool        fired       = false; // Only fires once per enemy move
};

// ---------------------------------------------------------------------------
// CombatSystem — manages tactical combat resolution
// ---------------------------------------------------------------------------
class CombatSystem
{
public:
    // --- Turn Management ---
    void BeginCombat(GameWorld& world, const std::vector<Character*>& participants);
    void EndCombat();
    std::vector<Character*> RollInitiative(std::vector<Character*>& participants);
    void BeginTurn(Character& c);
    void EndTurn(Character& c);

    // --- Movement ---
    bool MoveCharacter(GameWorld& world, Character& c, int destX, int destY);
    bool CrouchMove(GameWorld& world, Character& c, int destX, int destY);
    void TakeCover(Character& c);

    // --- Ranged Combat ---
    AttackResult ShootSingle(GameWorld& world, Character& attacker, Character& target,
                             int aimBonus = 0);
    AttackResult ShootBurst(GameWorld& world, Character& attacker, Character& target);
    bool Reload(Character& c);
    int  AddAim(Character& c);  // Returns new total aim bonus

    // --- Melee Combat ---
    AttackResult MeleeAttack(GameWorld& world, Character& attacker, Character& target);

    // --- Overwatch ---
    void SetOverwatch(Character& c, int dirX, int dirY);
    void CheckOverwatch(GameWorld& world, Character& mover, int newX, int newY);
    void ClearOverwatch(Character& c);

    // --- Stealth ---
    bool AttemptHide(GameWorld& world, Character& c);
    bool DetectHidden(const Character& observer, const Character& hidden,
                      const GameMap& map);

    // --- Suppression ---
    void ApplySuppression(Character& target);

    // --- Consumables ---
    bool UseConsumable(Character& c, ConsumableType type);

    // --- Utility ---
    int  CalculateHitTarget(const Character& attacker, const Character& target,
                            const GameWorld& world, int aimBonus, bool isBurst);
    int  CalculateDamage(const WeaponData& weapon, bool critHit, int meleeBonusDmg = 0);
    void ApplyWoundEffects(Character& target, int damage, bool critHit);

    // --- State ---
    bool IsCombatActive() const { return m_combatActive; }

private:
    bool                            m_combatActive = false;
    std::vector<OverwatchEntry>     m_overwatchers;
    std::vector<Character*>         m_turnOrder;
    int                             m_currentTurnIndex = 0;

    // Per-character state during combat
    struct CharacterCombatState
    {
        Character*  character = nullptr;
        int         aimBonus  = 0;
    };
    std::vector<CharacterCombatState> m_combatStates;

    CharacterCombatState* GetCombatState(Character& c);
};

} // namespace vamp
