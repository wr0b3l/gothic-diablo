#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include "game/arpgstatus.h"

class Interactive;
class Item;
class Npc;
class World;

// Diathic (2026-11-04): weapons with special properties (statuses on a hit: Podpalenie, Zamrożenie, Krwawienie,
// Zatrucie, Osłabienie), the exact stats of a weapon (the card in the book) and the workshop at the smith stations:
// modifications of a weapon (recipes: Gothic's materials). Only the hero's weapons, only in the ARPG view.
namespace ArpgWeapon {
  // a property: the status a hit puts on the target, chance in % of the hits that hurt
  struct Effect {
    ArpgStatus::Id status = ArpgStatus::Count;
    uint8_t        chance = 0;
    };
  constexpr int MaxEffects = 2;   // its own (by the weapon) + one from the workshop
  struct Effects {
    Effect e[MaxEffects];
    int    n = 0;
    };

  // what a status from a weapon does: how long; damage over time = part of the hit (the life it took) a second
  struct StatusCfg {
    uint64_t ms;
    float    perSec;
    int32_t  minPerSec;
    };
  const StatusCfg& cfg(ArpgStatus::Id id);
  constexpr uint64_t BurnMs   = 4000;  constexpr float BurnPerSec   = 0.15f;  // Podpalenie
  constexpr uint64_t BleedMs  = 4000;  constexpr float BleedPerSec  = 0.12f;  // Krwawienie
  constexpr uint64_t PoisonMs = 6000;  constexpr float PoisonPerSec = 0.08f;  // Zatrucie
  constexpr uint64_t FreezeMs = 1500;                                         // Zamrożenie (then ArpgStatus::FreezeImmuneMs)
  constexpr uint64_t WeakenMs = 6000;                                         // Osłabienie (ArpgStatus::WeakenCut)
  constexpr int32_t  DotMin   = 2;     // damage over time: at least this a second

  Effects effectsOf(World& w, const Item& it);   // its own (by the instance) + the modification
  // the hero's hit (a swing or a shot) took 'lost' life: the weapon's properties are rolled
  void    onHit(Npc& hero, Npc& victim, const Item& weapon, int32_t lost);
  std::string describe(const Effect& e);         // UTF-8: "Krwawienie: 25% ciosów, 12% ciosu co sekundę przez 4 s"

  // the damage of the weapon by kind (Gothic: damage[i] + damage_total for the kinds of damage_type)
  int32_t damageOf(const Item& it, size_t kind);
  const char* damageName(size_t kind);           // UTF-8: "sieczne", "obuchowe", ...
  // the hero (a human, the ARPG mode): his damage = the weapon in the hand (nullptr: none), not Gothic's running sum
  // of + / - (a rest of -225 made the swings of chapter 4 weak, 2026-11-05); false: not the hero, Gothic's way
  bool    syncHeroDamage(Npc& owner, const Item* inHand);

  // ---- the workshop (the smith stations) ----
  struct Need {
    const char* item;      // instance
    int32_t     count;
    };
  enum Fits : uint8_t {
    AnyMelee,
    Edge,      // DAM_EDGE: swords, axes
    Blunt,     // DAM_BLUNT: maces, hammers, staffs
    };
  struct Mod {
    const char*    name;   // UTF-8; on the weapon as the line ModPrefix + name (its TEXT[1])
    const char*    what;   // UTF-8: one line about it
    ArpgStatus::Id status; // Count: none
    uint8_t        chance;
    float          dmgMul; // the weapon's damage x this ...
    int32_t        dmgMin; // ... at least + this (0: no damage)
    Fits           fits;
    Need           need[3];
    };
  constexpr const char* ModPrefix = "Ulepszenie: ";
  size_t      modCount();
  const Mod&  mod(size_t i);
  int         modOf(const Item& it);                 // the modification on it, -1: none
  bool        modFits(const Mod& m, const Item& it);
  // what stops it (UTF-8), empty: it can be done
  std::string modBlocker(Npc& hero, const Item& it, size_t mod);
  // done: the materials taken, the weapon changed (the old modification replaced); false + msg: not done
  bool        modApply(Npc& hero, Item& it, size_t mod, std::string& msg);

  // the smith stations (an anvil BSANVIL* without hot steel, a grindstone BSSHARP*): the workshop opens instead of
  // Gothic's use (by the model's name: Interactive::schemeName logs mobs without a model)
  enum Station : uint8_t { NoStation, Anvil, Grindstone };
  Station     stationOf(const Interactive& m);
  bool        workshopNow(const Interactive& m, Npc& hero); // a station and not Gothic's forging (hot steel)
  void        requestWorkshop();
  bool        takeWorkshopRequest();
  }
