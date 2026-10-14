#include "arpgmenu.h"

#include <Tempest/Brush>
#include <Tempest/Color>

#include <algorithm>
#include <iterator>
#include <cmath>
#include <cstdio>

#include "graphics/inventoryrenderer.h"
#include "world/objects/npc.h"
#include "world/objects/item.h"
#include "world/world.h"
#include "game/gamescript.h"
#include "utils/gthfont.h"
#include "resources.h"
#include "gothic.h"

using namespace Tempest;

// ---------------------------------------------------------------------------------------------------------------
// text

static bool polishGame() {
  // Gothic's fonts draw the bytes of the game's code page; the Polish game is Windows-1250 ("Wartość:" in the scripts)
  static int cached = -1;
  if(cached>=0)
    return cached==1;
  auto w = Gothic::inst().world();
  if(w==nullptr)
    return true;
  bool pl = false;
  if(auto sym = w->script().findSymbol("NAME_VALUE")) {
    auto& s = sym->get_string();
    pl = (s.rfind("Warto",0)==0) || s.find('\x9c')!=std::string::npos;
    }
  cached = pl ? 1 : 0;
  return pl;
  }

std::string ArpgMenu::gtxt(std::string_view s) {
  struct Map { uint32_t cp; unsigned char cp1250; char ascii; };
  static const Map map[] = {
    {0x105,0xB9,'a'},{0x104,0xA5,'A'},{0x107,0xE6,'c'},{0x106,0xC6,'C'},{0x119,0xEA,'e'},{0x118,0xCA,'E'},
    {0x142,0xB3,'l'},{0x141,0xA3,'L'},{0x144,0xF1,'n'},{0x143,0xD1,'N'},{0x0F3,0xF3,'o'},{0x0D3,0xD3,'O'},
    {0x15B,0x9C,'s'},{0x15A,0x8C,'S'},{0x17A,0x9F,'z'},{0x179,0x8F,'Z'},{0x17C,0xBF,'z'},{0x17B,0xAF,'Z'},
    };
  const bool  pl = polishGame();
  std::string out;
  out.reserve(s.size());
  for(size_t i=0; i<s.size();) {
    const auto c = uint8_t(s[i]);
    uint32_t   cp  = c;
    size_t     len = 1;
    if((c&0xE0)==0xC0 && i+1<s.size()) {
      cp  = (uint32_t(c&0x1F)<<6) | uint32_t(uint8_t(s[i+1])&0x3F);
      len = 2;
      }
    else if((c&0xF0)==0xE0 && i+2<s.size()) {
      cp  = (uint32_t(c&0x0F)<<12) | (uint32_t(uint8_t(s[i+1])&0x3F)<<6) | uint32_t(uint8_t(s[i+2])&0x3F);
      len = 3;
      }
    i += len;
    if(cp<0x80) {
      out.push_back(char(cp));
      continue;
      }
    char rep = '?';
    for(auto& m:map)
      if(m.cp==cp) {
        rep = (pl || cp==0xF3 || cp==0xD3) ? char(m.cp1250) : m.ascii;
        break;
        }
    if(cp==0x2013 || cp==0x2014 || cp==0xB7)
      rep = '-';
    else if(cp==0xD7)
      rep = 'x';
    out.push_back(rep);
    }
  return out;
  }

static const GthFont& font(Resources::FontType t, float scale) {
  return Resources::font(t, scale);
  }

static const GthFont& bigFont(Resources::FontType t, float scale) {
  return Resources::font("font_old_20_white.tga", t, scale);
  }

// y = top of the line
static void text(Painter& p, const GthFont& f, int x, int y, std::string_view s) {
  f.drawText(p, x, y+f.pixelSize(), s);
  }

static void textRight(Painter& p, const GthFont& f, int right, int y, std::string_view s) {
  f.drawText(p, right-f.textSize(s).w, y+f.pixelSize(), s);
  }

static void textCenter(Painter& p, const GthFont& f, int cx, int y, std::string_view s) {
  f.drawText(p, cx-f.textSize(s).w/2, y+f.pixelSize(), s);
  }

// ---------------------------------------------------------------------------------------------------------------
// skill trees (descriptions only for now)

namespace {
struct TreeNode   { const char* name; int max; const char* desc; };
struct TreeBranch { const char* name; std::vector<TreeNode> nodes; };
struct Tree       { const char* name; const char* key; const char* root; std::vector<TreeBranch> branches; TreeNode keystone; const char* keyReq; };
}

static const std::vector<Tree>& skillTrees() {
  static const std::vector<Tree> t = {
    {"Cios", "LPM", "Zwykły atak bronią białą lub strzał z łuku",
      {
        {"Ostrze",       {{"Ostra klinga",5,"+4% obrażeń ciosu za punkt"},
                          {"Słaby punkt",3,"+5% szansy na pełne obrażenia (krytyk Gothica)"},
                          {"Głębokie rany",3,"Trafienie: krwawienie 5% obrażeń na sekundę przez 3 s, za punkt"}}},
        {"Rytm",         {{"Płynne ruchy",5,"+3% szybkości ataku za punkt"},
                          {"Wyczucie chwili",2,"Okno combo dłuższe o 25% za punkt"},
                          {"Czwarty cios",1,"Seria kończy się dodatkowym ciosem x1,5"}}},
        {"Zamach",       {{"Szeroki łuk",3,"Łuk cięcia 120 stopni, +20 za punkt (do 180)"},
                          {"Odrzut",2,"Cios odpycha małe stworzenia o 0,5 m za punkt"},
                          {"Dobicie",3,"+15% obrażeń wobec powalonych za punkt"}}},
        {"Krew za krew", {{"Pijawka",3,"2% zadanych obrażeń wraca jako zdrowie, za punkt"},
                          {"Twarda skóra",3,"Po trafieniu +5 ochrony przed bronią na 3 s, za punkt"}}},
        {"Łucznik",      {{"Szybki naciąg",5,"+4% szybkości strzałów za punkt"},
                          {"Przebicie",2,"Strzała przelatuje przez 1 wroga za punkt"},
                          {"Celne oko",3,"+5% szansy trafienia powyżej 15 m, za punkt"}}},
      },
      {"Wir",1,"Ostatni cios serii: obrót dookoła, trafia wszystkich wokół"}, "3/3 w Dobiciu albo Czwarty cios"},
    {"Atak ładowany", "PPM", "Ładuj 2 s, puść w złotym oknie: x3, zamach 180 stopni",
      {
        {"Ładowanie",    {{"Skupienie",4,"Ładowanie krótsze o 0,2 s za punkt (do 1,2 s)"},
                          {"Szersze okno",2,"Złote okno puszczenia +0,1 s za punkt"},
                          {"Pewna ręka",1,"Spóźnione puszczenie daje x2, a nie zwykły cios"}}},
        {"Siła",         {{"Miażdżący cios",5,"Mnożnik obrażeń +0,2 za punkt (do x4)"},
                          {"Wstrząs",3,"Powaleni wstają o 0,5 s później, za punkt"},
                          {"Łamacz gardy",1,"Atak ładowany przebija blok przeciwnika"}}},
        {"Fala",         {{"Fala uderzeniowa",1,"Zamach puszcza falę na 4 m przed postać (50% obrażeń)"},
                          {"Dalsza fala",3,"+1 m zasięgu fali za punkt"}}},
        {"Odnowienie",   {{"Opanowanie",4,"Odnowienie krótsze o 0,5 s za punkt"},
                          {"Pęd bitewny",1,"Zabicie atakiem ładowanym zeruje odnowienie"}}},
        {"Łuk",          {{"Strzała przeszywająca",1,"Strzał ładowany przebija wszystkich w linii"},
                          {"Grad strzał",3,"+1 strzała w wachlarzu za punkt"},
                          {"Ciężki grot",2,"Strzał ładowany odpycha, przy 2/2 przewraca"}}},
      },
      {"Gniew Innosa",1,"Atak ładowany podpala wrogów: ogień przez 4 s"}, "10 punktów w drzewku"},
    {"Fikołek", "Spacja", "Przewrót w stronę kursora, chwila bez obrażeń",
      {
        {"Zwinność",     {{"Zwinność",3,"Bez obrażeń dłużej o 0,1 s za punkt"},
                          {"Lekki krok",3,"Przerwa między fikołkami krótsza o 0,05 s, za punkt"},
                          {"Drugi oddech",1,"Dwa fikołki pod rząd, ładunek wraca po 3 s"}}},
        {"Dystans",      {{"Dalszy przewrót",3,"+15% dystansu za punkt"},
                          {"Przez szereg",1,"Fikołek przez wrogów, bez zatrzymania na nich"}}},
        {"Kontra",       {{"Kontratak",3,"Przez 2 s po fikołku cios +15% obrażeń, za punkt"},
                          {"Cios z przewrotu",1,"LPM w trakcie fikołka: cios zaraz po nim"}}},
        {"Kurz",         {{"Piasek w oczy",2,"Mijani wrogowie chybiają przez 1 s, za punkt"},
                          {"Podcięcie",1,"Wrogowie na drodze fikołka przewracają się"}}},
        {"Akrobatyka",   {{"Akrobata",1,"Fikołek przy upadku z wysokości: bez obrażeń"},
                          {"Wyczucie strzał",3,"+10% szans, że strzała chybi, przez 1 s po fikołku"}}},
      },
      {"Cień",1,"Po fikołku 1 s niewidzialności: wrogowie gubią cel"}, "3/3 w Kontrataku"},
    };
  return t;
  }

// ---------------------------------------------------------------------------------------------------------------

void ArpgMenu::open(Page p) {
  page   = p;
  scroll = 0;
  }

void ArpgMenu::close() {
  page     = P_Closed;
  hoverCls = 0;
  hits.clear();
  }

void ArpgMenu::toggle(Page p) {
  if(page==p)
    close(); else
    open(p);
  }

const ArpgMenu::Hit* ArpgMenu::hitAt(const Point& pos) const {
  for(auto& h:hits)
    if(h.r.contains(pos))
      return &h;
  return nullptr;
  }

void ArpgMenu::mouseWheel(int delta) {
  if(page!=P_Equip)
    return;
  scroll = std::clamp(scroll + (delta>0 ? -1 : 1), 0, maxScroll);
  }

bool ArpgMenu::mouseDown(const Point& pos, bool right, Npc& pl) {
  auto h = hitAt(pos);
  if(h==nullptr)
    return false;
  switch(h->kind) {
    case H_Tab:
      open(Page(h->id));
      return true;
    case H_Cat:
      cat    = h->id;
      scroll = 0;
      return true;
    case H_Tree:
      tree = h->id;
      return true;
    case H_Pack:
    case H_Equip:
      if(right || h->cls==0)
        return true; // right button: later (drop)
      if(h->equipped)
        pl.unequipItem(h->cls); else
        pl.useItem(h->cls);      // equip / drink / read, like in Gothic's inventory
      return true;
    }
  return false;
  }

bool ArpgMenu::spellKey(uint8_t slot, Npc& pl) {
  if(page!=P_Equip || hoverCls==0)
    return false;
  for(auto it = pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    if(it->clsId()!=hoverCls)
      continue;
    if((ItmFlags(it->mainFlag()) & ITM_CAT_RUNE)==0)
      return false;
    pl.useItem(hoverCls, slot, false);
    return true;
    }
  return false;
  }

// ---------------------------------------------------------------------------------------------------------------
// drawing helpers: Gothic's own textures

void ArpgMenu::drawPanel(Painter& p, const Rect& r) const {
  if(auto t = Resources::loadTexture("MENU_INGAME.TGA")) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    } else {
    p.setBrush(Brush(Color(0.f,0.f,0.f,0.7f), Painter::Alpha));
    p.drawRect(r.x, r.y, r.w, r.h);
    }
  }

void ArpgMenu::drawSlot(Painter& p, const Rect& r, bool equipped, bool hover) const {
  const char* name = equipped ? "INV_SLOT_EQUIPPED.TGA" : "INV_SLOT.TGA";
  if(auto t = Resources::loadTexture(name)) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  if(hover) {
    // the framed slot of Gothic's inventory (metal corners)
    if(auto t = Resources::loadTexture(equipped ? "INV_SLOT_EQUIPPED_FOCUS.TGA" : "INV_SLOT_FOCUS.TGA")) {
      const int d = r.w/10;
      p.setBrush(*t);
      p.drawRect(r.x-d, r.y-d, r.w+2*d, r.h+2*d, 0, 0, t->w(), t->h());
      }
    }
  }

void ArpgMenu::drawBar(Painter& p, const Rect& r, const char* fill, float v) const {
  if(auto t = Resources::loadTexture("BAR_BACK.TGA")) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  if(auto t = Resources::loadTexture(fill)) {
    const int bx = r.w*9/256, by = r.h*7/32; // the frame of BAR_BACK
    const int iw = int(float(r.w-2*bx)*std::clamp(v, 0.f, 1.f));
    p.setBrush(*t);
    p.drawRect(r.x+bx, r.y+by, iw, r.h-2*by, 0, 0, int(float(t->w())*std::clamp(v,0.f,1.f)), t->h());
    }
  }

void ArpgMenu::drawTabs(Painter& p, int w, float s, const Rect& left, const Rect& right) {
  static const char* names[3] = {"Ekwipunek (I)", "Postać (C)", "Umiejętności (T)"};
  const int gap = int(8*s);
  const int tw  = std::min(int(230*s), (right.x-(left.x+left.w)-4*gap)/3);
  const int th  = int(40*s);
  const int y   = left.y-th-int(10*s);
  const int x0  = (w-(3*tw+2*gap))/2;
  auto  bar = Resources::loadTexture("MENU_CHOICE_BACK.TGA");
  for(int i=0; i<3; ++i) {
    const Rect r(x0+i*(tw+gap), y, tw, th);
    const bool on = (page==P_Equip+i);
    const bool hv = r.contains(mouse);
    if(bar!=nullptr) {
      p.setBrush(Brush(*bar, on ? Color(1.f) : Color(0.75f,0.75f,0.75f,0.85f)));
      p.drawRect(r.x, r.y, r.w, r.h, 0, 0, bar->w(), bar->h());
      }
    auto& f = font(on ? Resources::FontType::Hi : (hv ? Resources::FontType::Normal : Resources::FontType::Disabled), s);
    textCenter(p, f, r.x+r.w/2, r.y+(r.h-f.pixelSize())/2, gtxt(names[i]));
    Hit h; h.r = r; h.kind = H_Tab; h.id = P_Equip+i;
    hits.push_back(h);
    }
  }

// ---------------------------------------------------------------------------------------------------------------

void ArpgMenu::paint(Painter& p, int w, int h, float s, InventoryRenderer& items, Npc& pl) {
  if(page==P_Closed)
    return;
  hits.clear();
  hoverCls = 0;

  const int M   = int(20*s);
  const int pw  = std::min(int(600*s), int(float(w)*0.36f));
  const int top = M+int(56*s);
  const Rect left (M,        top, pw, h-top-M);
  const Rect right(w-M-pw,   top, pw, h-top-M);

  drawPanel(p, left);
  drawPanel(p, right);
  drawTabs(p, w, s, left, right);

  switch(page) {
    case P_Closed:    break;
    case P_Equip:     paintEquip(p, s, left, right, items, pl); break;
    case P_Character: paintStats(p, s, left, right, pl);        break;
    case P_Skills:    paintTrees(p, s, left, right);            break;
    }

  // the hero stands in the middle: name under his feet
  auto& f = font(Resources::FontType::Disabled, s);
  textCenter(p, f, w/2, h-M-f.pixelSize(), gtxt("Q / E - obrót kamery   ·   Esc - zamknij"));
  }

static int itemCategory(const Item& it) {
  // 0 everything, 1 weapons, 2 armor, 3 magic, 4 potions, 5 food, 6 documents, 7 other
  const auto f = ItmFlags(it.mainFlag());
  if(f & (ITM_CAT_NF|ITM_CAT_FF|ITM_CAT_MUN))
    return 1;
  if(f & ITM_CAT_ARMOR)
    return 2;
  if(f & (ITM_CAT_RUNE|ITM_CAT_MAGIC))
    return 3;
  if(f & ITM_CAT_POTION)
    return 4;
  if(f & ITM_CAT_FOOD)
    return 5;
  if(f & ITM_CAT_DOCS)
    return 6;
  return 7;
  }

void ArpgMenu::paintEquip(Painter& p, float s, const Rect& left, const Rect& right, InventoryRenderer& items, Npc& pl) {
  auto&      inv  = pl.inventory();
  const int  pad  = int(18*s);
  auto&      fN   = font(Resources::FontType::Normal,   s);
  auto&      fD   = font(Resources::FontType::Disabled, s);
  auto&      fH   = font(Resources::FontType::Hi,       s);
  auto&      fBig = bigFont(Resources::FontType::Normal, s);
  const Item* info = nullptr;

  // ---- left: what the hero wears ----
  int y = left.y+pad;
  text(p, fBig, left.x+pad, y, gtxt("Bezimienny"));
  y += fBig.pixelSize()+int(4*s);
  {
    char lvl[64] = {};
    std::snprintf(lvl, sizeof(lvl), "Poziom %d", int(pl.level()));
    std::string line = gtxt(lvl);
    if(auto w = Gothic::inst().world())
      if(auto g = w->script().findSymbol("TXT_GUILDS"))
        line = std::string(g->get_string(uint16_t(pl.guild()))) + "  -  " + line;
    text(p, fD, left.x+pad, y, line);
    y += fD.pixelSize()+int(14*s);
  }

  static const struct { uint8_t slot; const char* name; } slots[] = {
    {0,"Broń biała"}, {1,"Broń dystansowa"}, {2,"Pancerz"}, {3,"Amulet"}, {4,"Pierścień"}, {5,"Pierścień"}, {6,"Pas"},
    };
  const int colW = (left.w-2*pad-pad)/2;
  const int ss   = std::min(int(92*s), colW/2);
  for(size_t i=0; i<std::size(slots); ++i) {
    const int  cx = left.x+pad + int(i%2)*(colW+pad);
    const int  cy = y + int(i/2)*(ss+int(14*s));
    const Rect r(cx, cy, ss, ss);
    const Item* it = inv.equippedIn(slots[i].slot);
    const bool hv  = r.contains(mouse);
    drawSlot(p, r, it!=nullptr, hv);
    if(it!=nullptr)
      items.drawItem(r.x+ss/10, r.y+ss/10, ss-ss/5, ss-ss/5, *it);
    text(p, fD, cx+ss+int(10*s), cy+int(6*s), gtxt(slots[i].name));
    if(it!=nullptr) {
      const int tw = colW-ss-int(14*s);
      fN.drawText(p, cx+ss+int(10*s), cy+int(10*s)+fD.pixelSize(), tw, ss-fD.pixelSize(), it->description(), AlignLeft);
      } else {
      text(p, fD, cx+ss+int(10*s), cy+int(10*s)+fD.pixelSize(), "-");
      }
    if(it!=nullptr) {
      Hit h; h.r = r; h.kind = H_Equip; h.cls = it->clsId(); h.equipped = true;
      hits.push_back(h);
      if(hv) {
        hoverCls = it->clsId();
        info     = it;
        }
      }
    }
  y += int((std::size(slots)+1)/2)*(ss+int(14*s)) + int(6*s);

  // life and mana (Gothic's bars)
  {
    const int bw = left.w-2*pad, bh = int(22*s);
    char txt[64] = {};
    drawBar(p, Rect(left.x+pad, y, bw, bh), "BAR_HEALTH.TGA",
            float(pl.attribute(ATR_HITPOINTS))/float(std::max(1,pl.attribute(ATR_HITPOINTSMAX))));
    std::snprintf(txt, sizeof(txt), "%d / %d", int(pl.attribute(ATR_HITPOINTS)), int(pl.attribute(ATR_HITPOINTSMAX)));
    textCenter(p, fN, left.x+left.w/2, y+(bh-fN.pixelSize())/2, txt);
    y += bh+int(6*s);
    drawBar(p, Rect(left.x+pad, y, bw, bh), "BAR_MANA.TGA",
            float(pl.attribute(ATR_MANA))/float(std::max(1,pl.attribute(ATR_MANAMAX))));
    std::snprintf(txt, sizeof(txt), "%d / %d", int(pl.attribute(ATR_MANA)), int(pl.attribute(ATR_MANAMAX)));
    textCenter(p, fN, left.x+left.w/2, y+(bh-fN.pixelSize())/2, txt);
    y += bh+int(14*s);
  }

  // runes and scrolls under the keys 3-0 (1 and 2 drink potions)
  {
    text(p, fD, left.x+pad, y, gtxt("Klawisze 3–0: najedź na runę albo zwój w plecaku i naciśnij klawisz"));
    y += fD.pixelSize()+int(8*s);
    const int gap = int(6*s);
    const int qs  = std::min(int(56*s), (left.w-2*pad-7*gap)/8);
    for(uint8_t k=0; k<8; ++k) {
      const Rect r(left.x+pad+k*(qs+gap), y, qs, qs);
      const Item* it = inv.currentSpell(k);
      const bool  hv = r.contains(mouse);
      drawSlot(p, r, false, hv);
      if(it!=nullptr) {
        items.drawItem(r.x+qs/10, r.y+qs/10, qs-qs/5, qs-qs/5, *it);
        if(hv) {
          hoverCls = it->clsId();
          info     = it;
          }
        }
      Hit h; h.r = r; h.kind = H_Equip; h.id = -1; h.cls = 0; h.count = (k+3)%10; // key number for the overlay
      if(it!=nullptr) {
        h.cls      = it->clsId();
        h.equipped = true;
        }
      hits.push_back(h);
      }
  }

  // ---- right: the backpack ----
  y = right.y+pad;
  text(p, fBig, right.x+pad, y, gtxt("Plecak"));
  {
    int32_t gold = 0;
    if(auto w = Gothic::inst().world())
      if(auto g = w->script().goldId())
        gold = int32_t(pl.itemCount(g->index()));
    char txt[64] = {};
    std::snprintf(txt, sizeof(txt), "%s %d", gtxt("Złoto:").c_str(), int(gold));
    textRight(p, fN, right.x+right.w-pad, y+int(6*s), txt);
  }
  y += fBig.pixelSize()+int(14*s);

  static const char* cats[8] = {"Wszystko","Broń","Pancerze","Magia","Mikstury","Jedzenie","Dokumenty","Inne"};
  {
    const int cw = (right.w-2*pad)/4;
    const int ch = fN.pixelSize()+int(10*s);
    auto bar = Resources::loadTexture("INV_TITEL.TGA");
    for(int i=0; i<8; ++i) {
      const Rect r(right.x+pad+(i%4)*cw, y+(i/4)*ch, cw-int(4*s), ch-int(2*s));
      const bool on = (cat==i), hv = r.contains(mouse);
      if(on && bar!=nullptr) {
        p.setBrush(*bar);
        p.drawRect(r.x, r.y, r.w, r.h, 0, 0, bar->w(), bar->h());
        }
      auto& f = on ? fH : (hv ? fN : fD);
      textCenter(p, f, r.x+r.w/2, r.y+(r.h-f.pixelSize())/2, gtxt(cats[i]));
      Hit h; h.r = r; h.kind = H_Cat; h.id = i;
      hits.push_back(h);
      }
    y += 2*ch+int(10*s);
  }

  const int descH = int(150*s);
  const int cols  = 6;
  const int gap   = int(6*s);
  const int cs    = (right.w-2*pad-(cols-1)*gap)/cols;
  const int rows  = std::max(1, (right.y+right.h-pad-descH-int(10*s)-y+gap)/(cs+gap));

  struct Cell { const Item* it; size_t cls; int32_t count; bool eq; };
  std::vector<Cell> list;
  const size_t goldId = [&]() -> size_t {
    if(auto w = Gothic::inst().world())
      if(auto g = w->script().goldId())
        return g->index();
    return size_t(-1);
    }();
  for(auto it = inv.iterator(Inventory::T_Inventory); it.isValid(); ++it) {
    const Item& itm = *it;
    if(itm.clsId()==goldId)
      continue;
    if(cat!=0 && itemCategory(itm)!=cat)
      continue;
    list.push_back({&itm, itm.clsId(), int32_t(it.count()), it.isEquipped()});
    }
  const int totalRows = int((list.size()+cols-1)/cols);
  maxScroll = std::max(0, totalRows-rows);
  scroll    = std::clamp(scroll, 0, maxScroll);
  for(int i=0; i<rows*cols; ++i) {
    const Rect   r(right.x+pad+(i%cols)*(cs+gap), y+(i/cols)*(cs+gap), cs, cs);
    const size_t id = size_t(i+scroll*cols);
    const bool   hv = r.contains(mouse);
    if(id>=list.size()) {
      drawSlot(p, r, false, false);
      continue;
      }
    auto& c = list[id];
    drawSlot(p, r, c.eq, hv);
    items.drawItem(r.x+cs/10, r.y+cs/10, cs-cs/5, cs-cs/5, *c.it);
    Hit h; h.r = r; h.kind = H_Pack; h.cls = c.cls; h.equipped = c.eq; h.count = c.count;
    hits.push_back(h);
    if(hv) {
      hoverCls = c.cls;
      info     = c.it;
      }
    }
  if(maxScroll>0) {
    char txt[32] = {};
    std::snprintf(txt, sizeof(txt), "%d / %d", scroll+1, maxScroll+1);
    textRight(p, fD, right.x+right.w-pad, y+rows*(cs+gap), txt);
    }

  paintInfo(p, s, Rect(right.x+pad, right.y+right.h-pad-descH, right.w-2*pad, descH), items, info);
  }

void ArpgMenu::paintInfo(Painter& p, float s, const Rect& r, InventoryRenderer& items, const Item* it) const {
  // like the description box of Gothic's inventory
  if(auto t = Resources::loadTexture("INV_DESC.TGA")) {
    p.setBrush(*t);
    p.drawRect(r.x, r.y, r.w, r.h, 0, 0, t->w(), t->h());
    }
  auto& fN = font(Resources::FontType::Normal,   s);
  auto& fD = font(Resources::FontType::Disabled, s);
  const int pad = int(12*s);
  if(it==nullptr) {
    textCenter(p, fD, r.x+r.w/2, r.y+(r.h-fD.pixelSize())/2,
               gtxt("Najedź na przedmiot. LPM: załóż / użyj / zdejmij"));
    return;
    }
  const int mw = r.h-2*pad;          // model on the right
  const int tw = r.w-2*pad-mw-pad;
  textCenter(p, fN, r.x+pad+tw/2, r.y+pad, it->description());
  int y = r.y+pad+fN.pixelSize()+int(6*s);
  for(size_t i=0; i<Item::MAX_UI_ROWS; ++i) {
    auto    txt = it->uiText(i);
    int32_t val = it->uiValue(i);
    if(txt.empty())
      continue;
    if(y+fD.pixelSize()>r.y+r.h-pad/2)
      break;
    text(p, fD, r.x+pad, y, txt);
    if(val!=0) {
      char v[32] = {};
      std::snprintf(v, sizeof(v), "%d", int(val));
      textRight(p, fN, r.x+pad+tw, y, v);
      }
    y += fD.pixelSize()+int(2*s);
    }
  items.drawItem(r.x+r.w-pad-mw, r.y+pad, mw, mw, *it);
  }

void ArpgMenu::paintStats(Painter& p, float s, const Rect& left, const Rect& right, Npc& pl) {
  const int pad  = int(18*s);
  auto&     fN   = font(Resources::FontType::Normal,   s);
  auto&     fD   = font(Resources::FontType::Disabled, s);
  auto&     fH   = font(Resources::FontType::Hi,       s);
  auto&     fBig = bigFont(Resources::FontType::Normal, s);
  auto      w    = Gothic::inst().world();
  const int line = fN.pixelSize()+int(8*s);
  char      buf[128] = {};

  auto row = [&](const Rect& panel, int y, std::string_view k, std::string_view v) {
    text(p, fD, panel.x+pad, y, k);
    textRight(p, fN, panel.x+panel.w-pad, y, v);
    };

  // ---- left: the hero ----
  int y = left.y+pad;
  text(p, fBig, left.x+pad, y, gtxt("Bezimienny"));
  y += fBig.pixelSize()+int(4*s);
  if(w!=nullptr)
    if(auto g = w->script().findSymbol("TXT_GUILDS")) {
      text(p, fD, left.x+pad, y, g->get_string(uint16_t(pl.guild())));
      y += fD.pixelSize()+int(12*s);
      }

  std::snprintf(buf, sizeof(buf), "%d", int(pl.level()));
  row(left, y, gtxt("Poziom"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.experience()), int(pl.experienceNext()));
  row(left, y, gtxt("Doświadczenie"), buf);
  y += line;
  {
    const int bh = int(18*s);
    const float v = pl.experienceNext()>0 ? float(pl.experience())/float(pl.experienceNext()) : 0.f;
    drawBar(p, Rect(left.x+pad, y, left.w-2*pad, bh), "BAR_MISC.TGA", v);
    y += bh+int(10*s);
  }
  std::snprintf(buf, sizeof(buf), "%d", int(pl.learningPoints()));
  row(left, y, gtxt("Punkty nauki"), buf);
  y += line;
  row(left, y, gtxt("Punkty umiejętności"), "0");
  y += line;
  text(p, fD, left.x+pad, y, gtxt("(do drzewek; później nauczyciele zamienią na nie punkty nauki)"));
  y += line+int(8*s);

  text(p, fH, left.x+pad, y, gtxt("Atrybuty"));
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_STRENGTH)));
  row(left, y, gtxt("Siła"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_DEXTERITY)));
  row(left, y, gtxt("Zręczność"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.attribute(ATR_HITPOINTS)), int(pl.attribute(ATR_HITPOINTSMAX)));
  row(left, y, gtxt("Zdrowie"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d / %d", int(pl.attribute(ATR_MANA)), int(pl.attribute(ATR_MANAMAX)));
  row(left, y, gtxt("Mana"), buf);
  y += line+int(8*s);

  text(p, fH, left.x+pad, y, gtxt("Ochrona"));
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_EDGE)));
  row(left, y, gtxt("Przed bronią"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_POINT)));
  row(left, y, gtxt("Przed strzałami"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_FIRE)));
  row(left, y, gtxt("Przed ogniem"), buf);
  y += line;
  std::snprintf(buf, sizeof(buf), "%d", int(pl.protection(PROT_MAGIC)));
  row(left, y, gtxt("Przed magią"), buf);

  // ---- right: Gothic's talents (names from the scripts) and the ARPG fight ----
  y = right.y+pad;
  text(p, fBig, right.x+pad, y, gtxt("Umiejętności Gothica"));
  y += fBig.pixelSize()+int(12*s);
  if(w!=nullptr) {
    auto& sc   = w->script();
    auto* tal  = sc.findSymbol("TXT_TALENTS");
    auto* talV = sc.findSymbol("TXT_TALENTS_SKILLS");
    if(tal!=nullptr && talV!=nullptr) {
      const bool g2  = Gothic::inst().version().game==2;
      const int  max = g2 ? TALENT_MAX_G2 : TALENT_MAX_G1;
      for(int i=0; i<max; ++i) {
        auto& name = tal->get_string(uint16_t(i));
        if(name.empty())
          continue;
        // skill level: "a|b|c" from the scripts, picked by the talent's skill
        std::string_view lv = talV->get_string(uint16_t(i));
        int sk = pl.talentSkill(Talent(i));
        for(int k=0; k<sk; ++k) {
          auto pos = lv.find('|');
          if(pos==std::string_view::npos) {
            lv = {};
            break;
            }
          lv = lv.substr(pos+1);
          }
        lv = lv.substr(0, lv.find('|'));
        std::string val(lv);
        if(i>=TALENT_1H && i<=TALENT_CROSSBOW) {
          std::snprintf(buf, sizeof(buf), "  %d%%", int(g2 ? pl.hitChance(Talent(i)) : pl.talentValue(Talent(i))));
          val += buf;
          }
        row(right, y, name, val);
        y += line;
        if(y>right.y+right.h/2+int(40*s))
          break;
        }
      }
    }
  y += int(10*s);

  text(p, fH, right.x+pad, y, gtxt("Walka"));
  y += line;
  {
    auto dmgOf = [](const Item* it) { return it!=nullptr ? int(it->handle().damage_total) : 0; };
    auto& inv  = pl.inventory();
    std::snprintf(buf, sizeof(buf), "%d", int(pl.attribute(ATR_STRENGTH))+dmgOf(inv.equippedIn(0)));
    row(right, y, gtxt("Obrażenia wręcz (siła + broń)"), buf);
    y += line;
    if(inv.equippedIn(1)!=nullptr) {
      std::snprintf(buf, sizeof(buf), "%d", (int(pl.attribute(ATR_DEXTERITY))+dmgOf(inv.equippedIn(1)))/2);
      row(right, y, gtxt("Obrażenia strzałą (x0,5)"), buf);
      y += line;
      }
    row(right, y, gtxt("Szybkość ataku"), "+20%");
    y += line;
    row(right, y, gtxt("Atak ładowany"), gtxt("x3, co 5 s"));
    y += line;
    row(right, y, gtxt("Fikołek bez obrażeń"), "0,45 s");
  }
  }

void ArpgMenu::paintTrees(Painter& p, float s, const Rect& left, const Rect& right) {
  const int pad  = int(18*s);
  auto&     fN   = font(Resources::FontType::Normal,   s);
  auto&     fD   = font(Resources::FontType::Disabled, s);
  auto&     fH   = font(Resources::FontType::Hi,       s);
  auto&     fY   = font(Resources::FontType::Yellow,   s);
  auto&     fBig = bigFont(Resources::FontType::Normal, s);
  auto&     tr   = skillTrees();
  tree = std::clamp(tree, 0, int(tr.size())-1);

  // ---- left: the trees ----
  int y = left.y+pad;
  text(p, fBig, left.x+pad, y, gtxt("Drzewka umiejętności"));
  y += fBig.pixelSize()+int(14*s);
  const int bh = int(64*s);
  for(size_t i=0; i<tr.size(); ++i) {
    const Rect r(left.x+pad, y, left.w-2*pad, bh);
    const bool on = (int(i)==tree), hv = r.contains(mouse);
    drawSlot(p, r, on, hv);
    char head[96] = {};
    std::snprintf(head, sizeof(head), "%s  (%s)", tr[i].name, tr[i].key);
    text(p, on ? fH : fN, r.x+int(14*s), r.y+int(8*s), gtxt(head));
    text(p, fD, r.x+int(14*s), r.y+int(12*s)+fN.pixelSize(), gtxt(tr[i].root));
    Hit h; h.r = r; h.kind = H_Tree; h.id = int(i);
    hits.push_back(h);
    y += bh+int(10*s);
    }
  y += int(10*s);
  text(p, fN, left.x+pad, y, gtxt("Punkty umiejętności: 0"));
  y += fN.pixelSize()+int(8*s);
  fD.drawText(p, left.x+pad, y+fD.pixelSize(), left.w-2*pad, left.y+left.h-y-pad,
              gtxt("Każda umiejętność ma własne drzewko (jak w Last Epoch). Na razie same opisy, bez działania w grze. "
                   "Punkty umiejętności dadzą później nauczyciele w zamian za punkty nauki."), AlignLeft);

  // ---- right: the branches of the chosen tree ----
  auto& t = tr[size_t(tree)];
  y = right.y+pad;
  text(p, fBig, right.x+pad, y, gtxt(t.name));
  y += fBig.pixelSize()+int(10*s);
  const int bottom = right.y+right.h-pad;
  for(auto& b:t.branches) {
    if(y+fY.pixelSize()>bottom)
      break;
    text(p, fY, right.x+pad, y, gtxt(b.name));
    y += fY.pixelSize()+int(4*s);
    for(auto& n:b.nodes) {
      if(y+fN.pixelSize()>bottom)
        break;
      char rank[16] = {};
      std::snprintf(rank, sizeof(rank), "0/%d", n.max);
      text(p, fN, right.x+pad+int(14*s), y, gtxt(n.name));
      textRight(p, fH, right.x+right.w-pad, y, rank);
      y += fN.pixelSize()+int(2*s);
      const int lw = right.w-2*pad-int(28*s);
      const int lh = fD.lineCount(lw, gtxt(n.desc))*fD.pixelSize();
      fD.drawText(p, right.x+pad+int(28*s), y+fD.pixelSize(), lw, lh+fD.pixelSize(), gtxt(n.desc), AlignLeft);
      y += lh+int(6*s);
      }
    y += int(4*s);
    }
  if(y+3*fN.pixelSize()<=bottom) {
    text(p, fY, right.x+pad, y, gtxt("Węzeł kluczowy: ")+gtxt(t.keystone.name)+"  ("+gtxt(t.keyReq)+")");
    y += fY.pixelSize()+int(2*s);
    fD.drawText(p, right.x+pad+int(28*s), y+fD.pixelSize(), right.w-2*pad-int(28*s), bottom-y, gtxt(t.keystone.desc), AlignLeft);
    }
  }

void ArpgMenu::paintOverlay(Painter& p, float s, Npc&) {
  // over the item models: how many, and the key of each rune slot
  if(page!=P_Equip)
    return;
  auto& fN = font(Resources::FontType::Normal,   s*0.9f);
  auto& fD = font(Resources::FontType::Disabled, s*0.85f);
  p.setBrush(Color(1.f));
  for(auto& h:hits) {
    if(h.kind==H_Pack && h.count>1) {
      char txt[16] = {};
      std::snprintf(txt, sizeof(txt), "%d", int(h.count));
      textRight(p, fN, h.r.x+h.r.w-int(4*s), h.r.y+h.r.h-fN.pixelSize()-int(4*s), txt);
      }
    if(h.kind==H_Equip && h.id==-1) {
      char txt[8] = {};
      std::snprintf(txt, sizeof(txt), "%d", int(h.count));
      text(p, fD, h.r.x+int(3*s), h.r.y+int(1*s), txt);
      }
    }
  }
