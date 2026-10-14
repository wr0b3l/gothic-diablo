#pragma once

#include <Tempest/Painter>
#include <Tempest/Rect>

#include <string>
#include <string_view>
#include <vector>

class Npc;
class Item;
class InventoryRenderer;
class GthFont;

// gothic-diablo: the ARPG window: equipment, character, skill trees.
// The hero himself is not drawn here: while it is open the camera stands in front of him (Camera::setArpgPortrait),
// so he is seen in the middle of the screen between the two panels, with his idle animation and his real equipment.
// Drawn with Gothic's own textures (MENU_INGAME, INV_SLOT*, INV_DESC, BAR_*) and item models (InventoryRenderer).
class ArpgMenu final {
  public:
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

    // uiLayer: panels, slots, texts, item models (into 'items'); overlay: what goes over the models (counts)
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
      };
    struct Hit {
      Tempest::Rect r;
      HitKind       kind     = H_Tab;
      int           id       = 0;
      size_t        cls      = 0;
      bool          equipped = false;
      int32_t       count    = 0;
      };

    Page                 page   = P_Closed;
    int                  cat    = 0;   // backpack: kind of items shown
    int                  tree   = 0;   // skill tree shown
    int                  scroll = 0;   // backpack: rows scrolled
    int                  maxScroll = 0;
    Tempest::Point       mouse;
    std::vector<Hit>     hits;
    size_t               hoverCls = 0;

    const Hit* hitAt(const Tempest::Point& pos) const;
    void   drawPanel (Tempest::Painter& p, const Tempest::Rect& r) const;
    void   drawSlot  (Tempest::Painter& p, const Tempest::Rect& r, bool equipped, bool hover) const;
    void   drawBar   (Tempest::Painter& p, const Tempest::Rect& r, const char* fill, float v) const;
    void   drawTabs  (Tempest::Painter& p, int w, float scale, const Tempest::Rect& left, const Tempest::Rect& right);
    void   paintEquip(Tempest::Painter& p, float scale, const Tempest::Rect& left, const Tempest::Rect& right, InventoryRenderer& items, Npc& pl);
    void   paintStats(Tempest::Painter& p, float scale, const Tempest::Rect& left, const Tempest::Rect& right, Npc& pl);
    void   paintTrees(Tempest::Painter& p, float scale, const Tempest::Rect& left, const Tempest::Rect& right);
    void   paintInfo (Tempest::Painter& p, float scale, const Tempest::Rect& r, InventoryRenderer& items, const Item* it) const;
  };
