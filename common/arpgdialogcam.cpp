#include "arpgdialogcam.h"

#include <algorithm>
#include <cmath>

#include "world/objects/npc.h"
#include "world/world.h"
#include "physics/dynamicworld.h"

using namespace Tempest;

static Vec3 flat(const Vec3& v) {
  return Vec3(v.x, 0, v.z);
  }

static Vec3 norm2(const Vec3& v, const Vec3& fallback) {
  const Vec3  f = flat(v);
  const float l = f.length();
  if(l<1.f)
    return fallback;
  return f/l;
  }

// how far the point is from the line of sight (subject -> camera); far away when it is behind one of them
static float offLine(const Vec3& pt, const Vec3& from, const Vec3& to) {
  const Vec3  d  = to-from;
  const float l2 = Vec3::dotProduct(d, d);
  if(l2<1.f)
    return 1e6f;
  const float t = Vec3::dotProduct(pt-from, d)/l2;
  if(t<=0.02f || t>=0.98f)
    return 1e6f;
  return (pt-(from+d*t)).length();
  }

static Vec3 face(const Npc& n) {
  // the head bone is the base of the head: eyes a bit above
  return n.mapHeadBone()+Vec3(0, 8.f, 0);
  }

void ArpgDialogCam::reset() {
  *this = ArpgDialogCam();
  }

ArpgDialogCam::Shot ArpgDialogCam::compute(Kind k, const Npc* who, const Input& in, World& world,
                                           float pushIn, float* quality) const {
  const Vec3 up      = Vec3(0, 1, 0);
  const Vec3 fwdPl   = norm2(faceOther-facePl, Vec3(1, 0, 0));
  // the side of the line between the two where the camera stays (180 degree rule)
  const Vec3 n       = Vec3(-fwdPl.z, 0, fwdPl.x)*side;
  const float sep    = flat(faceOther-facePl).length();

  Shot s;
  if(k==K_Two || who==nullptr) {
    const Vec3 mid = (facePl+faceOther)*0.5f;
    s.origin = mid + n*(sep*1.15f+90.f) - up*12.f;
    s.target = mid - up*18.f;
    } else {
    const Vec3 x = (who==in.pl ? facePl    : faceOther); // the subject
    const Vec3 y = (who==in.pl ? faceOther : facePl);    // the other one
    const Vec3 u = norm2(y-x, who==in.pl ? fwdPl : fwdPl*-1.f); // from the subject towards the other
    switch(k) {
      case K_OverShoulder: {
        s.origin = y + u*95.f + n*70.f + up*12.f;
        s.target = x - up*4.f;
        break;
        }
      case K_Medium: {
        const float d = std::clamp(sep*0.9f, 110.f, 160.f);
        s.origin = x + u*d + n*(d*0.5f) - up*8.f;
        s.target = x - up*18.f;
        break;
        }
      case K_Close:
      default: {
        s.origin = x + u*80.f + n*42.f;
        s.target = x - up*2.f;
        break;
        }
      }

    // the other one must not cover the face: big shoulder plates (necromancer robes, paladins), a bow on the back.
    // His head and shoulders keep 58 cm from the line of sight, else the camera goes further to its side.
    for(int i=0; i<14; ++i) {
      const Vec3 eye   = x+up*4.f;
      const bool clear = offLine(y,          eye, s.origin)>58.f &&
                         offLine(y-up*32.f,  eye, s.origin)>58.f &&
                         (s.origin-y).length()>45.f;
      if(clear)
        break;
      s.origin = s.origin + n*12.f;
      }
    }

  // slow push-in: the picture lives
  s.origin = s.origin + (s.target-s.origin)*pushIn;

  // in front of walls: the camera is pulled towards the subject
  float q = 1.f;
  if(auto physic = world.physic()) {
    const auto  r    = physic->ray(s.target, s.origin);
    const float full = (s.origin-s.target).length();
    if(r.hasCol && full>1.f) {
      const float hit = (r.v-s.target).length();
      const float nd  = std::max(20.f, hit-15.f);
      if(nd<full) {
        s.origin = s.target + (s.origin-s.target)*(nd/full);
        q        = nd/full;
        }
      }
    }
  if(quality!=nullptr)
    *quality = q;
  return s;
  }

void ArpgDialogCam::pickSide(const Input& in, World& world) {
  // the side the ARPG camera was on (no jump in the picture), unless walls are closer there
  const Vec3 fwdPl = norm2(faceOther-facePl, Vec3(1, 0, 0));
  const Vec3 n0    = Vec3(-fwdPl.z, 0, fwdPl.x);
  const Vec3 mid   = (facePl+faceOther)*0.5f;
  const float pref = (Vec3::dotProduct(flat(in.viewFrom-mid), n0)>=0.f) ? 1.f : -1.f;

  auto score = [&](float sd) {
    side = sd;
    float a = 0, b = 0, c = 0;
    compute(K_OverShoulder, in.pl,    in, world, 0, &a);
    compute(K_OverShoulder, in.other, in, world, 0, &b);
    compute(K_Two,          nullptr,  in, world, 0, &c);
    return a+b+c;
    };
  const float sp = score(pref);
  const float so = score(-pref);
  side = (so>sp+0.3f) ? -pref : pref;
  }

void ArpgDialogCam::cut(Kind k, const Npc* who, const Input& in, World& world) {
  // a shot hidden by walls: the best other one
  float q = 1.f;
  compute(k, who, in, world, 0, &q);
  if(q<0.5f && who!=nullptr) {
    Kind  best  = k;
    float bestQ = q;
    for(Kind alt:{K_Close, K_Medium, K_OverShoulder}) {
      float qa = 0;
      compute(alt, who, in, world, 0, &qa);
      if(qa>bestQ+0.05f) {
        best  = alt;
        bestQ = qa;
        }
      }
    k = best;
    }
  kind    = k;
  subject = who;
  shotT   = 0;
  cuts++;
  }

ArpgDialogCam::Shot ArpgDialogCam::tick(float dt, const Input& in, World& world) {
  if(in.pl==nullptr || in.other==nullptr)
    return last;

  // heads: smoothed (gestures move them), snapped on the first frame
  {
    const Vec3 a = face(*in.pl);
    const Vec3 b = face(*in.other);
    if(!faceInit) {
      facePl    = a;
      faceOther = b;
      faceInit  = true;
      } else {
      const float f = std::min(1.f, 5.f*dt);
      facePl    = facePl    + (a-facePl)*f;
      faceOther = faceOther + (b-faceOther)*f;
      }
  }
  if(side==0.f)
    pickSide(in, world);

  const Npc* listener = (in.speaker==in.pl ? in.other : in.pl);

  if(in.line!=line) {
    // a new line
    line      = in.line;
    lineT     = 0;
    lineLen   = in.lineLen;
    reacting  = false;
    reactDone = false;
    if(in.speaker!=nullptr) {
      const bool keep = (kind!=K_None && kind!=K_Two && subject==in.speaker && lineLen<1.f); // no flicker on short lines
      if(!keep) {
        Kind k = K_OverShoulder;
        if(in.speaker!=lineSpeaker || kind==K_Two || kind==K_None || subject!=in.speaker) {
          // the other one speaks: mostly over the shoulder, every few cuts closer or wider
          static const Kind seq[] = {K_OverShoulder, K_OverShoulder, K_Close, K_OverShoulder, K_Medium};
          k = seq[cuts%5];
          } else {
          // the same one goes on: another size
          k = (kind==K_OverShoulder ? K_Close : (kind==K_Close ? K_Medium : K_OverShoulder));
          }
        cut(k, in.speaker, in, world);
        }
      lineSpeaker = in.speaker;
      }
    } else {
    lineT += dt;
    }

  if(in.speaker!=nullptr) {
    // long lines: a look at the listener in the middle, then back with another size
    if(lineLen>6.f && !reacting && !reactDone && lineT>lineLen*0.45f) {
      beforeReact = kind;
      reacting    = true;
      cut((cuts%2)==0 ? K_Close : K_Medium, listener, in, world);
      }
    else if(reacting && shotT>std::clamp(lineLen*0.18f, 1.4f, 2.2f)) {
      reacting  = false;
      reactDone = true;
      cut(beforeReact==K_Close ? K_Medium : K_Close, in.speaker, in, world);
      }
    } else {
    reacting = false;
    if(in.choosing) {
      // the player picks: over the hero's shoulder on the other one
      if(!(kind==K_OverShoulder && subject==in.other))
        cut(K_OverShoulder, in.other, in, world);
      }
    else if(kind==K_None) {
      cut(K_Two, nullptr, in, world);
      }
    }

  shotT += dt;
  const float push = std::min(0.10f, 0.02f*shotT);
  last = compute(kind==K_None ? K_Two : kind, subject, in, world, push, nullptr);
  return last;
  }
