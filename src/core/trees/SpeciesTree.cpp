#include "SpeciesTree.hpp"
#include <cassert>

#include <likelihoods/LibpllEvaluation.hpp>
#include <likelihoods/ReconciliationEvaluation.hpp>
#include <trees/PerCoreGeneTrees.hpp>
#include <parallelization/ParallelContext.hpp>
#include <IO/FileSystem.hpp>
#include <set>
#include <functional>


SpeciesTree::SpeciesTree(const std::string &newick, bool fromFile):
  _speciesTree(newick, fromFile)
{
}


SpeciesTree::SpeciesTree(const std::unordered_set<std::string> &leafLabels):
  _speciesTree(leafLabels)
{
}
  
std::unique_ptr<SpeciesTree> SpeciesTree::buildRandomTree() const
{
   return std::make_unique<SpeciesTree>(_speciesTree.getLabels(true));
}

  
SpeciesTree::SpeciesTree(const Families &families):
  _speciesTree(getLabelsFromFamilies(families))
{
}


void SpeciesTree::setGlobalRates(const Parameters &globalRates) 
{
  assert(globalRates.dimensions() <= 3);
  _rates = Parameters(getTree().getNodesNumber(), globalRates);
}
void SpeciesTree::setRatesVector(const Parameters &rates) 
{
  _rates = rates;
}

const Parameters &SpeciesTree::getRatesVector() const
{
  return _rates;
}
  

double SpeciesTree::computeReconciliationLikelihood(PerCoreGeneTrees &geneTrees, RecModel model)
{
  double ll = 0.0;
  for (auto &tree: geneTrees.getTrees()) {
    ReconciliationEvaluation evaluation(_speciesTree.getRawPtr(), tree.mapping, model, false);
    evaluation.setRates(_rates);
    ll += evaluation.evaluate(*tree.geneTree);
  }
  ParallelContext::sumDouble(ll);
  return ll;
}

std::string SpeciesTree::toString() const
{
  std::string newick;
  LibpllParsers::getRtreeHierarchicalString(_speciesTree.getRawPtr(), newick);
  return newick;
}


void SpeciesTree::saveToFile(const std::string &newick, bool masterRankOnly)
{
  if (masterRankOnly && ParallelContext::getRank()) {
    return;
  }
  _speciesTree.save(newick);
}
/*  
pll_rnode_t *SpeciesTree::getRandomNode() 
{
  return getNode(rand() % (_speciesTree->tip_count + _speciesTree->inner_count)); 
}
*/
  
static void setRootAux(SpeciesTree &speciesTree, pll_rnode_t *root) {
  speciesTree.getTree().getRawPtr()->root = root; 
  root->parent = 0;
}

bool SpeciesTreeOperator::canChangeRoot(const SpeciesTree &speciesTree, int direction)
{
  bool left1 = direction % 2;
  auto root = speciesTree.getTree().getRoot();
  assert(root);
  auto newRoot = left1 ? root->left : root->right;
  return newRoot->left && newRoot->right;
}

std::string sideString(bool left) {
  if (left) {
    return std::string("left");
  } else {
    return std::string("right");
  }
}


void SpeciesTreeOperator::changeRoot(SpeciesTree &speciesTree, int direction)
{
  bool left1 = direction % 2;
  bool left2 = direction / 2;
  assert(canChangeRoot(speciesTree, left1));
  auto root = speciesTree.getTree().getRoot();
  auto rootLeft = root->left;
  auto rootRight = root->right;
  auto A = rootLeft->left;
  auto B = rootLeft->right;
  auto C = rootRight->left;
  auto D = rootRight->right;
  setRootAux(speciesTree, left1 ? rootLeft : rootRight);
  if (left1 && left2) {
    PLLRootedTree::setSon(rootLeft, root, false);
    PLLRootedTree::setSon(root, B, true);
    PLLRootedTree::setSon(root, rootRight, false);
  } else if (!left1 && !left2) {
    PLLRootedTree::setSon(rootRight, root, true);
    PLLRootedTree::setSon(root, C, false);
    PLLRootedTree::setSon(root, rootLeft, true);
  } else if (left1 && !left2) {
    PLLRootedTree::setSon(rootLeft, rootLeft->right, true);
    PLLRootedTree::setSon(rootLeft, root, false);
    PLLRootedTree::setSon(root, A, false);
    PLLRootedTree::setSon(root, rootRight, true);
  } else { // !left1 && left2
    PLLRootedTree::setSon(rootRight, root, true);
    PLLRootedTree::setSon(rootRight, C, false);
    PLLRootedTree::setSon(root, D, true);
    PLLRootedTree::setSon(root, rootLeft, false);
  }
}

void SpeciesTreeOperator::revertChangeRoot(SpeciesTree &speciesTree, int direction)
{
  changeRoot(speciesTree, 3 - direction);
}

pll_rnode_t *getBrother(pll_rnode_t *node) {
  auto father = node->parent;
  assert(father);
  return father->left == node ? father->right : father->left;
}
  
unsigned int SpeciesTreeOperator::applySPRMove(SpeciesTree &speciesTree, unsigned int prune, unsigned int regraft)
{
  auto pruneNode = speciesTree.getNode(prune);
  auto pruneFatherNode = pruneNode->parent;
  assert(pruneFatherNode);
  auto pruneGrandFatherNode = pruneFatherNode->parent;
  auto pruneBrotherNode = getBrother(pruneNode);
  unsigned int res = pruneBrotherNode->node_index;
  // prune
  if (pruneGrandFatherNode) {
    PLLRootedTree::setSon(pruneGrandFatherNode, pruneBrotherNode, pruneGrandFatherNode->left == pruneFatherNode);
  } else {
    setRootAux(speciesTree, pruneBrotherNode);
  }
  // regraft
  auto regraftNode = speciesTree.getNode(regraft);
  auto regraftParentNode = regraftNode->parent;
  if (!regraftParentNode) {
    // regraft is the root
    setRootAux(speciesTree, pruneFatherNode);
    PLLRootedTree::setSon(pruneFatherNode, regraftNode, pruneFatherNode->left != pruneNode);
  } else {
    PLLRootedTree::setSon(regraftParentNode, pruneFatherNode, regraftParentNode->left == regraftNode);
    PLLRootedTree::setSon(pruneFatherNode, regraftNode, pruneFatherNode->left != pruneNode);
  }
  return res;
}
  
void SpeciesTreeOperator::reverseSPRMove(SpeciesTree &speciesTree, unsigned int prune, unsigned int applySPRMoveReturnValue)
{
  applySPRMove(speciesTree, prune, applySPRMoveReturnValue);
}


// direction: 0 == from parent, 1 == from left, 2 == from right
static void recursiveGetNodes(pll_rnode_t *node, unsigned int direction, unsigned int radius, std::vector<unsigned int> &nodes, bool addNode = true)
{
  if (radius == 0 || node == 0) {
    return;
  }
  if (addNode) {
    nodes.push_back(node->node_index);
  }
  switch (direction) {
    case 0:
      recursiveGetNodes(node->left, 0, radius - 1, nodes);
      recursiveGetNodes(node->right, 0, radius - 1, nodes);
      break;
    case 1: case 2:
      recursiveGetNodes((direction == 1 ? node->right : node->left), 0, radius - 1, nodes);
      if (node->parent) {
        recursiveGetNodes(node->parent, node->parent->left == node ? 1 : 2, radius - 1, nodes);
      }
      break;
    default:
      assert(false);
  };

}
  
void SpeciesTreeOperator::getPossiblePrunes(SpeciesTree &speciesTree, std::vector<unsigned int> &prunes)
{
  for (auto pruneNode: speciesTree.getTree().getNodes()) {
    if (pruneNode == speciesTree.getTree().getRoot()) {
      continue;
    }
    prunes.push_back(pruneNode->node_index); 
  }
}
  
void SpeciesTreeOperator::getPossibleRegrafts(SpeciesTree &speciesTree, unsigned int prune, unsigned int radius, std::vector<unsigned int> &regrafts)
{
  /**
   *  Hack: we do not add the nodes at the first radius, because they are equivalent to moves from the second radius
   */
  radius +=1 ;
  auto pruneNode = speciesTree.getNode(prune);
  auto pruneParentNode = pruneNode->parent;
  if (!pruneParentNode) {
    return;
  }
  if (pruneParentNode->parent) {
    int parentDirection = (pruneParentNode->parent->left == pruneParentNode ? 1 : 2);
    recursiveGetNodes(pruneParentNode->parent, parentDirection, radius, regrafts, false);
  }
  recursiveGetNodes(getBrother(pruneNode)->left, 0, radius, regrafts, false);
  recursiveGetNodes(getBrother(pruneNode)->right, 0, radius, regrafts, false);
}
  
static size_t leafHash(const pll_rnode_t *leaf) {
  assert(leaf);
  std::hash<std::string> hash_fn;
  return hash_fn(std::string(leaf->label));
}

static size_t getTreeHashRec(const pll_rnode_t *node, size_t i) {
  assert(node);
  if (i == 0) 
    i = 1;
  if (!node->left) {
    return leafHash(node);
  }
  auto hash1 = getTreeHashRec(node->left, i + 1);
  auto hash2 = getTreeHashRec(node->right, i + 1);
  //Logger::info << "(" << hash1 << "," << hash2 << ") ";
  std::hash<size_t> hash_fn;
  auto m = std::min(hash1, hash2);
  auto M = std::max(hash1, hash2);
  return hash_fn(m * i + M);
}
  
size_t SpeciesTree::getHash() const
{
  auto res = getTreeHashRec(getTree().getRoot(), 0);
  return res % 100000;
  
}
void SpeciesTree::getLabelsToId(std::unordered_map<std::string, unsigned int> &map) const
{
  map.clear();
  for (auto node: getTree().getNodes()) {
    map.insert(std::pair<std::string, unsigned int>(node->label, node->node_index));
  }
}

std::unordered_set<std::string> SpeciesTree::getLabelsFromFamilies(const Families &families)
{
  GeneSpeciesMapping mappings;
  for (const auto &family: families) {
    std::string geneTreeStr;
    FileSystem::getFileContent(family.startingGeneTree, geneTreeStr);
    mappings.fill(family.mappingFile, geneTreeStr);
  }
  std::unordered_set<std::string> leaves;
  for (auto &mapping: mappings.getMap()) {
    leaves.insert(mapping.second);
  }
  return leaves;
}
  
