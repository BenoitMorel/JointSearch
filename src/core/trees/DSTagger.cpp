#include "DSTagger.hpp"
#include <IO/Logger.hpp>
#include <climits>
#include <sstream>


DSTagger::DSTagger(PLLUnrootedTree &tree):_tree(tree),
  _clvs(_tree.getDirectedNodesNumber())
{
  for (auto node: _tree.getPostOrderNodes()) {
    auto &clv = _clvs[node->node_index];
    _tagNode(node, clv);
  }
  
  // here we create a fake node with fake children
  // to run the _tagNode function on virtual roots
  pll_unode_t fakeNode; // virtual root
  pll_unode_t fakeNext;
  pll_unode_t fakeNextNext;
  fakeNode.next = &fakeNext;
  fakeNode.next->next = &fakeNextNext;
  unsigned int bestScore = UINT_MAX;
  for (auto branch: _tree.getBranches()) {
    fakeNext.back = branch;
    fakeNextNext.back = branch->back;
    CLV clv;
    _tagNode(&fakeNode, clv);
    if (clv.score == bestScore) {
      _bestRoots.push_back(branch);
    } else if (clv.score < bestScore) {
      bestScore = clv.score;
      _bestRoots.clear();
      _bestRoots.push_back(branch);
    }
  }
  auto root = getRoot();
  _rootFromNode(root);
  _rootFromNode(root->back);
}

void DSTagger::_tagNode(pll_unode_t *node, CLV &clv)
{
  if (!node->next) {
    // leaf case, nothing to do
    clv.clade.insert(node->clv_index);
    return;
  }
  auto &leftCLV = _clvs[node->next->back->node_index];
  auto &rightCLV = _clvs[node->next->next->back->node_index];
  auto &leftClade = leftCLV.clade;
  auto &rightClade = rightCLV.clade;
  clv.score = leftCLV.score + rightCLV.score;
  clv.clade.insert(leftClade.begin(), leftClade.end());
  clv.clade.insert(rightClade.begin(), rightClade.end());
  clv.isDup = clv.clade.size() != 
    (leftClade.size() + rightClade.size());
  if (clv.isDup) {
    if (clv.clade == leftClade || clv.clade == rightClade) {
      if (leftClade == rightClade) {
        clv.score += 1;
      } else {
        clv.score += 2;
      }
    } else {
      clv.score += 3;
    }
  }
}
  
void DSTagger::_rootFromNode(pll_unode_t *node)
{
  auto &clv = _clvs[node->node_index];
  clv.goesUp = true;
  if (node->next) {
    _rootFromNode(node->next->back);
    _rootFromNode(node->next->next->back);
  }
}


void DSTagger::print()
{
  auto root = getRoot();
  Logger::info << "DS Tagged:" << std::endl;
  TaggerUNodePrinter printer(_clvs);
  Logger::info << _tree.getNewickString(printer, 
      root,
      true) << std::endl;
}
    

void DSTagger::TaggerUNodePrinter::operator()(pll_unode_t *node, 
        std::stringstream &ss) const
{
  if (!node->next) {
    ss << node->label;
  } else {
    ss << (clvs[node->node_index].isDup ? "D" : "S"); 
  } 
  ss << ":" << node->length;
}
