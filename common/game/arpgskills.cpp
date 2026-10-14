#include "arpgskills.h"

#include <algorithm>

#include "game/serialize.h"

using namespace ArpgSkills;

// ---------------------------------------------------------------------------------------------------------------
// the trees. Icons: the game's item models.

const std::vector<Tree>& ArpgSkills::trees() {
  static const std::vector<Tree> t = {
    {"Cios", "LPM", "Zwykły atak bronią białą lub strzał z łuku", "ITMW_1H_SLD_SWORD",
      {
        {"Ostrze",       {{"Ostra klinga",5,"+4% obrażeń ciosu za punkt","ITMW_SCHWERT1"},
                          {"Słaby punkt",3,"+5% szansy na pełne obrażenia (krytyk Gothica)","ITMW_1H_VLK_DAGGER"},
                          {"Głębokie rany",3,"Trafienie: krwawienie 5% obrażeń na sekundę przez 3 s, za punkt","ITAT_CLAW"}}},
        {"Rytm",         {{"Płynne ruchy",5,"+3% szybkości ataku za punkt","ITMW_RAPIER"},
                          {"Wyczucie chwili",2,"Okno combo dłuższe o 25% za punkt","ITMI_SEXTANT"},
                          {"Czwarty cios",1,"Seria kończy się dodatkowym ciosem x1,5","ITMW_MEISTERDEGEN"}}},
        {"Zamach",       {{"Szeroki łuk",3,"Łuk cięcia 120 stopni, +20 za punkt (do 180)","ITMW_SENSE"},
                          {"Odrzut",2,"Cios odpycha małe stworzenia o 0,5 m za punkt","ITMW_KRIEGSHAMMER1"},
                          {"Dobicie",3,"+15% obrażeń wobec powalonych za punkt","ITMW_SCHLACHTAXT"}}},
        {"Krew za krew", {{"Pijawka",3,"2% zadanych obrażeń wraca jako zdrowie, za punkt","ITPO_HEALTH_01"},
                          {"Twarda skóra",3,"Po trafieniu +5 ochrony przed bronią na 3 s, za punkt","ITAT_CRAWLERPLATE"}}},
        {"Łucznik",      {{"Szybki naciąg",5,"+4% szybkości strzałów za punkt","ITRW_BOW_L_01"},
                          {"Przebicie",2,"Strzała przelatuje przez 1 wroga za punkt","ITRW_ARROW"},
                          {"Celne oko",3,"+5% szansy trafienia powyżej 15 m, za punkt","ITRW_BOW_H_01"}}},
      },
      {"Wir",1,"Ostatni cios serii: obrót dookoła, trafia wszystkich wokół","ITRU_WHIRLWIND"},
      "3/3 w Dobiciu albo Czwarty cios", {1,2}, {0, {{2,2},{1,2}}}},
    {"Atak ładowany", "PPM", "Ładuj 2 s, puść w złotym oknie: x3, zamach 180 stopni", "ITMW_ZWEIHAENDER1",
      {
        {"Ładowanie",    {{"Skupienie",4,"Ładowanie krótsze o 0,2 s za punkt (do 1,2 s)","ITMI_FOCUS"},
                          {"Szersze okno",2,"Złote okno puszczenia +0,1 s za punkt","ITMI_ROCKCRYSTAL"},
                          {"Pewna ręka",1,"Spóźnione puszczenie daje x2, a nie zwykły cios","ITRI_STR_01"}}},
        {"Siła",         {{"Miażdżący cios",5,"Mnożnik obrażeń +0,2 za punkt (do x4)","ITMW_STEINBRECHER"},
                          {"Wstrząs",3,"Powaleni wstają o 0,5 s później, za punkt","ITMW_MORGENSTERN"},
                          {"Łamacz gardy",1,"Atak ładowany przebija blok przeciwnika","ITMW_2H_ORCAXE_01"}}},
        {"Fala",         {{"Fala uderzeniowa",1,"Zamach puszcza falę na 4 m przed postać (50% obrażeń)","ITRU_WINDFIST"},
                          {"Dalsza fala",3,"+1 m zasięgu fali za punkt","ITSC_WINDFIST"}}},
        {"Odnowienie",   {{"Opanowanie",4,"Odnowienie krótsze o 0,5 s za punkt","ITPL_SPEED_HERB_01"},
                          {"Pęd bitewny",1,"Zabicie atakiem ładowanym zeruje odnowienie","ITPO_SPEED"}}},
        {"Łuk",          {{"Strzała przeszywająca",1,"Strzał ładowany przebija wszystkich w linii","ITRW_ADDON_MAGICARROW"},
                          {"Grad strzał",3,"+1 strzała w wachlarzu za punkt","ITRW_BOW_M_01"},
                          {"Ciężki grot",2,"Strzał ładowany odpycha, przy 2/2 przewraca","ITRW_BOLT"}}},
      },
      {"Gniew Innosa",1,"Atak ładowany podpala wrogów: ogień przez 4 s","ITMI_INNOSSTATUE"},
      "10 punktów w drzewku", {}, {10, {}}},
    {"Fikołek", "Spacja", "Przewrót w stronę kursora, chwila bez obrażeń", "ITBE_ADDON_DEX_10",
      {
        {"Zwinność",     {{"Zwinność",3,"Bez obrażeń dłużej o 0,1 s za punkt","ITRI_DEX_01"},
                          {"Lekki krok",3,"Przerwa między fikołkami krótsza o 0,05 s, za punkt","ITAT_WING"},
                          {"Drugi oddech",1,"Dwa fikołki pod rząd, ładunek wraca po 3 s","ITFO_ADDON_SCHNELLERHERING"}}},
        {"Dystans",      {{"Dalszy przewrót",3,"+15% dystansu za punkt","ITRI_DEX_02"},
                          {"Przez szereg",1,"Fikołek przez wrogów, bez zatrzymania na nich","ITAT_SHADOWHORN"}}},
        {"Kontra",       {{"Kontratak",3,"Przez 2 s po fikołku cios +15% obrażeń, za punkt","ITMW_ELBASTARDO"},
                          {"Cios z przewrotu",1,"LPM w trakcie fikołka: cios zaraz po nim","ITMW_SHORTSWORD1"}}},
        {"Kurz",         {{"Piasek w oczy",2,"Mijani wrogowie chybiają przez 1 s, za punkt","ITMI_SULFUR"},
                          {"Podcięcie",1,"Wrogowie na drodze fikołka przewracają się","ITMW_STABKEULE"}}},
        {"Akrobatyka",   {{"Akrobata",1,"Fikołek przy upadku z wysokości: bez obrażeń","ITAR_LEATHER_L"},
                          {"Wyczucie strzał",3,"+10% szans, że strzała chybi, przez 1 s po fikołku","ITAM_PROT_POINT_01"}}},
      },
      {"Cień",1,"Po fikołku 1 s niewidzialności: wrogowie gubią cel","ITAT_SHADOWFUR"},
      "3/3 w Kontrataku", {2}, {0, {{2,0}}}},
    };
  return t;
  }

std::string ArpgSkills::nodeKey(size_t tree, int branch, int node) {
  auto& t = trees()[tree];
  if(branch<0)
    return std::string(t.name)+"/"+t.keystone.name;
  return std::string(t.name)+"/"+t.branches[size_t(branch)].nodes[size_t(node)].name;
  }

std::string ArpgSkills::branchKey(size_t tree, int branch) {
  auto& t = trees()[tree];
  return std::string(t.name)+"/"+t.branches[size_t(branch)].name;
  }

static const Node& nodeOf(const NodeId& n) {
  auto& t = trees()[n.tree];
  if(n.branch<0)
    return t.keystone;
  return t.branches[size_t(n.branch)].nodes[size_t(n.node)];
  }

static bool exists(const NodeId& n) {
  auto& tr = trees();
  if(n.tree>=tr.size())
    return false;
  auto& t = tr[n.tree];
  if(n.branch<0)
    return true;
  if(size_t(n.branch)>=t.branches.size())
    return false;
  return n.node>=0 && size_t(n.node)<t.branches[size_t(n.branch)].nodes.size();
  }

// ---------------------------------------------------------------------------------------------------------------

void ArpgProgress::clear() {
  rk.clear();
  found.clear();
  bought = 0;
  }

void ArpgProgress::save(Serialize& fout) const {
  const uint8_t ver = 1;
  fout.write(ver, bought);
  fout.write(uint32_t(rk.size()));
  for(auto& i:rk)
    fout.write(i.first, i.second);
  fout.write(uint32_t(found.size()));
  for(auto& i:found)
    fout.write(i.first, i.second);
  }

void ArpgProgress::load(Serialize& fin) {
  clear();
  uint8_t ver = 0;
  fin.read(ver, bought);
  uint32_t n = 0;
  fin.read(n);
  for(uint32_t i=0; i<n; ++i) {
    std::string k;
    int32_t     v = 0;
    fin.read(k, v);
    rk[k] = v;
    }
  fin.read(n);
  for(uint32_t i=0; i<n; ++i) {
    std::string k;
    int32_t     v = 0;
    fin.read(k, v);
    found[k] = v;
    }
  }

int32_t ArpgProgress::rankOf(const Ranks& r, const NodeId& n) const {
  if(!exists(n))
    return 0;
  auto it = r.find(nodeKey(n.tree, n.branch, n.node));
  return it!=r.end() ? it->second : 0;
  }

int32_t ArpgProgress::rank(const NodeId& n) const {
  return rankOf(rk, n);
  }

static int32_t treePointsOf(const ArpgProgress::Ranks& r, size_t tree) {
  auto&   t   = trees()[tree];
  int32_t sum = 0;
  for(int b=0; b<int(t.branches.size()); ++b)
    for(int n=0; n<int(t.branches[size_t(b)].nodes.size()); ++n) {
      auto it = r.find(nodeKey(tree, b, n));
      if(it!=r.end())
        sum += it->second;
      }
  return sum;
  }

int32_t ArpgProgress::treePoints(size_t tree) const {
  if(tree>=trees().size())
    return 0;
  return treePointsOf(rk, tree);
  }

int32_t ArpgProgress::spentFree(const Ranks& r) const {
  // points found for a branch are spent there first; everything else comes from the free points
  int32_t sum = 0;
  auto&   tr  = trees();
  for(size_t t=0; t<tr.size(); ++t) {
    for(int b=0; b<int(tr[t].branches.size()); ++b) {
      int32_t inBranch = 0;
      for(int n=0; n<int(tr[t].branches[size_t(b)].nodes.size()); ++n)
        inBranch += rankOf(r, NodeId{t,b,n});
      auto    f   = found.find(branchKey(t, b));
      int32_t use = (f!=found.end()) ? std::min(std::max(0, f->second), inBranch) : 0;
      sum += inBranch-use;
      }
    sum += rankOf(r, NodeId{t,-1,0});
    }
  return sum;
  }

int32_t ArpgProgress::freePoints(int32_t level) const {
  return std::max(0, level)*PointsPerLevel + bought - spentFree(rk);
  }

int32_t ArpgProgress::branchLeft(size_t tree, int branch) const {
  if(tree>=trees().size() || branch<0 || size_t(branch)>=trees()[tree].branches.size())
    return 0;
  auto f = found.find(branchKey(tree, branch));
  if(f==found.end())
    return 0;
  int32_t inBranch = 0;
  for(int n=0; n<int(trees()[tree].branches[size_t(branch)].nodes.size()); ++n)
    inBranch += rank(NodeId{tree,branch,n});
  return std::max(0, f->second-inBranch);
  }

bool ArpgProgress::reqMet(const Ranks& r, const NodeId& n, std::string* why) const {
  auto& t = trees()[n.tree];
  if(n.branch>=0) {
    // the first node of a branch is open; the next one needs a point in the one before it
    if(n.node==0 || rankOf(r, NodeId{n.tree, n.branch, n.node-1})>0)
      return true;
    if(why!=nullptr)
      *why = std::string("Wymaga punktu w: ")+t.branches[size_t(n.branch)].nodes[size_t(n.node-1)].name;
    return false;
    }
  auto& kr = t.keyReq;
  if(kr.treePoints>0 && treePointsOf(r, n.tree)>=kr.treePoints)
    return true;
  for(auto& m:kr.anyMaxed) {
    const NodeId id{n.tree, m.first, m.second};
    if(exists(id) && rankOf(r, id)>=nodeOf(id).max)
      return true;
    }
  if(why!=nullptr)
    *why = std::string("Wymaga: ")+t.keyReqText;
  return false;
  }

bool ArpgProgress::valid(const Ranks& r) const {
  auto& tr = trees();
  for(size_t t=0; t<tr.size(); ++t) {
    for(int b=0; b<int(tr[t].branches.size()); ++b)
      for(int n=0; n<int(tr[t].branches[size_t(b)].nodes.size()); ++n) {
        const NodeId id{t,b,n};
        if(rankOf(r, id)>0 && !reqMet(r, id, nullptr))
          return false;
        }
    const NodeId key{t,-1,0};
    if(rankOf(r, key)>0 && !reqMet(r, key, nullptr))
      return false;
    }
  return true;
  }

bool ArpgProgress::canAdd(const NodeId& n, int32_t level, std::string* why) const {
  if(!exists(n))
    return false;
  if(rank(n)>=nodeOf(n).max) {
    if(why!=nullptr)
      *why = "Pełne";
    return false;
    }
  if(!reqMet(rk, n, why))
    return false;
  if(n.branch>=0 && branchLeft(n.tree, n.branch)>0)
    return true;
  if(freePoints(level)>0)
    return true;
  if(why!=nullptr)
    *why = "Brak wolnych punktów umiejętności";
  return false;
  }

bool ArpgProgress::add(const NodeId& n, int32_t level) {
  if(!canAdd(n, level))
    return false;
  rk[nodeKey(n.tree, n.branch, n.node)] = rank(n)+1;
  return true;
  }

bool ArpgProgress::canRemove(const NodeId& n, const Ranks& base) const {
  if(!exists(n))
    return false;
  const int32_t r = rank(n);
  if(r<=0 || r<=rankOf(base, n))
    return false; // only points added since the book was opened (a reset is done by a teacher)
  Ranks test = rk;
  test[nodeKey(n.tree, n.branch, n.node)] = r-1;
  return valid(test);
  }

bool ArpgProgress::remove(const NodeId& n, const Ranks& base) {
  if(!canRemove(n, base))
    return false;
  const auto k = nodeKey(n.tree, n.branch, n.node);
  if(--rk[k]<=0)
    rk.erase(k);
  return true;
  }
