#include "arpgkeys.h"

#include "gothic.h"

using namespace Tempest;

namespace ArpgKeys {

static const Info infos[Count] = {
  {"Fikołek (unik)",                      "keyDodge",        Event::K_Space   },
  {"Broń: dobycie / schowanie",           "keyWeapon",       Event::K_F       },
  {"Atak w miejscu (przytrzymaj + LPM)",  "keyInPlace",      Event::K_LShift  },
  {"Broń biała / łuk lub kusza",          "keySwapWeapon",   Event::K_X       },
  {"Chód / bieg",                         "keyWalk",         Event::K_R       },
  {"Skradanie",                           "keySneak",        Event::K_V       },
  {"Mikstura zdrowia",                    "keyPotionHealth", Event::K_1       },
  {"Mikstura many",                       "keyPotionMana",   Event::K_2       },
  {"Ekwipunek",                           "keyEquipment",    Event::K_I       },
  {"Postać",                              "keyCharacter",    Event::K_C       },
  {"Drzewka umiejętności",                "keySkills",       Event::K_T       },
  {"Duża mapa",                           "keyMap",          Event::K_M       },
  {"Minimapa / mapa na grze",             "keyMinimap",      Event::K_Tab     },
  {"Etykiety (przytrzymaj)",              "keyLabels",       Event::K_Z       },
  {"Dziennik",                            "keyLog",          Event::K_L       },
  {"Obrót kamery w lewo",                 "keyCamLeft",      Event::K_Q       },
  {"Obrót kamery w prawo",                "keyCamRight",     Event::K_E       },
  };

static constexpr std::string_view Section = "DIATHIC";

// Diathic 2026-10-18: Q / E back to the camera (v2). 2026-10-19 (v3): the skills of the trees on the keys 3-0
// (no G / F / B), the weapon on F, Shift = attack in place. Keys saved with the old defaults ("reset" in the menu
// writes all of them) move to the new ones once.
static constexpr int KeysVersion = 3;

static void migrate() {
  static bool done = false;
  if(done)
    return;
  done = true;
  const int ver = Gothic::settingsGetI(Section, "keysVersion");
  if(ver>=KeysVersion)
    return;
  struct Old { Action a; Event::KeyType k; };
  static const Old old[] = {
    {CamLeft, Event::K_Left}, {CamRight, Event::K_Right}, {Weapon, Event::K_LShift},
    };
  for(auto& o:old)
    if(Gothic::settingsGetI(Section, infos[o.a].ini)==int(o.k))
      Gothic::settingsSetI(Section, infos[o.a].ini, int(infos[o.a].def));
  // a key of ours saved on F or Shift (now the weapon and the attack in place): back to its default
  for(uint8_t i=0; i<Count; ++i) {
    const int v = Gothic::settingsGetI(Section, infos[i].ini);
    if(i!=Weapon && v==int(Event::K_F))
      Gothic::settingsSetI(Section, infos[i].ini, int(infos[i].def));
    if(i!=InPlace && v==int(Event::K_LShift))
      Gothic::settingsSetI(Section, infos[i].ini, int(infos[i].def));
    }
  Gothic::settingsSetI(Section, "keysVersion", KeysVersion);
  }

const Info& info(Action a) {
  return infos[a<Count ? a : 0];
  }

Event::KeyType key(Action a) {
  if(a>=Count)
    return Event::K_NoKey;
  migrate();
  const int v = Gothic::settingsGetI(Section, infos[a].ini);
  if(v>0 && v<int(Event::K_Last) && canBind(Event::KeyType(v)))
    return Event::KeyType(v);
  return infos[a].def;
  }

void setKey(Action a, Event::KeyType k) {
  if(a>=Count || !canBind(k))
    return;
  const Event::KeyType old = key(a);
  for(uint8_t i=0; i<Count; ++i)
    if(i!=a && key(Action(i))==k)
      Gothic::settingsSetI(Section, infos[i].ini, int(old));
  Gothic::settingsSetI(Section, infos[a].ini, int(k));
  }

void resetDefaults() {
  for(uint8_t i=0; i<Count; ++i)
    Gothic::settingsSetI(Section, infos[i].ini, int(infos[i].def));
  Gothic::settingsSetI(Section, "keysVersion", KeysVersion);
  }

bool is(Action a, Event::KeyType k) {
  return k!=Event::K_NoKey && key(a)==k;
  }

int actionOf(Event::KeyType k) {
  if(k==Event::K_NoKey)
    return -1;
  for(uint8_t i=0; i<Count; ++i)
    if(key(Action(i))==k)
      return i;
  return -1;
  }

bool canBind(Event::KeyType k) {
  if(k==Event::K_NoKey || k==Event::K_ESCAPE || k>=Event::K_Last)
    return false;
  if((k>=Event::K_3 && k<=Event::K_9) || k==Event::K_0)
    return false; // spell slots of Gothic
  return true;
  }

std::string keyName(Event::KeyType k) {
  switch(k) {
    case Event::K_Space:    return "Spacja";
    case Event::K_LShift:   return "Lewy Shift";
    case Event::K_RShift:   return "Prawy Shift";
    case Event::K_LControl: return "Lewy Ctrl";
    case Event::K_RControl: return "Prawy Ctrl";
    case Event::K_LAlt:     return "Lewy Alt";
    case Event::K_RAlt:     return "Prawy Alt";
    case Event::K_Tab:      return "Tab";
    case Event::K_CapsLock: return "Caps Lock";
    case Event::K_Return:   return "Enter";
    case Event::K_Back:     return "Backspace";
    case Event::K_Left:     return "Strzałka w lewo";
    case Event::K_Right:    return "Strzałka w prawo";
    case Event::K_Up:       return "Strzałka w górę";
    case Event::K_Down:     return "Strzałka w dół";
    case Event::K_NoKey:    return "-";
    default: break;
    }
  if(k>=Event::K_F1 && k<=Event::K_F24)
    return "F" + std::to_string(int(k-Event::K_F1)+1);
  if(k>=Event::K_A && k<=Event::K_Z)
    return std::string(1, char('A'+int(k-Event::K_A)));
  if(k>=Event::K_0 && k<=Event::K_9)
    return std::string(1, char('0'+int(k-Event::K_0)));
  switch(k) {
    case Event::K_Insert:   return "Insert";
    case Event::K_Delete:   return "Delete";
    case Event::K_Home:     return "Home";
    case Event::K_End:      return "End";
    case Event::K_PageUp:   return "Page Up";
    case Event::K_PageDown: return "Page Down";
    case Event::K_Pause:    return "Pause";
    case Event::K_LCommand: return "Lewy Command";
    case Event::K_RCommand: return "Prawy Command";
    default: break;
    }
  return "?";
  }

}
