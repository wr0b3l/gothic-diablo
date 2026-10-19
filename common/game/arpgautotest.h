#pragma once

#include <Tempest/Pixmap>
#include <Tempest/Vec>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

class World;
class Npc;

// gothic-diablo: the test bot (faza 10). Started with -autotest <scenario>; runs without windows or questions and
// closes the game at the end. Results: gra\dane\autotest\<date and time>\ (raport.html, pictures, log).
// Scenario "kamera" (część 1, objazd kamery): every point of the waynet in every world -> the hero there, a moment for
// the ARPG camera, then measurements (black screen, hidden hero, camera in the rock, FPS, heights) and a picture.
class ArpgAutotest final {
  public:
    struct Hooks {
      std::function<Tempest::Pixmap()> shot;    // the world without the interface, current camera
      std::function<bool()>            uiBusy;  // a talk, a letter, the chapter screen, the inventory
      std::function<void()>            closeUi; // ends all of them at once
      };

    ArpgAutotest();
    ~ArpgAutotest();

    static bool isEnabled();
    void        setHooks(Hooks h) { hooks = std::move(h); }
    bool        isActive() const { return state!=State::Off && state!=State::Done; }
    // once per game tick (MainWindow::tick, after the game update)
    void        tick();

  private:
    enum class State : uint8_t {
      Off,
      WaitGame,   // the new game is loading, its first talk
      NextWorld,
      WaitWorld,  // a world change is loading
      NextPoint,
      Settle,     // the hero is on the point: the camera settles, frames are counted
      Done,
      };

    struct Point {
      std::string   name;
      Tempest::Vec3 pos;
      float         dyNear     = 0;     // the biggest height difference to a connected point (cm)
      bool          underWater = false;
      };

    struct Result {
      float       blackMid  = 0;  // 0..1, black pixels in the middle of the screen
      float       blackAll  = 0;  // 0..1, the whole screen
      bool        hidden    = false; // something stands between the camera and the hero and the game shows it
      int         rock      = 0;  // 0 no, 1 camera in the rock and the game cuts it, 2 in the rock and not cut
      float       see       = 0;  // see-through circle 0..1
      bool        cut       = false;
      float       fps       = 0;
      float       fpsMin    = 0;
      float       fall      = 0;  // cm the hero went down from the point
      int         uiStops   = 0;  // talks that came up on the point
      float       score     = 0;
      std::string kinds;          // "czarny,fps" ...
      };

    Hooks                    hooks;
    State                    state = State::Off;
    std::string              runDir;
    std::FILE*               fPoints = nullptr;
    std::FILE*               fLog    = nullptr;
    std::string              startStamp;

    std::vector<std::string> worlds;     // file names, lower case: "newworld.zen"
    size_t                   worldId = 0;
    std::vector<Point>       points;
    size_t                   pointId = 0;
    size_t                   pointsDone = 0;   // all worlds
    size_t                   suspects   = 0;

    uint64_t                 t0        = 0;
    uint64_t                 lastFrame = 0;
    uint64_t                 frameSum  = 0;
    uint64_t                 frameMax  = 0;
    uint32_t                 frameNum  = 0;
    int                      uiStops   = 0;

    void        start();
    void        finish();
    bool        isWorld(const World& w, const std::string& file) const;
    void        collectPoints(World& w);
    void        placeHero(World& w, Npc& pl, const Point& p);
    void        measure(World& w, Npc& pl);
    void        writeReport() const;
    void        writeState(bool done) const;
    void        writePrevious() const;
    void        log(const std::string& line);
    std::string worldTag() const;
  };
