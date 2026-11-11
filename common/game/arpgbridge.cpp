#include "arpgbridge.h"

#include <Tempest/Log>
#include <zenkit/vobs/MovableObject.hh>

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>
#include <string_view>

#include "physics/dynamicworld.h"
#include "world/objects/interactive.h"
#include "world/objects/npc.h"
#include "world/objects/vob.h"
#include "world/triggers/abstracttrigger.h"
#include "world/triggers/codemaster.h"
#include "world/world.h"
#include "ui/arpgmenu.h"
#include "gothic.h"

using namespace Tempest;

// DRAGONISLAND.ZEN (Worlds_Addon.vdf), read 2026-11-10: the targets are two gold nuggets (oCMOB) by their vob names
// MOBNAME_SWITCH_BIGBRIDGE_01 (-22744, 3069, 11963) and _02 (-24393, 3081, 11954) - their focus name is only
// MOBNAME_SWITCH; behind each a zCTrigger (ISLE_BIGBRIDGE_FAR_TRIGGER_02 / _01, touch by an object, once) goes to
// ISLE_BIGBRIDGE_FAR_TRIGGER_CODEMASTER_01, which opens the mover ISLE_BIGBRIDGE_01 (the bridge slides from z 13468
// to z 11801). The levers ISLE_BIGBRIDGE_SWITCH_01 / _02 across the chasm only move ISLE_BIGBRIDGE_BLIND (a decoy).
// The near bank: a ledge at y ~2150, x -24500..-22800, z 10200..11000, with a notch for the bridge at x -23900..-23300.
namespace {
  constexpr std::string_view WorldName = "dragonisland.zen";
  constexpr std::string_view Target[2]     = {"MOBNAME_SWITCH_BIGBRIDGE_01", "MOBNAME_SWITCH_BIGBRIDGE_02"};
  constexpr std::string_view Prefix        = "DIATHIC_BRIDGE_BUTTON_";
  constexpr std::string_view PressEvent[2] = {"DIATHIC_BRIDGE_PRESS_1", "DIATHIC_BRIDGE_PRESS_2"};

  // the buttons: on the ledge on both sides of the notch, 1 m from the edge (target 01 is on the right, 02 on the left)
  const Vec3 ButtonAt[2] = {{-23000.f, 2150.f, 10800.f}, {-24200.f, 2150.f, 10800.f}};
  constexpr std::string_view Model = "LEVER_1_OC.MDS"; // the levers of the bridge use it (the hero has its animation)
  constexpr float LeverBase  = 125.f;  // cm: the model's origin above its foot (ISLE_BIGBRIDGE_SWITCH: y 3359, bbox 3234)
  constexpr float GroundUp   = 300.f;  // cm: the ground is looked for from this high above ButtonAt
  constexpr float GroundDown = 1000.f; // cm: and this deep

  // the near bank: the hint and Shift + LPM at the targets
  constexpr float BankMinX = -24700.f, BankMaxX = -22200.f;
  constexpr float BankMinY =   1900.f, BankMaxY =   2600.f;
  constexpr float BankMinZ =   9800.f, BankMaxZ =  11100.f;

  constexpr float SearchRadius = 300000.f; // cm: the targets anywhere on the island
  constexpr float TriggerNear  = 500.f;    // cm: the trigger box an arrow sets off is this close to its target
  constexpr float MasterNear   = 6000.f;   // cm: the CodeMaster of the bridge is this close to the targets

  std::string str(const Vec3& v) {
    return "(" + std::to_string(int(v.x)) + ", " + std::to_string(int(v.y)) + ", " + std::to_string(int(v.z)) + ")";
    }

  bool onBank(const Vec3& p) {
    return p.x>BankMinX && p.x<BankMaxX && p.y>BankMinY && p.y<BankMaxY && p.z>BankMinZ && p.z<BankMaxZ;
    }

  float boxDist(const Vec3& p, const Vec3& c, const Vec3& h) {
    const Vec3 d = {std::max(std::abs(p.x-c.x)-h.x, 0.f), std::max(std::abs(p.y-c.y)-h.y, 0.f), std::max(std::abs(p.z-c.z)-h.z, 0.f)};
    return d.length();
    }

  Interactive* makeButton(World& w, int side, const Vec3& foot) {
    const Vec3 pos = foot + Vec3(0, LeverBase, 0);
    zenkit::VSwitch v{};
    v.type        = zenkit::VirtualObjectType::oCMobSwitch;
    v.position    = zenkit::Vec3(pos.x, pos.y, pos.z);
    // as ISLE_BIGBRIDGE_SWITCH_01 / _02: no rotation - the front (local +Z) to the chasm, the hero stands on the ledge side
    v.rotation.columns[0] = zenkit::Vec3(1.f, 0.f, 0.f);
    v.rotation.columns[1] = zenkit::Vec3(0.f, 1.f, 0.f);
    v.rotation.columns[2] = zenkit::Vec3(0.f, 0.f, 1.f);
    v.bbox.min    = zenkit::Vec3(pos.x-15.f, foot.y,      pos.z-30.f);
    v.bbox.max    = zenkit::Vec3(pos.x+15.f, pos.y+30.f,  pos.z+15.f);
    v.show_visual = true;
    v.cd_static   = false;
    v.cd_dynamic  = true;
    v.vob_name    = std::string(Prefix) + std::to_string(side+1);
    v.target      = std::string(PressEvent[side]);
    v.state_count = 1;
    v.rewind      = true;
    auto vis  = std::make_shared<zenkit::VisualModel>();
    vis->type = zenkit::VisualType::MODEL;
    vis->name = std::string(Model);
    v.visual  = vis;

    auto p = Vob::load(nullptr, w, v, Vob::None);
    if(p==nullptr) {
      Log::i("Diathic: most - przycisk ", side+1, " nie powstal");
      return nullptr;
      }
    auto* it = dynamic_cast<Interactive*>(p.get());
    if(it!=nullptr)
      it->postValidate(); // the first pose, as after loading the world
    w.arpgAddVob(std::move(p));
    Log::i("Diathic: most - przycisk ", side+1, " ", str(pos));
    return it;
    }

  void init(World& w, const Vec3& from) {
    auto& st = w.arpgBridge();

    // the targets, by their vob names
    w.forEachInteractiveNear(from, SearchRadius, [&](Interactive& m) {
      for(int i=0; i<2; ++i)
        if(m.tag()==Target[i]) {
          st.tg[i]     = m.position();
          st.target[i] = &m;
          }
      });
    if(st.target[0]==nullptr || st.target[1]==nullptr) {
      Log::i("Diathic: most - nie znaleziono tarcz (01: ", st.target[0] ? "jest" : "brak", ", 02: ", st.target[1] ? "jest" : "brak", ")");
      return;
      }
    st.found = true;

    // the trigger an arrow sets off: its box at (or nearest to) the target
    float best[2] = {TriggerNear, TriggerNear};
    for(size_t i=0; i<w.arpgTriggerCount(); ++i) {
      auto& t = w.arpgTrigger(i);
      if(dynamic_cast<CodeMaster*>(&t)!=nullptr || !t.arpgArrowTrigger())
        continue;
      for(int k=0; k<2; ++k) {
        const float d = boxDist(st.tg[k], t.arpgCenter(), t.arpgHalfSize());
        if(d<best[k]) {
          best[k]     = d;
          st.touch[k] = &t;
          }
        }
      }
    if(st.touch[0]!=nullptr && st.touch[0]==st.touch[1])
      st.touch[1] = nullptr; // one box by both targets: the second button goes by the CodeMaster

    // the CodeMaster: the one with these triggers as its keys, else the nearest to the targets
    const Vec3 mid = (st.tg[0]+st.tg[1])*0.5f;
    float bestCm = MasterNear;
    for(size_t i=0; i<w.arpgTriggerCount(); ++i) {
      auto* cm = dynamic_cast<CodeMaster*>(&w.arpgTrigger(i));
      if(cm==nullptr || cm->arpgSlaves().size()<2)
        continue;
      bool keyOf = false;
      for(auto& s:cm->arpgSlaves())
        for(auto* t:st.touch)
          if(t!=nullptr && t->name()==s)
            keyOf = true;
      const float d = keyOf ? 0.f : (cm->position()-mid).length();
      if(d<bestCm) {
        bestCm    = d;
        st.master = cm;
        }
      }

    for(int k=0; k<2; ++k)
      Log::i("Diathic: most - tarcza ", Target[k], " ", str(st.tg[k]), ", wyzwalacz: ",
             st.touch[k]!=nullptr ? std::string(st.touch[k]->name()) + " (" + std::to_string(int(best[k])) + " cm)" : std::string("brak"));
    if(auto* cm = dynamic_cast<CodeMaster*>(st.master)) {
      std::string sl;
      for(auto& s:cm->arpgSlaves())
        sl += (sl.empty() ? "" : ", ") + s;
      Log::i("Diathic: most - CodeMaster ", cm->name(), ", klucze: ", sl);
      }

    // the buttons, on the ground of the ledge
    for(int k=0; k<2; ++k) {
      Vec3 foot = ButtonAt[k];
      const Vec3 top = foot + Vec3(0, GroundUp, 0);
      const auto g   = w.physic()->ray(top, top-Vec3(0, GroundUp+GroundDown, 0));
      if(g.hasCol)
        foot.y = g.v.y;
      st.button[k] = makeButton(w, k, foot);
      }
    }
  }

void ArpgBridge::tick(World& w, Npc& hero) {
  if(w.name()!=WorldName)
    return;
  auto& st = w.arpgBridge();
  if(!st.init) {
    st.init = true;
    init(w, hero.position());
    }
  if(st.found && !st.hinted && onBank(hero.position())) {
    st.hinted = true;
    Gothic::inst().onPrint(ArpgMenu::gtxt("Most: naciśnij obie dźwignie po bokach przejścia (albo Shift + LPM łukiem w lewą i prawą tarczę)"));
    }
  }

Interactive* ArpgBridge::aimTarget(World& w, const Vec3& hero, const Vec3& aim) {
  if(w.name()!=WorldName || !onBank(hero))
    return nullptr;
  auto& st = w.arpgBridge();
  if(!st.found)
    return nullptr;
  // the target on the side of the click: by the line between the targets
  const Vec3  mid  = (st.tg[0]+st.tg[1])*0.5f;
  const Vec3  side = st.tg[1]-st.tg[0];
  const float s    = (aim.x-mid.x)*side.x + (aim.z-mid.z)*side.z;
  return st.target[s>=0.f ? 1 : 0];
  }

bool ArpgBridge::onEvent(World& w, const TriggerEvent& e) {
  for(int k=0; k<2; ++k) {
    if(e.target!=PressEvent[k])
      continue;
    if(e.type!=TriggerEvent::T_Trigger)
      return true;
    auto& st = w.arpgBridge();
    if(st.touch[k]!=nullptr) {
      Log::i("Diathic: most - przycisk ", k+1, " -> wyzwalacz ", st.touch[k]->name());
      st.touch[k]->arpgTouch(); // as an arrow on the target (AbstractTrigger::Cb::onCollide)
      return true;
      }
    auto* cm = dynamic_cast<CodeMaster*>(st.master);
    if(cm!=nullptr && size_t(k)<cm->arpgSlaves().size()) {
      // a key not taken by the other button's trigger
      const auto& sl  = cm->arpgSlaves();
      std::string key = sl[size_t(k)];
      const int other = 1-k;
      if(st.touch[other]!=nullptr && st.touch[other]->name()==key)
        key = sl[size_t(other)];
      Log::i("Diathic: most - przycisk ", k+1, " -> CodeMaster ", cm->name(), " klucz ", key);
      w.triggerEvent(TriggerEvent(std::string(cm->name()), key, TriggerEvent::T_Trigger));
      return true;
      }
    Log::i("Diathic: most - przycisk ", k+1, " bez wyzwalacza i CodeMaster");
    return true;
    }
  return false;
  }

bool ArpgBridge::isButton(const Interactive& m) {
  return m.tag().rfind(Prefix, 0)==0;
  }
