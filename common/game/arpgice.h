#pragma once

class Npc;
class World;

namespace ArpgIce {
  // Extra ice-golem attack in ARPG fights. The projectile aims at the hero's
  // position when cast, so moving away or dodging avoids the impact.
  void tick(World& w, Npc& hero);
  }
