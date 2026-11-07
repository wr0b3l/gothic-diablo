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
#include <Tempest/Log>

#include "ui/arpgmenu.h"
#include "world/objects/npc.h"
#include "world/objects/sound.h"
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
  :k(k), ph(phaseOf(k, npc)) {
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
    {"DRAGON_FIRE",          Fire  },  // Feomathar
    {"DRAGON_FIRE_ISLAND",   Fire  },  // Feodaron on Irdorath; the story's immunity stays in Gothic's scripts
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

int ArpgBoss::phaseOf(Kind k, const Npc& npc) {
  const float p2 = (k==Raven) ? Phase2 : DragonPhase2;
  const float p3 = (k==Raven) ? Phase3 : DragonPhase3;
  const int32_t mx = std::max(1, npc.attribute(ATR_HITPOINTSMAX));
  const float   v  = float(npc.attribute(ATR_HITPOINTS))/float(mx);
  if(v<p3)
    return 3;
  if(v<p2)
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
    if(fly!=Ground) {
      fly   = Ground; // falls down (Gothic's gravity again)
      pitch = roll = 0;
      boss.arpgSetFlap(0.f);
      }
    return;
    }
  const auto d = boss.position()-hero.position();
  if(boss.isAttack() && boss.target()==&hero && d.x*d.x+d.z*d.z<Range*Range)
    activeUntil = now+ActiveMs;
  if(fly!=Ground && !hero.isDead() && d.x*d.x+d.z*d.z<Range*Range)
    activeUntil = now+ActiveMs; // in the air Gothic's AI sleeps: the fight goes on while the hero is near

  // the flight ends: the minions are dead, 5 minutes, the hero gone
  if(fly==Air && (minionsAlive(w)==0 || now>=flyStart+FlyMaxMs || !active(now) || hero.isDead()))
    land(boss, w);
  if(fly==Wait && (!active(now) || hero.isDead()))
    fly = Ground;

  // a new phase (the life only goes down here: the dragons' regeneration does not take a phase back)
  const int p = phaseOf(k, boss);
  while(ph<p) {
    ph++;
    onPhase(boss, hero, w);
    }

  const bool canAttack = (fly==Ground || fly==Air);
  if(active(now) && !hero.isDead() && !boss.isDown() && now>=shieldUntil && canAttack) {
    if(nextAttack==0)
      nextAttack = now+3000; // the first one a moment after the fight starts
    if(now>=nextAttack && now>=busyUntil && !boss.isAttackAnim()) {
      // one attack at a time (decision 2026-11-04): not during Gothic's own attack, nothing else until it has hit
      const size_t was = zn.size();
      attack(boss, hero, w, now);
      uint64_t last = now;
      for(size_t i=was; i<zn.size(); ++i)
        last = std::max(last, zn[i].fire);
      busyUntil  = last+GapMs;
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
  if(k==Raven || (ph!=2 && ph!=3))
    return;
  // the golems of the dragon's element (decision 2026-11-04; the undead dragon: skeletons), at 66% and at 33%
  int n = 0;
  switch(k) {
    case Swamp:  n = spawn(boss, w, "SWAMPGOLEM",      Minions,   "spellFX_Waterfist_COLLIDEFX");    break;
    case Rock:   n = spawn(boss, w, "STONEGOLEM",      Minions,   "spellFX_Concussionbolt_COLLIDE"); break;
    case Fire:   n = spawn(boss, w, "FIREGOLEM",       Minions,   "spellFX_InstantFireball_COLLIDE"); break;
    case Ice:    n = spawn(boss, w, "ICEGOLEM",        Minions,   "spellFX_Icebolt_COLLIDE");        break;
    case Undead: n = spawn(boss, w, "LESSER_SKELETON", Skeletons, "spellFX_Massdeath_EXPLOSION");    break;
    case Raven:  break;
    }
  // the dragon goes up while they live (after the roar)
  if(n>0 && k!=Raven)
    startFlight(boss, w, shieldUntil);
  }

int ArpgBoss::spawn(Npc& boss, World& w, std::string_view inst, int count, std::string_view fx) {
  const size_t id = w.script().findSymbolIndex(inst);
  if(id==size_t(-1))
    return 0;
  int ret = 0;
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
      ret++;
      }
    }
  return ret;
  }

int ArpgBoss::minionsAlive(World& w) const {
  int n = 0;
  w.detectNpc(flyCenter, Range, [&](Npc& m) {
    if(m.arpgIsMinion() && !m.isDead())
      n++;
    });
  return n;
  }

void ArpgBoss::hintAir(World& w) const {
  const uint64_t now = w.tickCount();
  if(now<hintAt)
    return;
  hintAt = now+4000;
  Gothic::inst().onPrint(ArpgMenu::gtxt("Smok jest w powietrzu: tylko łuk, kusza albo czar"));
  }

void ArpgBoss::startFlight(Npc& boss, World& w, uint64_t at) {
  (void)boss;
  if(fly==Landing) {
    flyPending = true; // up again when it is down
    return;
    }
  if(fly!=Ground) {
    flyStart = w.tickCount(); // already up: the new minions keep it there, 5 minutes from now
    return;
    }
  fly   = Wait; // the roar first (Gothic's AI goes on), then the take-off
  flyT0 = at;
  }

void ArpgBoss::land(Npc& boss, World& w) {
  fly     = Landing;
  flyT0   = w.tickCount();
  flyFrom = boss.position();
  }

static float smooth01(float t) {
  t = std::clamp(t, 0.f, 1.f);
  return t*t*(3.f-2.f*t);
  }

bool ArpgBoss::tickFlight(Npc& boss, World& w) {
  // Diathic (decision 2026-11-04): the dragon flies over its arena while its minions live: up (2.5 s), a circle
  // (smaller between walls, lower under a ceiling), down to the place it took off from (3 s). Gothic's AI and
  // gravity sleep meanwhile (Npc::tick); the wings beat (Pose: the bones with WING in their name), the body rises
  // with each beat, the nose goes up at the take-off, it leans into the circle
  const uint64_t now = w.tickCount();
  const float    dt  = (flyLast!=0 && now>flyLast) ? std::min(float(now-flyLast)/1000.f, 0.1f) : 0.f;
  flyLast = now;
  if(fly==Wait) {
    if(now<flyT0)
      return false;
    // the take-off: the arena = here; the room above and around
    flyCenter = boss.position();
    flyFrom   = flyCenter;
    flyStart  = now;
    flyT0     = now;
    flyH      = FlyHeight;
    flyR      = FlyRadius;
    if(auto* phy = w.physic()) {
      const Vec3 up0 = flyCenter+Vec3(0.f, 150.f, 0.f);
      const auto top = phy->ray(up0, flyCenter+Vec3(0.f, FlyHeight+500.f, 0.f));
      if(top.hasCol)
        flyH = std::clamp(top.v.y-flyCenter.y-500.f, FlyMinHeight, FlyHeight);
      const Vec3 mid = flyCenter+Vec3(0.f, flyH, 0.f);
      for(int i=0; i<8; ++i) {
        const float a   = float(i)*float(M_PI)/4.f;
        const Vec3  dir = {std::cos(a), 0.f, std::sin(a)};
        const auto  h   = phy->ray(mid, mid+dir*(FlyRadius+600.f));
        if(h.hasCol)
          flyR = std::min(flyR, std::max(0.f, (h.v-mid).length()-600.f));
        }
      }
    flyAng = boss.rotationRad();
    flyDir = (w.script().rand(2)==0) ? 1.f : -1.f;
    fly    = TakeOff;
    const bool flyAni = boss.playAnimByName("S_FLY", BS_NONE)!=nullptr;
    if(!flyAni)
      boss.setAnim(Npc::Anim::Idle);
    // for the test: is there a flight animation, how are the bones of the wings called
    Tempest::Log::i("Diathic: smok startuje, animacja S_FLY: ", flyAni ? "tak" : "nie",
                    ", wysokosc ", int(flyH), " cm, okrag ", int(flyR), " cm; kosci: ", boss.arpgBoneNames());
    ArpgBosses::sound(w, "DRG_Attack", flyCenter+Vec3(0.f, 200.f, 0.f));
    }

  const float beat = float(now-flyStart)*2.f*float(M_PI)/float(FlapMs);
  const float oldA = boss.rotation();
  Vec3        pos  = boss.position();
  Vec3        face = {};
  switch(fly) {
    case Ground:
    case Wait:
      return false;
    case TakeOff: {
      const float t  = std::clamp(float(now-flyT0)/float(TakeOffMs), 0.f, 1.f);
      const float k  = smooth01(t);
      const Vec3  to = flyCenter + Vec3(std::cos(flyAng)*flyR, 0.f, std::sin(flyAng)*flyR);
      pos   = flyFrom + (to-flyFrom)*k;
      pos.y = flyFrom.y + flyH*(1.f-(1.f-t)*(1.f-t)) + FlyBob*std::sin(beat+float(M_PI))*t;
      pitch = 18.f*std::sin(float(M_PI)*t);
      roll *= 0.9f;
      face  = to-flyFrom;
      boss.arpgSetFlap(FlapDeg*1.4f*std::sin(beat));
      if(t>=1.f) {
        fly   = Air;
        flyT0 = now;
        }
      break;
      }
    case Air: {
      const float w0 = (flyR>50.f) ? FlySpeed/flyR : 0.f;
      flyAng += flyDir*w0*dt;
      pos   = flyCenter + Vec3(std::cos(flyAng)*flyR, flyH, std::sin(flyAng)*flyR);
      pos.y += FlyBob*std::sin(beat+float(M_PI));
      pitch = 4.f + 3.f*std::sin(beat);
      if(flyR>50.f) {
        face = Vec3(-std::sin(flyAng), 0.f, std::cos(flyAng))*flyDir;
        } else if(auto* pl = w.player()) {
        face = pl->position()-pos; // no room for a circle: it hovers and looks at the hero
        }
      boss.arpgSetFlap(FlapDeg*std::sin(beat));
      break;
      }
    case Landing: {
      const float t = std::clamp(float(now-flyT0)/float(LandMs), 0.f, 1.f);
      const float k = smooth01(t);
      pos   = flyFrom + (flyCenter-flyFrom)*k;
      pos.y = flyFrom.y + (flyCenter.y-flyFrom.y)*(t*t);
      pitch = -8.f*std::sin(float(M_PI)*t);
      roll *= 0.9f;
      face  = flyCenter-flyFrom;
      boss.arpgSetFlap(FlapDeg*1.4f*(1.f-t)*std::sin(beat));
      if(t>=1.f) {
        fly   = Ground;
        pitch = roll = 0;
        boss.arpgSetFlap(0.f);
        boss.setPosition(flyCenter);
        boss.playAnimByName("T_WARN", BS_NONE); // down again: the roar
        ArpgBosses::effect(w, "spellFX_Concussionbolt_COLLIDE", flyCenter);
        nextAttack = std::max(nextAttack, now+3000);
        busyUntil  = std::max(busyUntil,  now+2000);
        if(flyPending) {
          flyPending = false;
          if(minionsAlive(w)>0)
            startFlight(boss, w, now+PhaseShieldMs); // after the roar, up again
          }
        return true;
        }
      break;
      }
    }
  boss.setPosition(pos);
  face.y = 0.f;
  if(face.quadLength()>1.f)
    boss.setDirection(face);
  if(fly==Air && dt>0.f) {
    // leaning into the circle: like the hero's lean in a turn (PlayerControl::arpgRunLean: the sign of the turn)
    const float da   = std::remainder(boss.rotation()-oldA, 360.f);
    const float want = (std::fabs(da)>0.01f) ? std::copysign(BankDeg, da) : 0.f;
    roll += (want-roll)*std::min(1.f, 2.f*dt);
    }
  return true;
  }

void ArpgBoss::announce(Npc& boss, World& w, std::string_view name, std::string_view sound, uint64_t until) {
  abilityName  = name;
  abilityUntil = until;
  if(!sound.empty())
    ArpgBosses::sound(w, sound, boss.position()+Vec3(0.f, 200.f, 0.f));
  }

uint64_t ArpgBoss::onOwnAttack(Npc& boss, World& w, uint64_t aniMs) {
  // Diathic (decisions 2026-11-03): Gothic's own attack of a boss — a dragon's bite or breath, Raven's swing:
  // a pause after it (the bosses attacked too often), a dragon's attack has a sign on the ground and a roar
  const uint64_t now = w.tickCount();
  if(k==Raven) {
    busyUntil = now+aniMs+RavenRestMs;
    return RavenRestMs;
    }
  busyUntil  = now+aniMs+RestMs;                                   // one attack at a time
  nextAttack = std::max(nextAttack, now+aniMs+GapMs);
  auto&       gs    = w.script();
  auto*       hero  = w.player();
  // Gothic's reach (FightAlgo::prefferedAttackDistance): the bases of both guilds + the fist (dragons have no weapon)
  float       reach = 600.f;
  if(hero!=nullptr) {
    const auto& gv = gs.guildVal();
    reach = float(gv.fight_range_base[boss.guild()] + gv.fight_range_base[hero->guild()] + gv.fight_range_fist[boss.guild()]);
    }
  const float a     = boss.rotationRad();
  Zone z;
  z.shape = Cone;
  z.look  = (k==Ice) ? Frost : ((k==Swamp) ? Poison : ((k==Undead) ? Dark : Flame));
  z.sign  = true;
  z.pos   = boss.position();
  if(hero!=nullptr)
    z.pos.y = hero->position().y;
  z.dirX  = std::cos(a);
  z.dirZ  = std::sin(a);
  z.r     = reach;
  z.w     = MeleeHalfDeg;
  z.start = now;
  z.fire  = now + std::max<uint64_t>(300, uint64_t(float(aniMs)*MeleeSignPart));
  z.end   = z.fire;
  zn.push_back(z);
  ArpgBosses::sound(w, "DRG_Attack", boss.position()+Vec3(0.f, 200.f, 0.f));
  return RestMs;
  }

void ArpgBoss::onOwnSpell(Npc& boss, World& w) {
  // Gothic's spell of a dragon (the fireball, the ball of death): a longer rest, nothing else meanwhile
  (void)boss;
  const uint64_t now = w.tickCount();
  busyUntil  = now+SpellRestMs;
  nextAttack = std::max(nextAttack, now+GapMs);
  }

void ArpgBoss::attack(Npc& boss, Npc& hero, World& w, uint64_t now) {
  const Vec3 hp = hero.position();
  Vec3       bp = boss.position();
  if(fly!=Ground)
    bp.y = hp.y; // from the air: the shapes on the ground
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
      announce(boss, w, "Trujące kałuże", "MFX_Geyser_Rumble", z.fire);
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
      announce(boss, w, "Spadające głazy", "DRG_Rock_Attack", z.fire);
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
      announce(boss, w, "Zionięcie ogniem", "DRG_Attack", z.fire);
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
      if(attacks%2==1) {
        announce(boss, w, "Bryła lodu", "MFX_Icewave_Cast", z.fire);
        Zone c = z;
        c.shape = Circle;
        c.look  = Cube;
        c.pos   = hp;
        c.r     = IceCubeR;
        c.hit   = IceCubeHit;
        zn.push_back(c);
        break;
        }
      // the ice breath: a line towards the hero; phases 2, 3: a fan of three
      announce(boss, w, "Lodowy podmuch", "DRG_Attack", z.fire);
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
      announce(boss, w, "Fala ciemności", "MFX_BreathOfDeath_Invest", z.fire);
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
        announce(boss, w, "Gniew Beliara", "MFX_BreathOfDeath_Invest", z.fire);
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
        announce(boss, w, "Piorun Beliara", "MFX_Thunderball_Invest4", now+BoltWarnMs);
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
    case Cube:   hero.arpgApplyStatus(ArpgStatus::IceCube, ArpgStatus::IceCubeMs, 0.f,   &boss); break;
    case Plain:
    case Dark:
    case Bolt:
      break;
    }
  }

void ArpgBoss::resolve(Npc& boss, Npc& hero, World& w, uint64_t now) {
  const Vec3 hp = hero.position();
  for(auto& z:zn) {
    if(!z.fired && now>=z.fire && z.sign) {
      z.fired = true; // a dragon's own attack: Gothic hits, the shape was only the sign
      continue;
      }
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
      // and how it sounds (one per attack: the first shape of it)
      if(z.start!=lastSoundAt) {
        lastSoundAt = z.start;
        std::string_view snd;
        switch(z.look) {
          case Poison: snd = "MFX_Geyser_Fountain"; break;
          case Flame:  snd = "MFX_Firestorm_Cast";  break;
          case Frost:  snd = "MFX_Icewave_Cast";    break;
          case Cube:   snd = "MFX_Icewave_Cast";    break;
          case Dark:   snd = "MFX_Massdeath_Cast";  break;
          case Bolt:   snd = "MFX_BeliarWeap_A4";   break;
          case Plain:  snd = "GOL_StepBoom";        break;
          }
        ArpgBosses::sound(w, snd, z.pos);
        }
      switch(z.shape) {
        case Circle:
          ArpgBosses::effect(w, z.look==Poison ? "spellFX_Waterfist_COLLIDEFX" :
                                (z.look==Flame ? "spellFX_InstantFireball_COLLIDE" :
                                 (z.look==Cube ? "spellFX_IceSpell_SENDPERCEPTION" : "spellFX_Concussionbolt_COLLIDE")), z.pos);
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

void ArpgBosses::sound(World& w, std::string_view name, const Tempest::Vec3& at) {
  auto s = ::Sound(w, ::Sound::T_Regular, name, at, SoundRange, false);
  s.play();
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
