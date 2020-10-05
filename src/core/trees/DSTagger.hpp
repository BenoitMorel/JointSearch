#pragma once

#include <vector>
#include <set>
#include <trees/PLLUnrootedTree.hpp>
class Clade;

/**
 *  Apply astral-pro tagging
 *  Assumes that the species ID is stored in 
 *  the clv_index field of the pll_unode_t structure
 */
class DSTagger {
public:
  DSTagger(PLLUnrootedTree &tree);
  const std::vector<pll_unode_t *> &getBestRoots() const{
    return _bestRoots;
  }
  bool isDuplication(unsigned int nodeIndex) const {
    return _clvs[nodeIndex].isDup;
  }
  void print();
private:
  PLLUnrootedTree &_tree;
  
  using Clade = std::set<unsigned int>;
  struct CLV {
    unsigned int score;
    bool isDup;
    Clade clade;
    CLV(): score(0), isDup(false) {}
  };
  
  std::vector<CLV> _clvs;
  std::vector<pll_unode_t *> _bestRoots;
  
  void _tagNode(pll_unode_t *node, CLV &clv);

  
  struct TaggerUNodePrinter {
    TaggerUNodePrinter(std::vector<CLV> &clvs):clvs(clvs) { }
    std::vector<CLV> &clvs;
    void operator()(pll_unode_t *node, 
        std::stringstream &ss) const;
  };
};



