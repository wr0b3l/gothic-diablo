#include "arpgice.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "game/arpgboss.h"
#include "game/arpgstatus.h"
#include "game/constants.h"
#include "world/objects/npc.h"
#include "world/world.h"

void ArpgIce::tick(World& w, Npc& hero) {
  if(hero.isDead())
    return;
  constexpr float range = 1500.f;
  const uint64_t now = w.tickCount();
  std::vector<Npc*> golems;
  w.detectNpc(hero.position(), range, [&](Npc& n) {
    if(n.guild()==GIL_ICEGOLEM && !n.isDead() && !n.isDown())
      golems.push_back(&n);
    });
  for(Npc* golem:golems) {
    Tempest::Vec3 aim;
    if(golem->arpgIceTakeBolt(now, aim)) {
      ArpgBosses::effect(w, "spellFX_Icebolt_COLLIDE", aim);
      const auto d = hero.position()-aim;
      if(std::abs(d.y)<180.f && d.x*d.x+d.z*d.z<120.f*120.f) {
        const int32_t hp = std::max(1, hero.attribute(ATR_HITPOINTSMAX));
        const float part = golem->arpgIsMinion() ? 0.04f : 0.08f;
        const int32_t hit = std::max<int32_t>(1, int32_t(std::lround(float(hp)*part)));
        if(hero.arpgDirectHit(*golem, hit))
          hero.arpgApplyStatus(ArpgStatus::Chill, 1200, 0.f, golem);
        }
      continue;
      }
    if(!golem->arpgIceBoltReady(now) || golem->target()!=&hero || golem->isAttackAnim() ||
       !golem->canSeeNpc(hero, true))
      continue;
    aim = hero.position();
    golem->arpgIceStartBolt(now, aim);
    // Cast and impact have separate effects; the hit stays at the position
    // chosen at cast time, rather than following a hero who dodges away.
    ArpgBosses::effect(w, "spellFX_Icebolt_COLLIDE", golem->position()+Tempest::Vec3(0.f, 140.f, 0.f));
    }
  }
