#pragma once

#include <Tempest/Point>

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

class Npc;
class World;

// Diathic: bosses of the ARPG view — the dragons and Raven (decisions 2026-10-31, docs/plan/decyzje.md "Bossowie").
// Gothic's fight AI, dialogs and story stay; on top: 3 phases by life, attacks with a warning on the ground,
// minions. Only in the ARPG view (ArpgBosses::tick is called from PlayerControl::tickMove there).
namespace ArpgBossCfg {
  // phases: life below this part of the maximum
  constexpr float    Phase2        = 0.70f;  // Raven
  constexpr float    Phase3        = 0.35f;
  constexpr float    DragonPhase2  = 0.66f;  // the dragons (decision 2026-11-04): minions and a flight at both
  constexpr float    DragonPhase3  = 0.33f;
  constexpr float    DragonHpMul   = 1.3f;   // the dragons' life (decision 2026-11-04)
  constexpr float    DragonStagger = 0.7f;   // a dragon's hit after a parry: x4 of Zachwianie x this (2026-11-04)
  constexpr uint64_t PhaseShieldMs = 1500;   // the roar of a new phase: no damage taken
  constexpr float    Range         = 4000.f; // cm: bosses further from the hero do nothing extra
  constexpr uint64_t ActiveMs      = 5000;   // still "in the fight" this long after Gothic's attack state ends

  // attacks with a warning (damage = part of the hero's maximum life, the armour does not count: dodge them)
  constexpr uint64_t WarnMs        = 1600;   // the red shape on the ground before the hit (decision 2026-11-03: was 1.3 s)
  constexpr uint64_t AttackMs1     = 16000;  // one attack every ... in phase 1 (2026-11-04: was 13 s, before 10 s)
  constexpr uint64_t AttackMs2     = 13000;  // ... in phases 2 and 3 (2026-11-04: was 10 s, before 7 s)
  constexpr uint64_t GapMs         = 3000;   // one attack at a time: after a warning attack hits, nothing else this long
  constexpr float    HeroR         = 30.f;   // the hero's body for the hit test, cm
  constexpr float    SoundRange    = 6000.f; // cm: the warning and hit sounds

  // Gothic's own attacks of the bosses (decisions 2026-11-03)
  constexpr uint64_t RestMs        = 2500;   // a dragon stands this long after its swing / breath (2026-11-04: was 1.5 s)
  constexpr uint64_t SpellRestMs   = 5000;   // ... after its spell (the fireball, the ball of death)
  constexpr uint64_t RavenRestMs   = 600;    // Raven: after a swing
  constexpr float    MaxHit        = 0.35f;  // one hit of a boss takes at most this part of the hero's maximum life
  constexpr float    MeleeHalfDeg  = 35.f;   // the sign of a dragon's own attack: a cone in front of it
  constexpr float    MeleeSignPart = 0.55f;  // ... shown for this part of the attack animation (before the hit)

  // Pandrodor: poison puddles
  constexpr float    PuddleR       = 250.f;
  constexpr float    PuddleHit     = 0.10f;
  constexpr uint64_t PuddleMs      = 6000;   // the puddle stays
  constexpr float    PoisonDps     = 0.03f;  // Zatrucie: per second, of the maximum life
  constexpr uint64_t PoisonMs      = 3000;
  // Pedrakhan: falling boulders
  constexpr float    RockR         = 220.f;
  constexpr float    RockHit       = 0.25f;
  // Feomathar: the fire breath (cone), fire patches in phase 3
  constexpr float    BreathLen     = 1000.f;
  constexpr float    BreathHalfDeg = 30.f;
  constexpr float    BreathHit     = 0.30f;
  constexpr float    BurnDps       = 0.03f;
  constexpr uint64_t BurnMs        = 4000;
  constexpr float    PatchR        = 200.f;
  constexpr uint64_t PatchMs       = 6000;
  // Finkregh: the ice breath (line), Zmrożenie = the hero only walks
  constexpr float    IceLen        = 1400.f;
  constexpr float    IceHalfW      = 150.f;
  constexpr float    IceHit        = 0.25f;
  constexpr uint64_t ChillMs       = 3000;
  constexpr float    IceFanDeg     = 20.f;   // phases 2 and 3: three lines
  // the undead dragon and Raven (phase 3): a wave running out in a ring
  constexpr float    WaveStartR    = 300.f;
  constexpr float    WaveMaxR      = 1800.f;
  constexpr float    WaveSpeed     = 700.f;  // cm/s
  constexpr float    WaveBand      = 120.f;
  constexpr float    WaveHit       = 0.25f;
  constexpr float    RavenWaveMaxR = 1200.f;
  constexpr float    RavenWaveHit  = 0.20f;
  // Raven: Beliar's lightning (line)
  constexpr float    BoltLen       = 1200.f;
  constexpr float    BoltHalfW     = 110.f;
  constexpr float    BoltHit       = 0.25f;
  constexpr uint64_t BoltWarnMs    = 1000;
  // minions: how many, how far from the boss
  constexpr int      Minions       = 2;
  constexpr int      Skeletons     = 3;
  constexpr float    MinionDist    = 500.f;
  constexpr float    MinionDmg     = 0.5f;   // their hits on the hero x this (decision 2026-11-04)

  // a dragon takes less (decision 2026-11-04): a part of the damage taken away
  constexpr float    ResistRanged  = 0.25f;
  constexpr float    ResistMelee   = 0.15f;
  constexpr float    ResistSpell   = 0.50f;

  // a dragon flies over its arena while its minions live, at most FlyMaxMs (decision 2026-11-04);
  // in the air only bows, crossbows and spells reach it
  constexpr uint64_t FlyMaxMs      = 5*60*1000;
  constexpr uint64_t TakeOffMs     = 2500;
  constexpr uint64_t LandMs        = 3000;
  constexpr float    FlyHeight     = 750.f;  // cm above the ground (less under a low ceiling: 500 cm free above)
  constexpr float    FlyMinHeight  = 350.f;
  constexpr float    FlyRadius     = 800.f;  // a circle over the arena (smaller between walls: 600 cm free around)
  constexpr float    FlySpeed      = 300.f;  // cm/s along the circle
  constexpr uint64_t FlapMs        = 1100;   // one beat of the wings
  constexpr float    FlapDeg       = 32.f;   // wings up / down in the air (take-off and landing: x1.4)
  constexpr float    FlyBob        = 45.f;   // cm: the body rises with each beat
  constexpr float    BankDeg       = 12.f;   // leaning into the circle
  // the camera in a fight with a dragon: further away, the whole dragon in the picture (2026-11-04)
  constexpr float    CamZoom       = 1.35f;
  constexpr float    CamFlyZoom    = 1.6f;
  }

class ArpgBoss final {
  public:
    enum Kind : uint8_t { Swamp, Rock, Fire, Ice, Undead, Raven };
    enum Shape : uint8_t { Circle, Cone, Line, Ring };
    enum Look : uint8_t { Plain, Poison, Flame, Frost, Dark, Bolt };

    // a shape on the ground: a red warning until `fire`, then the hit; puddles and patches stay until `end`;
    // a ring runs out from `pos` (radius r -> r2) between `fire` and `end`
    struct Zone {
      Shape         shape  = Circle;
      Look          look   = Plain;
      Tempest::Vec3 pos    = {};
      float         dirX   = 1, dirZ = 0; // cone, line: direction
      float         r      = 0;           // radius, length; ring: start radius
      float         r2     = 0;           // ring: end radius
      float         w      = 0;           // cone: half angle (deg), line: half width, ring: band
      float         hit    = 0;           // damage: part of the hero's maximum life
      uint64_t      start  = 0;
      uint64_t      fire   = 0;
      uint64_t      end    = 0;           // = fire: one hit
      bool          fired  = false;
      bool          struck = false;       // ring: the hero was hit already
      uint64_t      next   = 0;           // puddles, patches: the next touch
      bool          sign   = false;       // only the picture (a dragon's own attack): no hit, no effects
      };

    explicit ArpgBoss(Kind k, const Npc& npc);

    static bool  detect(World& w, const Npc& npc, Kind& out);

    Kind                     kind() const { return k; }
    // the life marks of the phases on the bar (the dragons 66 / 33%, Raven 70 / 35%)
    float                    phase2() const { return k==Raven ? ArpgBossCfg::Phase2 : ArpgBossCfg::DragonPhase2; }
    float                    phase3() const { return k==Raven ? ArpgBossCfg::Phase3 : ArpgBossCfg::DragonPhase3; }
    int                      phase() const { return ph; }
    bool                     shielded(uint64_t now) const { return now<shieldUntil; }
    bool                     active(uint64_t now) const { return now<activeUntil; }
    const std::vector<Zone>& zones() const { return zn; }
    // the name of the strong attack being prepared (for the line under the boss bar), empty when none
    std::string_view         ability(uint64_t now) const { return now<abilityUntil ? abilityName : std::string_view(); }
    uint64_t                 abilityEnd() const { return abilityUntil; }
    // the zone progress for the picture: 0..1 of the warning, then of the hit / the stay
    static float             progress(const Zone& z, uint64_t now);
    static float             ringRadius(const Zone& z, uint64_t now);

    void tick(Npc& boss, Npc& hero, World& w);
    // Gothic's own attack of the boss starts (Npc::implAiTick, MV_ATTACK): the pause after it, the sign and the sound
    uint64_t onOwnAttack(Npc& boss, World& w, uint64_t aniMs);
    void     onOwnSpell (Npc& boss, World& w);
    // one attack at a time: Gothic's own attack waits for the warning attack (and the rest after the last one)
    bool     canOwnAttack(uint64_t now) const { return fly==Ground && now>=busyUntil; }

    // the flight (decision 2026-11-04): Npc::tick hands the dragon over while it flies (no AI, no gravity)
    bool     flying() const { return fly!=Ground; }
    bool     tickFlight(Npc& boss, World& w);
    float    flyPitch() const { return pitch; } // degrees: the nose up
    float    flyRoll()  const { return roll;  } // degrees: leaning into the circle
    void     hintAir(World& w) const;           // the line "in the air: bows, crossbows, spells" (not too often)

  private:
    enum Fly : uint8_t { Ground, Wait, TakeOff, Air, Landing };

    Kind              k;
    int               ph          = 1;
    uint64_t          shieldUntil = 0;
    uint64_t          activeUntil = 0;
    uint64_t          nextAttack  = 0;
    uint32_t          attacks     = 0;
    std::vector<Zone> zn;
    std::string_view  abilityName;
    uint64_t          abilityUntil = 0;
    uint64_t          lastSoundAt  = 0;
    uint64_t          busyUntil    = 0;

    Fly               fly          = Ground;
    uint64_t          flyT0        = 0;       // the current part of the flight started
    uint64_t          flyStart     = 0;       // the take-off: FlyMaxMs from it
    uint64_t          flyLast      = 0;       // the last tickFlight
    Tempest::Vec3     flyCenter    = {};      // the arena: where it took off (on the ground)
    Tempest::Vec3     flyFrom      = {};      // where the take-off / the landing started
    float             flyH         = 0, flyR = 0, flyAng = 0, flyDir = 1;
    float             pitch        = 0, roll = 0;
    mutable uint64_t  hintAt       = 0;
    bool              flyPending   = false;   // new minions came while it was landing: up again after it

    void        startFlight(Npc& boss, World& w, uint64_t at);
    void        land       (Npc& boss, World& w);
    int         minionsAlive(World& w) const;

    static int  phaseOf(Kind k, const Npc& npc);
    void        onPhase(Npc& boss, Npc& hero, World& w);
    void        attack (Npc& boss, Npc& hero, World& w, uint64_t now);
    void        resolve(Npc& boss, Npc& hero, World& w, uint64_t now);
    void        strike (Npc& boss, Npc& hero, World& w, const Zone& z);
    int         spawn  (Npc& boss, World& w, std::string_view inst, int count, std::string_view fx);
    void        announce(Npc& boss, World& w, std::string_view name, std::string_view sound, uint64_t until);
  };

namespace ArpgBosses {
  void tick(World& w, Npc& hero);               // every frame in the ARPG view
  Npc* shown(World& w, Npc& hero);              // the boss for the bar at the top (the nearest one in a fight)
  void effect(World& w, std::string_view name, const Tempest::Vec3& at); // Gothic's visual effect at a point
  void sound (World& w, std::string_view name, const Tempest::Vec3& at); // Gothic's sound (SFX instance) at a point
  }
