#include "arpgplotbot.h"

#include <Tempest/Application>
#include <Tempest/Log>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

#include "diathicassets.h"
#include "commandline.h"
#include "gothic.h"
#include "camera.h"
#include "game/gamescript.h"
#include "game/gamesession.h"
#include "game/playercontrol.h"
#include "game/questlog.h"
#include "game/inventory.h"
#include "game/arpgstatus.h"
#include "ui/dialogmenu.h"
#include "world/focus.h"
#include "world/objects/item.h"
#include "world/objects/npc.h"
#include "world/waypath.h"
#include "world/world.h"
#include "physics/dynamicworld.h"

using namespace Tempest;

namespace {
  // timing (ms)
  constexpr uint64_t kReadyMs       = 3000;    // after the new game / the save: the world settles
  constexpr uint64_t kPickGapMs     = 350;     // between two picks in a talk (the list animates in)
  constexpr uint64_t kTalkClickMs   = 2500;    // a click on the npc again when the talk does not start
  constexpr uint64_t kTalkStartMs   = 30000;   // the talk does not start: error
  constexpr uint64_t kLineWaitMs    = 10000;   // the line asked for does not show up in the list: error
  constexpr uint64_t kStrayStuckMs  = 12000;   // a talk nobody expected does not end: closed by force
  constexpr uint64_t kCheckMs       = 3000;    // "sprawdz": the script has this long to do its part
  constexpr uint64_t kWaitMs        = 60000;   // "czekaj" without "na N"
  constexpr uint64_t kStepMs        = 900000;  // any step: 15 minutes at most
  constexpr uint64_t kKillMs        = 240000;
  constexpr uint64_t kTakeMs        = 120000;
  constexpr uint64_t kDefendMs      = 60000;
  constexpr uint64_t kPotionGapMs   = 4000;

  // walking (cm)
  constexpr float    kRouteMinGap   = 300.f;   // route points closer than this to the last one are dropped
  constexpr float    kRoutePass     = 350.f;   // a route point counts as passed
  constexpr float    kProgress      = 80.f;    // closer by this much = progress
  constexpr uint64_t kStuckMs       = 6000;    // no progress this long = stuck once
  constexpr int      kStuckJump     = 5;       // stuck this many times on one point: moved there (event)
  constexpr int      kMaxJumps      = 6;       // moved this many times in one walk: the walk failed
  constexpr uint64_t kReclickMs     = 3000;

  // talking, fighting (cm)
  constexpr float    kTalkWalk      = 450.f;   // walk this close, then click on the npc
  constexpr float    kFightWalk     = 1200.f;
  constexpr float    kThreatRange   = 1200.f;  // an enemy this close that goes for the hero: the bot fights it
  constexpr float    kThreatNear    = 900.f;   // a beast (or an armed man) this close: fought before it bites
  constexpr float    kDrawRange     = 1500.f;  // an enemy this close: the weapon out
  constexpr float    kQuietRange    = 1500.f;  // no enemy this close: time to heal, loot, pick up
  constexpr int64_t  kParryLeadMs   = 180;     // the block starts this long before the enemy's hit (ArpgStatus::ParryWindowMs 250)
  constexpr int64_t  kWindUpMs      = 650;     // a swing this far from its hit: the answer is chosen, a parry stops attacking
  constexpr float    kParryChance   = 0.75f;   // a decent player: not every swing is answered
  constexpr uint64_t kDodgeGapMs    = 3000;    // at most one dodge in this time
  constexpr uint64_t kReachMs       = 5000;    // no closer to the target in this time (and not in reach): out of reach
  constexpr uint64_t kIgnoreMs      = 20000;
  constexpr float    kChargeChance  = 0.35f;
  constexpr uint64_t kSkillGapMs    = 1500;
  constexpr float    kHealBelow     = 0.70f;   // between fights: eat below this part of life ...
  constexpr float    kHealUpTo      = 0.90f;   // ... up to this part
  constexpr float    kPotionBelow   = 0.50f;   // ... and a potion only below this part (when there is no food)
  constexpr uint64_t kHealGapMs     = 1600;    // one bite / gulp at a time (the animation)
  constexpr float    kLootRange     = 600.f;   // bodies right where the fight was (no going back for them)
  constexpr float    kPickRange     = 500.f;   // things close by, in sight, and ahead on the way
  constexpr uint64_t kChoreMs       = 8000;    // a body / an item the bot cannot reach in this time is left
  constexpr float    kDefendLeave   = 1800.f;
  constexpr float    kTakeRange     = 3000.f;

  // health
  constexpr float    kPotionAt      = 0.40f;

  constexpr size_t   kMaxErrSaves   = 5;
  constexpr size_t   kPanelDone     = 7;       // the panel: steps done, steps to come, events
  constexpr size_t   kPanelNext     = 7;
  constexpr size_t   kPanelEvents   = 5;

  std::string upper(std::string_view s) {
    std::string r(s);
    for(auto& c:r)
      c = char(std::toupper(static_cast<unsigned char>(c)));
    return r;
    }

  bool same(std::string_view a, std::string_view b) {
    return upper(a)==upper(b);
    }

  std::string stamp(const char* fmt) {
    char        buf[64] = {};
    std::time_t t  = std::time(nullptr);
    std::tm*    lt = std::localtime(&t);
    if(lt!=nullptr)
      std::strftime(buf, sizeof(buf), fmt, lt);
    return buf;
    }

  std::string js(std::string_view s) {
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

  // cp1250 (the Polish game's texts) -> UTF-8; other bytes above 0x7F -> '?'
  std::string toUtf8(std::string_view s) {
    static const std::pair<uint8_t, uint16_t> map[] = {
      {0xB9,0x105},{0xA5,0x104},{0xE6,0x107},{0xC6,0x106},{0xEA,0x119},{0xCA,0x118},{0xB3,0x142},{0xA3,0x141},
      {0xF1,0x144},{0xD1,0x143},{0xF3,0x0F3},{0xD3,0x0D3},{0x9C,0x15B},{0x8C,0x15A},{0x9F,0x17A},{0x8F,0x179},
      {0xBF,0x17C},{0xAF,0x17B},{0xE4,0x0E4},{0xC4,0x0C4},{0xF6,0x0F6},{0xD6,0x0D6},{0xFC,0x0FC},{0xDC,0x0DC},{0xDF,0x0DF},
      };
    std::string out;
    out.reserve(s.size());
    for(char ch:s) {
      const auto c = uint8_t(ch);
      if(c<0x80) {
        out += ch;
        continue;
        }
      uint16_t cp = 0;
      for(auto& m:map)
        if(m.first==c)
          cp = m.second;
      if(cp==0) {
        out += '?';
        continue;
        }
      out += char(0xC0 | (cp>>6));
      out += char(0x80 | (cp&0x3F));
      }
    return out;
    }

  std::string nameOf(const Npc& n) {
    return toUtf8(n.displayName());
    }

  std::string meters(float cm) {
    char buf[32] = {};
    std::snprintf(buf, sizeof(buf), cm<1000.f ? "%.1f m" : "%.0f m", double(cm)/100.0);
    return buf;
    }

  float dist2d(const Vec3& a, const Vec3& b) {
    return std::hypot(a.x-b.x, a.z-b.z);
    }

  bool isNumber(const std::string& s) {
    if(s.empty())
      return false;
    size_t i = (s[0]=='-' || s[0]=='+') ? 1 : 0;
    if(i>=s.size())
      return false;
    for(; i<s.size(); ++i)
      if(!std::isdigit(static_cast<unsigned char>(s[i])))
        return false;
    return true;
    }

  // the line that ends a talk: DIA_<npc>_EXIT first ("KONIEC"), a FirstEXIT (it may start a chapter) only when there is no other
  const DialogMenu::ArpgBotLine* exitLine(const std::vector<DialogMenu::ArpgBotLine>& lines) {
    const DialogMenu::ArpgBotLine* any = nullptr;
    for(auto& l:lines) {
      const std::string u = upper(l.name);
      if(u.find("EXIT")==std::string::npos)
        continue;
      if(u.size()>5 && u.compare(u.size()-5, 5, "_EXIT")==0)
        return &l;
      if(any==nullptr)
        any = &l;
      }
    return any;
    }

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

  const char kReportHtml[] = R"HTML(<!doctype html>
<html lang="pl"><head><meta charset="utf-8"><title>Diathic - bot fabuły</title>
<style>
:root{--bg:#16130f;--card:#221d17;--line:#3a3128;--txt:#e8dcc8;--dim:#a8977c;--gold:#d8a64a;--red:#e0604a;--green:#7cbf6a}
body{margin:0;background:var(--bg);color:var(--txt);font:14px/1.45 system-ui,Segoe UI,sans-serif}
header,section{padding:12px 16px;border-bottom:1px solid var(--line)}
h1{margin:0 0 4px;font-size:20px;color:var(--gold)}h2{margin:0 0 8px;font-size:16px;color:var(--gold)}
.dim{color:var(--dim)}.ok{color:var(--green)}.blad{color:var(--red)}.pom{color:var(--dim)}
#pierwszy{background:#3a1d17;border:1px solid var(--red);border-radius:6px;padding:10px;margin-top:8px}
table{border-collapse:collapse;width:100%}td,th{padding:4px 8px;border-bottom:1px solid var(--line);vertical-align:top;text-align:left}
td.n{color:var(--dim);white-space:nowrap}td.t{font-family:Consolas,monospace;font-size:13px;word-break:break-word}
img{max-width:320px;display:block;margin-top:4px;border:1px solid var(--line)}
label{margin-right:12px}select,input{background:var(--card);color:var(--txt);border:1px solid var(--line);border-radius:4px;padding:3px 6px}
.zm{color:var(--gold)}
</style></head><body>
<header><h1 id="tytul">Bot fabuły</h1><div id="stan" class="dim">brak danych</div><div id="pierwszy" style="display:none"></div></header>
<section><h2>Kroki</h2><label>Pokaż <select id="fr"><option value="">wszystkie</option><option value="blad">błędy</option><option value="zm">zmiany wobec poprzedniego</option></select></label><span id="ile" class="dim"></span>
<table id="kroki"><thead><tr><th>#</th><th>Krok</th><th>Wynik</th><th>Czas</th><th>Uwagi</th></tr></thead><tbody></tbody></table></section>
<section><h2>Zdarzenia</h2><label>Rodzaj <select id="fz"></select></label>
<table id="zdarzenia"><thead><tr><th>Krok</th><th>Rodzaj</th><th>Opis</th></tr></thead><tbody></tbody></table></section>
<script>var K=[],Z=[],PREV={},STAN=null,POPRZEDNI="";function k(o){K.push(o)}function z(o){Z.push(o)}</script>
<script src="stan.js"></script>
<script src="poprzedni.js"></script>
<script src="kroki.js"></script>
<script src="zdarzenia.js"></script>
<script>
var WYN={ok:"OK",blad:"BŁĄD",pom:"pominięty"};
var RODZ={zaciecie:"zacięcie (przeniesiony)",leczenie:"leczenie bota",rozmowa:"rozmowa nieoczekiwana",wybor:"wybór automatyczny",obrona:"obrona",smierc:"śmierć (gra dalej)",mikstura:"mikstura",lup:"łup z ciała",bron:"lepsza broń",zbieranie:"zbieranie",walidacja:"walidacja nazw",zapis:"zapis gry",czas:"przewinięcie czasu"};
function $(i){return document.getElementById(i)}
function el(t,c,x){var e=document.createElement(t);if(c)e.className=c;if(x!=null)e.textContent=x;return e}
function sek(ms){return ms<1000?ms+" ms":(ms<120000?(ms/1000).toFixed(1)+" s":(ms/60000).toFixed(1)+" min")}
function start(){
  if(STAN){$("tytul").textContent="Bot fabuły - "+STAN.scenariusz;
    $("stan").textContent="Start "+STAN.start+" · krok "+STAN.krok+" / "+STAN.kroki+" · OK "+STAN.ok+" · błędy "+STAN.bledy+" · pominięte "+STAN.pom+" · śmierci "+(STAN.smierci||0)+" · mikstury "+(STAN.mikstury||0)+" · parowania "+(STAN.parowaniaOk||0)+"/"+(STAN.parowania||0)+" · uniki "+(STAN.uniki||0)+" · ładowane "+(STAN.ladowane||0)+" · umiejętności "+(STAN.umiejetnosci||0)+" · zdarzenia "+STAN.zdarzenia+" · zacięcia "+STAN.zaciecia+" · czas "+sek(STAN.ms)+(STAN.koniec?" · zakończony ("+STAN.powod+")":" · w toku")+(POPRZEDNI?" · porównanie z "+POPRZEDNI:"")+" · scenariusz: "+STAN.zrodlo;}
  var f=K.filter(function(o){return o.r=="blad"})[0];
  if(f){var p=$("pierwszy");p.style.display="";p.appendChild(el("b",null,"Pierwszy błąd: krok "+f.n+" (linia "+f.l+"): "));p.appendChild(el("span","t",f.t));p.appendChild(el("div",null,f.u));if(f.img){var i=el("img");i.src=f.img;p.appendChild(i)}if(f.zapis)p.appendChild(el("div","dim","zapis gry: "+f.zapis))}
  $("fr").onchange=rysuj;
  var rz={};Z.forEach(function(o){rz[o.k]=1});var fz=$("fz");fz.appendChild(new Option("wszystkie",""));Object.keys(rz).forEach(function(x){fz.appendChild(new Option(RODZ[x]||x,x))});fz.onchange=rysujZ;
  rysuj();rysujZ();
}
function rysuj(){
  var fr=$("fr").value,tb=document.querySelector("#kroki tbody");tb.innerHTML="";var n=0;
  K.forEach(function(o){var p=PREV[o.t+"|"+o.l],zm=POPRZEDNI&&p&&p.r!=o.r;
    if(fr=="blad"&&o.r!="blad")return;if(fr=="zm"&&!zm)return;n++;
    var tr=el("tr");tr.appendChild(el("td","n",o.n+" (l. "+o.l+")"));tr.appendChild(el("td","t",o.t));
    var w=el("td",o.r,WYN[o.r]||o.r);if(zm)w.appendChild(el("div","zm","było: "+(WYN[p.r]||p.r)));tr.appendChild(w);
    tr.appendChild(el("td","n",sek(o.ms)));var u=el("td",null,o.u||"");if(o.img){var a=el("a");a.href=o.img;a.target="_blank";var i=el("img");i.loading="lazy";i.src=o.img;a.appendChild(i);u.appendChild(a)}if(o.zapis)u.appendChild(el("div","dim","zapis: "+o.zapis));tr.appendChild(u);tb.appendChild(tr)});
  $("ile").textContent=" "+n+" kroków";
}
function rysujZ(){
  var fz=$("fz").value,tb=document.querySelector("#zdarzenia tbody");tb.innerHTML="";
  Z.forEach(function(o){if(fz&&o.k!=fz)return;var tr=el("tr");tr.appendChild(el("td","n",o.n));tr.appendChild(el("td",null,RODZ[o.k]||o.k));var u=el("td",null,o.t);if(o.img){var a=el("a");a.href=o.img;a.target="_blank";var i=el("img");i.loading="lazy";i.src=o.img;a.appendChild(i);u.appendChild(a)}tr.appendChild(u);tb.appendChild(tr)});
}
if(POPRZEDNI){k=function(o){PREV[o.t+"|"+o.l]=o};var sc=document.createElement("script");sc.src="../"+POPRZEDNI+"/kroki.js";sc.onload=sc.onerror=function(){k=function(o){K.push(o)};start()};document.body.appendChild(sc);}else start();
</script></body></html>
)HTML";
  }

ArpgPlotBot::ArpgPlotBot(std::string n, std::string dir, Hooks h)
  :hooks(std::move(h)), name(std::move(n)), runDir(std::move(dir)) {
  std::error_code ec;
  std::filesystem::create_directories(runDir + "img", ec);
  fSteps     = std::fopen((runDir+"kroki.js").c_str(), "wb");
  fEvents    = std::fopen((runDir+"zdarzenia.js").c_str(), "wb");
  fLog       = std::fopen((runDir+"log.txt").c_str(), "wb");
  startLabel = std::string(CommandLine::inst().arpgAutotestFrom());
  runT0      = Application::tickCount();
  writeReport();
  writePrevious();
  if(!load()) {
    log("brak scenariusza " + name + " (autotest\\" + name + ".txt ani wbudowanego)");
    finish("brak scenariusza");
    return;
    }
  log("scenariusz " + name + " (" + source + "), kroków " + std::to_string(steps.size()) +
      (startLabel.empty() ? std::string() : ", od etykiety " + startLabel));
  writeState();
  }

ArpgPlotBot::~ArpgPlotBot() {
  for(auto f:{fSteps, fEvents, fLog})
    if(f!=nullptr)
      std::fclose(f);
  }

bool ArpgPlotBot::load() {
  // a file next to the saves wins (edit without a new build), otherwise the one built into the game
  const std::string file = "autotest/" + name + ".txt";
  std::ifstream     in(file, std::ios::binary);
  if(in) {
    std::stringstream ss;
    ss << in.rdbuf();
    source = "gra\\dane\\autotest\\" + name + ".txt";
    parse(ss.str());
    return !steps.empty();
    }
  auto a = DiathicAsset::get("autotest_" + name + ".txt");
  if(a.empty())
    return false;
  source = "wbudowany autotest_" + name + ".txt";
  parse(std::string(a.data, a.len));
  return !steps.empty();
  }

void ArpgPlotBot::parse(const std::string& text) {
  std::istringstream in(text);
  std::string        ln;
  int                line = 0;
  while(std::getline(in, ln)) {
    ++line;
    if(!ln.empty() && ln.back()=='\r')
      ln.pop_back();
    const size_t hash = ln.find('#');
    if(hash!=std::string::npos)
      ln.resize(hash);
    std::istringstream       ws(ln);
    std::vector<std::string> tok;
    std::string              t;
    while(ws >> t)
      tok.push_back(t);
    if(tok.empty())
      continue;
    const std::string verb = tok[0];
    tok.erase(tok.begin());
    if(verb=="gdy") {
      for(auto& x:tok)
        handlers.push_back(x);
      continue;
      }
    if(verb=="po_bledzie") {
      stopOnError = !tok.empty() && tok[0]=="stop";
      continue;
      }
    if(verb=="po_smierci") {
      endOnDeath = !tok.empty() && tok[0]=="koniec";
      continue;
      }
    Step s;
    s.line = line;
    s.verb = verb;
    s.arg  = std::move(tok);
    // the text without the comment, single spaces
    s.text = verb;
    for(auto& x:s.arg)
      s.text += " " + x;
    if(s.verb=="?" && !s.arg.empty()) {
      // "? krok": optional
      s.optional = true;
      s.verb     = s.arg[0];
      s.arg.erase(s.arg.begin());
      }
    if(s.verb=="jesli") {
      // "jesli <warunek> to <krok>": the step only when the condition holds
      auto to = std::find(s.arg.begin(), s.arg.end(), "to");
      if(to!=s.arg.end() && to+1!=s.arg.end()) {
        s.cond.assign(s.arg.begin(), to);
        s.verb = *(to+1);
        s.arg.assign(to+2, s.arg.end());
        } else {
        s.verb = "jesli?"; // no "to": an invalid step
        }
      }
    steps.push_back(std::move(s));
    }
  }

// ---------------------------------------------------------------------------------------------------------------------
// names

Npc* ArpgPlotBot::findNpc(World& w, const std::string& n) const {
  const size_t id = w.script().findSymbolIndex(n);
  if(id==size_t(-1))
    return nullptr;
  return w.findNpcByInstance(id);
  }

size_t ArpgPlotBot::itemId(World& w, const std::string& n) const {
  if(w.script().arpgClassOf(n)!="C_ITEM")
    return size_t(-1);
  return w.script().findSymbolIndex(n);
  }

bool ArpgPlotBot::intValue(World& w, const std::string& n, int32_t& out) const {
  if(isNumber(n)) {
    out = int32_t(std::strtol(n.c_str(), nullptr, 10));
    return true;
    }
  auto sym = w.script().findSymbol(n);
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::INT)
    return false;
  out = sym->get_int();
  return true;
  }

bool ArpgPlotBot::targetPos(World& w, const std::string& n, Vec3& out, float& reach) const {
  if(w.script().arpgClassOf(n)=="C_NPC") {
    if(auto npc = findNpc(w, n)) {
      out   = npc->position();
      reach = 300.f;
      return true;
      }
    return false;
    }
  if(auto wp = w.findPoint(n, false)) {
    out   = wp->position();
    reach = 250.f;
    return true;
    }
  return false;
  }

bool ArpgPlotBot::checkName(World& w, const std::string& n, const char* kind, std::string& why) {
  auto&             sc  = w.script();
  const std::string k   = kind;
  const std::string cls = sc.arpgClassOf(n);
  bool              ok  = true;
  if(k=="npc")
    ok = cls=="C_NPC";
  else if(k=="info")
    ok = cls=="C_INFO";
  else if(k=="item")
    ok = cls=="C_ITEM";
  else if(k=="line")
    ok = cls=="C_INFO" || (sc.findSymbol(n)!=nullptr && sc.findSymbol(n)->type()==zenkit::DaedalusDataType::FUNCTION);
  else if(k=="int") {
    int32_t v = 0;
    ok = intValue(w, n, v);
    }
  else if(k=="topic")
    ok = sc.findSymbol(n)!=nullptr && sc.findSymbol(n)->type()==zenkit::DaedalusDataType::STRING;
  else if(k=="place")
    ok = cls=="C_NPC" || w.findPoint(n, false)!=nullptr || !upper(n).starts_with("NW_"); // other worlds: checked on the way
  if(!ok)
    why += (why.empty() ? "" : "; ") + std::string("nieznane ") + kind + " " + n;
  return ok;
  }

bool ArpgPlotBot::checkCond(World& w, const std::vector<std::string>& a, size_t from, std::string& why) {
  if(from>=a.size()) {
    why += "brak warunku";
    return false;
    }
  if(a[from]=="nie")
    return checkCond(w, a, from+1, why);
  const std::string& k = a[from];
  auto arg = [&](size_t i) -> std::string { return from+i<a.size() ? a[from+i] : std::string(); };
  if(k=="info")      return checkName(w, arg(1), "info", why);
  if(k=="zmienna")   return checkName(w, arg(1), "int", why) && checkName(w, arg(3), "int", why);
  if(k=="przedmiot") return checkName(w, arg(1), "item", why);
  if(k=="dziennik")  return checkName(w, arg(1), "topic", why);
  if(k=="martwy")    return checkName(w, arg(1), "npc", why);
  if(k=="gildia")    return checkName(w, arg(1), "int", why);
  if(k=="rozdzial")  return true;
  if(k=="blisko")    return checkName(w, arg(1), "npc", why) && checkName(w, arg(2), "place", why);
  if(k=="obok")      return checkName(w, arg(1), "npc", why);
  if(k=="godzina")   return true;
  why += "nieznany warunek " + k;
  return false;
  }

void ArpgPlotBot::validate(World& w) {
  size_t bad = 0;
  for(auto& s:steps) {
    std::string why;
    bool        ok = s.cond.empty() || checkCond(w, s.cond, 0, why);
    const auto& v  = s.verb;
    auto at = [&](size_t i) -> std::string { return i<s.arg.size() ? s.arg[i] : std::string(); };
    if(v=="rozmowa") {
      ok = checkName(w, at(0), "npc", why) && ok;
      if(s.arg.size()>1)
        ok = checkName(w, at(1), "info", why) && ok;
      }
    else if(v=="wybor")
      ok = checkName(w, at(0), "line", why) && ok;
    else if(v=="zabij") {
      for(auto& x:s.arg)
        ok = checkName(w, x, "npc", why) && ok;
      ok = ok && !s.arg.empty();
      }
    else if(v=="zabierz" || v=="zaloz" || v=="uzyj")
      ok = checkName(w, at(0), "item", why) && ok;
    else if(v=="idz" || v=="przenies")
      ok = checkName(w, at(0), "place", why) && ok;
    else if(v=="czekaj" || v=="sprawdz")
      ok = checkCond(w, s.arg, 0, why) && ok;
    else if(v=="spij")
      ok = at(0).size()==5 && at(0)[2]==':';
    else if(v=="koniec" || v=="etykieta" || v=="zapisz")
      ok = true;
    else {
      ok  = false;
      why = "nieznany krok " + v;
      }
    if(!ok) {
      s.invalid = true;
      ++bad;
      event("walidacja", "linia " + std::to_string(s.line) + ": " + s.text + " - " + why, false);
      }
    }
  for(auto& h:handlers) {
    std::string why;
    if(!checkName(w, h, "line", why))
      event("walidacja", "gdy " + h + " - " + why, false);
    }
  log("walidacja: " + std::to_string(bad) + " kroków z błędami");
  }

// ---------------------------------------------------------------------------------------------------------------------
// the run

void ArpgPlotBot::startRun(World& w) {
  validate(w);
  if(!startLabel.empty()) {
    bool found = false;
    for(size_t i=0; i<steps.size(); ++i)
      if(steps[i].verb=="etykieta" && !steps[i].arg.empty() && same(steps[i].arg[0], startLabel)) {
        for(size_t j=0; j<i; ++j) {
          ++nSkip;
          if(fSteps!=nullptr) {
            const std::string ln = "k({n:" + std::to_string(j+1) + ",l:" + std::to_string(steps[j].line) + ",t:" + js(steps[j].text) +
                                   ",r:\"pom\",ms:0,u:\"przed etykieta " + startLabel + "\"});\n";
            std::fputs(ln.c_str(), fSteps);
            }
          }
        cur   = i;
        found = true;
        break;
        }
    if(!found)
      log("brak etykiety " + startLabel + " – od początku");
    if(fSteps!=nullptr)
      std::fflush(fSteps);
    }
  started   = true;
  savesSeen = Gothic::inst().arpgBotSaves();
  Gothic::inst().setArpgBotGuard(!endOnDeath);
  stepT0  = phaseT0 = Application::tickCount();
  phase   = 0;
  writeState();
  }

void ArpgPlotBot::finish(const char* why) {
  if(done && fSteps==nullptr)
    return;
  done = true;
  Gothic::inst().setArpgBotGuard(false);
  log(std::string("koniec: ") + why + ", ok " + std::to_string(nOk) + ", błędy " + std::to_string(nFail));
  if(hooks.player!=nullptr)
    hooks.player->arpgBotRelease();
  writeState(true, why);
  for(auto f:{&fSteps, &fEvents, &fLog})
    if(*f!=nullptr) {
      std::fclose(*f);
      *f = nullptr;
      }
  }

void ArpgPlotBot::panel(ArpgBotPanel& out, const Npc* pl) const {
  out = ArpgBotPanel();
  out.title = "Bot fabuły: " + name + (done ? " (koniec)" : "");
  const uint64_t ms = Application::tickCount()-runT0;
  char t[32] = {};
  std::snprintf(t, sizeof(t), "%u:%02u:%02u", unsigned(ms/3600000), unsigned(ms/60000%60), unsigned(ms/1000%60));
  out.stats = "krok " + std::to_string(std::min(cur+1, steps.size())) + "/" + std::to_string(steps.size()) +
              " | OK " + std::to_string(nOk) + " | błędy " + std::to_string(nFail) + " | śmierci " + std::to_string(nDeaths) + " | parowania " + std::to_string(nParryOk) + "/" + std::to_string(nParry) +
              " | uniki " + std::to_string(nDodge) + " | zdarzenia " + std::to_string(nEvents) +
              " | " + t;
  if(pl!=nullptr)
    out.stats += " | zdrowie " + std::to_string(pl->attribute(ATR_HITPOINTS)) + "/" + std::to_string(pl->attribute(ATR_HITPOINTSMAX));
  if(!started)
    out.doing = "czeka na świat gry";
  else if(cur<steps.size()) {
    out.current = steps[cur].text;
    out.doing   = doing.empty() ? std::string("...") : doing;
    }
  out.done   = recentSteps;
  out.events = recentEvents;
  for(size_t i=cur+1; i<steps.size() && out.next.size()<kPanelNext; ++i)
    out.next.push_back({steps[i].text, "", steps[i].invalid ? 2 : 0});
  }

std::string ArpgPlotBot::summary() const {
  return name + ": ok " + std::to_string(nOk) + ", błędy " + std::to_string(nFail) + ", zdarzenia " + std::to_string(nEvents);
  }

std::string ArpgPlotBot::picture(const char* tag) {
  if(!hooks.shot)
    return "";
  Pixmap full = hooks.shot();
  if(full.w()==0)
    return "";
  Pixmap d = downscale(full, 1280);
  if(d.w()==0)
    return "";
  char idx[32] = {};
  std::snprintf(idx, sizeof(idx), "%03u_%s_%u", unsigned(cur+1), tag, unsigned(nEvents+nFail));
  const std::string img = std::string("img/") + idx + ".jpg";
  d.save((runDir+img).c_str(), "jpg");
  return img;
  }

void ArpgPlotBot::event(const char* kind, const std::string& text, bool pic) {
  ++nEvents;
  recentEvents.push_back({text, kind, 4});
  if(recentEvents.size()>kPanelEvents)
    recentEvents.erase(recentEvents.begin());
  const std::string img = pic ? picture(kind) : std::string();
  log(std::string("[") + kind + "] " + text);
  if(fEvents==nullptr)
    return;
  const std::string ln = "z({n:" + std::to_string(cur+1) + ",k:" + js(kind) + ",t:" + js(text) +
                         (img.empty() ? std::string() : ",img:" + js(img)) + "});\n";
  std::fputs(ln.c_str(), fEvents);
  std::fflush(fEvents);
  }

void ArpgPlotBot::skipStep(const std::string& why) {
  const Step& s = steps[cur];
  ++nSkip;
  log("POM  " + std::to_string(cur+1) + " l." + std::to_string(s.line) + " " + s.text + " - " + why);
  if(fSteps!=nullptr) {
    const std::string ln = "k({n:" + std::to_string(cur+1) + ",l:" + std::to_string(s.line) + ",t:" + js(s.text) +
                           ",r:\"pom\",ms:" + std::to_string(Application::tickCount()-stepT0) + ",u:" + js(why) + "});\n";
    std::fputs(ln.c_str(), fSteps);
    std::fflush(fSteps);
    }
  recentSteps.push_back({s.text, why, 3});
  if(recentSteps.size()>kPanelDone)
    recentSteps.erase(recentSteps.begin());
  note.clear();
  doing.clear();
  walkWhat.clear();
  goalOn         = false;
  fightWalkUntil = 0;
  walk     = Walk();
  phase    = 0;
  picked   = false;
  stepT0   = phaseT0 = Application::tickCount();
  if(hooks.player!=nullptr)
    hooks.player->arpgBotRelease();
  ++cur;
  writeState();
  if(cur>=steps.size())
    finish("koniec scenariusza");
  }

void ArpgPlotBot::nextStep(bool ok, const std::string& why) {
  if(!ok && steps[cur].optional) {
    skipStep("opcjonalny, nie wyszedł: " + why);
    return;
    }
  const Step&    s   = steps[cur];
  const uint64_t ms  = Application::tickCount()-stepT0;
  std::string    img, sav;
  std::string    u   = why;
  if(!note.empty())
    u = u.empty() ? note : u + " | " + note;
  if(ok)
    ++nOk;
  else {
    ++nFail;
    img = picture("blad");
    if(nErrSaves<kMaxErrSaves && hooks.save && (hooks.dialogs==nullptr || hooks.dialogs->arpgBotNpc()==nullptr)) {
      // a save from the moment of the error (load: -save bot_blad<N>)
      ++nErrSaves;
      sav = "bot_blad" + std::to_string(nErrSaves);
      hooks.save("save_slot_" + sav + ".sav", "Bot: blad " + std::to_string(nErrSaves) + " (" + name + ")");
      }
    }
  log(std::string(ok ? "OK   " : "BLAD ") + std::to_string(cur+1) + " l." + std::to_string(s.line) + " " + s.text +
      (u.empty() ? std::string() : " - " + u));
  if(fSteps!=nullptr) {
    const std::string ln = "k({n:" + std::to_string(cur+1) + ",l:" + std::to_string(s.line) + ",t:" + js(s.text) +
                           ",r:" + (ok ? "\"ok\"" : "\"blad\"") + ",ms:" + std::to_string(ms) + ",u:" + js(u) +
                           (img.empty() ? std::string() : ",img:" + js(img)) +
                           (sav.empty() ? std::string() : ",zapis:" + js("-save " + sav)) + "});\n";
    std::fputs(ln.c_str(), fSteps);
    std::fflush(fSteps);
    }
  goalOn         = false;
  fightWalkUntil = 0;
  recentSteps.push_back({s.text, u, ok ? 1 : 2});
  if(recentSteps.size()>kPanelDone)
    recentSteps.erase(recentSteps.begin());
  note.clear();
  doing.clear();
  walkWhat.clear();
  walk     = Walk();
  phase    = 0;
  picked   = false;
  stepT0   = phaseT0 = Application::tickCount();
  if(hooks.player!=nullptr)
    hooks.player->arpgBotRelease();
  ++cur;
  writeState();
  if(!ok && stopOnError)
    finish("błąd (po_bledzie stop)");
  else if(cur>=steps.size())
    finish("koniec scenariusza");
  }

void ArpgPlotBot::writeState(bool end, const std::string& why) const {
  std::FILE* f = std::fopen((runDir+"stan.js").c_str(), "wb");
  if(f==nullptr)
    return;
  const std::string dir = std::filesystem::path(runDir).parent_path().filename().string();
  const std::string s   = "STAN={scenariusz:" + js(name) + ",zrodlo:" + js(source) + ",start:" + js(dir) +
                          ",krok:" + std::to_string(std::min(cur+1, steps.size())) + ",kroki:" + std::to_string(steps.size()) +
                          ",ok:" + std::to_string(nOk) + ",bledy:" + std::to_string(nFail) + ",pom:" + std::to_string(nSkip) +
                          ",zdarzenia:" + std::to_string(nEvents) + ",leczenia:" + std::to_string(nHeal) +
                          ",zaciecia:" + std::to_string(nStuck) + ",smierci:" + std::to_string(nDeaths) +
                          ",mikstury:" + std::to_string(nPotions) + ",parowania:" + std::to_string(nParry) +
                          ",parowaniaOk:" + std::to_string(nParryOk) + ",uniki:" + std::to_string(nDodge) +
                          ",ladowane:" + std::to_string(nCharge) + ",umiejetnosci:" + std::to_string(nSkill) + ",ms:" + std::to_string(Application::tickCount()-runT0) +
                          ",koniec:" + (end ? "true" : "false") + ",powod:" + js(why) + "};\n";
  std::fputs(s.c_str(), f);
  std::fclose(f);
  }

void ArpgPlotBot::writeReport() const {
  if(std::FILE* f = std::fopen((runDir+"raport.html").c_str(), "wb")) {
    std::fwrite(kReportHtml, 1, sizeof(kReportHtml)-1, f);
    std::fclose(f);
    }
  }

void ArpgPlotBot::writePrevious() const {
  // the newest earlier run of the same scenario: the report shows what changed
  std::string     prev;
  const std::string self = std::filesystem::path(runDir).parent_path().filename().string();
  const std::string tail = "_" + name;
  std::error_code ec;
  for(auto& e:std::filesystem::directory_iterator("autotest", ec)) {
    if(!e.is_directory())
      continue;
    const std::string n = e.path().filename().string();
    if(n==self || n>self || n.size()<=tail.size() || n.compare(n.size()-tail.size(), tail.size(), tail)!=0)
      continue;
    if(!std::filesystem::exists(e.path()/"kroki.js"))
      continue;
    if(n>prev)
      prev = n;
    }
  if(std::FILE* f = std::fopen((runDir+"poprzedni.js").c_str(), "wb")) {
    const std::string s = "POPRZEDNI=" + js(prev) + ";\n";
    std::fputs(s.c_str(), f);
    std::fclose(f);
    }
  }

void ArpgPlotBot::log(const std::string& line) {
  Log::i("autotest: ", line);
  if(fLog==nullptr)
    return;
  const std::string s = stamp("%H:%M:%S ") + line + "\n";
  std::fputs(s.c_str(), fLog);
  std::fflush(fLog);
  }

// ---------------------------------------------------------------------------------------------------------------------
// what the bot does between the steps: health, enemies, talks nobody asked for

void ArpgPlotBot::guardHealth(Npc& pl, uint64_t now) {
  const int32_t hp  = pl.attribute(ATR_HITPOINTS);
  const int32_t max = std::max(1, pl.attribute(ATR_HITPOINTSMAX));
  // a blow that would have killed the hero (Gothic::arpgBotGuard left 1 point): a death in the report, life back
  if(Gothic::inst().arpgBotSaves()!=savesSeen) {
    savesSeen = Gothic::inst().arpgBotSaves();
    ++nDeaths;
    ++nHeal;
    event("smierc", "bohater by zginął" + (lastEnemy.empty() ? std::string() : " (" + lastEnemy + ")") +
          " – życie przywrócone, gra toczy się dalej", true);
    pl.changeAttribute(ATR_HITPOINTS, max-pl.attribute(ATR_HITPOINTS), false);
    return;
    }
  const float k = float(hp)/float(max);
  if(k<kPotionAt && now-potionT>kPotionGapMs && hooks.player!=nullptr) {
    potionT = now;
    if(hooks.player->arpgBotPotion()) {
      ++nPotions;
      event("mikstura", "zdrowie " + std::to_string(hp) + "/" + std::to_string(max) +
            (lastEnemy.empty() ? std::string() : ", walka z " + lastEnemy), false);
      }
    }
  }

float ArpgPlotBot::roll01() {
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return float(rng & 0xFFFF)/65535.f;
  }

bool ArpgPlotBot::defendTick(World& w, Npc& pl, uint64_t now) {
  if(hooks.player==nullptr)
    return false;
  for(size_t i=0; i<ignored.size();)
    if(ignored[i].second<now)
      ignored.erase(ignored.begin()+int(i)); else
      ++i;
  auto isIgnored = [&](const Npc* n) {
    for(auto& i:ignored)
      if(i.first==n)
        return true;
    return false;
    };
  if(defend!=nullptr) {
    const bool gone = defend->isDead() || defend->isDown() ||
                      dist2d(defend->position(), pl.position())>kDefendLeave || now-defendT0>kDefendMs;
    if(gone) {
      if(defend->isDead() || defend->isDown())
        bodies.push_back(defend);
      defend = nullptr;
      fightEnd(pl);
      walk.clickT   = 0; // the walk goes on with a new click
      walk.clickIdx = size_t(-1);
      walk.bestT    = now;
      return false;
      }
    if(fightTick(w, pl, *defend, now)<0) {
      event("obrona", "nie dosięga: " + nameOf(*defend) + " – zostawiony na " + std::to_string(kIgnoreMs/1000) + " s", false);
      ignored.push_back({defend, now+kIgnoreMs});
      defend = nullptr;
      fightEnd(pl);
      return false;
      }
    doing = "obrona: " + nameOf(*defend) + " (" + std::to_string(defend->attribute(ATR_HITPOINTS)) + "/" +
            std::to_string(defend->attribute(ATR_HITPOINTSMAX)) + "), " + meters(dist2d(defend->position(), pl.position())) +
            (lastMove.empty() ? std::string() : " | " + lastMove);
    return true;
    }
  // who goes for the hero: the hero as its target, a swing, a beast close by, an armed man close by
  Npc*  best  = nullptr;
  float bestD = kThreatRange;
  bool  near  = false;
  w.detectNpc(pl.position(), kDrawRange, [&](Npc& npc) {
    if(&npc==&pl || npc.isDead() || npc.isDown() || std::abs(npc.position().y-pl.position().y)>350.f)
      return;
    if(!hooks.player->arpgIsEnemy(npc) || isIgnored(&npc))
      return;
    near = true;
    const float d     = dist2d(npc.position(), pl.position());
    const bool  armed = npc.weaponState()!=WeaponState::NoWeapon;
    const bool  wants = npc.target()==&pl && (armed || npc.isMonster());
    const bool  close = d<kThreatNear && (npc.isMonster() || armed);
    if(!(wants || npc.isAttackAnim() || close) || d>=bestD)
      return;
    best  = &npc;
    bestD = d;
    });
  if(near)
    hooks.player->arpgBotDraw(); // an enemy around: the weapon out before it comes
  if(best==nullptr)
    return false;
  defend    = best;
  lastEnemy = nameOf(*best);
  defendT0  = now;
  event("obrona", nameOf(*best) + " (" + std::to_string(int(bestD)) + " cm)", false);
  fightTick(w, pl, *best, now);
  return true;
  }

void ArpgPlotBot::fightEnd(Npc& pl) {
  (void)pl;
  if(hooks.player==nullptr)
    return;
  if(blocking)
    hooks.player->arpgBlock(false);
  if(charging)
    hooks.player->arpgChargeRelease();
  blocking  = charging = false;
  plan      = Plan::None;
  clickedTg = nullptr;
  fightFor  = nullptr;
  hooks.player->arpgBotLock(nullptr);
  hooks.player->arpgBotRelease();
  lastMove.clear();
  }

int ArpgPlotBot::fightTick(World& w, Npc& pl, Npc& tg, uint64_t now) {
  auto P = hooks.player;
  if(P==nullptr)
    return 0;
  const float d   = dist2d(pl.position(), tg.position());
  const float hpK = float(pl.attribute(ATR_HITPOINTS))/float(std::max(1, pl.attribute(ATR_HITPOINTSMAX)));

  // reachability: closer and closer, or in reach / swinging; otherwise (stairs, a rock between) out of reach
  if(fightFor!=&tg) {
    fightFor   = &tg;
    fightBest  = d;
    fightBestT = now;
    }
  if(d<fightBest-50.f) {
    fightBest  = d;
    fightBestT = now;
    }
  if(d<=300.f || pl.isAttackAnim() || blocking)
    fightBestT = now;
  if(now-fightBestT>kReachMs)
    return -1;

  // who swings at the hero now, and when the soonest hit lands
  int     swings  = 0;
  int64_t soonest = -1;
  Npc*    who     = nullptr;
  w.detectNpc(pl.position(), 450.f, [&](Npc& n) {
    if(&n==&pl || n.isDead() || n.isDown() || !n.isAttackAnim() || !P->arpgIsEnemy(n))
      return;
    ++swings;
    const int64_t h = n.arpgNextHitMs();
    if(h>=0 && (soonest<0 || h<soonest)) {
      soonest = h;
      who     = &n;
      }
    });

  // the block held, facing the one who swings: let go after the hit; a parried enemy is staggered (x4 for the next hit)
  if(blocking) {
    if(answered!=nullptr)
      P->arpgBotLock(const_cast<Npc*>(answered));
    const bool hitGone = (who==nullptr || who!=answered) && now-blockT0>120;
    if(!hitGone && now-blockT0<=800)
      return 0;
    P->arpgBlock(false);
    blocking  = false;
    clickedTg = nullptr; // the counter at once
    if(answered!=nullptr && answered->arpgHasStatus(ArpgStatus::Stagger)) {
      ++nParryOk;
      lastMove = "parowanie udane – kontra x4";
      P->arpgBotLock(const_cast<Npc*>(answered));
      P->arpgBotClick(Focus(*const_cast<Npc*>(answered)), true, true);
      clickedTg = answered;
      return 0;
      }
    lastMove = "blok";
    }
  P->arpgBotLock(&tg); // like Ctrl held: the charge, the skills and the clicks go at the target

  // the charge: released when full; a swing coming: released at once
  if(charging) {
    if((who!=nullptr && soonest<300) || P->arpgChargeTime()>=P->arpgChargeTimeFull() || now-chargeT0>4000) {
      P->arpgChargeRelease();
      charging = false;
      ++nCharge;
      lastMove = "atak ładowany";
      }
    return 0;
    }

  // a swing on its way: the answer is chosen once (wind-up), then carried out at the right moment
  if(who!=nullptr && soonest<=kWindUpMs) {
    const uint64_t at = now+uint64_t(soonest);
    if(!(who==answered && (at>answeredAt ? at-answeredAt : answeredAt-at)<250)) {
      answered   = who;
      answeredAt = at;
      const bool danger = (swings>=2 && hpK<0.5f) || hpK<0.25f;
      if(!P->arpgCanBlock()) {
        P->arpgBotDraw();
        plan     = Plan::None;
        lastMove = "broń w rękę";
        }
      else if(danger && now-dodgeT>kDodgeGapMs)
        plan = Plan::Dodge;
      else
        plan = roll01()<kParryChance ? Plan::Parry : Plan::None;
      if(plan==Plan::None && lastMove!="broń w rękę")
        lastMove = "atak przez zamach";
      }
    if(plan==Plan::Dodge) {
      plan   = Plan::None;
      dodgeT = now;
      const auto  a   = pl.position()-who->position();
      const float dir = std::atan2(a.z, a.x)*180.f/float(M_PI) + (roll01()<0.5f ? 70.f : -70.f);
      P->arpgBotDodge(dir);
      clickedTg = nullptr;
      ++nDodge;
      lastMove = "unik (" + std::to_string(swings) + " atakuje, życie " + std::to_string(int(hpK*100.f)) + "%)";
      return 0;
      }
    if(plan==Plan::Parry) {
      if(soonest>kParryLeadMs) {
        // no new swing of the hero: the block must be able to start in time
        P->arpgBotRelease();
        clickedTg = nullptr;
        lastMove  = "czeka na zamach " + nameOf(*who);
        return 0;
        }
      plan     = Plan::None;
      P->arpgBotLock(who);
      P->arpgBlock(true);
      blocking = true;
      blockT0  = now;
      ++nParry;
      lastMove = "parowanie";
      return 0;
      }
    }

  // the skills of the trees (keys 3-0): ready, the target in reach
  if(now-skillT>kSkillGapMs && d<=300.f && !tg.arpgHasStatus(ArpgStatus::Stagger)) {
    for(int i=0; i<8; ++i) {
      const auto sl = P->arpgSkillSlot(i);
      if(sl.skill==PlayerControl::SkNone || !sl.known || !sl.usable || sl.active || sl.left>0.f)
        continue;
      P->arpgSkillKey(i);
      skillT    = now;
      clickedTg = nullptr;
      ++nSkill;
      lastMove = "umiejętność (klawisz " + std::to_string((i+3)%10) + ")";
      return 0;
      }
    }

  // the charged attack now and then: the enemy close and not swinging, no one else swinging
  if(swings==0 && !tg.isAttackAnim() && d<=260.f && P->arpgSpecialLeft()<=0.f && now-chargeTry>2500 &&
     !tg.arpgHasStatus(ArpgStatus::Stagger)) {
    chargeTry = now;
    if(roll01()<kChargeChance) {
      P->arpgChargeStart();
      charging  = true;
      chargeT0  = now;
      clickedTg = nullptr;
      lastMove  = "ładuje atak";
      return 0;
      }
    }

  // plain attacks, held (the combo goes on); a new click only for a new target or when the order ended
  if(clickedTg!=&tg || P->arpgBotIdle()) {
    clickedTg = &tg;
    P->arpgBotClick(Focus(tg), true, true);
    if(lastMove.empty() || lastMove=="blok" || lastMove.rfind("czeka", 0)==0)
      lastMove = "atak";
    }
  return 0;
  }

int32_t ArpgPlotBot::hpBonus(World& w, const Item& it) const {
  // the life an item gives back, as its description in the inventory says it (food, plants, potions)
  auto sym = w.script().findSymbol("NAME_Bonus_HP");
  if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::STRING)
    return 0;
  const std::string& bonus = sym->get_string();
  auto& h = it.handle();
  for(size_t i=0; i<std::size(h.text); ++i)
    if(h.text[i]==bonus)
      return h.count[i];
  return 0;
  }

bool ArpgPlotBot::healTick(World& w, Npc& pl, uint64_t now) {
  const int32_t hp  = pl.attribute(ATR_HITPOINTS);
  const int32_t max = std::max(1, pl.attribute(ATR_HITPOINTSMAX));
  const float   k   = float(hp)/float(max);
  auto endHeal = [&]() {
    if(healFrom>=0 && !healLog.empty())
      event("leczenie", "między walkami: " + healLog + " (życie " + std::to_string(healFrom) + " → " +
            std::to_string(hp) + "/" + std::to_string(max) + ")", false);
    healFrom = -1;
    healLog.clear();
    };
  if(healFrom<0 && k>=kHealBelow)
    return false;
  if(healFrom>=0 && k>=kHealUpTo) {
    endHeal();
    return false;
    }
  if(now-healT<kHealGapMs)
    return healFrom>=0; // eating: wait for the bite
  if(pl.weaponState()!=WeaponState::NoWeapon || pl.isDown())
    return false;
  // the smallest food that heals, otherwise (low life) the smallest potion
  const Item* best  = nullptr;
  int32_t     bestV = 0;
  for(int pass=0; pass<2 && best==nullptr; ++pass) {
    if(pass==1 && k>=kPotionBelow)
      break;
    const int32_t cat = pass==0 ? int32_t(ITM_CAT_FOOD) : int32_t(ITM_CAT_POTION);
    for(auto it = pl.inventory().iterator(Inventory::T_Inventory); it.isValid(); ++it) {
      if((it->handle().main_flag & cat)==0)
        continue;
      const int32_t v = hpBonus(w, *it);
      if(v>0 && (best==nullptr || v<bestV)) {
        best  = &*it;
        bestV = v;
        }
      }
    }
  if(best==nullptr) {
    endHeal();
    return false;
    }
  if(healFrom<0)
    healFrom = hp;
  healT = now;
  hooks.player->arpgBotRelease();
  hooks.player->arpgBotGround(pl.position()); // stand still for the bite
  const std::string what = toUtf8(best->displayName());
  healLog += (healLog.empty() ? "" : ", ") + what;
  doing = "leczy się: " + what + " (+" + std::to_string(bestV) + ", życie " + std::to_string(hp) + "/" + std::to_string(max) + ")";
  pl.useItem(best->clsId());
  return true;
  }

bool ArpgPlotBot::choresTick(World& w, Npc& pl, uint64_t now) {
  if(hooks.player==nullptr || defend!=nullptr || pl.isDown())
    return false;
  // only in peace: no enemy close
  bool enemy = false;
  w.detectNpc(pl.position(), kQuietRange, [&](Npc& n) {
    if(&n!=&pl && !n.isDead() && !n.isDown() && hooks.player->arpgIsEnemy(n))
      enemy = true;
    });
  if(enemy)
    return false;
  if(pl.weaponState()!=WeaponState::NoWeapon) {
    // peace: the weapon away (eating and a new weapon need free hands)
    hooks.player->arpgBotLock(nullptr);
    hooks.player->arpgBotHolster();
    }
  if(healTick(w, pl, now))
    return true;

  auto giveUp = [&](const void* p, const std::string& what) {
    givenUp.push_back(p);
    event("zbieranie", "nie dosięga: " + what, false);
    choreBody = nullptr;
    choreItem = nullptr;
    walk.on   = false;
    };
  auto bestWeapon = [&]() {
    if(pl.weaponState()!=WeaponState::NoWeapon)
      return;
    const std::string nw = pl.arpgEquipBestWeapons();
    if(!nw.empty())
      event("bron", "założona lepsza broń: " + toUtf8(nw), false);
    };

  // 1. bodies of the enemies killed: "take all"
  if(choreBody==nullptr) {
    for(size_t i=0; i<bodies.size();) {
      Focus f;
      f.npc = bodies[i];
      if(w.validateFocus(f).npc==nullptr || dist2d(bodies[i]->position(), pl.position())>kLootRange) {
        bodies.erase(bodies.begin()+int(i));
        continue;
        }
      choreBody = bodies[i];
      choreT0   = now;
      walk.on   = false;
      bodies.erase(bodies.begin()+int(i));
      break;
      }
    }
  if(choreBody!=nullptr) {
    Focus f;
    f.npc = choreBody;
    if(w.validateFocus(f).npc==nullptr) {
      choreBody = nullptr;
      return true;
      }
    if(now-choreT0>kChoreMs) {
      giveUp(choreBody, "ciało " + nameOf(*choreBody));
      return true;
      }
    if(dist2d(choreBody->position(), pl.position())>220.f) {
      if(!walk.on)
        walkStart(w, pl, choreBody->position(), 180.f, false);
      walkWhat = "idzie do ciała: " + nameOf(*choreBody);
      if(walkTick(w, pl, now)<0)
        giveUp(choreBody, "ciało " + nameOf(*choreBody));
      return true;
      }
    const std::string got = pl.arpgLootAll(*choreBody);
    if(!got.empty())
      event("lup", nameOf(*choreBody) + ": " + toUtf8(got), false);
    choreBody = nullptr;
    walk.on   = false;
    bestWeapon();
    return true;
    }

  // 2. things lying around: weapons, arrows, food, potions, gold - nobody's (taking someone's is theft)
  if(choreItem==nullptr) {
    Item* best  = nullptr;
    float bestD = kPickRange;
    const int32_t want = int32_t(ITM_CAT_NF | ITM_CAT_FF | ITM_CAT_MUN | ITM_CAT_FOOD | ITM_CAT_POTION);
    const Vec3 eye = pl.position()+Vec3(0, 100.f, 0);
    Vec3       ahead = {};
    if(goalOn) {
      ahead = goal-pl.position();
      ahead.y = 0;
      const float l = ahead.length();
      ahead = l>1.f ? ahead/l : Vec3();
      }
    w.forEachItemNear(pl.position(), kPickRange, [&](Item& it) {
      auto& h = it.handle();
      if(((h.main_flag & want)==0 && !it.isGold()) || h.owner!=0 || h.owner_guild!=0)
        return;
      if(std::find(givenUp.begin(), givenUp.end(), &it)!=givenUp.end())
        return;
      const float dd = dist2d(it.position(), pl.position());
      if(dd>=bestD || std::abs(it.position().y-pl.position().y)>200.f)
        return;
      // ahead on the way (no going back for it), unless right at the hero's feet
      Vec3 to = it.position()-pl.position();
      to.y = 0;
      if(goalOn && dd>250.f && ahead.length()>0.5f && Vec3::dotProduct(to/std::max(dd,1.f), ahead)<0.3f)
        return;
      // in sight: nothing between
      if(w.physic()!=nullptr && w.physic()->ray(eye, it.position()+Vec3(0, 30.f, 0)).hasCol)
        return;
      best  = &it;
      bestD = dd;
      });
    if(best!=nullptr) {
      choreItem = best;
      choreT0   = now;
      walk.on   = false;
      }
    }
  if(choreItem!=nullptr) {
    auto*  it = static_cast<Item*>(choreItem);
    Focus  f;
    f.item = it;
    if(w.validateFocus(f).item==nullptr) {
      // gone from the world: taken
      choreItem = nullptr;
      walk.on   = false;
      bestWeapon();
      return true;
      }
    const std::string name = toUtf8(it->displayName());
    if(now-choreT0>kChoreMs) {
      giveUp(it, name);
      return true;
      }
    const float dd = dist2d(it->position(), pl.position());
    if(dd>500.f) {
      if(!walk.on)
        walkStart(w, pl, it->position(), 350.f, false);
      walkWhat = "idzie po: " + name;
      if(walkTick(w, pl, now)<0)
        giveUp(it, name);
      return true;
      }
    walk.on = false;
    if(hooks.player->arpgBotIdle() || now-walk.clickT>kTalkClickMs) {
      walk.clickT = now;
      hooks.player->arpgBotClick(f, false, false);
      }
    doing = "podnosi: " + name + " (" + meters(dd) + ")";
    return true;
    }
  return false;
  }

bool ArpgPlotBot::strayTalk(uint64_t now, bool quiet) {
  auto dlg = hooks.dialogs;
  if(dlg==nullptr || dlg->arpgBotNpc()==nullptr) {
    strayT = 0;
    return false;
    }
  if(strayT==0)
    strayT = now;
  doing = std::string(quiet ? "kończy rozmowę: " : "rozmowa poza scenariuszem: ") + nameOf(*dlg->arpgBotNpc());
  dlg->arpgBotSkip();
  std::vector<DialogMenu::ArpgBotLine> lines;
  if(!dlg->arpgBotLines(lines)) {
    if(now-strayT>kStrayStuckMs) {
      event("rozmowa", std::string("nie kończy się – zamknięta: ") + nameOf(*dlg->arpgBotNpc()), true);
      dlg->arpgAbort();
      strayT = 0;
      }
    return true;
    }
  if(now-pickT<kPickGapMs)
    return true;
  pickT = now;
  const std::string who = nameOf(*dlg->arpgBotNpc());
  std::string       all;
  for(auto& l:lines)
    all += (all.empty() ? "" : ", ") + l.name;
  for(auto& h:handlers)
    for(auto& l:lines)
      if(same(l.name, h)) {
        event("rozmowa", who + ": " + l.name + " (gdy)", false);
        dlg->arpgBotPick(l.name);
        return true;
        }
  if(auto ex = exitLine(lines)) {
    if(!quiet)
      event("rozmowa", who + ": koniec (" + all + ")", false);
    dlg->arpgBotPick(ex->name);
    return true;
    }
  bool onlyTopics = true;
  if(auto w = Gothic::inst().world())
    for(auto& l:lines)
      onlyTopics = onlyTopics && w->script().arpgClassOf(l.name)=="C_INFO";
  if(onlyTopics) {
    event("rozmowa", who + ": zamknięta bez wyboru (" + all + ")", false);
    dlg->arpgAbort();
    strayT = 0;
    return true;
    }
  event("wybor", who + ": " + lines[0].name + " z [" + all + "]", false);
  dlg->arpgBotPick(lines[0].name);
  return true;
  }

// ---------------------------------------------------------------------------------------------------------------------
// walking: the waynet gives the road, the hero walks it with ARPG clicks from point to point

void ArpgPlotBot::walkStart(World& w, Npc& pl, const Vec3& dest, float reach, bool stepGoal) {
  if(stepGoal) {
    goal   = dest;
    goalOn = true;
    }
  walk        = Walk();
  walk.on     = true;
  walk.dest   = dest;
  walk.reach  = reach;
  walk.bestT  = Application::tickCount();
  walk.best   = 1e9f;
  if(auto end = w.findWayPoint(dest)) {
    WayPath path = w.wayTo(pl, *end);
    Vec3    last = pl.position();
    while(auto p = path.pop()) {
      const Vec3 v = p->position();
      if(dist2d(v, last)<kRouteMinGap)
        continue;
      walk.route.push_back(v);
      last = v;
      }
    }
  walk.route.push_back(dest);
  }

int ArpgPlotBot::walkTick(World& w, Npc& pl, uint64_t now) {
  (void)w;
  if(!walk.on || hooks.player==nullptr)
    return -1;
  const Vec3 pos = pl.position();
  if(dist2d(pos, walk.dest)<=walk.reach) {
    hooks.player->arpgBotRelease();
    hooks.player->arpgBotGround(pos); // stop here
    walk.on = false;
    return 1;
    }
  // passed points
  while(walk.idx+1<walk.route.size() && dist2d(pos, walk.route[walk.idx])<kRoutePass) {
    ++walk.idx;
    walk.best  = 1e9f;
    walk.bestT = now;
    walk.stuck = 0;
    }
  const Vec3  goal = walk.route[walk.idx];
  const float d    = dist2d(pos, goal);
  if(d<walk.best-kProgress) {
    walk.best  = d;
    walk.bestT = now;
    }
  if(now-walk.bestT>kStuckMs) {
    ++walk.stuck;
    walk.bestT = now;
    walk.best  = d;
    if(walk.stuck>=kStuckJump) {
      // stuck for good on this point: moved there, the report shows where
      ++nStuck;
      ++walk.jumps;
      char buf[128] = {};
      std::snprintf(buf, sizeof(buf), "przy %.0f %.0f %.0f, cel %.0f %.0f %.0f", double(pos.x), double(pos.y), double(pos.z),
                    double(goal.x), double(goal.y), double(goal.z));
      event("zaciecie", buf, true);
      pl.setPosition(goal);
      pl.updateTransform();
      if(auto c = Gothic::inst().camera())
        c->reset(&pl);
      walk.stuck = 0;
      if(walk.jumps>kMaxJumps)
        return -1;
      }
    else if(walk.stuck%2==0 && walk.idx+1<walk.route.size()) {
      ++walk.idx; // the next point may be easier to reach
      }
    walk.clickIdx = size_t(-1);
    }
  if(walk.clickIdx!=walk.idx || hooks.player->arpgBotIdle() || now-walk.clickT>kReclickMs) {
    walk.clickIdx = walk.idx;
    walk.clickT   = now;
    hooks.player->arpgBotGround(walk.route[walk.idx]);
    }
  doing = (walkWhat.empty() ? std::string("idzie") : walkWhat) + ": " + meters(dist2d(pos, walk.dest)) +
          ", punkt drogi " + std::to_string(walk.idx+1) + "/" + std::to_string(walk.route.size()) +
          (walk.stuck>0 ? ", zacięcie " + std::to_string(walk.stuck) + "/" + std::to_string(kStuckJump) : std::string()) +
          (walk.jumps>0 ? ", przeniesienia " + std::to_string(walk.jumps) : std::string());
  return 0;
  }

// ---------------------------------------------------------------------------------------------------------------------
// conditions

bool ArpgPlotBot::cond(World& w, Npc& pl, const std::vector<std::string>& a, size_t from, std::string& why) const {
  if(from>=a.size()) {
    why = "brak warunku";
    return false;
    }
  if(a[from]=="nie") {
    const bool r = cond(w, pl, a, from+1, why);
    why = "nie (" + why + ")";
    return !r;
    }
  auto arg = [&](size_t i) -> std::string { return from+i<a.size() ? a[from+i] : std::string(); };
  auto cmp = [](int32_t x, const std::string& op, int32_t y) {
    if(op=="==") return x==y;
    if(op=="!=") return x!=y;
    if(op==">=") return x>=y;
    if(op=="<=") return x<=y;
    if(op==">")  return x>y;
    if(op=="<")  return x<y;
    return false;
    };
  const std::string& k = a[from];
  auto&              sc = w.script();
  if(k=="info") {
    why = "info " + arg(1);
    return sc.arpgKnowsInfo(pl, arg(1));
    }
  if(k=="zmienna") {
    int32_t x = 0, y = 0;
    if(!intValue(w, arg(1), x) || !intValue(w, arg(3), y)) {
      why = "zmienna " + arg(1) + ": nieznana";
      return false;
      }
    why = arg(1) + " = " + std::to_string(x) + ", oczekiwane " + arg(2) + " " + arg(3);
    return cmp(x, arg(2), y);
    }
  if(k=="przedmiot") {
    const size_t id = itemId(w, arg(1));
    if(id==size_t(-1)) {
      why = "nieznany przedmiot " + arg(1);
      return false;
      }
    const int32_t n   = int32_t(pl.itemCount(id));
    const auto    op  = arg(2).empty() ? std::string(">=") : arg(2);
    int32_t       val = 1;
    if(!arg(3).empty())
      intValue(w, arg(3), val);
    why = arg(1) + " x" + std::to_string(n);
    return cmp(n, op, val);
    }
  if(k=="dziennik") {
    auto sym = sc.findSymbol(arg(1));
    if(sym==nullptr || sym->type()!=zenkit::DaedalusDataType::STRING) {
      why = "nieznany temat " + arg(1);
      return false;
      }
    const std::string topic = sym->get_string();
    const auto&       ql    = sc.questLog();
    for(size_t i=0; i<ql.questCount(); ++i) {
      auto& q = ql.quest(i);
      if(q.name!=topic)
        continue;
      const std::string st = arg(2);
      why = "dziennik " + arg(1) + " stan " + std::to_string(int(q.status));
      if(st.empty())
        return true;
      if(st=="aktywny")    return q.status==QuestLog::Status::Running;
      if(st=="sukces")     return q.status==QuestLog::Status::Success;
      if(st=="porazka")    return q.status==QuestLog::Status::Failed;
      if(st=="nieaktualny")return q.status==QuestLog::Status::Obsolete;
      return false;
      }
    why = "brak tematu " + arg(1) + " w dzienniku";
    return false;
    }
  if(k=="martwy") {
    auto npc = findNpc(w, arg(1));
    why = arg(1) + (npc==nullptr ? " nie ma w świecie" : (npc->isDead() ? " martwy" : " żyje"));
    return npc!=nullptr && npc->isDead();
    }
  if(k=="gildia") {
    int32_t g = 0;
    intValue(w, arg(1), g);
    why = "gildia " + std::to_string(pl.guild()) + ", oczekiwana " + arg(1);
    return int32_t(pl.guild())==g;
    }
  if(k=="rozdzial") {
    int32_t ch = 0, want = 0;
    intValue(w, "KAPITEL", ch);
    intValue(w, arg(1), want);
    why = "rozdział " + std::to_string(ch);
    return ch==want;
    }
  if(k=="blisko") {
    auto npc = findNpc(w, arg(1));
    auto wp  = w.findPoint(arg(2), false);
    int32_t r = 500;
    intValue(w, arg(3), r);
    if(npc==nullptr || wp==nullptr) {
      why = "blisko: brak " + (npc==nullptr ? arg(1) : arg(2));
      return false;
      }
    const float d = (npc->position()-wp->position()).length();
    why = arg(1) + " " + std::to_string(int(d)) + " cm od " + arg(2);
    return d<=float(r);
    }
  if(k=="obok") {
    auto npc = findNpc(w, arg(1));
    int32_t r = 500;
    intValue(w, arg(2), r);
    if(npc==nullptr) {
      why = "obok: brak " + arg(1);
      return false;
      }
    const float d = (npc->position()-pl.position()).length();
    why = arg(1) + " " + std::to_string(int(d)) + " cm";
    return d<=float(r);
    }
  if(k=="godzina") {
    auto hm = [](const std::string& t) { return t.size()==5 ? std::atoi(t.substr(0,2).c_str())*60 + std::atoi(t.substr(3,2).c_str()) : -1; };
    const auto now  = w.time();
    const int  m    = int(now.hour()*60 + now.minute());
    const int  a0   = hm(arg(1)), a1 = hm(arg(2));
    why = "godzina " + std::to_string(m/60) + ":" + std::to_string(m%60);
    if(a0<0 || a1<0)
      return false;
    return a0<=a1 ? (m>=a0 && m<a1) : (m>=a0 || m<a1);
    }
  why = "nieznany warunek " + k;
  return false;
  }

// ---------------------------------------------------------------------------------------------------------------------

void ArpgPlotBot::tick(World& w, Npc& pl) {
  if(done)
    return;
  const uint64_t now = Application::tickCount();
  if(readyT==0)
    readyT = now;
  if(now-readyT<kReadyMs)
    return;
  if(!started) {
    startRun(w);
    if(done)
      return;
    }
  if(w.currentCs()!=nullptr)
    return; // a cutscene plays

  if(pl.isDead()) {
    event("smierc", std::string("bohater zginął") + (defend!=nullptr ? std::string(" (") + nameOf(*defend) + ")" : std::string()), true);
    if(cur<steps.size())
      nextStep(false, "śmierć bohatera");
    finish("śmierć bohatera");
    return;
    }
  guardHealth(pl, now);
  if(cur>=steps.size()) {
    finish("koniec scenariusza");
    return;
    }

  const Step& s = steps[cur];
  if(s.invalid) {
    nextStep(false, "złe nazwy (walidacja)");
    return;
    }
  if(s.verb=="etykieta") {
    nextStep(true, "");
    return;
    }
  if(!s.cond.empty()) {
    std::string why;
    if(!cond(w, pl, s.cond, 0, why)) {
      skipStep("warunek niespełniony: " + why);
      return;
      }
    steps[cur].cond.clear(); // holds: the step itself from now on
    }
  const bool talkStep = s.verb=="rozmowa" || s.verb=="wybor" || s.verb=="koniec";
  if(!talkStep) {
    // checks keep the talk of the step before open; steps that need the hero free end it quietly; any other talk is noted
    const bool check   = s.verb=="sprawdz" || s.verb=="czekaj" || s.verb=="etykieta";
    Npc*       talking = hooks.dialogs!=nullptr ? hooks.dialogs->arpgBotNpc() : nullptr;
    const bool ours    = talking!=nullptr && talking==lastTalk;
    if(talking!=nullptr && !(ours && check) && strayTalk(now, ours))
      return;
    const bool fights = s.verb!="spij" && s.verb!="zapisz" && s.verb!="sprawdz" && s.verb!="etykieta";
    if(talking==nullptr && fights && defendTick(w, pl, now))
      return;
    // between the fights, on the way: heal, take from the bodies, pick up, the best weapon
    const bool free = s.verb=="idz" || s.verb=="zabierz" || s.verb=="czekaj" || s.verb=="zabij";
    if(talking==nullptr && free && choresTick(w, pl, now))
      return;
    }
  if(now-stepT0>kStepMs) {
    nextStep(false, "limit czasu kroku");
    return;
    }

  const auto& v = s.verb;
  if(v=="rozmowa")       doTalk (w, pl, s, now);
  else if(v=="wybor")    doPick (w, pl, s, now);
  else if(v=="koniec")   doExit (w, pl, s, now);
  else if(v=="idz")      doGo   (w, pl, s, now);
  else if(v=="zabij")    doKill (w, pl, s, now);
  else if(v=="zabierz")  doTake (w, pl, s, now);
  else if(v=="zaloz" || v=="uzyj") doUse(w, pl, s, now);
  else if(v=="czekaj")   doWait (w, pl, s, now, false);
  else if(v=="sprawdz")  doWait (w, pl, s, now, true);
  else if(v=="spij")     doSleep(w, pl, s, now);
  else if(v=="zapisz")   doSave (w, pl, s, now);
  else if(v=="przenies") doJump (w, pl, s, now);
  else if(v=="etykieta") nextStep(true, "");
  else                   nextStep(false, "nieznany krok");
  }

void ArpgPlotBot::doTalk(World& w, Npc& pl, const Step& s, uint64_t now) {
  auto        dlg  = hooks.dialogs;
  Npc*        npc  = findNpc(w, s.arg[0]);
  const auto  info = s.arg.size()>1 ? s.arg[1] : std::string();
  if(npc==nullptr) {
    nextStep(false, "nie ma " + s.arg[0] + " w świecie " + std::string(w.name()));
    return;
    }
  if(npc->isDead()) {
    nextStep(false, s.arg[0] + " nie żyje");
    return;
    }
  if(phase==0) {
    knewBefore = !info.empty() && w.script().arpgKnowsInfo(pl, info);
    picked     = false;
    talkRetry  = 0;
    phase      = 1;
    phaseT0    = now;
    }
  Npc* talking = dlg!=nullptr ? dlg->arpgBotNpc() : nullptr;
  if(talking!=nullptr && talking!=npc) {
    strayTalk(now, talking==lastTalk);
    return;
    }
  if(phase<=2 && talking==npc) {
    phase   = 3;
    phaseT0 = now;
    lastTalk = npc;
    }

  if(phase==1) {
    // walk up to the npc (it may move: the walk starts again); on the way the fights and the chores
    if(defendTick(w, pl, now))
      return;
    if(choresTick(w, pl, now))
      return;
    const float d = dist2d(pl.position(), npc->position());
    if(d<=kTalkWalk) {
      phase   = 2;
      phaseT0 = now;
      return;
      }
    if(!walk.on || dist2d(walk.dest, npc->position())>500.f)
      walkStart(w, pl, npc->position(), kTalkWalk);
    walkWhat = "idzie do " + nameOf(*npc);
    if(walkTick(w, pl, now)<0)
      nextStep(false, "nie doszedł do " + s.arg[0]);
    return;
    }
  if(phase==2) {
    doing = "klik w " + nameOf(*npc) + ", czeka na rozmowę " + std::to_string((now-phaseT0)/1000) + " s" +
            (talkRetry>0 ? " (ponownie " + std::to_string(talkRetry) + ")" : std::string());
    // a click on the npc, like a player
    if(dist2d(pl.position(), npc->position())>kTalkWalk*2.f) {
      phase = 1;
      return;
      }
    if(now-phaseT0>kTalkStartMs) {
      nextStep(false, "rozmowa z " + s.arg[0] + " się nie zaczyna");
      return;
      }
    if(hooks.player!=nullptr && (hooks.player->arpgBotIdle() || now-walk.clickT>kTalkClickMs)) {
      walk.clickT = now;
      hooks.player->arpgBotClick(Focus(*npc), false, false);
      }
    return;
    }

  // phase 3: the talk is on; phase 4: the line was picked, its words go by
  if(talking==nullptr) {
    // closed (the npc ended it)
    if(info.empty() || (w.script().arpgKnowsInfo(pl, info) && (picked || !knewBefore))) {
      nextStep(true, picked ? "" : "rozmowa zamknięta");
      return;
      }
    if(!picked && ++talkRetry<=3) {
      // closed before the line came (a guard's warning, a greeting): the npc is asked again
      phase   = 2;
      phaseT0 = now;
      walk.clickT = 0;
      return;
      }
    nextStep(false, "rozmowa zamknięta, " + info + " nieznane");
    return;
    }
  dlg->arpgBotSkip();
  std::vector<DialogMenu::ArpgBotLine> lines;
  if(!dlg->arpgBotLines(lines)) {
    doing = "rozmowa z " + nameOf(*npc) + ": " + (phase==4 ? "wybrał " + info + ", kwestie" : std::string("kwestie (przewijane)"));
    return;
    }
  doing = "rozmowa z " + nameOf(*npc) + (info.empty() ? std::string() : ": szuka " + info) +
          " (linii: " + std::to_string(lines.size()) + ", " + std::to_string((now-phaseT0)/1000) + " s)";
  if(phase==4) {
    if(now-pickT>=kPickGapMs)
      nextStep(true, ""); // the words of the picked line went by
    return;
    }
  if(info.empty()) {
    nextStep(true, "");
    return;
    }
  if(now-pickT<kPickGapMs)
    return;
  for(auto& l:lines)
    if(same(l.name, info)) {
      pickT = now;
      if(dlg->arpgBotPick(l.name)) {
        picked = true;
        phase  = 4;
        }
      return;
      }
  if(w.script().arpgKnowsInfo(pl, info)) {
    // an important line: it ran by itself when the talk began (also before this step)
    nextStep(true, knewBefore ? "już znane" : "samo (ważne)");
    return;
    }
  for(auto& h:handlers)
    for(auto& l:lines)
      if(same(l.name, h)) {
        pickT = now;
        event("rozmowa", nameOf(*npc) + ": " + l.name + " (gdy)", false);
        dlg->arpgBotPick(l.name);
        phaseT0 = now;
        return;
        }
  if(now-phaseT0>kLineWaitMs) {
    std::string all;
    for(auto& l:lines)
      all += (all.empty() ? "" : ", ") + l.name;
    if(knewBefore)
      nextStep(true, info + " już znane (" + all + ")");
    else
      nextStep(false, "brak " + info + " – dostępne: " + all);
    }
  }

void ArpgPlotBot::doPick(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)w; (void)pl;
  auto dlg = hooks.dialogs;
  if(dlg==nullptr || dlg->arpgBotNpc()==nullptr) {
    if(phase==1) {
      nextStep(true, "rozmowa zamknięta po wyborze");
      return;
      }
    if(now-stepT0>kCheckMs)
      nextStep(false, "brak rozmowy");
    return;
    }
  dlg->arpgBotSkip();
  doing = "wybór w rozmowie z " + nameOf(*dlg->arpgBotNpc()) + ": " + s.arg[0] + (phase==1 ? " (wybrany, kwestie)" : "");
  std::vector<DialogMenu::ArpgBotLine> lines;
  if(!dlg->arpgBotLines(lines))
    return;
  if(phase==1) {
    if(now-pickT>=kPickGapMs)
      nextStep(true, "");
    return;
    }
  if(now-pickT<kPickGapMs)
    return;
  for(auto& l:lines)
    if(same(l.name, s.arg[0])) {
      pickT = now;
      if(dlg->arpgBotPick(l.name))
        phase = 1;
      return;
      }
  if(now-stepT0>kLineWaitMs) {
    std::string all;
    for(auto& l:lines)
      all += (all.empty() ? "" : ", ") + l.name;
    nextStep(false, "brak " + s.arg[0] + " – dostępne: " + all);
    }
  }

void ArpgPlotBot::doExit(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)w; (void)pl; (void)s;
  auto dlg = hooks.dialogs;
  if(dlg==nullptr || dlg->arpgBotNpc()==nullptr) {
    nextStep(true, phase==1 ? "" : "już zamknięta");
    return;
    }
  dlg->arpgBotSkip();
  std::vector<DialogMenu::ArpgBotLine> lines;
  if(!dlg->arpgBotLines(lines)) {
    if(now-stepT0>kStrayStuckMs) {
      dlg->arpgAbort();
      nextStep(false, "rozmowa się nie kończy – zamknięta");
      }
    return;
    }
  if(now-pickT<kPickGapMs)
    return;
  if(auto ex = exitLine(lines)) {
    pickT = now;
    dlg->arpgBotPick(ex->name);
    phase = 1;
    return;
    }
  std::string all;
  for(auto& l:lines)
    all += (all.empty() ? "" : ", ") + l.name;
  dlg->arpgAbort();
  nextStep(false, "brak wyjscia – dostępne: " + all);
  }

void ArpgPlotBot::doGo(World& w, Npc& pl, const Step& s, uint64_t now) {
  Vec3  dest  = {};
  float reach = 0;
  if(!targetPos(w, s.arg[0], dest, reach)) {
    nextStep(false, "nieznany cel " + s.arg[0] + " w świecie " + std::string(w.name()));
    return;
    }
  if(s.arg.size()>1)
    reach = float(std::max(50, std::atoi(s.arg[1].c_str())));
  if(!walk.on || dist2d(walk.dest, dest)>500.f)
    walkStart(w, pl, dest, reach);
  walkWhat = "idzie do " + s.arg[0];
  const int r = walkTick(w, pl, now);
  if(r>0)
    nextStep(true, walk.jumps>0 ? "z przeniesieniem" : "");
  else if(r<0)
    nextStep(false, "nie doszedł");
  }

void ArpgPlotBot::doKill(World& w, Npc& pl, const Step& s, uint64_t now) {
  if(hooks.player==nullptr)
    return;
  Npc* tg = nullptr;
  for(auto& n:s.arg) {
    auto npc = findNpc(w, n);
    if(npc!=nullptr && !npc->isDead()) {
      tg = npc;
      break;
      }
    }
  for(auto& n:s.arg)
    if(auto npc = findNpc(w, n); npc!=nullptr && npc->isDead() &&
       std::find(bodies.begin(), bodies.end(), npc)==bodies.end() && std::find(givenUp.begin(), givenUp.end(), npc)==givenUp.end())
      bodies.push_back(npc);
  if(tg==nullptr) {
    fightEnd(pl);
    nextStep(true, "");
    return;
    }
  if(now-stepT0>kKillMs) {
    fightEnd(pl);
    nextStep(false, std::string("limit czasu walki, żyje: ") + nameOf(*tg));
    return;
    }
  const float d = dist2d(pl.position(), tg->position());
  const bool  viaWaynet = now<fightWalkUntil && d>250.f;
  if(d>kFightWalk || viaWaynet) {
    // far, or out of reach of the fight clicks (stairs, a rock between): the waynet takes the hero there
    if(!walk.on || dist2d(walk.dest, tg->position())>300.f)
      walkStart(w, pl, tg->position(), viaWaynet ? 200.f : kFightWalk*0.8f);
    walkWhat = "idzie do " + nameOf(*tg) + (viaWaynet ? " (drogą, poza zasięgiem)" : " (walka)");
    if(walkTick(w, pl, now)<0)
      nextStep(false, std::string("nie doszedł do ") + nameOf(*tg));
    return;
    }
  walk.on   = false;
  lastEnemy = nameOf(*tg);
  if(fightTick(w, pl, *tg, now)<0) {
    event("obrona", "poza zasięgiem: " + nameOf(*tg) + " – drogą przez punkty", false);
    fightEnd(pl);
    fightWalkUntil = now+15000;
    return;
    }
  size_t alive = 0;
  for(auto& n:s.arg)
    if(auto npc = findNpc(w, n); npc!=nullptr && !npc->isDead())
      ++alive;
  doing = "walczy z " + nameOf(*tg) + " (" + std::to_string(tg->attribute(ATR_HITPOINTS)) + "/" +
          std::to_string(tg->attribute(ATR_HITPOINTSMAX)) + "), " + meters(d) + ", żywych celów " + std::to_string(alive) +
          ", " + std::to_string((now-stepT0)/1000) + " s" + (lastMove.empty() ? std::string() : " | " + lastMove);
  }

void ArpgPlotBot::doTake(World& w, Npc& pl, const Step& s, uint64_t now) {
  const size_t id = itemId(w, s.arg[0]);
  if(id==size_t(-1) || hooks.player==nullptr) {
    nextStep(false, "nieznany przedmiot " + s.arg[0]);
    return;
    }
  if(phase==0) {
    countBefore = pl.itemCount(id);
    phase       = 1;
    }
  if(pl.itemCount(id)>countBefore) {
    nextStep(true, "");
    return;
    }
  if(now-stepT0>kTakeMs) {
    nextStep(false, "nie podniósł " + s.arg[0]);
    return;
    }
  const float range = s.arg.size()>1 ? float(std::max(100, std::atoi(s.arg[1].c_str()))) : kTakeRange;
  Item*       best  = nullptr;
  float       bestD = range;
  w.forEachItemNear(pl.position(), range, [&](Item& it) {
    if(it.clsId()!=id)
      return;
    const float d = (it.position()-pl.position()).length();
    if(d<bestD) {
      bestD = d;
      best  = &it;
      }
    });
  if(best==nullptr) {
    nextStep(false, "brak " + s.arg[0] + " w promieniu " + std::to_string(int(range)) + " cm");
    return;
    }
  if(bestD>600.f) {
    if(!walk.on || dist2d(walk.dest, best->position())>300.f)
      walkStart(w, pl, best->position(), 400.f);
    walkWhat = "idzie po " + s.arg[0];
    if(walkTick(w, pl, now)<0)
      nextStep(false, "nie doszedł do " + s.arg[0]);
    return;
    }
  walk.on = false;
  if(hooks.player->arpgBotIdle() || now-walk.clickT>kTalkClickMs) {
    walk.clickT = now;
    hooks.player->arpgBotClick(Focus(*best), false, false);
    }
  doing = "podnosi " + s.arg[0] + " (" + meters(bestD) + ")";
  }

void ArpgPlotBot::doUse(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)now;
  const size_t id = itemId(w, s.arg[0]);
  if(id==size_t(-1) || pl.itemCount(id)==0) {
    nextStep(false, "brak " + s.arg[0] + " w plecaku");
    return;
    }
  pl.useItem(id);
  nextStep(true, "");
  }

void ArpgPlotBot::doWait(World& w, Npc& pl, const Step& s, uint64_t now, bool once) {
  // "czekaj <warunek> [na <s>]"
  std::vector<std::string> a = s.arg;
  uint64_t                 limit = once ? kCheckMs : kWaitMs;
  if(a.size()>=2 && a[a.size()-2]=="na") {
    limit = uint64_t(std::max(1, std::atoi(a.back().c_str())))*1000;
    a.resize(a.size()-2);
    }
  std::string why;
  if(cond(w, pl, a, 0, why)) {
    nextStep(true, once ? "" : why);
    return;
    }
  doing = std::string(once ? "sprawdza" : "czeka") + " (" + std::to_string((now-stepT0)/1000) + "/" + std::to_string(limit/1000) + " s): " + why;
  if(now-stepT0>limit)
    nextStep(false, why);
  }

void ArpgPlotBot::doSleep(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)pl; (void)now;
  const std::string& t = s.arg[0];
  const int h = std::atoi(t.substr(0,2).c_str());
  const int m = std::atoi(t.substr(3,2).c_str());
  w.setDayTime(h, m);
  event("czas", "sen do " + t, false);
  nextStep(true, "");
  }

void ArpgPlotBot::doSave(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)w; (void)pl; (void)now;
  if(!hooks.save) {
    nextStep(false, "brak zapisu");
    return;
    }
  const std::string n = s.arg.empty() ? std::to_string(cur+1) : s.arg[0];
  hooks.save("save_slot_bot_" + n + ".sav", "Bot: " + n + " (" + name + ")");
  event("zapis", "-save bot_" + n, false);
  nextStep(true, "-save bot_" + n);
  }

void ArpgPlotBot::doJump(World& w, Npc& pl, const Step& s, uint64_t now) {
  (void)now;
  Vec3  dest  = {};
  float reach = 0;
  if(!targetPos(w, s.arg[0], dest, reach)) {
    nextStep(false, "nieznany cel " + s.arg[0]);
    return;
    }
  pl.setPosition(dest);
  pl.updateTransform();
  if(auto c = Gothic::inst().camera())
    c->reset(&pl);
  event("zaciecie", "przeniesienie ze scenariusza: " + s.arg[0], false);
  nextStep(true, "");
  }
