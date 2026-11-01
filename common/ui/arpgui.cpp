#include "arpgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <unordered_map>

#include <zenkit/MultiResolutionMesh.hh>

#include "world/objects/item.h"
#include "utils/fileext.h"
#include "gothic.h"
#include "resources.h"

namespace {
// "Rozmiar interfejsu": the levels and their scale (the bar, life and mana, the texts on the bar)
constexpr float HudScales[] = {0.8f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f, 1.4f, 1.5f, 1.6f};
constexpr int   HudDefault  = 5; // 130%
}

namespace ArpgUi {
const std::vector<ArpgGfx::Option>& options() {
  static const std::vector<ArpgGfx::Option> opts = {
    {"uiHudScale", "Rozmiar interfejsu", "Wielkość paska umiejętności, pasków życia i many oraz napisów na pasku.",
     {"80%", "90%", "100%", "110%", "120%", "130%", "140%", "150%", "160%"}, HudDefault, false},
    };
  return opts;
  }

bool owns(std::string_view ini) {
  for(const auto& o : options())
    if(o.ini==ini)
      return true;
  return false;
  }

int value(std::string_view ini) {
  for(const auto& o : options()) {
    if(o.ini!=ini)
      continue;
    if(Gothic::settingsGetS("DIATHIC", ini).empty())
      return o.def;
    return std::clamp(Gothic::settingsGetI("DIATHIC", ini), 0, int(o.levels.size())-1);
    }
  return 0;
  }

float hudScale() {
  const int v = value("uiHudScale");
  return HudScales[std::clamp(v, 0, int(std::size(HudScales))-1)];
  }

RuneFace runeFace(const Item& itm) {
  // Gothic II has no spell icons: the sign is on the rune's face (ITARRUNE1-3, scrolls ITAR_SCROLL*), a cell of
  // the atlas; the part of the texture is read from the model once and kept
  static std::unordered_map<std::string, RuneFace> cache;
  std::string vis = itm.handle().visual;
  for(auto& c:vis)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  if(auto it = cache.find(vis); it!=cache.end())
    return it->second;
  RuneFace& ret = cache[vis];
  if(!FileExt::hasExt(vis,"3DS"))
    return ret;
  std::string mrm = vis;
  FileExt::exchangeExt(mrm,"3DS","MRM");
  const auto* entry = Resources::vdfsIndex().find(mrm);
  if(entry==nullptr)
    return ret;
  zenkit::MultiResolutionMesh zmsh;
  auto reader = entry->open_read();
  zmsh.load(reader.get());
  float best = 0.5f; // a symbol is a cell of the atlas, not the whole texture
  for(auto& sm:zmsh.sub_meshes) {
    std::string tex = sm.mat.texture;
    for(auto& c:tex)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    if(tex.rfind("ITAR",0)!=0 || sm.wedges.empty())
      continue;
    float u0 = 1e9f, v0 = 1e9f, u1 = -1e9f, v1 = -1e9f;
    for(auto& wd:sm.wedges) {
      u0 = std::min(u0, wd.texture.x);
      v0 = std::min(v0, wd.texture.y);
      u1 = std::max(u1, wd.texture.x);
      v1 = std::max(v1, wd.texture.y);
      }
    const float fu = std::floor(u0), fv = std::floor(v0);
    u0 -= fu; u1 -= fu;
    v0 -= fv; v1 -= fv;
    const float area = (u1-u0)*(v1-v0);
    if(area<=0.f || area>=best || u1>1.001f || v1>1.001f)
      continue;
    const Tempest::Texture2d* t = Resources::loadTexture(sm.mat.texture);
    if(t==nullptr || t->isEmpty())
      continue;
    best = area;
    ret  = RuneFace{t, u0, v0, u1, v1};
    }
  return ret;
  }
}
