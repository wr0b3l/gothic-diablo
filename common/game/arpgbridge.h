#pragma once

#include <Tempest/Point>

class World;
class Npc;
class Interactive;
class TriggerEvent;

// Diathic (decisions 2026-11-09, 2026-11-10): Irdorath's big bridge - two buttons on the ground on the left and the
// right of the hero (the side of each original target), made the first time he comes near the targets; a button does
// what an arrow on its target does (the same trigger, or the key of the CodeMaster when no trigger is found), so
// Gothic's own mechanism lowers the bridge after both. Outside the saved vob tree; only in the ARPG view.
// The targets, the triggers, the CodeMaster, the places and the model go to log.txt ("Diathic: most").
namespace ArpgBridge {
  void tick(World& w, Npc& hero);                // PlayerControl::tickMove: the targets once per world, the buttons
  bool onEvent(World& w, const TriggerEvent& e); // World::execTriggerEvent: a button pressed (true = handled)
  bool isButton(const Interactive& m);           // the label "Przycisk mostu"
  // Shift + LPM with a bow on the near bank: the target on the side of the click (nullptr = not here)
  Interactive* aimTarget(World& w, const Tempest::Vec3& hero, const Tempest::Vec3& aim);
  }
