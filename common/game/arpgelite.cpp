#include "arpgelite.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "game/arpgboss.h"
#include "game/constants.h"
#include "game/gamescript.h"
#include "utils/versioninfo.h"
#include "world/objects/npc.h"
#include "world/world.h"

float ArpgElite::chapterMul(World& w) {
  const int ch = w.script().arpgChapter();
  return std::pow(ChapterMul, float(ch-1));
  }

bool ArpgElite::redAttacker(World& w, const Npc& npc) {
  if(w.version().game!=2)
    return false;
  switch(npc.guild()) {
    case GIL_TROLL:                // troll, black troll
    case GIL_DRACONIAN:            // lizard men
    case GIL_SHADOWBEAST:          // shadowbeast, bloodhound
    case GIL_SHADOWBEAST_SKELETON:
    case GIL_STONEGOLEM:
    case GIL_FIREGOLEM:
    case GIL_ICEGOLEM:
    case GIL_SWAMPGOLEM:
    case GIL_DEMON:                // demon, demon lord
      return true;
    case GIL_ORC: {
      // orc elite (OrcElite_Roam, OrcElite_Rest, the colonels: OrkElite_AntiPaladinOrkOberst...)
      auto* sym = w.script().findSymbol(size_t(npc.instanceSymbol()));
      if(sym==nullptr)
        return false;
      std::string n = sym->name();
      for(auto& c:n)
        c = char(std::toupper(static_cast<unsigned char>(c)));
      return n.rfind("ORCELITE",0)==0 || n.rfind("ORKELITE",0)==0;
      }
    default:
      return false;
    }
  }

static int32_t pendingXp = 0;

void ArpgElite::leaderKilled(int32_t xp) {
  if(xp>0)
    pendingXp += xp;
  }

// a monster that may get into a group with a leader
static bool packCandidate(World& w, Npc& n, const Npc& hero) {
  if(&n==&hero || n.isDead() || !n.arpgIsBeast() || n.arpgIsMinion())
    return false;
  switch(n.guild()) {
    case GIL_SHEEP:
    case GIL_MEATBUG:
    case GIL_SUMMONED_GOBBO_SKELETON:
    case GIL_SUMMONED_WOLF:
    case GIL_SUMMONED_SKELETON:
    case GIL_SUMMONED_GOLEM:
    case GIL_SUMMONED_DEMON:
    case GIL_SummonedGuardian:
    case GIL_SummonedZombie:
    case GIL_FRIENDLY_ORC:
    case GIL_DRAGON:
      return false;
    default:
      break;
    }
  if(w.script().personAttitude(n, hero)==ATT_FRIENDLY)
    return false;
  ArpgBoss::Kind k = ArpgBoss::Swamp;
  return !ArpgBoss::detect(w, n, k);
  }

static void rollGroups(World& w, Npc& hero) {
  std::vector<Npc*> seeds;
  w.detectNpc(hero.position(), ArpgElite::RollRange, [&](Npc& n) {
    if(&n!=&hero && n.arpgIsBeast() && !n.arpgEliteRolled())
      seeds.push_back(&n);
    });

  std::vector<Npc*> group;
  std::vector<Npc*> seen;
  for(auto seed:seeds) {
    if(seed->arpgEliteRolled() || std::find(seen.begin(), seen.end(), seed)!=seen.end())
      continue;
    if(w.version().game!=2 || !packCandidate(w, *seed, hero)) {
      seed->arpgSetEliteRolled(); // never in a group (a boss, a sheep, a friend...)
      continue;
      }
    // the group: a chain of monsters of the same guild, each within PackRadius of another one
    group.clear();
    group.push_back(seed);
    for(size_t i=0; i<group.size() && int(group.size())<ArpgElite::PackMaxSize; ++i) {
      w.detectNpc(group[i]->position(), ArpgElite::PackRadius, [&](Npc& n) {
        if(int(group.size())>=ArpgElite::PackMaxSize || n.arpgEliteRolled() || n.guild()!=seed->guild())
          return;
        if(std::find(group.begin(), group.end(), &n)!=group.end() || !packCandidate(w, n, hero))
          return;
        group.push_back(&n);
        });
      }
    if(int(group.size())<ArpgElite::PackMinSize) {
      seen.push_back(seed); // alone for now: checked again later (others may come)
      continue;
      }
    // the group is checked once: with a leader or not
    for(auto n:group)
      n->arpgSetEliteRolled();
    if(w.script().rand(100)>=ArpgElite::PackChance)
      continue;
    // the leader: the strongest one
    Npc* leader = group[0];
    for(auto n:group)
      if(n->attribute(ATR_HITPOINTSMAX)>leader->attribute(ATR_HITPOINTSMAX))
        leader = n;
    const uint32_t id = 1u + w.script().rand(0x7ffffffe);
    for(auto n:group)
      n->arpgSetPack(id, n==leader);
    }
  }

void ArpgElite::tick(World& w, Npc& hero) {
  if(pendingXp>0) {
    const int32_t xp = pendingXp;
    pendingXp = 0;
    w.script().arpgGivePlayerXp(xp);
    }

  static uint64_t last = 0;
  const uint64_t  now  = w.tickCount();
  if(now>=last && now<last+1000)
    return;
  last = now;
  rollGroups(w, hero);
  }
