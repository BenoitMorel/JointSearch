#pragma once

#include <likelihoods/ReconciliationEvaluation.hpp>
#include <IO/LibpllParsers.hpp>
#include <families/Families.hpp>
#include <string>
#include <maths/Parameters.hpp>
#include <util/enums.hpp>
#include <memory>
#include <trees/PLLRootedTree.hpp>

class PerCoreGeneTrees;


class SpeciesTree {
public:
  SpeciesTree(const std::string &newick, bool isFile = true);
  SpeciesTree(const std::unordered_set<std::string> &leafLabels);
  SpeciesTree(const Families &families);
  // forbid copy
  SpeciesTree(const SpeciesTree &) = delete;
  SpeciesTree & operator = (const SpeciesTree &) = delete;
  SpeciesTree(SpeciesTree &&) = delete;
  SpeciesTree & operator = (SpeciesTree &&) = delete;

  
  
  
  std::unique_ptr<SpeciesTree> buildRandomTree() const;

  void setGlobalRates(const Parameters &rates);
  void setRatesVector(const Parameters &rates);
  const Parameters &getRatesVector() const;
  double computeReconciliationLikelihood(PerCoreGeneTrees &geneTrees, RecModel model);

  std::string toString() const;
  pll_rnode_t *getRandomNode();
  pll_rnode_t *getNode(unsigned int nodeIndex) {return _speciesTree.getNode(nodeIndex);}
  friend std::ostream& operator<<(std::ostream& os, SpeciesTree &speciesTree) {
    os << speciesTree.toString() << "(" << speciesTree.getTree().getLeavesNumber() << " taxa)" << std::endl;
    return os;
  }

  const PLLRootedTree &getTree() const {return _speciesTree;}
  PLLRootedTree &getTree() {return _speciesTree;}

  void saveToFile(const std::string &newick, bool masterRankOnly);
  size_t getHash() const;
  void getLabelsToId(std::unordered_map<std::string, unsigned int> &map) const;
private:
  PLLRootedTree _speciesTree;
  Parameters _rates;
  void buildFromLabels(const std::unordered_set<std::string> &leafLabels);
  static std::unordered_set<std::string> getLabelsFromFamilies(const Families &families);
};


class SpeciesTreeOperator {
public:
  static bool canChangeRoot(const SpeciesTree &speciesTree, int direction);

  /**
   * Change the root to the neighboring branch described by direction where direction is in [0:4[
   */
  static void changeRoot(SpeciesTree &speciesTree, int direction);
  static void revertChangeRoot(SpeciesTree &speciesTree, int direction);
  static unsigned int applySPRMove(SpeciesTree &speciesTree, unsigned int prune, unsigned int regraft);
  static void reverseSPRMove(SpeciesTree &speciesTree, unsigned int prune, unsigned int applySPRMoveReturnValue);
  static void getPossiblePrunes(SpeciesTree &speciesTree, std::vector<unsigned int> &prunes);
  static void getPossibleRegrafts(SpeciesTree &speciesTree, unsigned int prune, unsigned int radius, std::vector<unsigned int> &regrafts);

};




