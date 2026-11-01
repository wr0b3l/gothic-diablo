#pragma once

#include <string_view>
#include <vector>

#include "graphics/arpggfx.h"

class Item;
namespace Tempest { class Texture2d; }

// Diathic (2026-11-01): the settings of the ARPG view's interface, rows of "Grafika Diathic" after the graphics
// agent's ones (the same Option record, an own table, saved in [DIATHIC] of Gothic.ini)
namespace ArpgUi {
  const std::vector<ArpgGfx::Option>& options();
  bool  owns(std::string_view ini);        // a row of this table (not ArpgGfx's)
  int   value(std::string_view ini);       // the level, like ArpgGfx::value
  float hudScale();                        // "Rozmiar interfejsu": 0.8 .. 1.6, default 1.3

  // the symbol on a rune or a scroll: the smallest part of an ITAR* texture its model uses (the face with the sign);
  // tex null: not found (the model is drawn instead)
  struct RuneFace {
    const Tempest::Texture2d* tex = nullptr;
    float                     u0 = 0, v0 = 0, u1 = 0, v1 = 0;
    };
  RuneFace runeFace(const Item& itm);
  }
