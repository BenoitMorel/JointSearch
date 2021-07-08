#pragma once

#include <search/SpeciesRootSearch.hpp>
#include <trees/SpeciesTree.hpp>
#include <IO/FamiliesFileParser.hpp>
#include "UndatedDLMultiModel.hpp"
#include "UndatedDTLMultiModel.hpp"
#include <trees/PLLRootedTree.hpp>
#include <parallelization/PerCoreGeneTrees.hpp>
#include <memory>
#include <vector>
#include <maths/ModelParameters.hpp>

class RecModelInfo;
using MultiEvaluation = ReconciliationModelInterface;
using MultiEvaluationPtr = 
  std::shared_ptr<MultiEvaluation>;
using PerCoreMultiEvaluation = std::vector<MultiEvaluationPtr>;

class GTSpeciesTreeLikelihoodEvaluator: public SpeciesTreeLikelihoodEvaluatorInterface {
public:
  GTSpeciesTreeLikelihoodEvaluator()
  {}
  void setEvaluations(PLLRootedTree &speciesTree,
      ModelParameters &modelRates, 
      const Families &families,
      PerCoreMultiEvaluation &evaluations,
      PerCoreGeneTrees &geneTrees) {
    _speciesTree = &speciesTree;
    _modelRates = &modelRates;
    _families = &families;
    _evaluations = &evaluations;
    _geneTrees = &geneTrees;
  }
  virtual ~GTSpeciesTreeLikelihoodEvaluator() {}
  virtual double computeLikelihood();
  virtual double computeLikelihoodFast();
  virtual bool providesFastLikelihoodImpl() const {return false;}
  virtual double optimizeModelRates(bool thorough = false);
  virtual void pushRollback() {}
  virtual void popAndApplyRollback() {}
  virtual void fillPerFamilyLikelihoods(PerFamLL &perFamLL);
  virtual void getTransferInformation(PLLRootedTree &speciesTree,
    TransferFrequencies &frequencies,
    PerSpeciesEvents &perSpeciesEvents);
  virtual bool pruneSpeciesTree() const {return _modelRates->info.pruneSpeciesTree;}
private:
  PLLRootedTree *_speciesTree;
  ModelParameters *_modelRates;
  const Families *_families;
  PerCoreMultiEvaluation *_evaluations;
  PerCoreGeneTrees *_geneTrees;
};


class GTSpeciesTreeOptimizer: public SpeciesTree::Listener {
public:
  GTSpeciesTreeOptimizer(const std::string speciesTreeFile, 
      const Families &families, 
      const RecModelInfo &info,
      const std::string &outputDir);

  void optimize();
  double sprSearch(unsigned int radius);
  double rootSearch(unsigned int maxDepth);
  double transferSearch();
  void onSpeciesTreeChange(const std::unordered_set<pll_rnode_t *> *nodesToInvalidate);

private:
  std::unique_ptr<SpeciesTree> _speciesTree;
  PerCoreGeneTrees _geneTrees;
  PerCoreMultiEvaluation _evaluations;
  GTSpeciesTreeLikelihoodEvaluator _evaluator;
  ModelParameters _modelRates;
  std::string _outputDir;
  SpeciesSearchState _searchState;
  std::string saveCurrentSpeciesTreeId(std::string str = "inferred_species_tree.newick", bool masterRankOnly = true);
  void saveCurrentSpeciesTreePath(const std::string &str, bool masterRankOnly = true);
};



