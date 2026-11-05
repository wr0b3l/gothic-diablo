#include "arpgweapon.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <unordered_map>

#include "game/gamescript.h"
#include "game/constants.h"
#include "camera.h"
#include "ui/arpgmenu.h"
#include "world/objects/interactive.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"

using namespace ArpgWeapon;

// ---- the properties of the game's weapons (decision 2026-11-04: "some weapons") ----
// by the instance (upper case, as in the scripts' symbol table); a prefix ends with '*'
namespace {
  struct Base {
    const char*    inst;
    ArpgStatus::Id status;
    uint8_t        chance;
    };
  const Base bases[] = {
    // Podpalenie: Innos' fire, rubies, dragons
    {"ITMW_1H_BLESSED_*",          ArpgStatus::Burn,   20},  // ostrza magiczne, Gniew Innosa
    {"ITMW_2H_BLESSED_*",          ArpgStatus::Burn,   20},  // Miecz Zakonu, Święty Kat
    {"ITMW_RUBINKLINGE",           ArpgStatus::Burn,   15},  // Rubinowe ostrze
    {"ITMW_DRACHENSCHNEIDE",       ArpgStatus::Burn,   15},  // Smocza Zguba
    {"ITMW_ADDON_STAB04",          ArpgStatus::Burn,   15},  // Kostur Ulthara
    {"ITRW_ADDON_FIREBOW",         ArpgStatus::Burn,   25},  // Ognisty łuk
    // Zamrożenie: magic ore, runes, water
    {"ITMW_1H_SPECIAL_*",          ArpgStatus::Freeze, 10},  // broń z rudy
    {"ITMW_2H_SPECIAL_*",          ArpgStatus::Freeze, 10},
    {"ITMW_RUNENSCHWERT",          ArpgStatus::Freeze, 12},  // Miecz runiczny
    {"ITMW_ADDON_STAB03",          ArpgStatus::Freeze, 15},  // Wodny kostur
    {"ITMW_ADDON_STAB05",          ArpgStatus::Freeze, 15},  // Tajfun
    {"ITRW_ADDON_MAGICBOW",        ArpgStatus::Freeze, 12},  // Magiczny łuk
    // Krwawienie: serrated, spiked, the executioner's
    {"ITMW_FOLTERAXT",             ArpgStatus::Bleed,  30},  // Katowski topór
    {"ITMW_BERSERKERAXT",          ArpgStatus::Bleed,  25},  // Topór berserkera
    {"ITMW_BARBARENSTREITAXT",     ArpgStatus::Bleed,  25},  // Barbarzyński topór bojowy
    {"ITMW_ORKSCHLAECHTER",        ArpgStatus::Bleed,  25},  // Orkowa Zguba
    {"ITMW_NAGELKNUEPPEL",         ArpgStatus::Bleed,  20},  // Maczuga z kolcami
    {"ITMW_NAGELKEULE",            ArpgStatus::Bleed,  20},  // Pałka z kolcami
    {"ITMW_NAGELKEULE2",           ArpgStatus::Bleed,  20},  // Ciężka pałka z kolcami
    {"ITMW_MORGENSTERN",           ArpgStatus::Bleed,  20},  // Buława i łańcuch
    {"ITMW_SPICKER",               ArpgStatus::Bleed,  20},  // Rębiczerep
    {"ITMW_SHORTSWORD4",           ArpgStatus::Bleed,  20},  // Wilczy kieł
    {"ITMW_1H_SWORD_L_03",         ArpgStatus::Bleed,  20},  // Nóż na wilki
    {"ITMW_ADDON_KNIFE01",         ArpgStatus::Bleed,  20},  // Nóż na wilki
    {"ITMW_ADDON_HACKER_*",        ArpgStatus::Bleed,  20},  // maczety
    {"ITMW_2H_ORCAXE_*",           ArpgStatus::Bleed,  15},  // orkowe topory
    // Zatrucie
    {"ITMW_2H_ORCSWORD_01",        ArpgStatus::Poison, 25},  // Jaszczurzy miecz
    {"ITMW_ADDON_BANDITTRADER",    ArpgStatus::Poison, 20},  // Pałasz bandytów
    // Osłabienie: heavy blows, storms, Beliar
    {"ITMW_RABENSCHNABEL",         ArpgStatus::Weaken, 25},  // Kruczy Dziób
    {"ITMW_KRIEGSHAMMER1",         ArpgStatus::Weaken, 15},  // Młot wojenny
    {"ITMW_KRIEGSHAMMER2",         ArpgStatus::Weaken, 15},  // Ciężki młot wojenny
    {"ITMW_STURMBRINGER",          ArpgStatus::Weaken, 20},  // Ostrze Burzy
    {"ITMW_ZWEIHAENDER3",          ArpgStatus::Weaken, 20},  // Runa mocy
    {"ITMW_INQUISITOR",            ArpgStatus::Weaken, 20},  // Inkwizytor
    {"ITMW_ADDON_KEULE_*",         ArpgStatus::Weaken, 20},  // Sługa Wiatru, Sługa Burzy
    {"ITRW_ADDON_MAGICCROSSBOW",   ArpgStatus::Weaken, 20},  // Magiczna kusza
    {"ITMW_BELIARWEAPON_*",        ArpgStatus::Weaken, 20},  // Szpon Beliara
    };

  // ---- the workshop's recipes (decision 2026-11-04: "a few to start with") ----
  const Mod mods[] = {
    {"Naostrzenie",        "+10% obrażeń broni (co najmniej +3)", ArpgStatus::Count,  0, 1.10f, 3, AnyMelee,
     {{"ITMISWORDRAW", 1}, {"ITMI_COAL", 1}, {nullptr, 0}}},
    {"Ząbkowane ostrze",   "Krwawienie",                          ArpgStatus::Bleed,  25, 1.f, 0, Edge,
     {{"ITMISWORDRAW", 1}, {"ITAT_TEETH", 5}, {nullptr, 0}}},
    {"Hartowanie w ogniu", "Podpalenie",                          ArpgStatus::Burn,   20, 1.f, 0, AnyMelee,
     {{"ITMISWORDRAW", 1}, {"ITMI_SULFUR", 2}, {"ITMI_PITCH", 1}}},
    {"Mroźna stal",        "Zamrożenie",                          ArpgStatus::Freeze, 12, 1.f, 0, AnyMelee,
     {{"ITMI_AQUAMARINE", 1}, {"ITMI_NUGGET", 2}, {nullptr, 0}}},
    {"Jadowite ostrze",    "Zatrucie",                            ArpgStatus::Poison, 30, 1.f, 0, Edge,
     {{"ITAT_STING", 3}, {"ITMI_PITCH", 1}, {nullptr, 0}}},
    {"Ciężkie okucie",     "Osłabienie",                          ArpgStatus::Weaken, 25, 1.f, 0, Blunt,
     {{"ITMISWORDRAW", 1}, {"ITMI_ROCKCRYSTAL", 2}, {nullptr, 0}}},
    };

  std::string upper(std::string_view s) {
    std::string r(s);
    for(auto& c:r)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    return r;
    }

  const Base* baseOf(World& w, const Item& it) {
    static std::unordered_map<size_t,const Base*> cache;
    static const World* cacheW = nullptr;
    if(cacheW!=&w) {
      cache.clear();
      cacheW = &w;
      }
    const size_t cls = it.clsId();
    if(auto f = cache.find(cls); f!=cache.end())
      return f->second;
    const Base* ret = nullptr;
    if(auto sym = w.script().findSymbol(cls)) {
      const std::string nm = upper(sym->name());
      for(auto& b:bases) {
        const std::string_view key(b.inst);
        const bool pre = !key.empty() && key.back()=='*';
        if(pre ? nm.compare(0, key.size()-1, key.substr(0, key.size()-1))==0 : nm==key) {
          ret = &b;
          break;
          }
        }
      }
    cache[cls] = ret;
    return ret;
    }

  bool isMelee(const Item& it) {
    return (ItmFlags(it.mainFlag()) & ITM_CAT_NF)!=0;
    }
  bool isRanged(const Item& it) {
    return (ItmFlags(it.mainFlag()) & ITM_CAT_FF)!=0;
    }

  bool requested = false;
  }

const StatusCfg& ArpgWeapon::cfg(ArpgStatus::Id id) {
  static const StatusCfg none   = {0,        0.f,          0};
  static const StatusCfg burn   = {BurnMs,   BurnPerSec,   DotMin};
  static const StatusCfg bleed  = {BleedMs,  BleedPerSec,  DotMin};
  static const StatusCfg poison = {PoisonMs, PoisonPerSec, DotMin};
  static const StatusCfg freeze = {FreezeMs, 0.f,          0};
  static const StatusCfg weaken = {WeakenMs, 0.f,          0};
  switch(id) {
    case ArpgStatus::Burn:   return burn;
    case ArpgStatus::Bleed:  return bleed;
    case ArpgStatus::Poison: return poison;
    case ArpgStatus::Freeze: return freeze;
    case ArpgStatus::Weaken: return weaken;
    default:                 return none;
    }
  }

Effects ArpgWeapon::effectsOf(World& w, const Item& it) {
  Effects ret;
  if(!isMelee(it) && !isRanged(it))
    return ret;
  if(auto b = baseOf(w, it)) {
    ret.e[ret.n].status = b->status;
    ret.e[ret.n].chance = b->chance;
    ++ret.n;
    }
  const int m = modOf(it);
  if(m>=0 && mods[m].status!=ArpgStatus::Count && ret.n<MaxEffects) {
    // the same status twice: the better chance
    for(int i=0; i<ret.n; ++i)
      if(ret.e[i].status==mods[m].status) {
        ret.e[i].chance = std::max(ret.e[i].chance, mods[m].chance);
        return ret;
        }
    ret.e[ret.n].status = mods[m].status;
    ret.e[ret.n].chance = mods[m].chance;
    ++ret.n;
    }
  return ret;
  }

void ArpgWeapon::onHit(Npc& hero, Npc& victim, const Item& weapon, int32_t lost) {
  if(lost<=0 || victim.isDead() || victim.isDown())
    return;
  auto&      w  = hero.world();
  const auto ef = effectsOf(w, weapon);
  for(int i=0; i<ef.n; ++i) {
    const auto& e = ef.e[i];
    if(w.script().rand(100)>=uint32_t(e.chance))
      continue;
    const auto& c = cfg(e.status);
    float value = 0.f;
    if(ArpgStatus::info(e.status).dot)
      value = std::max(float(c.minPerSec), float(lost)*c.perSec);
    const bool had = victim.arpgHasStatus(e.status);
    victim.arpgApplyStatus(e.status, c.ms, value, &hero);
    if(!had && victim.arpgHasStatus(e.status))
      w.arpgPopup(victim.position()+Tempest::Vec3(0,215.f,0), int32_t(e.status), 8);
    }
  }

std::string ArpgWeapon::describe(const Effect& e) {
  const auto& c = cfg(e.status);
  char buf[160] = {};
  const int   s = int(c.ms/1000), ds = int((c.ms%1000)/100);
  char secs[16] = {};
  if(ds!=0)
    std::snprintf(secs, sizeof(secs), "%d,%d s", s, ds); else
    std::snprintf(secs, sizeof(secs), "%d s", s);
  switch(e.status) {
    case ArpgStatus::Burn:
    case ArpgStatus::Bleed:
    case ArpgStatus::Poison:
      std::snprintf(buf, sizeof(buf), "%s: %d%% ciosów - %d%% ciosu co sekundę przez %s",
                    ArpgStatus::info(e.status).name, int(e.chance), int(std::lround(c.perSec*100.f)), secs);
      break;
    case ArpgStatus::Freeze:
      std::snprintf(buf, sizeof(buf), "%s: %d%% ciosów - wróg stoi %s (nie bossowie)",
                    ArpgStatus::info(e.status).name, int(e.chance), secs);
      break;
    case ArpgStatus::Weaken:
      std::snprintf(buf, sizeof(buf), "%s: %d%% ciosów - wróg zadaje %d%% mniej przez %s",
                    ArpgStatus::info(e.status).name, int(e.chance), int(std::lround(ArpgStatus::WeakenCut*100.f)), secs);
      break;
    default:
      break;
    }
  return buf;
  }

int32_t ArpgWeapon::damageOf(const Item& it, size_t kind) {
  auto& h = it.handle();
  if(kind>=zenkit::DamageType::NUM)
    return 0;
  int32_t v = h.damage[kind];
  if((uint32_t(h.damage_type) & (1u<<kind))!=0)
    v += h.damage_total;
  return v;
  }

bool ArpgWeapon::syncHeroDamage(Npc& owner, const Item* inHand) {
  // the hero's own damage is 0 (PC_Hero), the fists use the strength; a transformed hero (a monster) stays Gothic's
  if(!owner.isPlayer() || owner.isMonster() || !Camera::isArpgEnabled())
    return false;
  auto& h = owner.handle();
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
    h.damage[i] = inHand!=nullptr ? damageOf(*inHand, i) : 0;
  return true;
  }

const char* ArpgWeapon::damageName(size_t kind) {
  static const char* names[zenkit::DamageType::NUM] = {
    "bariera", "obuchowe", "sieczne", "od ognia", "odrzut", "magiczne", "kłute", "upadek"
    };
  return kind<zenkit::DamageType::NUM ? names[kind] : "";
  }

// ---- the workshop ----

size_t ArpgWeapon::modCount() {
  return std::size(mods);
  }

const Mod& ArpgWeapon::mod(size_t i) {
  return mods[std::min(i, std::size(mods)-1)];
  }

int ArpgWeapon::modOf(const Item& it) {
  auto& t = it.handle().text[1];
  if(t.empty())
    return -1;
  for(size_t i=0; i<std::size(mods); ++i)
    if(t==ArpgMenu::gtxt(std::string(ModPrefix)+mods[i].name))
      return int(i);
  return -1;
  }

bool ArpgWeapon::modFits(const Mod& m, const Item& it) {
  if(!isMelee(it))
    return false;
  const auto dt = uint32_t(it.handle().damage_type);
  switch(m.fits) {
    case AnyMelee: return true;
    case Edge:     return (dt & (1u<<zenkit::DamageType::EDGE))!=0  || it.handle().damage[zenkit::DamageType::EDGE]>0;
    case Blunt:    return (dt & (1u<<zenkit::DamageType::BLUNT))!=0 || it.handle().damage[zenkit::DamageType::BLUNT]>0;
    }
  return false;
  }

std::string ArpgWeapon::modBlocker(Npc& hero, const Item& it, size_t mi) {
  const Mod& m = mod(mi);
  if(!modFits(m, it)) {
    if(!isMelee(it))
      return "Tylko broń biała";
    return m.fits==Edge ? "Tylko broń sieczna (miecze, topory)" : "Tylko broń obuchowa (buławy, młoty, pałki)";
    }
  if(it.count()>1)
    return "Masz kilka takich broni - zostaw jedną";
  if(!it.handle().text[1].empty() && modOf(it)<0)
    return "Tej broni nie da się ulepszyć";
  if(modOf(it)==int(mi))
    return "Ta broń ma już to ulepszenie";
  auto& sc = hero.world().script();
  std::string miss;
  for(auto& n:m.need) {
    if(n.item==nullptr)
      continue;
    const size_t id   = sc.findSymbolIndex(n.item);
    const size_t have = id==size_t(-1) ? 0 : hero.itemCount(id);
    if(have<size_t(n.count))
      miss = "Brakuje materiałów";
    }
  return miss;
  }

bool ArpgWeapon::modApply(Npc& hero, Item& it, size_t mi, std::string& msg) {
  msg = modBlocker(hero, it, mi);
  if(!msg.empty())
    return false;
  const Mod& m  = mod(mi);
  auto&      w  = hero.world();
  auto&      sc = w.script();

  // the weapon as the scripts made it: the old modification goes away
  auto base = std::make_shared<zenkit::IItem>();
  sc.initializeInstanceItem(base, it.clsId());

  for(auto& n:m.need) {
    if(n.item==nullptr)
      continue;
    const size_t id = sc.findSymbolIndex(n.item);
    if(id!=size_t(-1))
      hero.delItem(id, uint32_t(n.count));
    }

  hero.arpgWeaponStats(it, -1);
  auto& h = it.handle();
  for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
    h.damage[i] = base->damage[i];
  h.damage_total = base->damage_total;
  for(size_t i=0; i<zenkit::IItem::text_count; ++i) {
    h.text [i] = base->text[i];
    h.count[i] = base->count[i];
    }
  h.value = base->value;

  if(m.dmgMin>0) {
    auto raise = [&](int32_t v) {
      if(v<=0)
        return v;
      return std::max(v+m.dmgMin, int32_t(std::lround(float(v)*m.dmgMul)));
      };
    if(h.damage_type!=0 && h.damage_total>0) {
      h.damage_total = raise(h.damage_total);
      } else {
      for(size_t i=0; i<zenkit::DamageType::NUM; ++i)
        h.damage[i] = raise(h.damage[i]);
      }
    // the row of Gothic's description with the damage
    for(size_t i=0; i<zenkit::IItem::text_count; ++i)
      if(h.count[i]==base->damage_total && base->damage_total>0 && !h.text[i].empty()) {
        h.count[i] = h.damage_total;
        break;
        }
    }
  h.text [1] = ArpgMenu::gtxt(std::string(ModPrefix)+m.name);
  h.count[1] = 0;
  h.value    = int32_t(std::lround(float(base->value)*1.25f)); // worth a bit more
  for(size_t i=0; i<zenkit::IItem::text_count; ++i)
    if(h.count[i]==base->value && base->value>0 && !h.text[i].empty())
      h.count[i] = h.value;
  hero.arpgWeaponStats(it, +1);

  hero.emitSoundEffect("FORGE_ANVIL", 1500.f, true);
  msg = std::string(m.name)+" - gotowe";
  return true;
  }

Station ArpgWeapon::stationOf(const Interactive& m) {
  const std::string v = upper(m.arpgVisual());
  if(v.rfind("BSANVIL", 0)==0)
    return Anvil;
  if(v.rfind("BSSHARP", 0)==0)
    return Grindstone;
  return NoStation;
  }

bool ArpgWeapon::workshopNow(const Interactive& m, Npc& hero) {
  const auto st = stationOf(m);
  if(st==NoStation)
    return false;
  if(st==Anvil && !m.arpgUseItem().empty()) {
    // hot steel in the backpack: Gothic's forging
    const size_t id = hero.world().script().findSymbolIndex(m.arpgUseItem());
    if(id!=size_t(-1) && hero.itemCount(id)>0)
      return false;
    }
  return true;
  }

void ArpgWeapon::requestWorkshop() {
  requested = true;
  }

bool ArpgWeapon::takeWorkshopRequest() {
  const bool r = requested;
  requested = false;
  return r;
  }
