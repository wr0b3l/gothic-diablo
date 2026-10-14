#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

class Serialize;

// gothic-diablo: skill trees (like Last Epoch: every skill has its own tree) and the hero's skill points.
// Texts are UTF-8 (ArpgMenu::gtxt converts them for Gothic's fonts); icons are item instances of the game.
namespace ArpgSkills {
  struct Node   { const char* name; int max; const char* desc; const char* icon; };
  struct Branch { const char* name; std::vector<Node> nodes; };
  struct KeyReq {
    int                              treePoints = 0;  // this many points in the tree's branches ...
    std::vector<std::pair<int,int>>  anyMaxed;        // ... or one of these nodes (branch, node) at its maximum
    };
  struct Tree {
    const char*          name;
    const char*          key;
    const char*          root;
    const char*          icon;
    std::vector<Branch>  branches;
    Node                 keystone;
    const char*          keyReqText;
    std::vector<int>     keyFrom;   // branches drawn leading to the keystone (empty: all of them)
    KeyReq               keyReq;
    };

  const std::vector<Tree>& trees();

  // the key a node's rank is saved under: "Tree/Node" (stays valid when the layout of a tree changes)
  std::string nodeKey(size_t tree, int branch, int node);   // branch -1: the keystone
  std::string branchKey(size_t tree, int branch);           // "Tree/Branch": points found for one branch

  // where a node is: branch -1 = the keystone (node is ignored)
  struct NodeId { size_t tree = 0; int branch = 0; int node = 0; };
  }

class ArpgProgress final {
  public:
    static constexpr int32_t PointsPerLevel = 1;

    using Ranks = std::map<std::string,int32_t>;

    void     clear();
    void     save(Serialize& fout) const;
    void     load(Serialize& fin);

    int32_t  rank      (const ArpgSkills::NodeId& n) const;
    int32_t  treePoints(size_t tree) const;              // spent in the branches of a tree (not the keystone)
    int32_t  freePoints(int32_t level) const;            // to spend in any tree
    int32_t  branchLeft(size_t tree, int branch) const;  // found for this branch and not spent yet

    // false + why (UTF-8) when it is not possible
    bool     canAdd   (const ArpgSkills::NodeId& n, int32_t level, std::string* why = nullptr) const;
    bool     add      (const ArpgSkills::NodeId& n, int32_t level);
    // one point back; 'base': the ranks when the book was opened (a point can only be taken back the same visit)
    bool     canRemove(const ArpgSkills::NodeId& n, const Ranks& base) const;
    bool     remove   (const ArpgSkills::NodeId& n, const Ranks& base);

    const Ranks& ranks() const { return rk; }

    int32_t  bought = 0;  // from teachers (learning points turned into skill points)
    Ranks    found;       // branchKey -> points from finds (stone tablets), only for that branch

  private:
    Ranks    rk;          // nodeKey -> rank

    int32_t  rankOf(const Ranks& r, const ArpgSkills::NodeId& n) const;
    bool     reqMet(const Ranks& r, const ArpgSkills::NodeId& n, std::string* why) const;
    bool     valid (const Ranks& r) const;
    int32_t  spentFree(const Ranks& r) const;
  };
