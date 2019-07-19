#include "LibpllParsers.hpp"
#include <fstream>
#include <streambuf>
#include <algorithm>
#include <parallelization/ParallelContext.hpp>
#include <cstring>
#include <sstream>
#include <stack>

extern "C" {
#include <pll.h>
}
  
void LibpllParsers::labelRootedTree(pll_rtree_t *tree)
{
  assert(tree);
  unsigned int index = 0;
  for (unsigned int i = 0; i < tree->tip_count + tree->inner_count; ++i) {
    auto node = tree->nodes[i];
    if (!node->label) {
      auto label = std::string("species_" + std::to_string(index++));
      node->label = static_cast<char*>(malloc(sizeof(char) * (label.size() + 1)));
      std::strcpy(node->label, label.c_str());
    }
  }
}

void LibpllParsers::labelRootedTree(const std::string &unlabelledNewickFile, const std::string &labelledNewickFile)
{
  pll_rtree_t *tree = readRootedFromFile(unlabelledNewickFile);
  labelRootedTree(tree);
  saveRtree(tree->root, labelledNewickFile);
  pll_rtree_destroy(tree, free);
}

pll_utree_t *LibpllParsers::readNewickFromFile(const std::string &newickFilename)
{
  std::ifstream t(newickFilename);
  if (!t)
    throw LibpllException("Could not load open newick file ", newickFilename);
  
  std::string str((std::istreambuf_iterator<char>(t)),
                       std::istreambuf_iterator<char>());
  
  pll_utree_t *res = 0;
  try {
    res = readNewickFromStr(str);
  } catch (...) {
    throw LibpllException("Error while reading tree from file: ", newickFilename);
  }
  return res;
}


pll_utree_t *LibpllParsers::readNewickFromStr(const std::string &newickString)
{
  auto utree =  pll_utree_parse_newick_string_unroot(newickString.c_str());
  if (!utree) 
    throw LibpllException("Error while reading tree from std::string: ", newickString);
  return utree;
}

pll_rtree_t *LibpllParsers::readRootedFromFile(const std::string &newickFile)
{
  auto tree = pll_rtree_parse_newick(newickFile.c_str());
  if (!tree) {
    throw LibpllException("Error while reading tree from file: ", newickFile);
  }
  return tree;
}

pll_rtree_t *LibpllParsers::readRootedFromStr(const std::string &newickString)
{
  auto rtree =  pll_rtree_parse_newick_string(newickString.c_str());
  if (!rtree) 
    throw LibpllException("Error while reading tree from std::string: ", newickString);
  return rtree;
}
  
void LibpllParsers::saveUtree(const pll_unode_t *utree, 
  const std::string &fileName, 
  bool append)
{
  std::ofstream os(fileName, (append ? std::ofstream::app : std::ofstream::out));
  char *newick = pll_utree_export_newick_rooted(utree, 0);
  os << newick;
  os.close();
  free(newick);
}
void LibpllParsers::saveRtree(const pll_rnode_t *rtree, 
    const std::string &fileName)
{
  std::ofstream os(fileName, std::ofstream::out);
  char *newick = pll_rtree_export_newick(rtree, 0);
  os << newick;
  os.close();
  free(newick);
}
  
void LibpllParsers::getRtreeNewickString(const pll_rtree_t *rtree, std::string &newick)
{
  char *newickStr = pll_rtree_export_newick(rtree->root, 0);
  newick = std::string(newickStr);
  free(newickStr);
}

void rtreeHierarchicalStringAux(const pll_rnode_t *node, std::vector<bool> &lefts, std::ostringstream &os)
{
  if (!node) {
    return;
  }
  for (unsigned int i = 0; i < lefts.size(); ++i) {
    auto left = lefts[i];
    if (i == lefts.size() - 1) {
      os << "---";
    } else {
      if (left) {
        os << "|  ";
      } else {
        os << "   ";
      }
    }
  }
  os << (node->label ? node->label : "null") << std::endl;
  lefts.push_back(true);
  rtreeHierarchicalStringAux(node->left, lefts, os);
  lefts[lefts.size() - 1] = false;
  rtreeHierarchicalStringAux(node->right, lefts, os);
  lefts.pop_back();
}

void LibpllParsers::getRtreeHierarchicalString(const pll_rtree_t *rtree, std::string &newick)
{
  std::ostringstream os;
  std::vector<bool> lefts;
  rtreeHierarchicalStringAux(rtree->root, lefts, os);
  newick = os.str();
}

std::vector<unsigned int> LibpllParsers::parallelGetTreeSizes(const Families &families) 
{
  unsigned int treesNumber = static_cast<unsigned int>(families.size());
  std::vector<unsigned int> localTreeSizes((treesNumber - 1 ) / ParallelContext::getSize() + 1, 0);
  for (auto i = ParallelContext::getBegin(treesNumber); i < ParallelContext::getEnd(treesNumber); i ++) {
    pll_utree_t *tree = LibpllParsers::readNewickFromFile(families[i].startingGeneTree);
    unsigned int taxa = tree->tip_count;
    localTreeSizes[i - ParallelContext::getBegin(treesNumber)] = taxa;
    pll_utree_destroy(tree, 0);
  }
  std::vector<unsigned int> treeSizes;
  ParallelContext::concatenateUIntVectors(localTreeSizes, treeSizes);
  treeSizes.erase(remove(treeSizes.begin(), treeSizes.end(), 0), treeSizes.end());
  assert(treeSizes.size() == families.size());
  return treeSizes;
}
void LibpllParsers::fillLeavesFromUtree(pll_utree_t *utree, std::unordered_set<std::string> &leaves)
{
  for (unsigned int i = 0; i < utree->tip_count + utree->inner_count; ++i) {
    auto node = utree->nodes[i];
    if (!node->next) {
      leaves.insert(std::string(node->label));
    }
  }
}

void LibpllParsers::fillLeavesFromRtree(pll_rtree_t *rtree, std::unordered_set<std::string> &leaves)
{
  for (unsigned int i = 0; i < rtree->tip_count + rtree->inner_count; ++i) {
    auto node = rtree->nodes[i];
    if (!node->left) {
      leaves.insert(std::string(node->label));
    }
  }
}

  
