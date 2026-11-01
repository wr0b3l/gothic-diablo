#pragma once

#include <cstdint>

class World;
class Npc;

// Diathic (2026-11-01): monsters stronger chapter by chapter, attacks with a red aura (no parry, a block costs
// 3 charges) and groups with an elite leader. Only in the ARPG view.
namespace ArpgElite {
  // life and damage of monsters (and the bosses) x ChapterMul for each chapter after the first, compounded
  constexpr float    ChapterMul     = 1.15f;

  // groups with an elite leader: monsters of one guild this close to each other (a chain) = one group
  constexpr float    PackRadius     = 1500.f; // cm
  constexpr int      PackMinSize    = 2;      // the leader and at least one more
  constexpr int      PackMaxSize    = 8;
  constexpr uint32_t PackChance     = 25;     // % of the groups with a leader
  constexpr float    LeaderHp       = 3.f;    // life of the leader
  constexpr float    LeaderDmg      = 1.5f;   // its damage
  constexpr float    LeaderScale    = 1.15f;  // a bigger model
  constexpr float    LeaderXp       = 3.f;    // Gothic's experience for the kill x this
  constexpr float    PackDmg        = 1.25f;  // damage of the others while their leader lives ...
  constexpr float    PackLeaderRange= 4000.f; // ... and is this close (cm)
  constexpr int32_t  XpPerLevel     = 10;     // Gothic's XP_PER_VICTORY: experience for a kill = level x this
  constexpr float    RollRange      = 6000.f; // cm around the hero: monsters get their group when he comes this close

  // attacks with a red aura: the aura first, the next attack after RedWarnMs is the red one
  constexpr uint32_t RedEveryMin    = 6000;   // ms between them in a fight with the hero
  constexpr uint32_t RedEveryMax    = 10000;
  constexpr uint32_t RedLeaderMin   = 4000;   // an elite leader: more often
  constexpr uint32_t RedLeaderMax   = 7000;
  constexpr uint64_t RedWarnMs      = 800;    // the aura shows at least this long before the red attack starts
  constexpr uint64_t RedHoldMs      = 4000;   // no attack this long with the aura on: it goes out
  constexpr int32_t  RedBlockCost   = 3;      // block charges a blocked red attack takes

  float chapterMul(World& w);                 // ChapterMul ^ (chapter-1)
  bool  redAttacker(World& w, const Npc& npc); // by kind: orc elite, lizard men, trolls, shadowbeasts, golems, demons
                                               // (elite leaders too: Npc::arpgRedCapable)
  void  tick(World& w, Npc& hero);            // every frame in the ARPG view: groups near the hero, leader's experience
  void  leaderKilled(int32_t xp);             // experience for the hero, given in the next tick
  }
