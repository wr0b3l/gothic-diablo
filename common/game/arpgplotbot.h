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
class Item;
class PlayerControl;
class DialogMenu;

// gothic-diablo: what the test bot shows on the screen while it plays (MainWindow draws it over everything,
// the pictures of the report are taken without it). Texts in UTF-8.
struct ArpgBotPanel {
  struct Line {
    std::string text;
    std::string note;
    int         kind = 0;   // 0 plain, 1 done well, 2 error, 3 skipped, 4 event
    };
  std::string       title;
  std::string       stats;
  std::string       current;  // the step now
  std::string       doing;    // what the bot does in it this moment
  std::vector<Line> done;     // the last steps, the newest at the end
  std::vector<Line> next;
  std::vector<Line> events;   // the last events, the newest at the end
  };

// gothic-diablo: the story bot (faza 10, część 4). Plays a scenario (autotest/<name>.txt: one step per line, names from the
// scripts) the honest way: walks with the ARPG click along the waynet, talks by clicking the npc and picking the lines,
// fights with ARPG clicks, drinks potions. Results: gra\dane\autotest\<date>_<name>\ (raport.html, kroki.js, zdarzenia.js).
class ArpgPlotBot final {
  public:
    struct Hooks {
      std::function<Tempest::Pixmap()>                            shot;    // the world without the interface
      std::function<void(const std::string&, const std::string&)> save;    // slot file, name in the menu
      PlayerControl*                                              player  = nullptr;
      DialogMenu*                                                 dialogs = nullptr;
      };

    ArpgPlotBot(std::string name, std::string runDir, Hooks hooks);
    ~ArpgPlotBot();

    bool        isDone() const { return done; }
    // once per game tick, while the game is not loading and the hero exists
    void        tick(World& w, Npc& pl);
    std::string summary() const;
    void        panel(ArpgBotPanel& out, const Npc* pl) const;

  private:
    struct Step {
      int                      line = 0;
      std::string              verb;
      std::vector<std::string> arg;
      std::string              text;
      bool                     invalid = false;
      };

    struct Walk {
      bool                       on     = false;
      Tempest::Vec3              dest   = {};
      float                      reach  = 0;
      std::vector<Tempest::Vec3> route;
      size_t                     idx    = 0;
      float                      best   = 0;
      uint64_t                   bestT  = 0;
      uint64_t                   clickT = 0;
      size_t                     clickIdx = size_t(-1);
      int                        stuck  = 0;
      int                        jumps  = 0;   // teleports in this walk (stuck for good)
      };

    Hooks                    hooks;
    std::string              name;
    std::string              runDir;
    std::string              source;           // where the scenario came from
    std::vector<Step>        steps;
    std::vector<std::string> handlers;         // "gdy": lines picked whenever they show up in a talk nobody expected
    bool                     stopOnError = false;
    std::string              startLabel;

    bool                     started = false;
    bool                     done    = false;
    uint64_t                 readyT  = 0;
    size_t                   cur     = 0;
    int                      phase   = 0;
    uint64_t                 stepT0  = 0;
    uint64_t                 phaseT0 = 0;
    uint64_t                 pickT   = 0;
    std::string              note;
    bool                     knewBefore = false;
    bool                     picked     = false;
    int                      talkRetry  = 0;
    size_t                   countBefore = 0;
    Npc*                     lastTalk   = nullptr;

    Walk                     walk;
    // fighting (ArpgPlotBot::fightTick): the bot reads the enemy's swing (Npc::arpgNextHitMs) and parries, blocks, dodges,
    // charges, uses the skills of the trees
    bool                     blocking   = false;
    uint64_t                 blockT0    = 0;
    bool                     charging   = false;
    uint64_t                 chargeT0   = 0;
    uint64_t                 chargeTry  = 0;
    const Npc*               answered   = nullptr;   // the swing already answered: who ...
    uint64_t                 answeredAt = 0;         // ... and when it lands
    uint64_t                 skillT     = 0;
    uint32_t                 rng        = 0x2545F491u;
    size_t                   nParry = 0, nParryOk = 0, nDodge = 0, nCharge = 0, nSkill = 0;
    std::string              lastMove;              // the last move in a fight (the panel)
    // between the fights: bodies to take from, things to pick up, food and potions
    std::vector<Npc*>        bodies;
    std::vector<const void*> givenUp;               // items and bodies the bot could not reach
    void*                    choreItem  = nullptr;
    Npc*                     choreBody  = nullptr;
    uint64_t                 choreT0    = 0;
    uint64_t                 healT      = 0;
    int32_t                  healFrom   = -1;       // life when the healing began (-1: not healing)
    std::string              healLog;
    std::string              walkWhat;          // "idzie do Vatras" (the panel)
    std::string              doing;             // what the bot does now (the panel)
    std::vector<ArpgBotPanel::Line> recentSteps;
    std::vector<ArpgBotPanel::Line> recentEvents;
    Npc*                     defend   = nullptr;
    uint64_t                 defendT0 = 0;
    uint64_t                 attackT  = 0;
    uint64_t                 potionT  = 0;
    uint64_t                 strayT   = 0;

    size_t                   nOk = 0, nFail = 0, nSkip = 0, nEvents = 0, nHeal = 0, nStuck = 0, nErrSaves = 0;
    size_t                   nDeaths = 0, nPotions = 0;
    uint32_t                 savesSeen  = 0;      // Gothic::arpgBotSaves when last looked
    bool                     endOnDeath = false;  // "po_smierci koniec": a death ends the run (otherwise 1 point left, noted)
    std::string              lastEnemy;           // the last one fought (the report of a death)
    uint64_t                 runT0 = 0;
    std::FILE*               fSteps  = nullptr;
    std::FILE*               fEvents = nullptr;
    std::FILE*               fLog    = nullptr;

    bool        load();
    void        parse(const std::string& text);
    void        validate(World& w);
    bool        checkName(World& w, const std::string& n, const char* kind, std::string& why);
    bool        checkCond(World& w, const std::vector<std::string>& a, size_t from, std::string& why);

    void        startRun(World& w);
    void        finish(const char* why);
    void        nextStep(bool ok, const std::string& why);
    void        event(const char* kind, const std::string& text, bool picture);
    std::string picture(const char* tag);
    void        writeState(bool end = false, const std::string& why = std::string()) const;
    void        writeReport() const;
    void        writePrevious() const;
    void        log(const std::string& line);

    void        guardHealth(Npc& pl, uint64_t now);
    bool        defendTick(World& w, Npc& pl, uint64_t now);
    void        fightTick(World& w, Npc& pl, Npc& tg, uint64_t now);
    void        fightEnd(Npc& pl);
    bool        choresTick(World& w, Npc& pl, uint64_t now);
    bool        healTick(World& w, Npc& pl, uint64_t now);
    int32_t     hpBonus(World& w, const Item& it) const;
    float       roll01();
    bool        strayTalk(uint64_t now, bool quiet);

    void        walkStart(World& w, Npc& pl, const Tempest::Vec3& dest, float reach);
    int         walkTick(World& w, Npc& pl, uint64_t now);  // 1 arrived, 0 on the way, -1 failed

    Npc*        findNpc(World& w, const std::string& n) const;
    bool        targetPos(World& w, const std::string& n, Tempest::Vec3& out, float& reach) const;
    size_t      itemId(World& w, const std::string& n) const;
    bool        intValue(World& w, const std::string& n, int32_t& out) const;
    bool        cond(World& w, Npc& pl, const std::vector<std::string>& a, size_t from, std::string& why) const;

    void        doTalk (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doPick (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doExit (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doGo   (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doKill (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doTake (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doUse  (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doWait (World& w, Npc& pl, const Step& s, uint64_t now, bool once);
    void        doSleep(World& w, Npc& pl, const Step& s, uint64_t now);
    void        doSave (World& w, Npc& pl, const Step& s, uint64_t now);
    void        doJump (World& w, Npc& pl, const Step& s, uint64_t now);
  };
