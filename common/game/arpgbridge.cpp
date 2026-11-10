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

namespace {
  constexpr std::string_view WorldName = "dragonisland.zen";
  // the two original targets across the chasm
  constexpr std::string_view Target[2]     = {"MOBNAME_SWITCH_BIGBRIDGE_01", "MOBNAME_SWITCH_BIGBRIDGE_02"};
  constexpr std::string_view Prefix        = "DIATHIC_BRIDGE_BUTTON_";
  constexpr std::string_view PressEvent[2] = {"DIATHIC_BRIDGE_PRESS_1", "DIATHIC_BRIDGE_PRESS_2"};

  // 2026-11-10 (the user: "the buttons low enough to aim at them"): the buttons stand on the ground on the left
  // and the right of the hero, the first time he comes this near to the targets - no guessing of the banks and walls
  constexpr float SearchRadius = 300000.f; // cm: the targets anywhere on the island
  constexpr float SpawnDist    = 4500.f;   // cm (horizontal) from the middle of the targets
  constexpr float SideOffset   = 250.f;    // cm from the hero to each button
  constexpr float SideMin      = 120.f;    // cm: the nearest a button may be when a wall is close
  constexpr float WallGap      = 60.f;     // cm in front of a wall in the way
  constexpr float GroundUp     = 150.f;    // cm: the ground is looked for from this high above the hero
  constexpr float GroundDown   = 600.f;    // cm: and this deep
  constexpr float TriggerNear  = 500.f;    // cm: the trigger box an arrow sets off is this close to its target
  constexpr float MasterNear   = 6000.f;   // cm: the CodeMaster of the bridge is this close to the targets
  constexpr bool  FaceFlip     = false;    // the model's front = its local Z towards the hero (flip after the test if wrong)
  // the model: a switch already used on the island (its animation surely exists), floor ones first, else these
  constexpr std::string_view Models[] = {"LEVER_1_OC.MDS", "TOUCHPLATE_STONE.MDS"};

  std::string str(const Vec3& v) {
    return "(" + std::to_string(int(v.x)) + ", " + std::to_string(int(v.y)) + ", " + std::to_string(int(v.z)) + ")";
    }

  Vec3 flat(Vec3 v) {
    v.y = 0;
    const float l = v.length();
    return l>0.001f ? v/l : Vec3(1,0,0);
    }

  float boxDist(const Vec3& p, const Vec3& c, const Vec3& h) {
    const Vec3 d = {std::max(std::abs(p.x-c.x)-h.x, 0.f), std::max(std::abs(p.y-c.y)-h.y, 0.f), std::max(std::abs(p.z-c.z)-h.z, 0.f)};
    return d.length();
    }

  std::string pickModel(World& w, const Vec3& at) {
    std::string lever, turn, plate, any;
    w.forEachInteractiveNear(at, SearchRadius, [&](Interactive& m) {
      if(!m.arpgIsSwitch() || m.arpgVisual().empty() || ArpgBridge::isButton(m))
        return;
      const std::string_view sch = m.schemeName();
      auto& slot = sch=="LEVER" ? lever : sch=="TURNSWITCH" ? turn : sch=="TOUCHPLATE" ? plate : any;
      if(slot.empty())
        slot = m.arpgVisual();
      });
    for(auto* s:{&lever, &turn, &plate, &any})
      if(!s->empty())
        return *s;
    return std::string(Models[0]);
    }

  void init(World& w, const Vec3& from) {
    auto& st = w.arpgBridge();

    bool has[2] = {};
    w.forEachInteractiveNear(from, SearchRadius, [&](Interactive& m) {
      for(int i=0; i<2; ++i)
        if(m.focusName()==Target[i]) {
          st.tg[i] = m.position();
          has[i]   = true;
          }
      });
    if(!has[0] || !has[1]) {
      Log::i("Diathic: most - nie znaleziono tarcz (01: ", has[0] ? "jest" : "brak", ", 02: ", has[1] ? "jest" : "brak", ")");
      return;
      }
    st.found = true;

    // the trigger an arrow sets off: its box at (or nearest to) the target; the CodeMaster nearest to the targets
    float best[2] = {TriggerNear, TriggerNear};
    float bestCm  = MasterNear;
    const Vec3 mid = (st.tg[0]+st.tg[1])*0.5f;
    for(size_t i=0; i<w.arpgTriggerCount(); ++i) {
      auto& t = w.arpgTrigger(i);
      if(auto* cm = dynamic_cast<CodeMaster*>(&t)) {
        const float d = (t.position()-mid).length();
        if(cm->arpgSlaves().size()>=2 && d<bestCm) {
          bestCm    = d;
          st.master = &t;
          }
        continue;
        }
      if(!t.arpgArrowTrigger())
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

    for(int k=0; k<2; ++k)
      Log::i("Diathic: most - tarcza ", Target[k], " ", str(st.tg[k]), ", wyzwalacz: ",
             st.touch[k]!=nullptr ? std::string(st.touch[k]->name()) + " (" + std::to_string(int(best[k])) + " cm)" : std::string("brak"));
    if(auto* cm = dynamic_cast<CodeMaster*>(st.master)) {
      std::string sl;
      for(auto& s:cm->arpgSlaves())
        sl += (sl.empty() ? "" : ", ") + s;
      Log::i("Diathic: most - CodeMaster ", cm->name(), " ", str(cm->position()), ", klucze: ", sl);
      } else {
      Log::i("Diathic: most - brak CodeMaster w ", int(MasterNear/100.f), " m od tarcz");
      }
    }

  Vec3 placeAt(World& w, const Vec3& hero, const Vec3& dir) {
    // sideways from the hero, not into a wall, on the ground
    const Vec3 chest = hero + Vec3(0, 100.f, 0);
    float off = SideOffset;
    const auto r = w.physic()->ray(chest, chest+dir*SideOffset);
    if(r.hasCol)
      off = std::max((r.v-chest).length()-WallGap, SideMin);
    Vec3 p = hero + dir*off;
    const Vec3 top = {p.x, hero.y+GroundUp, p.z};
    const auto g   = w.physic()->ray(top, top-Vec3(0, GroundUp+GroundDown, 0));
    p.y = g.hasCol ? g.v.y : hero.y;
    return p;
    }

  void makeButton(World& w, int side, const Vec3& pos, const Vec3& face, const std::string& model) {
    zenkit::VSwitch v{};
    v.type        = zenkit::VirtualObjectType::oCMobSwitch;
    v.position    = zenkit::Vec3(pos.x, pos.y, pos.z);
    const Vec3 f  = FaceFlip ? Vec3(-face.x, 0, -face.z) : Vec3(face.x, 0, face.z);
    v.rotation.columns[0] = zenkit::Vec3(f.z, 0.f, -f.x); // right = up x forward
    v.rotation.columns[1] = zenkit::Vec3(0.f, 1.f, 0.f);
    v.rotation.columns[2] = zenkit::Vec3(f.x, 0.f, f.z);
    v.bbox.min    = zenkit::Vec3(pos.x-50.f, pos.y,       pos.z-50.f);
    v.bbox.max    = zenkit::Vec3(pos.x+50.f, pos.y+150.f, pos.z+50.f);
    v.show_visual = true;
    v.cd_static   = false;
    v.cd_dynamic  = true;
    v.vob_name    = std::string(Prefix) + std::to_string(side+1);
    v.target      = std::string(PressEvent[side]);
    v.state_count = 1;
    v.rewind      = true;
    auto vis  = std::make_shared<zenkit::VisualModel>();
    vis->type = zenkit::VisualType::MODEL;
    vis->name = model;
    v.visual  = vis;

    auto p = Vob::load(nullptr, w, v, Vob::None);
    if(p==nullptr)
      return;
    if(auto* it = dynamic_cast<Interactive*>(p.get()))
      it->postValidate(); // the first pose, as after loading the world
    w.arpgAddVob(std::move(p));
    }

  void spawn(World& w, const Vec3& hero) {
    auto& st = w.arpgBridge();
    st.spawned = true;

    const Vec3 mid  = (st.tg[0]+st.tg[1])*0.5f;
    const Vec3 side = flat(st.tg[1]-st.tg[0]);
    const std::string model = pickModel(w, mid);
    for(int k=0; k<2; ++k) {
      // on the side of target k
      const Vec3 dir = Vec3::dotProduct(st.tg[k]-mid, side)>=0.f ? side : side*-1.f;
      const Vec3 pos = placeAt(w, hero, dir);
      makeButton(w, k, pos, flat(hero-pos), model);
      Log::i("Diathic: most - przycisk ", k+1, " ", str(pos), " (bohater ", str(hero), "), model ", model);
      }
    Gothic::inst().onPrint(ArpgMenu::gtxt("Most: obok Ciebie stoją dwa przyciski, po lewej i prawej - naciśnij oba"));
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
  if(!st.found || st.spawned)
    return;
  const Vec3  p   = hero.position();
  const Vec3  mid = (st.tg[0]+st.tg[1])*0.5f;
  const float dx  = p.x-mid.x, dz = p.z-mid.z;
  if(dx*dx+dz*dz<SpawnDist*SpawnDist && !hero.isInAir())
    spawn(w, p);
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
