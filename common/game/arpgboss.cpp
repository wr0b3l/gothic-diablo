#include "arpgboss.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>
#include <string>

#include "game/arpgstatus.h"
#include "game/gamescript.h"
#include "graphics/effect.h"
#include "physics/dynamicworld.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "gothic.h"

using namespace ArpgBossCfg;
using Tempest::Vec3;

namespace {
std::mt19937& rng() {
  static std::mt19937 r{0xD1A7u};
  return r;
  }

float rnd(float a, float b) {
  return std::uniform_real_distribution<float>(a, b)(rng());
  }

float degToRad(float d) {
  return d*float(M_PI)/180.f;
  }

bool inside(const ArpgBoss::Zone& z, const Vec3& p) {
  if(std::abs(p.y-z.pos.y)>300.f)
    return false;
  const float dx = p.x-z.pos.x, dz = p.z-z.pos.z;
  switch(z.shape) {
    case ArpgBoss::Circle:
      return dx*dx+dz*dz <= (z.r+HeroR)*(z.r+HeroR);
    case ArpgBoss::Cone: {
      const float len = std::sqrt(dx*dx+dz*dz);
      if(len>z.r+HeroR)
        return false;
      if(len<150.f)
        return true; // at the mouth
      return (dx*z.dirX+dz*z.dirZ)/len >= std::cos(degToRad(z.w));
      }
    case ArpgBoss::Line: {
      const float t = dx*z.dirX+dz*z.dirZ;
      if(t<-HeroR || t>z.r+HeroR)
        return false;
      return std::abs(-dx*z.dirZ+dz*z.dirX) <= z.w+HeroR;
      }
    case ArpgBoss::Ring:
      return false; // tested as it runs out (ArpgBoss::resolve)
    }
  return false;
  }
}

ArpgBoss::ArpgBoss(Kind k, const Npc& npc)
  :k(k), ph(phaseOf(npc)) {
  // a game loaded during the fight: the phase from the life, without the roar and new minions
  }

bool ArpgBoss::detect(World& w, const Npc& npc, Kind& out) {
  auto* sym = w.script().findSymbol(size_t(npc.instanceSymbol()));
  if(sym==nullptr)
    return false;
  std::string n = sym->name();
  for(auto& c:n)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  static const struct { const char* name; Kind kind; } bosses[] = {
    {"DRAGON_SWAMP",         Swamp },  // Pandrodor
    {"DRAGON_ROCK",          Rock  },  // Pedrakhan
    {"DRAGON_FIRE",          Fire  },  // Feomathar (not Dragon_Fire_Island: Feodaron is immortal)
    {"DRAGON_ICE",           Ice   },  // Finkregh
    {"DRAGON_UNDEAD",        Undead},  // Irdorath
    {"BDT_1090_ADDON_RAVEN", Raven },  // Kruk
    };
  for(auto& b:bosses)
    if(n==b.name) {
      out = b.kind;
      return true;
      }
  return false;
  }

int ArpgBoss::phaseOf(const Npc& npc) {
  const int32_t mx = std::max(1, npc.attribute(ATR_HITPOINTSMAX));
  const float   v  = float(npc.attribute(ATR_HITPOINTS))/float(mx);
  if(v<Phase3)
    return 3;
  if(v<Phase2)
    return 2;
  return 1;
  }

float ArpgBoss::progress(const Zone& z, uint64_t now) {
  if(now<z.fire)
    return z.fire>z.start ? std::clamp(float(now-z.start)/float(z.fire-z.start), 0.f, 1.f) : 1.f;
  if(z.end>z.fire)
    return std::clamp(float(now-z.fire)/float(z.end-z.fire), 0.f, 1.f);
  return 1.f;
  }

float ArpgBoss::ringRadius(const Zone& z, uint64_t now) {
  if(now<=z.fire)
    return z.r;
  return std::min(z.r2, z.r + WaveSpeed*float(now-z.fire)/1000.f);
  }

void ArpgBoss::tick(Npc& boss, Npc& hero, World& w) {
  const uint64_t now = w.tickCount();
  if(boss.isDead()) {
    zn.clear();
    activeUntil = 0;
    return;
    }
  const auto d = boss.position()-hero.position();
  if(boss.isAttack() && boss.target()==&hero && d.x*d.x+d.z*d.z<Range*Range)
    activeUntil = now+ActiveMs;

  // a new phase (the life only goes down here: the dragons' regeneration does not take a phase back)
  const int p = phaseOf(boss);
  while(ph<p) {
    ph++;
    onPhase(boss, hero, w);
    }

  if(active(now) && !hero.isDead() && !boss.isDown() && now>=shieldUntil) {
    if(nextAttack==0)
      nextAttack = now+3000; // the first one a moment after the fight starts
    if(now>=nextAttack) {
      attack(boss, hero, w, now);
      nextAttack = now + (ph==1 ? AttackMs1 : AttackMs2);
      }
    }
  else if(!active(now)) {
    nextAttack = 0;
    }
  resolve(boss, hero, w, now);
  }

void ArpgBoss::onPhase(Npc& boss, Npc& hero, World& w) {
  (void)hero;
  const uint64_t now = w.tickCount();
  shieldUntil = now+PhaseShieldMs;
  nextAttack  = shieldUntil+500;
  if(k!=Raven)
    boss.playAnimByName("T_WARN", BS_NONE); // the roar (the sound is in the animation)
  ArpgBosses::effect(w, k==Raven ? "spellFX_BELIARSRAGE_COLLIDE" : "spellFX_Massdeath_EXPLOSION", boss.position());

  if(k==Raven && ph==2)
    spawn(boss, w, "STONEGUARDIAN", Minions, "spellFX_BELIARSRAGE_COLLIDE");
  if(ph!=3)
    return;
  switch(k) {
    case Swamp:  spawn(boss, w, "SWAMPGOLEM",      Minions,   "spellFX_Waterfist_COLLIDEFX");   break;
    case Rock:   spawn(boss, w, "STONEGOLEM",      Minions,   "spellFX_Concussionbolt_COLLIDE"); break;
    case Fire:   spawn(boss, w, "FIREWARAN",       Minions,   "spellFX_InstantFireball_COLLIDE"); break;
    case Ice:    spawn(boss, w, "ICEGOLEM",        Minions,   "spellFX_Icebolt_COLLIDE");        break;
    case Undead: spawn(boss, w, "LESSER_SKELETON", Skeletons, "spellFX_Massdeath_EXPLOSION");    break;
    case Raven:  break;
    }
  }

void ArpgBoss::spawn(Npc& boss, World& w, std::string_view inst, int count, std::string_view fx) {
  const size_t id = w.script().findSymbolIndex(inst);
  if(id==size_t(-1))
    return;
  auto* phy = w.physic();
  for(int i=0; i<count; ++i) {
    const float a   = rnd(0.f, 2.f*float(M_PI));
    Vec3        pos = boss.position() + Vec3(std::cos(a)*MinionDist, 0.f, std::sin(a)*MinionDist);
    if(phy!=nullptr) {
      const auto g = phy->landRay(pos+Vec3(0.f, 400.f, 0.f), 1500.f);
      if(!g.hasCol)
        continue; // over a cliff
      pos.y = g.v.y;
      }
    if(auto n = w.addNpc(id, pos)) {
      n->arpgSetMinion(true);
      ArpgBosses::effect(w, fx, pos);
      }
    }
  }

void ArpgBoss::attack(Npc& boss, Npc& hero, World& w, uint64_t now) {
  (void)w;
  const Vec3 hp = hero.position();
  const Vec3 bp = boss.position();
  float dx = hp.x-bp.x, dz = hp.z-bp.z;
  const float dl = std::sqrt(dx*dx+dz*dz);
  if(dl<1.f) {
    const float a = boss.rotationRad();
    dx = std::cos(a);
    dz = std::sin(a);
    } else {
    dx /= dl;
    dz /= dl;
    }
  const float base = std::atan2(dz, dx);

  Zone z;
  z.start = now;
  z.fire  = now+WarnMs;
  z.end   = z.fire;
  z.pos   = hp;

  auto around = [&](float range) {
    const float a = rnd(0.f, 2.f*float(M_PI)), r = rnd(0.25f, 1.f)*range;
    return hp + Vec3(std::cos(a)*r, 0.f, std::sin(a)*r);
    };

  switch(k) {
    case Swamp: {
      // poison puddles: on the hero and (phases 2, 3) next to him; they stay
      const int n = (ph==1) ? 1 : 3;
      for(int i=0; i<n; ++i) {
        Zone c = z;
        c.shape = Circle;
        c.look  = Poison;
        c.pos   = (i==0) ? hp : around(400.f);
        c.r     = PuddleR;
        c.hit   = PuddleHit;
        c.end   = c.fire+PuddleMs;
        zn.push_back(c);
        }
      break;
      }
    case Rock: {
      // boulders falling from above
      const int n = (ph==1) ? 2 : 4;
      for(int i=0; i<n; ++i) {
        Zone c = z;
        c.shape = Circle;
        c.look  = Plain;
        c.pos   = (i==0) ? hp : around(500.f);
        c.r     = RockR;
        c.hit   = RockHit;
        zn.push_back(c);
        }
      break;
      }
    case Fire: {
      // the breath: a cone towards the hero; phase 3: fire patches stay in it
      Zone c = z;
      c.shape = Cone;
      c.look  = Flame;
      c.pos   = Vec3(bp.x, hp.y, bp.z);
      c.dirX  = dx;
      c.dirZ  = dz;
      c.r     = BreathLen;
      c.w     = BreathHalfDeg;
      c.hit   = BreathHit;
      zn.push_back(c);
      if(ph==3) {
        for(int i=0; i<3; ++i) {
          const float a = base + degToRad(rnd(-BreathHalfDeg+5.f, BreathHalfDeg-5.f));
          const float r = rnd(300.f, BreathLen-100.f);
          Zone f = z;
          f.shape = Circle;
          f.look  = Flame;
          f.pos   = Vec3(bp.x+std::cos(a)*r, hp.y, bp.z+std::sin(a)*r);
          f.r     = PatchR;
          f.hit   = 0.f;
          f.end   = f.fire+PatchMs;
          zn.push_back(f);
          }
        }
      break;
      }
    case Ice: {
      // the ice breath: a line towards the hero; phases 2, 3: a fan of three
      const int n = (ph==1) ? 1 : 3;
      for(int i=0; i<n; ++i) {
        const float a = base + degToRad(i==0 ? 0.f : (i==1 ? -IceFanDeg : IceFanDeg));
        Zone c = z;
        c.shape = Line;
        c.look  = Frost;
        c.pos   = Vec3(bp.x, hp.y, bp.z);
        c.dirX  = std::cos(a);
        c.dirZ  = std::sin(a);
        c.r     = IceLen;
        c.w     = IceHalfW;
        c.hit   = IceHit;
        zn.push_back(c);
        }
      break;
      }
    case Undead: {
      // a wave of darkness running out from the dragon: roll through it
      Zone c = z;
      c.shape = Ring;
      c.look  = Dark;
      c.pos   = Vec3(bp.x, hp.y, bp.z);
      c.r     = WaveStartR;
      c.r2    = WaveMaxR;
      c.w     = WaveBand;
      c.hit   = WaveHit;
      c.end   = c.fire + uint64_t((WaveMaxR-WaveStartR)/WaveSpeed*1000.f);
      zn.push_back(c);
      break;
      }
    case Raven: {
      if(ph==1)
        return; // the swordsman with Beliar's weapon, as in Gothic
      if(ph==3 && attacks%2==1) {
        // Gniew Beliara: a wave from Raven
        Zone c = z;
        c.shape = Ring;
        c.look  = Dark;
        c.pos   = Vec3(bp.x, hp.y, bp.z);
        c.r     = WaveStartR*0.5f;
        c.r2    = RavenWaveMaxR;
        c.w     = WaveBand;
        c.hit   = RavenWaveHit;
        c.end   = c.fire + uint64_t((c.r2-c.r)/WaveSpeed*1000.f);
        zn.push_back(c);
        } else {
        // Beliar's lightning: a line towards the hero
        Zone c = z;
        c.shape = Line;
        c.look  = Bolt;
        c.fire  = now+BoltWarnMs;
        c.end   = c.fire;
        c.pos   = Vec3(bp.x, hp.y, bp.z);
        c.dirX  = dx;
        c.dirZ  = dz;
        c.r     = BoltLen;
        c.w     = BoltHalfW;
        c.hit   = BoltHit;
        zn.push_back(c);
        }
      break;
      }
    }
  attacks++;
  }

void ArpgBoss::strike(Npc& boss, Npc& hero, World& w, const Zone& z) {
  (void)w;
  const int32_t mx = std::max(1, hero.attribute(ATR_HITPOINTSMAX));
  if(z.hit>0.f) {
    const int32_t dmg = std::max<int32_t>(1, int32_t(std::lround(float(mx)*z.hit)));
    if(!hero.arpgDirectHit(boss, dmg))
      return; // rolled through it
    }
  switch(z.look) {
    case Poison: hero.arpgApplyStatus(ArpgStatus::Poison, PoisonMs, float(mx)*PoisonDps, &boss); break;
    case Flame:  hero.arpgApplyStatus(ArpgStatus::Burn,   BurnMs,   float(mx)*BurnDps,   &boss); break;
    case Frost:  hero.arpgApplyStatus(ArpgStatus::Chill,  ChillMs,  0.f,                 &boss); break;
    case Plain:
    case Dark:
    case Bolt:
      break;
    }
  }

void ArpgBoss::resolve(Npc& boss, Npc& hero, World& w, uint64_t now) {
  const Vec3 hp = hero.position();
  for(auto& z:zn) {
    if(!z.fired && now>=z.fire) {
      z.fired = true;
      z.next  = now+1000;
      if(z.shape!=Ring && !hero.isDead() && inside(z, hp))
        strike(boss, hero, w, z);
      // what it looks like when it hits
      auto along = [&](std::string_view fx, float step) {
        for(float t=step*0.5f; t<z.r; t+=step)
          ArpgBosses::effect(w, fx, z.pos+Vec3(z.dirX*t, 40.f, z.dirZ*t));
        };
      switch(z.shape) {
        case Circle:
          ArpgBosses::effect(w, z.look==Poison ? "spellFX_Waterfist_COLLIDEFX" :
                                (z.look==Flame ? "spellFX_InstantFireball_COLLIDE" : "spellFX_Concussionbolt_COLLIDE"), z.pos);
          break;
        case Cone:
          along("spellFX_InstantFireball_COLLIDE", 250.f);
          break;
        case Line:
          along(z.look==Bolt ? "spellFX_Zap_COLLIDE" : "spellFX_Icebolt_COLLIDE", 350.f);
          if(z.look==Bolt)
            ArpgBosses::effect(w, "spellFX_BELIARSRAGE_COLLIDE", z.pos+Vec3(z.dirX*z.r, 0.f, z.dirZ*z.r));
          break;
        case Ring:
          ArpgBosses::effect(w, "spellFX_Massdeath_EXPLOSION", z.pos);
          break;
        }
      }
    if(!z.fired || hero.isDead())
      continue;
    if(z.shape==Ring && !z.struck && now<=z.end) {
      // the wave hits once, where it runs over the hero
      const float R  = ringRadius(z, now);
      const float dx = hp.x-z.pos.x, dz = hp.z-z.pos.z;
      if(std::abs(hp.y-z.pos.y)<300.f && std::abs(std::sqrt(dx*dx+dz*dz)-R) <= z.w*0.5f+HeroR) {
        z.struck = true;
        strike(boss, hero, w, z);
        }
      }
    if(z.shape==Circle && z.end>z.fire && now<z.end && inside(z, hp)) {
      // standing in a puddle / a fire patch: the status goes on
      const int32_t mx = std::max(1, hero.attribute(ATR_HITPOINTSMAX));
      if(z.look==Poison)
        hero.arpgApplyStatus(ArpgStatus::Poison, PoisonMs, float(mx)*PoisonDps, &boss);
      else if(z.look==Flame)
        hero.arpgApplyStatus(ArpgStatus::Burn, BurnMs, float(mx)*BurnDps, &boss);
      }
    }
  zn.erase(std::remove_if(zn.begin(), zn.end(), [now](const Zone& z) {
    return z.fired && now>std::max(z.end, z.fire)+150;
    }), zn.end());
  }

void ArpgBosses::tick(World& w, Npc& hero) {
  // the bosses near the hero first (a minion added while the list of people is being walked through would break it)
  std::vector<Npc*> list;
  w.detectNpc(hero.position(), Range, [&](Npc& n) {
    if(&n==&hero)
      return;
    if(!n.arpgBossChecked()) {
      ArpgBoss::Kind k = ArpgBoss::Swamp;
      if(ArpgBoss::detect(w, n, k))
        n.arpgSetBoss(std::make_unique<ArpgBoss>(k, n));
      else
        n.arpgSetBoss(nullptr);
      }
    if(n.arpgBoss()!=nullptr)
      list.push_back(&n);
    });
  for(auto n:list)
    n->arpgBoss()->tick(*n, hero, w);
  }

Npc* ArpgBosses::shown(World& w, Npc& hero) {
  const uint64_t now  = w.tickCount();
  Npc*           best = nullptr;
  float          bd   = 0;
  w.detectNpc(hero.position(), Range, [&](Npc& n) {
    auto b = n.arpgBoss();
    if(b==nullptr || n.isDead() || !b->active(now))
      return;
    const auto  d  = n.position()-hero.position();
    const float d2 = d.x*d.x+d.z*d.z;
    if(best==nullptr || d2<bd) {
      best = &n;
      bd   = d2;
      }
    });
  return best;
  }

void ArpgBosses::effect(World& w, std::string_view name, const Tempest::Vec3& at) {
  const VisualFx* vfx = Gothic::inst().loadVisualFx(name);
  if(vfx==nullptr)
    return;
  Effect e(*vfx, w, at);
  e.setActive(true);
  e.setLooped(false);
  w.runEffect(std::move(e));
  }
