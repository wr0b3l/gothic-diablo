#pragma once

#include <Tempest/Point>

#include <cstdint>

class Npc;
class World;

// gothic-diablo: the "director" of cinematic dialogs (like Mass Effect).
// Shot / reverse shot over the listener's shoulder, close-ups and medium shots of the one who speaks,
// a reaction of the listener in the middle of long lines, a two-shot to open the talk; while the player
// picks an answer: over the hero's shoulder on the other one. The camera stays on one side of the line
// between the two (180 degree rule), pushes in slowly and is pulled in front of walls.
class ArpgDialogCam final {
  public:
    struct Input {
      const Npc*    pl       = nullptr;
      const Npc*    other    = nullptr;
      const Npc*    speaker  = nullptr; // who says the current line (nullptr: nobody)
      uint32_t      line     = 0;       // changes with every new line
      float         lineLen  = 0;       // seconds
      bool          choosing = false;   // the player picks an answer
      Tempest::Vec3 viewFrom = {};      // where the camera was before the talk (picks the side)
      };

    struct Shot {
      Tempest::Vec3 origin = {};
      Tempest::Vec3 target = {};
      };

    void reset();
    Shot tick(float dt, const Input& in, World& world);

  private:
    enum Kind : uint8_t {
      K_None,
      K_Two,          // both, from the side
      K_OverShoulder, // the subject, over the shoulder of the other
      K_Medium,       // the subject from the waist up, 3/4 from the front
      K_Close,        // the subject's face
      };

    Kind          kind      = K_None;
    const Npc*    subject   = nullptr;
    float         shotT     = 0;      // seconds since the cut
    uint32_t      line      = 0;
    float         lineT     = 0;
    float         lineLen   = 0;
    const Npc*    lineSpeaker = nullptr;
    uint32_t      cuts      = 0;
    bool          reacting  = false;
    bool          reactDone = false;
    Kind          beforeReact = K_None;
    float         side      = 0;      // +1 / -1: the side of the line the camera stays on
    bool          faceInit  = false;
    Tempest::Vec3 facePl    = {};     // smoothed head positions
    Tempest::Vec3 faceOther = {};
    Shot          last;

    void  cut     (Kind k, const Npc* who, const Input& in, World& world);
    Shot  compute (Kind k, const Npc* who, const Input& in, World& world, float pushIn, float* quality) const;
    void  pickSide(const Input& in, World& world);
  };
