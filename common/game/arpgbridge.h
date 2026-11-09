#pragma once

class World;
class Npc;
class Interactive;
class TriggerEvent;

// Diathic (decision 2026-11-09): Irdorath's big bridge - two buttons on the walls in front of it (on the side of each
// original target) do what an arrow on that target does: they set off the same trigger, so Gothic's own mechanism
// lowers the bridge after both. Made on every load of the world, outside the saved vob tree; only in the ARPG view.
// The places, the triggers and the model go to log.txt ("Diathic: most").
namespace ArpgBridge {
  void tick(World& w, Npc& hero);                // PlayerControl::tickMove: the buttons once per world, the hint
  bool onEvent(World& w, const TriggerEvent& e); // World::execTriggerEvent: a button pressed (true = handled)
  bool isButton(const Interactive& m);           // the label "Przycisk mostu"
  }
