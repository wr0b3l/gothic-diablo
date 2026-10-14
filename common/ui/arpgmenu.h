#pragma once

#include <Tempest/Painter>
#include <Tempest/Rect>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

class Npc;
class Item;
class World;
class InventoryRenderer;
class GthFont;

// gothic-diablo: the ARPG window: equipment, character, skill trees.
// The hero himself is not drawn here: while it is open the camera stands in front of him (Camera::setArpgPortrait),
// so he is seen in the middle of the screen between the two pages, with his idle animation and his real equipment.
// Drawn as an open book of the game (BOOK_BROWN_L/R pages, the book fonts, LETTERS sheets, INV_SLOT frames)
// with the game's item models (InventoryRenderer) - also as the icons of the skill tree nodes.
class ArpgMenu final {
  public:
    ArpgMenu();
    ~ArpgMenu();

    enum Page : uint8_t {
      P_Closed,
      P_Equip,
      P_Character,
      P_Skills,
      };

    bool   isOpen()  const { return page!=P_Closed; }
    Page   current() const { return page; }
    void   open(Page p);
    void   close();
    void   toggle(Page p);   // the page that is shown again: closes

    // uiLayer: pages, slots, texts, item models (into 'items'); overlay: what goes over the models (counts)
    void   paint       (Tempest::Painter& p, int w, int h, float scale, InventoryRenderer& items, Npc& pl);
    void   paintOverlay(Tempest::Painter& p, float scale, Npc& pl);

    bool   mouseDown (const Tempest::Point& pos, bool right, Npc& pl);
    void   mouseMove (const Tempest::Point& pos) { mouse = pos; }
    void   mouseWheel(int delta);
    bool   spellKey  (uint8_t slot, Npc& pl);   // 3..10: the rune / scroll under the cursor goes to that key

    // UTF-8 text for Gothic's fonts (they draw the bytes of the game's code page; Polish game = Windows-1250)
    static std::string gtxt(std::string_view utf8);

  private:
    enum HitKind : uint8_t {
      H_Tab,
      H_Cat,
      H_Pack,
      H_Equip,
      H_Tree,
      H_Node,
      };
    struct Hit {
      Tempest::Rect r;
      HitKind       kind     = H_Tab;
      int           id       = 0;
      size_t        cls      = 0;
      bool          equipped = false;
      int32_t       count    = 0;
      };
    struct Fonts;

    Page                 page   = P_Closed;
    int                  cat    = 0;   // backpack: kind of items shown
    int                  tree   = 0;   // skill tree shown
    int                  scroll = 0;   // backpack: rows scrolled
    int                  maxScroll = 0;
    float                k      = 1;   // size of the window: 1 = 1080 lines
    Tempest::Point       mouse;
    std::vector<Hit>     hits;
    size_t               hoverCls = 0;

    // models of the game's items as icons of the skill nodes (made once, dropped on close or with a new world)
    const World*                                         iconWorld = nullptr;
    std::vector<std::pair<std::string,std::unique_ptr<Item>>> icons;
    const Item*          iconItem(const char* instance);

    const Hit*     hitAt(const Tempest::Point& pos) const;
    Tempest::Rect  drawPage (Tempest::Painter& p, const Tempest::Rect& r, bool left) const;
    void   drawSlot  (Tempest::Painter& p, const Tempest::Rect& r, bool equipped, bool hover) const;
    void   drawBar   (Tempest::Painter& p, const Tempest::Rect& r, const char* fill, float v) const;
    void   drawRule  (Tempest::Painter& p, int x, int y, int w) const;
    int    drawSection(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& c, int y, std::string_view name) const;
    void   drawTabs  (Tempest::Painter& p, int w, const Fonts& f);
    void   drawHint  (Tempest::Painter& p, int w, int h, const Fonts& f) const;
    void   paintEquip(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items, Npc& pl);
    void   paintStats(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, Npc& pl);
    void   paintTrees(Tempest::Painter& p, const Fonts& f, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items);
    void   paintInfo (Tempest::Painter& p, const Fonts& f, const Tempest::Rect& r, InventoryRenderer& items, const Item* it) const;
  };
