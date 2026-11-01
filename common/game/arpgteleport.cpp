#include "arpgteleport.h"

#include <cctype>
#include <string>

#include "game/gamescript.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/world.h"

namespace {
std::string upper(std::string_view s) {
  std::string r(s);
  for(auto& c:r)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return r;
  }
}

const std::vector<ArpgTeleport::Dest>& ArpgTeleport::dests() {
  // Spell_Teleport_Alle.d: the places of Spell_Cast_* (ItRu_Teleport_3 is unused: no name in the game)
  static const std::vector<Dest> d = {
    {"ITRU_TELEPORTSEAPORT",      "Miasto portowe",            "HAFEN",                   "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTMONASTERY",    "Klasztor",                  "KLOSTER",                 "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTFARM",         "Majątek ziemski",           "BIGFARM",                 "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTXARDAS",       "Wieża Xardasa",             "XARDAS",                  "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTTAVERNE",      "Tawerna",                   "NW_TAVERNE_04",           "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTPASSNW",       "Przełęcz Khorinis",         "LEVELCHANGE",             "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_PALTELEPORTSECRET",    "Tajna komnata paladynów",   "NW_PAL_SECRETCHAMBER",    "NEWWORLD.ZEN", "Khorinis"},
    {"ITRU_TELEPORTPASSOW",       "Przełęcz Górniczej Doliny", "SPAWN_MOLERAT02_SPAWN01", "OLDWORLD.ZEN", "Górnicza Dolina"},
    {"ITRU_TELEPORTOC",           "Zamek",                     "OC_MAGE_CENTER",          "OLDWORLD.ZEN", "Górnicza Dolina"},
    {"ITRU_TELEPORTOWDEMONTOWER", "Stara wieża demonów",       "DT_E3_03",                "OLDWORLD.ZEN", "Górnicza Dolina"},
    };
  return d;
  }

const ArpgTeleport::Dest* ArpgTeleport::destOf(World& w, const Item& itm) {
  auto* sym = w.script().findSymbol(itm.clsId());
  if(sym==nullptr)
    return nullptr;
  const std::string n = upper(sym->name());
  if(n.rfind("ITRU_",0)!=0)
    return nullptr;
  for(auto& d:dests())
    if(n==d.inst)
      return &d;
  return nullptr;
  }

std::vector<ArpgTeleport::Owned> ArpgTeleport::owned(World& w, Npc& hero) {
  std::vector<Owned> ret;
  for(auto& d:dests()) {
    const size_t cls = w.script().findSymbolIndex(d.inst);
    if(cls==size_t(-1) || hero.itemCount(cls)==0)
      continue;
    ret.push_back({&d, cls});
    }
  return ret;
  }

bool ArpgTeleport::here(World& w, const Dest& d) {
  return upper(w.name())==d.world && w.findPoint(d.wp, false)!=nullptr;
  }

int32_t ArpgTeleport::cost(World& w) {
  auto* sym = w.script().findSymbol("SPL_COST_TELEPORT");
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INT || !sym->is_const())
    return 10;
  return sym->get_int();
  }
