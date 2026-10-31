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
  constexpr float    Phase2        = 0.70f;
  constexpr float    Phase3        = 0.35f;
  constexpr uint64_t PhaseShieldMs = 1500;   // the roar of a new phase: no damage taken
  constexpr float    Range         = 4000.f; // cm: bosses further from the hero do nothing extra
  constexpr uint64_t ActiveMs      = 5000;   // still "in the fight" this long after Gothic's attack state ends

  // attacks with a warning (damage = part of the hero's maximum life, the armour does not count: dodge them)
  constexpr uint64_t WarnMs        = 1300;   // the red shape on the ground before the hit
  constexpr uint64_t AttackMs1     = 10000;  // one attack every ... in phase 1
  constexpr uint64_t AttackMs2     = 7000;   // ... in phases 2 and 3
  constexpr float    HeroR         = 30.f;   // the hero's body for the hit test, cm

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
      };

    explicit ArpgBoss(Kind k, const Npc& npc);

    static bool  detect(World& w, const Npc& npc, Kind& out);

    Kind                     kind() const { return k; }
    int                      phase() const { return ph; }
    bool                     shielded(uint64_t now) const { return now<shieldUntil; }
    bool                     active(uint64_t now) const { return now<activeUntil; }
    const std::vector<Zone>& zones() const { return zn; }
    // the zone progress for the picture: 0..1 of the warning, then of the hit / the stay
    static float             progress(const Zone& z, uint64_t now);
    static float             ringRadius(const Zone& z, uint64_t now);

    void tick(Npc& boss, Npc& hero, World& w);

  private:
    Kind              k;
    int               ph          = 1;
    uint64_t          shieldUntil = 0;
    uint64_t          activeUntil = 0;
    uint64_t          nextAttack  = 0;
    uint32_t          attacks     = 0;
    std::vector<Zone> zn;

    static int  phaseOf(const Npc& npc);
    void        onPhase(Npc& boss, Npc& hero, World& w);
    void        attack (Npc& boss, Npc& hero, World& w, uint64_t now);
    void        resolve(Npc& boss, Npc& hero, World& w, uint64_t now);
    void        strike (Npc& boss, Npc& hero, World& w, const Zone& z);
    void        spawn  (Npc& boss, World& w, std::string_view inst, int count, std::string_view fx);
  };

namespace ArpgBosses {
  void tick(World& w, Npc& hero);               // every frame in the ARPG view
  Npc* shown(World& w, Npc& hero);              // the boss for the bar at the top (the nearest one in a fight)
  void effect(World& w, std::string_view name, const Tempest::Vec3& at); // Gothic's visual effect at a point
  }
