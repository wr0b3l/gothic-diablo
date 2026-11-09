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
#include "world/world.h"
#include "ui/arpgmenu.h"
#include "gothic.h"

using namespace Tempest;

namespace {
  constexpr std::string_view WorldName = "dragonisland.zen";
  // the two original targets across the chasm (PR #34, DRAGONISLAND.ZEN)
  constexpr std::string_view Target[2] = {"MOBNAME_SWITCH_BIGBRIDGE_01", "MOBNAME_SWITCH_BIGBRIDGE_02"};
  constexpr std::string_view Prefix    = "DIATHIC_BRIDGE_BUTTON_";
  constexpr std::string_view PressEvent[2]  = {"DIATHIC_BRIDGE_PRESS_1", "DIATHIC_BRIDGE_PRESS_2"};

  // the near bank in front of the bridge (PR #34: the hero there sees the hint); the side to put the buttons on
  const Vec3 NearBank = {-23500.f, 2200.f, 14500.f};
  constexpr float AreaMinX = -25000.f, AreaMaxX = -22000.f;
  constexpr float AreaMinY =   1700.f, AreaMaxY =   2700.f;
  constexpr float AreaMinZ =  13500.f, AreaMaxZ =  15500.f;

  // the places of the buttons, found from the targets: across the chasm, then sideways to the walls
  constexpr float TriggerNear  = 600.f;   // cm: the trigger an arrow sets off is this close to its target
  constexpr float MarchStep    = 100.f;   // cm: steps along the bridge looking for the chasm and the near bank
  constexpr float MarchMax     = 8000.f;  // cm: the farthest the near bank may be from the targets
  constexpr float RayDown      = 4000.f;  // cm: how deep the ground is looked for
  constexpr float ChasmDrop    = 600.f;   // cm: ground this much lower than at the targets (or none) = the chasm
  constexpr float IntoBank     = 300.f;   // cm from the edge of the chasm onto the near bank
  constexpr float ButtonHeight = 120.f;   // cm above the ground
  constexpr float WallMax      = 1500.f;  // cm: the farthest wall from the bridge axis
  constexpr float WallGap      = 15.f;    // cm in front of the wall
  constexpr float NoWallSide   = 400.f;   // cm from the axis when no wall is found
  constexpr bool  FaceFlip     = false;   // the model's front = its local Z away from the wall (flip after the test if wrong)
  // the model: a switch already used in this world (its animation surely exists), else these
  constexpr std::string_view Models[] = {"TOUCHPLATE_STONE.MDS", "LEVER_1_OC.MDS"};

  std::string str(const Vec3& v) {
    return "(" + std::to_string(int(v.x)) + ", " + std::to_string(int(v.y)) + ", " + std::to_string(int(v.z)) + ")";
    }

  Vec3 flat(Vec3 v) {
    v.y = 0;
    const float l = v.length();
    return l>0.001f ? v/l : Vec3(0,0,1);
    }

  bool groundAt(World& w, const Vec3& at, float y0, float& gy) {
    const Vec3 from = {at.x, y0+800.f, at.z}; // from above: the rays see only the outer side of the ground
    const auto r    = w.physic()->ray(from, from-Vec3(0, RayDown, 0));
    if(!r.hasCol)
      return false;
    gy = r.v.y;
    return true;
    }

  std::string pickModel(World& w) {
    std::string any, lever;
    w.forEachInteractiveNear(NearBank, 60000.f, [&](Interactive& m) {
      if(!m.arpgIsSwitch() || m.arpgVisual().empty() || ArpgBridge::isButton(m))
        return;
      const std::string_view sch = m.schemeName();
      if(sch=="TOUCHPLATE" && any.rfind("TOUCHPLATE",0)!=0)
        any = m.arpgVisual();
      else if(sch=="LEVER" && lever.empty())
        lever = m.arpgVisual();
      else if(any.empty())
        any = m.arpgVisual();
      });
    if(any.rfind("TOUCHPLATE",0)==0)
      return any;
    if(!lever.empty())
      return lever;
    if(!any.empty())
      return any;
    return std::string(Models[0]);
    }

  void makeButton(World& w, int side, const Vec3& pos, const Vec3& face, const std::string& model) {
    zenkit::VSwitch v{};
    v.type        = zenkit::VirtualObjectType::oCMobSwitch;
    v.position    = zenkit::Vec3(pos.x, pos.y, pos.z);
    const Vec3 f  = FaceFlip ? Vec3(-face.x, 0, -face.z) : Vec3(face.x, 0, face.z);
    v.rotation.columns[0] = zenkit::Vec3(f.z, 0.f, -f.x); // right = up x forward
    v.rotation.columns[1] = zenkit::Vec3(0.f, 1.f, 0.f);
    v.rotation.columns[2] = zenkit::Vec3(f.x, 0.f, f.z);
    v.bbox.min    = zenkit::Vec3(pos.x-50.f, pos.y-60.f, pos.z-50.f);
    v.bbox.max    = zenkit::Vec3(pos.x+50.f, pos.y+60.f, pos.z+50.f);
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

  void init(World& w) {
    auto& st = w.arpgBridge();

    // the targets
    Vec3 tg[2] = {};
    bool has[2] = {};
    w.forEachInteractiveNear(NearBank, 10000.f, [&](Interactive& m) {
      for(int i=0; i<2; ++i)
        if(m.focusName()==Target[i]) {
          tg[i]  = m.position();
          has[i] = true;
          }
      });
    if(!has[0] || !has[1]) {
      Log::i("Diathic: most - nie znaleziono tarcz (", has[0] ? "jest" : "brak", " 01, ", has[1] ? "jest" : "brak", " 02)");
      return;
      }

    // the triggers the arrows set off: the nearest to each target
    float best[2] = {TriggerNear, TriggerNear};
    for(size_t i=0; i<w.arpgTriggerCount(); ++i) {
      auto& t = w.arpgTrigger(i);
      if(!t.arpgArrowTrigger())
        continue;
      for(int k=0; k<2; ++k) {
        const float d = (t.arpgCenter()-tg[k]).length();
        if(d<best[k]) {
          best[k]     = d;
          st.touch[k] = &t;
          }
        }
      }
    for(int k=0; k<2; ++k)
      Log::i("Diathic: most - tarcza ", Target[k], " ", str(tg[k]), ", wyzwalacz: ",
             st.touch[k]!=nullptr ? std::string(st.touch[k]->name()) : std::string("brak"),
             st.touch[k]!=nullptr ? " (" + std::to_string(int(best[k])) + " cm)" : std::string());

    // the bridge axis: across the line of the targets, towards the near bank
    const Vec3 mid  = (tg[0]+tg[1])*0.5f;
    const Vec3 side = flat(tg[1]-tg[0]);
    Vec3       fwd  = Vec3(-side.z, 0, side.x);
    if(Vec3::dotProduct(NearBank-mid, fwd)<0.f)
      fwd = fwd*-1.f;

    // along it: the far bank, the chasm, the near bank
    float y0 = NearBank.y;
    float g  = 0;
    if(groundAt(w, mid, mid.y, g))
      y0 = g;
    const float floorY = std::min(y0, NearBank.y) - ChasmDrop; // lower than both banks = the chasm
    bool  chasm = false, found = false;
    Vec3  edge;
    float edgeY = y0;
    for(float t=MarchStep; t<=MarchMax; t+=MarchStep) {
      const Vec3 at    = mid + fwd*t;
      const bool solid = groundAt(w, at, std::max(y0, NearBank.y), g) && g>floorY;
      if(!solid) {
        chasm = true;
        continue;
        }
      if(chasm) {
        edge  = at;
        edgeY = g;
        found = true;
        break;
        }
      }
    if(!found) {
      Log::i("Diathic: most - nie znaleziono przepaści ani bliskiego brzegu od ", str(mid));
      return;
      }

    Vec3 base = edge + fwd*IntoBank;
    if(groundAt(w, base, edgeY, g))
      edgeY = g;
    base.y = edgeY + ButtonHeight;

    const std::string model = pickModel(w);
    for(int k=0; k<2; ++k) {
      // on the side of target k
      const Vec3 dir = Vec3::dotProduct(tg[k]-mid, side)>=0.f ? side : side*-1.f;
      const auto r   = w.physic()->ray(base, base+dir*WallMax);
      const Vec3 pos = r.hasCol ? r.v - dir*WallGap : base + dir*NoWallSide;
      makeButton(w, k, pos, dir*-1.f, model);
      Log::i("Diathic: most - przycisk ", k+1, " ", str(pos), r.hasCol ? " na scianie" : " bez sciany",
             ", model ", model);
      }
    }
  }

void ArpgBridge::tick(World& w, Npc& hero) {
  if(w.name()!=WorldName)
    return;
  auto& st = w.arpgBridge();
  if(!st.init) {
    st.init = true;
    init(w);
    }
  if(!st.hinted) {
    const Vec3 p = hero.position();
    if(p.x>AreaMinX && p.x<AreaMaxX && p.y>AreaMinY && p.y<AreaMaxY && p.z>AreaMinZ && p.z<AreaMaxZ) {
      Gothic::inst().onPrint(ArpgMenu::gtxt("Most: naciśnij oba przyciski na ścianach przed mostem (albo zestrzel obie tarcze)"));
      st.hinted = true;
      }
    }
  }

bool ArpgBridge::onEvent(World& w, const TriggerEvent& e) {
  for(int k=0; k<2; ++k) {
    if(e.target!=PressEvent[k])
      continue;
    if(e.type!=TriggerEvent::T_Trigger)
      return true;
    auto* t = w.arpgBridge().touch[k];
    Log::i("Diathic: most - przycisk ", k+1, t!=nullptr ? " -> wyzwalacz tarczy" : " bez wyzwalacza");
    if(t!=nullptr)
      t->arpgTouch(); // as an arrow on the target (AbstractTrigger::Cb::onCollide)
    return true;
    }
  return false;
  }

bool ArpgBridge::isButton(const Interactive& m) {
  return m.tag().rfind(Prefix, 0)==0;
  }
