#include "arpgautotest.h"

#include <Tempest/Application>
#include <Tempest/Log>
#include <Tempest/SystemApi>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <ctime>
#include <filesystem>

#include "commandline.h"
#include "gothic.h"
#include "camera.h"
#include "game/gamesession.h"
#include "physics/dynamicworld.h"
#include "world/objects/npc.h"
#include "world/world.h"
#include "game/arpgplotbot.h"
#include "ui/dialogmenu.h"
#include "ui/inventorymenu.h"
#include "ui/arpgmenu.h"
#include "utils/gthfont.h"
#include "resources.h"

using namespace Tempest;

namespace {
  // timing (ms)
  constexpr uint64_t kStartMs        = 5000;   // after the new game: its first talk, the chapter screen
  constexpr uint64_t kWorldMs        = 3000;   // after a world change
  constexpr uint64_t kWorldTimeoutMs = 240000; // a world that does not load is skipped
  constexpr uint64_t kSettleMs       = 1500;   // the hero on the point: the ARPG camera settles (blends, line of sight)
  constexpr uint64_t kFpsSkipMs      = 400;    // the first frames after the jump are not counted
  constexpr int      kUiRetries      = 3;      // a talk on the point: closed, the point measured again

  // the time of day: pictures in daylight (black = a fault, not the night)
  constexpr int64_t  kDayFrom = 9;
  constexpr int64_t  kDayTo   = 16;

  // pictures
  constexpr uint32_t kThumbW     = 480;
  constexpr uint32_t kDetailW    = 1280;
  constexpr int      kBlackLevel = 10;     // max(r,g,b) <= this = black

  // measurements -> "suspicious"
  constexpr float    kBlackMid    = 0.25f;  // black in the middle of the screen
  constexpr float    kFpsLow      = 30.f;
  constexpr float    kFallCm      = 500.f;
  constexpr float    kStepCm      = 400.f;  // height difference to a neighbour point (vertical maps)
  constexpr float    kHideMarginCm= 60.f;

  // score weights (the report sorts by the score)
  constexpr float    kScoreBlack   = 100.f; // * black fraction in the middle
  constexpr float    kScoreHidden  = 60.f;
  constexpr float    kScoreRockBad = 50.f;
  constexpr float    kScoreRockCut = 5.f;
  constexpr float    kScoreFps     = 2.f;   // * missing FPS below kFpsLow
  constexpr float    kScoreFall    = 40.f;
  constexpr float    kScoreStep    = 10.f;
  constexpr float    kScoreUi      = 5.f;
  constexpr float    kDetailScore  = 25.f;  // from this score a big picture is saved too

  const char* const  kDefaultWorlds[] = {"newworld.zen", "oldworld.zen", "addonworld.zen", "dragonisland.zen"};

  std::string lower(std::string_view s) {
    std::string r(s);
    for(auto& c:r)
      c = char(std::tolower(static_cast<unsigned char>(c)));
    return r;
    }

  std::string stamp(const char* fmt) {
    char        buf[64] = {};
    std::time_t t  = std::time(nullptr);
    std::tm*    lt = std::localtime(&t);
    if(lt!=nullptr)
      std::strftime(buf, sizeof(buf), fmt, lt);
    return buf;
    }

  std::string jsonStr(std::string_view s) {
    std::string r = "\"";
    for(char c:s) {
      if(c=='"' || c=='\\') {
        r += '\\';
        r += c;
        }
      else if(static_cast<unsigned char>(c)<0x20)
        r += ' ';
      else
        r += c;
      }
    r += '"';
    return r;
    }

  int iround(float v) {
    return int(std::lround(v));
    }

  // box filter, RGBA8 -> RGBA8 of width w
  Pixmap downscale(const Pixmap& src, uint32_t w) {
    const uint32_t sw = src.w(), sh = src.h();
    if(sw==0 || sh==0 || src.format()!=TextureFormat::RGBA8)
      return Pixmap();
    w = std::min(w, sw);
    const uint32_t h = std::max<uint32_t>(1, uint32_t(uint64_t(sh)*w/sw));
    Pixmap   dst(w, h, TextureFormat::RGBA8);
    auto*    s = static_cast<const uint8_t*>(src.data());
    auto*    d = static_cast<uint8_t*>(dst.data());
    for(uint32_t y=0; y<h; ++y) {
      const uint32_t y0 = uint32_t(uint64_t(y)*sh/h);
      const uint32_t y1 = std::max(y0+1, uint32_t(uint64_t(y+1)*sh/h));
      for(uint32_t x=0; x<w; ++x) {
        const uint32_t x0 = uint32_t(uint64_t(x)*sw/w);
        const uint32_t x1 = std::max(x0+1, uint32_t(uint64_t(x+1)*sw/w));
        uint32_t sum[3] = {};
        uint32_t n      = 0;
        for(uint32_t yy=y0; yy<y1; ++yy)
          for(uint32_t xx=x0; xx<x1; ++xx) {
            const uint8_t* p = s + (size_t(yy)*sw + xx)*4;
            sum[0] += p[0];
            sum[1] += p[1];
            sum[2] += p[2];
            ++n;
            }
        uint8_t* q = d + (size_t(y)*w + x)*4;
        for(int c=0; c<3; ++c)
          q[c] = uint8_t(sum[c]/std::max(n, 1u));
        q[3] = 255;
        }
      }
    return dst;
    }

  // black fraction of the whole picture and of its middle (the middle half in both directions: the hero)
  void blackness(const Pixmap& pm, float& all, float& mid) {
    all = mid = 0;
    const uint32_t w = pm.w(), h = pm.h();
    if(w==0 || h==0)
      return;
    auto*    d = static_cast<const uint8_t*>(pm.data());
    uint32_t nAll = 0, nMid = 0, numMid = 0;
    for(uint32_t y=0; y<h; ++y)
      for(uint32_t x=0; x<w; ++x) {
        const uint8_t* p     = d + (size_t(y)*w + x)*4;
        const bool     black = std::max({p[0], p[1], p[2]})<=kBlackLevel;
        const bool     inMid = x>=w/4 && x<w*3/4 && y>=h/4 && y<h*3/4;
        nAll += black ? 1 : 0;
        if(inMid) {
          ++numMid;
          nMid += black ? 1 : 0;
          }
        }
    all = float(nAll)/float(w*h);
    mid = numMid>0 ? float(nMid)/float(numMid) : 0.f;
    }

  // the static page of the report; the data comes in punkty.js (one line per point, written as the bot goes),
  // stan.js (progress) and poprzedni.js (the run before: comparison)
  const char kReportHtml[] = R"HTML(<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><title>Diathic - bot testowy</title>
<style>
:root{--bg:#16130f;--card:#221d17;--line:#3a3128;--txt:#e8dcc8;--dim:#a8977c;--gold:#d8a64a;--red:#e0604a;--green:#7cbf6a}
body{margin:0;background:var(--bg);color:var(--txt);font:14px/1.45 system-ui,Segoe UI,sans-serif}
header,#filtry,#podsum{padding:12px 16px;border-bottom:1px solid var(--line)}
h1{margin:0 0 4px;font-size:20px;color:var(--gold)}
#stan{color:var(--dim)}
#filtry{display:flex;flex-wrap:wrap;gap:12px;align-items:center;position:sticky;top:0;background:var(--bg);z-index:2}
select,input,button{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:4px;padding:4px 6px}
#podsum table{border-collapse:collapse}#podsum td,#podsum th{padding:2px 10px;text-align:right;border-bottom:1px solid var(--line)}
#podsum th:first-child,#podsum td:first-child{text-align:left}
#lista{display:grid;grid-template-columns:repeat(auto-fill,minmax(330px,1fr));gap:12px;padding:16px}
.k{background:var(--card);border:1px solid var(--line);border-radius:6px;overflow:hidden}
.k img{width:100%;display:block;aspect-ratio:16/9;object-fit:cover;background:#000}
.k .b{padding:8px 10px}.k .t{display:flex;justify-content:space-between;gap:8px;font-weight:600}
.s{color:var(--gold)}.tag{display:inline-block;margin:4px 4px 0 0;padding:1px 6px;border-radius:3px;background:#3a2a1c;font-size:12px}
.m{color:var(--dim);font-size:12px;margin-top:4px}.r{font-family:Consolas,monospace;font-size:12px;color:var(--dim);user-select:all}
.gorzej{color:var(--red)}.lepiej{color:var(--green)}
#wiecej{margin:0 16px 24px}
</style></head><body>
<header><h1>Bot testowy - objazd kamery</h1><div id="stan">brak danych</div></header>
<div id="filtry">
<label>Świat <select id="fw"></select></label>
<label>Rodzaj <select id="fk"></select></label>
<label>Porównanie <select id="fc"><option value="">wszystkie</option><option value="gorzej">gorzej niż poprzednio</option><option value="lepiej">lepiej niż poprzednio</option><option value="nowe">nowe miejsca</option></select></label>
<label>Ocena od <input id="fs" type="number" value="1" min="0" style="width:5em"></label>
<span id="ile"></span></div>
<div id="podsum"></div>
<div id="lista"></div><button id="wiecej">Pokaż więcej</button>
<script>var D=[],PREV={},STAN=null,POPRZEDNI="";function P(o){D.push(o)}</script>
<script src="stan.js"></script>
<script src="poprzedni.js"></script>
<script src="punkty.js"></script>
<script>
var RODZAJ={czarny:"czarny ekran",zasloniety:"bohater zasłonięty",skala:"kamera w skale (wycięta)","skala!":"kamera w skale (czarno)",fps:"niski FPS",upadek:"bohater spadł",pion:"duża różnica wysokości",rozmowa:"rozmowa na punkcie"};
var PROG=15,STRONA=200,pokazane=STRONA;
function $(i){return document.getElementById(i)}
function el(t,c,x){var e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e}
function porownaj(o){var p=PREV[o.w+"|"+o.p];if(!p)return POPRZEDNI?"nowe":"";var d=o.s-p.s;o.ps=p.s;return d>=PROG?"gorzej":(d<=-PROG?"lepiej":"")}
function start(){
  D.forEach(function(o){o.cmp=porownaj(o)});
  var sw={},sk={};D.forEach(function(o){sw[o.w]=1;(o.k||[]).forEach(function(k){sk[k]=1})});
  var fw=$("fw");fw.appendChild(new Option("wszystkie",""));Object.keys(sw).forEach(function(w){fw.appendChild(new Option(w,w))});
  var fk=$("fk");fk.appendChild(new Option("wszystkie",""));Object.keys(sk).forEach(function(k){fk.appendChild(new Option(RODZAJ[k]||k,k))});
  ["fw","fk","fc","fs"].forEach(function(i){$(i).onchange=function(){pokazane=STRONA;rysuj()}});
  $("wiecej").onclick=function(){pokazane+=STRONA;rysuj()};
  var s=STAN?("Start "+STAN.start+" · świat "+STAN.swiat+" · punkty "+STAN.gotowe+(STAN.koniec?" · zakończony "+STAN.czas:" · w toku (ostatni zapis "+STAN.czas+")")):"brak stan.js";
  if(POPRZEDNI)s+=" · porównanie z "+POPRZEDNI;$("stan").textContent=s;
  podsumowanie();rysuj();
}
function podsumowanie(){
  var t={};D.forEach(function(o){var r=t[o.w]||(t[o.w]={n:0,p:0,g:0,l:0,k:{}});r.n++;if(o.s>0)r.p++;if(o.cmp=="gorzej")r.g++;if(o.cmp=="lepiej")r.l++;(o.k||[]).forEach(function(k){r.k[k]=(r.k[k]||0)+1})});
  var tab=el("table"),h=el("tr");["Świat","punkty","podejrzane"].concat(Object.keys(RODZAJ).map(function(k){return RODZAJ[k]})).concat(POPRZEDNI?["gorzej","lepiej"]:[]).forEach(function(x){h.appendChild(el("th",null,x))});tab.appendChild(h);
  Object.keys(t).forEach(function(w){var r=t[w],tr=el("tr");[w,r.n,r.p].concat(Object.keys(RODZAJ).map(function(k){return r.k[k]||0})).concat(POPRZEDNI?[r.g,r.l]:[]).forEach(function(x){tr.appendChild(el("td",null,x))});tab.appendChild(tr)});
  $("podsum").appendChild(tab);
}
function rysuj(){
  var w=$("fw").value,k=$("fk").value,c=$("fc").value,s=+$("fs").value||0;
  var L=D.filter(function(o){return(!w||o.w==w)&&(!k||(o.k||[]).indexOf(k)>=0)&&(!c||o.cmp==c)&&o.s>=s});
  L.sort(function(a,b){return b.s-a.s});
  $("ile").textContent=L.length+" miejsc";
  var li=$("lista");li.innerHTML="";
  L.slice(0,pokazane).forEach(function(o){
    var k=el("div","k"),a=el("a");a.href=o.d||o.img;a.target="_blank";var im=el("img");im.loading="lazy";im.src=o.img;a.appendChild(im);k.appendChild(a);
    var b=el("div","b"),t=el("div","t");t.appendChild(el("span",null,o.w+" · "+o.p));t.appendChild(el("span","s",o.s));b.appendChild(t);
    (o.k||[]).forEach(function(x){b.appendChild(el("span","tag",RODZAJ[x]||x))});
    if(o.cmp)b.appendChild(el("span","tag "+o.cmp,o.cmp+(o.ps!=null?" (było "+o.ps+")":"")));
    b.appendChild(el("div","m","czerń środka "+o.cb+"% · ekranu "+o.ab+"% · FPS "+o.f+" (min "+o.fm+") · prześwit "+o.see+"%"+(o.cut?" · wycięcie":"")+" · Δ wys. "+o.dy+" cm · spadek "+o.fall+" cm"+(o.uw?" · woda":"")));
    b.appendChild(el("div","r","MARVIN, F2: goto waypoint "+o.p));
    b.appendChild(el("div","m","pozycja "+o.x+" "+o.y+" "+o.z));
    k.appendChild(b);li.appendChild(k);
  });
  $("wiecej").style.display=L.length>pokazane?"":"none";
}
if(POPRZEDNI){P=function(o){PREV[o.w+"|"+o.p]=o};var sc=document.createElement("script");sc.src="../"+POPRZEDNI+"/punkty.js";sc.onload=sc.onerror=start;document.body.appendChild(sc)}else start();
</script></body></html>
)HTML";
  }

ArpgAutotest::ArpgAutotest() {
  if(!isEnabled())
    return;
  // "kamera": the camera tour; any other name: a scenario of the story bot (autotest\<name>.txt or built in)
  state = CommandLine::inst().arpgAutotest()=="kamera" ? State::WaitGame : State::Plot;
  }

ArpgAutotest::~ArpgAutotest() {
  if(fPoints!=nullptr)
    std::fclose(fPoints);
  if(fLog!=nullptr)
    std::fclose(fLog);
  }

bool ArpgAutotest::isEnabled() {
  return !CommandLine::inst().arpgAutotest().empty();
  }

void ArpgAutotest::start() {
  startStamp = stamp("%Y-%m-%d_%H%M%S");
  t0         = Application::tickCount();
  runDir     = "autotest/" + startStamp + "/";
  std::error_code ec;
  std::filesystem::create_directories(runDir + "img", ec);
  if(ec)
    Log::e("autotest: nie mogę utworzyć ", runDir);

  // worlds: -autotest-swiaty newworld,oldworld (with or without .zen), otherwise all four
  std::string list = std::string(CommandLine::inst().arpgAutotestWorlds());
  size_t      at   = 0;
  while(at<list.size()) {
    size_t end = list.find(',', at);
    if(end==std::string::npos)
      end = list.size();
    std::string w = lower(list.substr(at, end-at));
    w.erase(std::remove_if(w.begin(), w.end(), [](char c){ return std::isspace(static_cast<unsigned char>(c))!=0; }), w.end());
    if(!w.empty()) {
      if(w.size()<4 || w.compare(w.size()-4, 4, ".zen")!=0)
        w += ".zen";
      worlds.push_back(w);
      }
    at = end+1;
    }
  if(worlds.empty())
    worlds.assign(std::begin(kDefaultWorlds), std::end(kDefaultWorlds));

  fPoints = std::fopen((runDir+"punkty.js").c_str(), "wb");
  fLog    = std::fopen((runDir+"log.txt").c_str(), "wb");
  writeReport();
  writePrevious();
  writeState(false);

  std::string names;
  for(auto& w:worlds)
    names += (names.empty() ? "" : ", ") + w;
  log("start: kamera, swiaty: " + names + ", co " + std::to_string(CommandLine::inst().arpgAutotestEvery()) +
      ". punkt, limit " + std::to_string(CommandLine::inst().arpgAutotestLimit()));
  Log::i("autotest: wyniki w ", runDir);
  }

void ArpgAutotest::finish() {
  writeState(true);
  log("koniec: punkty " + std::to_string(pointsDone) + ", podejrzane " + std::to_string(suspects));
  Log::i("autotest: koniec, punkty ", pointsDone, ", podejrzane ", suspects);
  if(fPoints!=nullptr) {
    std::fclose(fPoints);
    fPoints = nullptr;
    }
  if(fLog!=nullptr) {
    std::fclose(fLog);
    fLog = nullptr;
    }
  state = State::Done;
  Tempest::SystemApi::exit();
  }

bool ArpgAutotest::isWorld(const World& w, const std::string& file) const {
  std::string n = lower(w.name());
  const size_t cut = n.find_last_of("\\/");
  if(cut!=std::string::npos)
    n = n.substr(cut+1);
  return n==file;
  }

std::string ArpgAutotest::worldTag() const {
  if(worldId>=worlds.size())
    return "";
  std::string t = worlds[worldId];
  if(t.size()>4)
    t.resize(t.size()-4);
  for(auto& c:t)
    c = char(std::toupper(static_cast<unsigned char>(c)));
  return t;
  }

void ArpgAutotest::collectPoints(World& w) {
  points.clear();
  pointId = 0;
  w.arpgForEachWayPoint([this](const WayPoint& wp) {
    Point p;
    p.name       = wp.name;
    p.pos        = wp.position();
    p.underWater = wp.underWater;
    for(auto& c:wp.connections())
      if(c.point!=nullptr)
        p.dyNear = std::max(p.dyNear, std::abs(c.point->position().y - p.pos.y));
    points.push_back(std::move(p));
    });

  const size_t total = points.size();
  const size_t every = size_t(std::max(1, CommandLine::inst().arpgAutotestEvery()));
  if(every>1) {
    std::vector<Point> keep;
    for(size_t i=0; i<points.size(); i+=every)
      keep.push_back(std::move(points[i]));
    points = std::move(keep);
    }
  const int limit = CommandLine::inst().arpgAutotestLimit();
  if(limit>0 && points.size()>size_t(limit))
    points.resize(size_t(limit));

  std::filesystem::create_directories(runDir + "img/" + worldTag());
  log("swiat " + worldTag() + ": punktow drogi " + std::to_string(total) + ", do sprawdzenia " + std::to_string(points.size()));
  }

void ArpgAutotest::placeHero(World& w, Npc& pl, const Point& p) {
  Gothic::inst().setGodMode(true);
  const auto t = w.time();
  if(t.hour()<kDayFrom || t.hour()>=kDayTo)
    w.setDayTime(12, 0);

  pl.setInteraction(nullptr, true);
  pl.clearAiQueue();
  pl.setPosition(p.pos);
  pl.updateTransform();
  if(auto c = Gothic::inst().camera())
    c->reset(&pl);

  t0        = Application::tickCount();
  lastFrame = 0;
  frameSum  = 0;
  frameMax  = 0;
  frameNum  = 0;
  }

bool ArpgAutotest::uiOrTalk() const {
  return (hooks.uiBusy && hooks.uiBusy()) || (hooks.dialogs!=nullptr && hooks.dialogs->isActive());
  }

void ArpgAutotest::closeUiAndTalk() {
  if(hooks.dialogs!=nullptr)
    hooks.dialogs->arpgAbort();
  if(hooks.closeUi)
    hooks.closeUi();
  }

void ArpgAutotest::tickPlot() {
  if(plot==nullptr) {
    const std::string sc  = std::string(CommandLine::inst().arpgAutotest());
    const std::string dir = "autotest/" + stamp("%Y-%m-%d_%H%M%S") + "_" + sc + "/";
    ArpgPlotBot::Hooks h;
    h.shot    = hooks.shot;
    h.save    = hooks.save;
    h.player  = hooks.player;
    h.dialogs = hooks.dialogs;
    h.inventory = hooks.inventory;
    plot = std::make_unique<ArpgPlotBot>(sc, dir, std::move(h));
    Log::i("autotest: bot fabuly, wyniki w ", dir);
    }
  if(plot->isDone()) {
    Log::i("autotest: ", plot->summary());
    state = State::Done;
    Tempest::SystemApi::exit();
    return;
    }
  auto world = Gothic::inst().world();
  auto pl    = Gothic::inst().player();
  if(world==nullptr || pl==nullptr || Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return;
  // letters, the inventory: the bot does not read; talks are the bot's own business; the trade window only while the
  // scenario buys or sells
  const bool trading = plot->keepsTrade() && hooks.inventory!=nullptr && hooks.inventory->isOpen()==InventoryMenu::State::Trade;
  if(hooks.uiBusy && hooks.uiBusy() && !trading)
    hooks.closeUi();
  plot->tick(*world, *pl);
  }

void ArpgAutotest::tick() {
  if(state==State::Off || state==State::Done)
    return;
  if(state==State::Plot) {
    tickPlot();
    return;
    }
  if(runDir.empty())
    start();

  const uint64_t now = Application::tickCount();
  auto  world = Gothic::inst().world();
  auto  pl    = Gothic::inst().player();
  if(Gothic::inst().checkLoading()!=Gothic::LoadState::Idle)
    return;

  switch(state) {
    case State::Off:
    case State::Plot:
    case State::Done:
      break;

    case State::WaitGame: {
      if(world==nullptr || pl==nullptr) {
        t0 = now;
        return;
        }
      if(uiOrTalk())
        closeUiAndTalk();
      if(world->currentCs()!=nullptr || now-t0<kStartMs)
        return;
      state = State::NextWorld;
      break;
      }

    case State::NextWorld: {
      if(worldId>=worlds.size()) {
        finish();
        return;
        }
      if(world!=nullptr && isWorld(*world, worlds[worldId])) {
        collectPoints(*world);
        state = State::NextPoint;
        break;
        }
      auto gs = Gothic::inst().gameSession();
      if(gs==nullptr) {
        finish();
        return;
        }
      log("zmiana swiata: " + worlds[worldId]);
      gs->changeWorld(worlds[worldId], "");
      t0        = now;
      lastFrame = 0;
      state     = State::WaitWorld;
      break;
      }

    case State::WaitWorld: {
      if(now-t0>kWorldTimeoutMs) {
        log("swiat " + worlds[worldId] + " sie nie wczytal - pomijam");
        ++worldId;
        state = State::NextWorld;
        break;
        }
      if(world==nullptr || pl==nullptr || !isWorld(*world, worlds[worldId]))
        return;
      if(uiOrTalk())
        closeUiAndTalk();
      if(world->currentCs()!=nullptr)
        return;
      // the loading itself is not ticked: wait kWorldMs of game time after it
      if(lastFrame==0)
        lastFrame = now;
      if(now-lastFrame<kWorldMs)
        return;
      lastFrame = 0;
      state     = State::NextWorld;
      break;
      }

    case State::NextPoint: {
      if(world==nullptr || pl==nullptr)
        return;
      if(pointId>=points.size()) {
        log("swiat " + worldTag() + ": gotowe");
        writeState(false);
        ++worldId;
        state = State::NextWorld;
        break;
        }
      uiStops = 0;
      placeHero(*world, *pl, points[pointId]);
      state = State::Settle;
      break;
      }

    case State::Settle: {
      if(world==nullptr || pl==nullptr)
        return;
      if(uiOrTalk() || world->currentCs()!=nullptr) {
        // a talk (an NPC with important news) or a cutscene came up: closed, the point once more
        closeUiAndTalk();
        if(uiStops<kUiRetries) {
          ++uiStops;
          placeHero(*world, *pl, points[pointId]);
          return;
          }
        // it keeps coming back: measured as it is (flagged "rozmowa")
        }
      if(now-t0>=kFpsSkipMs) {
        if(lastFrame!=0) {
          const uint64_t dt = now-lastFrame;
          frameSum += dt;
          frameMax  = std::max(frameMax, dt);
          ++frameNum;
          }
        lastFrame = now;
        }
      if(now-t0<kSettleMs)
        return;
      measure(*world, *pl);
      ++pointId;
      state = State::NextPoint;
      break;
      }
    }
  }

void ArpgAutotest::measure(World& w, Npc& pl) {
  const Point& pt = points[pointId];
  Result       r;
  r.uiStops = uiStops;
  r.fps     = frameSum>0 ? 1000.f*float(frameNum)/float(frameSum) : 0.f;
  r.fpsMin  = frameMax>0 ? 1000.f/float(frameMax) : 0.f;
  r.fall    = pt.pos.y - pl.position().y;

  // the camera and the hero
  if(auto cam = Gothic::inst().camera(); cam!=nullptr && w.physic()!=nullptr) {
    auto inv = cam->view();
    inv.inverse();
    Vec3 eye = {};
    inv.project(eye);

    const auto& av     = cam->arpgView();
    const Vec3  center = av.enabled ? av.center : pl.position()+Vec3(0,90,0);
    const Vec3  feet   = av.enabled ? av.feet   : pl.position();
    const float dist   = (center-eye).length();
    r.see = av.seeThrough;
    r.cut = av.cutHeight>0.f;

    auto& physic = *w.physic();
    // hidden: the first wall on the way is not cut away (ceiling cut, rock cut) and no see-through circle
    const auto hit = physic.ray(eye, center);
    if(hit.hasCol && (hit.v-eye).length()<dist-kHideMarginCm) {
      const float hitDist = (hit.v-eye).length();
      const bool  cutAway = (av.cutHeight>0.f && hit.v.y>feet.y+av.cutHeight-10.f) || (av.nearCut>0.f && hitDist<av.nearCut);
      r.hidden = !cutAway && av.seeThrough<0.5f;
      }
    // the camera in the rock: the first surface is seen from the inside
    const auto in = physic.rayBothSides(eye, center);
    if(in.hasCol && Vec3::dotProduct(in.n, center-eye)>0.f)
      r.rock = av.nearCut>0.f || av.cutHeight>0.f ? 1 : 2;
    }

  // the picture
  Pixmap thumb, full;
  if(hooks.shot) {
    full  = hooks.shot();
    thumb = downscale(full, kThumbW);
    blackness(thumb, r.blackAll, r.blackMid);
    }

  // the score
  auto kind = [&r](const char* k) {
    if(!r.kinds.empty())
      r.kinds += ',';
    r.kinds += k;
    };
  if(r.blackMid>=kBlackMid) {
    r.score += kScoreBlack*r.blackMid;
    kind("czarny");
    }
  if(r.hidden) {
    r.score += kScoreHidden;
    kind("zasloniety");
    }
  if(r.rock==2) {
    r.score += kScoreRockBad;
    kind("skala!");
    }
  else if(r.rock==1) {
    r.score += kScoreRockCut;
    kind("skala");
    }
  if(r.fps>0.f && r.fps<kFpsLow) {
    r.score += kScoreFps*(kFpsLow-r.fps);
    kind("fps");
    }
  if(r.fall>kFallCm) {
    r.score += kScoreFall;
    kind("upadek");
    }
  if(pt.dyNear>kStepCm) {
    r.score += kScoreStep;
    kind("pion");
    }
  if(r.uiStops>0) {
    r.score += kScoreUi;
    kind("rozmowa");
    }
  if(r.score>0.f) {
    ++suspects;
    }

  // the files: a small picture always, a big one for suspicious points
  const std::string tag  = worldTag();
  char              idx[16] = {};
  std::snprintf(idx, sizeof(idx), "%05u", unsigned(pointId));
  const std::string img  = "img/" + tag + "/" + idx + ".jpg";
  std::string       big;
  if(thumb.w()>0)
    thumb.save((runDir+img).c_str(), "jpg");
  if(r.score>=kDetailScore && full.w()>0) {
    big = "img/" + tag + "/" + idx + "_d.jpg";
    Pixmap d = downscale(full, kDetailW);
    if(d.w()>0)
      d.save((runDir+big).c_str(), "jpg");
    }

  std::string kinds = "[";
  {
    size_t at = 0;
    while(at<r.kinds.size()) {
      size_t end = r.kinds.find(',', at);
      if(end==std::string::npos)
        end = r.kinds.size();
      kinds += (kinds.size()>1 ? "," : "") + jsonStr(r.kinds.substr(at, end-at));
      at = end+1;
      }
  }
  kinds += "]";

  const Vec3 pos = pt.pos;
  std::string line = "P({w:" + jsonStr(tag) + ",i:" + std::to_string(pointId) + ",p:" + jsonStr(pt.name) +
                     ",x:" + std::to_string(iround(pos.x)) + ",y:" + std::to_string(iround(pos.y)) + ",z:" + std::to_string(iround(pos.z)) +
                     ",s:" + std::to_string(iround(r.score)) + ",k:" + kinds +
                     ",cb:" + std::to_string(iround(r.blackMid*100.f)) + ",ab:" + std::to_string(iround(r.blackAll*100.f)) +
                     ",see:" + std::to_string(iround(r.see*100.f)) + ",cut:" + (r.cut ? "1" : "0") +
                     ",f:" + std::to_string(iround(r.fps)) + ",fm:" + std::to_string(iround(r.fpsMin)) +
                     ",dy:" + std::to_string(iround(pt.dyNear)) + ",fall:" + std::to_string(iround(std::max(0.f, r.fall))) +
                     ",uw:" + (pt.underWater ? "1" : "0") +
                     ",img:" + jsonStr(img) + (big.empty() ? std::string() : ",d:" + jsonStr(big)) + "});\n";
  if(fPoints!=nullptr) {
    std::fputs(line.c_str(), fPoints);
    std::fflush(fPoints);
    }

  ++pointsDone;
  if(r.score>0.f) {
    auto& rp = recentPoints;
    rp.push_back(tag + " " + pt.name + ": " + std::to_string(iround(r.score)) + " (" + r.kinds + ")");
    if(rp.size()>5)
      rp.erase(rp.begin());
    }
  if(r.score>0.f || pointId%100==0)
    log(tag + " " + std::to_string(pointId+1) + "/" + std::to_string(points.size()) + " " + pt.name +
        " ocena " + std::to_string(iround(r.score)) + (r.kinds.empty() ? "" : " (" + r.kinds + ")"));
  if(pointsDone%20==0)
    writeState(false);
  }

void ArpgAutotest::writeReport() const {
  if(std::FILE* f = std::fopen((runDir+"raport.html").c_str(), "wb")) {
    std::fwrite(kReportHtml, 1, sizeof(kReportHtml)-1, f);
    std::fclose(f);
    }
  }

void ArpgAutotest::writeState(bool done) const {
  if(std::FILE* f = std::fopen((runDir+"stan.js").c_str(), "wb")) {
    const std::string s = "STAN={start:" + jsonStr(startStamp) + ",swiat:" + jsonStr(worldTag()) +
                          ",gotowe:" + std::to_string(pointsDone) + ",podejrzane:" + std::to_string(suspects) +
                          ",koniec:" + (done ? "true" : "false") + ",czas:" + jsonStr(stamp("%Y-%m-%d %H:%M:%S")) + "};\n";
    std::fputs(s.c_str(), f);
    std::fclose(f);
    }
  }

void ArpgAutotest::writePrevious() const {
  // the newest earlier run with results: the report compares the two
  std::string     prev;
  std::error_code ec;
  for(auto& e:std::filesystem::directory_iterator("autotest", ec)) {
    if(!e.is_directory())
      continue;
    const std::string n = e.path().filename().string();
    if(n==startStamp || n>startStamp || !std::filesystem::exists(e.path()/"punkty.js"))
      continue;
    if(n>prev)
      prev = n;
    }
  if(std::FILE* f = std::fopen((runDir+"poprzedni.js").c_str(), "wb")) {
    const std::string s = "POPRZEDNI=" + jsonStr(prev) + ";\n";
    std::fputs(s.c_str(), f);
    std::fclose(f);
    }
  }

void ArpgAutotest::log(const std::string& line) {
  if(fLog==nullptr)
    return;
  const std::string s = stamp("%H:%M:%S ") + line + "\n";
  std::fputs(s.c_str(), fLog);
  std::fflush(fLog);
  }

// ---------------------------------------------------------------------------------------------------------------------
// the panel

namespace {
  // one line in the width: cut at the end ("..")
  std::string fitLine(const GthFont& f, std::string s, int width) {
    if(f.textSize(s).w<=width)
      return s;
    while(s.size()>3 && f.textSize(s + "..").w>width)
      s.pop_back();
    return s + "..";
    }
  }

void ArpgAutotest::paint(Tempest::Painter& p, int w, int h) const {
  using namespace Tempest;
  ArpgBotPanel pn;
  if(plot!=nullptr)
    plot->panel(pn, Gothic::inst().player());
  else if(state!=State::Off && state!=State::Plot) {
    pn.title = "Bot: objazd kamery" + std::string(state==State::Done ? " (koniec)" : "");
    pn.stats = "świat " + worldTag() + " | punkt " + std::to_string(std::min(pointId+1, points.size())) + "/" +
               std::to_string(points.size()) + " | wszystkie " + std::to_string(pointsDone) + " | podejrzane " + std::to_string(suspects);
    if(pointId<points.size())
      pn.current = points[pointId].name;
    switch(state) {
      case State::WaitGame:  pn.doing = "czeka na nową grę"; break;
      case State::NextWorld:
      case State::WaitWorld: pn.doing = "zmiana świata"; break;
      case State::NextPoint: pn.doing = "następny punkt"; break;
      case State::Settle:    pn.doing = "kamera się ustawia, pomiar"; break;
      default: break;
      }
    for(auto& r:recentPoints)
      pn.events.push_back({r, "", 4});
    }
  else
    return;

  const float k    = std::max(0.6f, float(h)/1080.f);
  const auto& fnt  = Resources::font(Resources::FontType::Normal, 0.9f*k);
  const auto& fntH = Resources::font("font_old_10_white.tga", Resources::FontType::Normal, 1.15f*k);
  const int   pad  = int(10*k);
  const int   lh   = fnt.pixelSize()+int(4*k);
  const int   lhH  = fntH.pixelSize()+int(6*k);
  const int   pw   = std::min(int(600*k), w/2);
  const int   x0   = int(14*k);
  const int   y0   = int(80*k);
  const int   tw   = pw-2*pad;

  // the height: title, stats, then the sections with their headers
  int lines = 1; // stats
  int heads = 1; // title
  if(!pn.done.empty())   { heads++; lines += int(pn.done.size()); }
  heads++; lines += 2;                                   // "Teraz": the step and what it does
  if(!pn.next.empty())   { heads++; lines += int(pn.next.size()); }
  if(!pn.events.empty()) { heads++; lines += int(pn.events.size()); }
  const int ph = 2*pad + heads*lhH + lines*lh;

  p.setBrush(Brush(Color(0.f, 0.f, 0.f, 0.62f), Painter::Alpha));
  p.drawRect(x0, y0, pw, ph);
  p.setBrush(Brush(Color(0.85f, 0.65f, 0.29f, 0.8f), Painter::Alpha));
  p.drawRect(x0, y0, pw, std::max(1, int(2*k)));

  const Color gold  (0.95f, 0.78f, 0.42f, 1.f);
  const Color white (0.92f, 0.88f, 0.80f, 1.f);
  const Color dim   (0.62f, 0.58f, 0.50f, 1.f);
  const Color green (0.55f, 0.88f, 0.45f, 1.f);
  const Color red   (1.00f, 0.45f, 0.35f, 1.f);
  const Color event (0.90f, 0.72f, 0.45f, 1.f);

  int y = y0+pad;
  auto head = [&](const std::string& t) {
    GthFont f = fntH;
    f.setColor(gold);
    y += lhH;
    f.drawText(p, x0+pad, y-int(4*k), ArpgMenu::gtxt(t));
    };
  auto line = [&](const std::string& t, const Color& c, int indent) {
    GthFont f = fnt;
    f.setColor(c);
    y += lh;
    f.drawText(p, x0+pad+indent, y-int(3*k), fitLine(f, ArpgMenu::gtxt(t), tw-indent));
    };
  auto kindColor = [&](int kind) {
    switch(kind) {
      case 1: return green;
      case 2: return red;
      case 3: return dim;
      case 4: return event;
      }
    return white;
    };
  const int ind = int(14*k);

  head(pn.title);
  line(pn.stats, dim, 0);
  if(!pn.done.empty()) {
    head("Zrobione");
    for(auto& d:pn.done) {
      const char* mark = d.kind==1 ? "OK   " : (d.kind==2 ? "BŁĄD " : "-    ");
      line(mark + d.text + (d.note.empty() ? std::string() : "  — " + d.note), kindColor(d.kind), ind);
      }
    }
  head("Teraz");
  line("> " + (pn.current.empty() ? std::string("-") : pn.current), gold, ind);
  line(pn.doing.empty() ? std::string("-") : pn.doing, white, ind*2);
  if(!pn.next.empty()) {
    head("Dalej");
    for(auto& n:pn.next)
      line(n.text, n.kind==2 ? red : dim, ind);
    }
  if(!pn.events.empty()) {
    head(plot!=nullptr ? "Zdarzenia" : "Podejrzane punkty");
    for(auto& e:pn.events)
      line((e.note.empty() ? std::string() : "[" + e.note + "] ") + e.text, kindColor(e.kind), ind);
    }
  }
