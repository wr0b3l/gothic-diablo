#pragma once

#include <cstdint>
#include <cstddef>
#include <string_view>
#include <vector>

class World;
class Npc;
class Item;

// Diathic (2026-11-01): Gothic's teleport runes are one "Runa teleportacji": one entry in the backpack and on the
// bar, its use opens the map with the places of the runes the hero has (a rune picked up = its place unlocked)
namespace ArpgTeleport {
  struct Dest {
    std::string_view inst;      // the rune's instance, upper case
    std::string_view name;      // the place (Polish, UTF-8)
    std::string_view wp;        // the waypoint of Spell_Cast_* (AI_Teleport)
    std::string_view world;     // NEWWORLD.ZEN / OLDWORLD.ZEN
    std::string_view worldName; // for a place in another world
    };

  const std::vector<Dest>& dests();
  const Dest* destOf(World& w, const Item& itm);   // the place of a teleport rune (null: not one)

  struct Owned {
    const Dest* dest = nullptr;
    size_t      cls  = 0;      // the rune in the backpack
    };
  std::vector<Owned> owned(World& w, Npc& hero);  // the places the hero has the runes of, in the order of dests()
  bool    here(World& w, const Dest& d);          // in this world (the spell works only there)
  int32_t cost(World& w);                          // SPL_COST_TELEPORT (Gothic: 10)
  }
