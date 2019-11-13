#pragma once

#include <util/enums.hpp>
#include <IO/GeneSpeciesMapping.hpp>
#include <likelihoods/reconciliation_models/AbstractReconciliationModel.hpp>
#include <vector>
#include <memory>
#include <maths/DTLRates.hpp>
#include <maths/Parameters.hpp>
#include <trees/PLLUnrootedTree.hpp>
#include <trees/PLLRootedTree.hpp>

double log(ScaledValue v) ;
  
/**
 *  Wrapper around the reconciliation likelihood classes
 */
class ReconciliationEvaluation {
public:
  
  /**
   *  Constructor 
   *  @param speciesTree rooted species tree (std::fixed)
   *  @param initialGeneTree initial gene tree
   *  @param geneSpeciesMapping gene-to-species geneSpeciesMappingping
   *  @param recModel the reconciliation model to use
   *  @param rootedGeneTree should we compute the likelihood of a rooted or unrooted gene tree?
   */
  ReconciliationEvaluation(PLLRootedTree &speciesTree,
    PLLUnrootedTree &initialGeneTree,
    const GeneSpeciesMapping& geneSpeciesMapping,
    RecModel recModel,
    bool rootedGeneTree);
  
  /**
   * Forbid copy
   */
  ReconciliationEvaluation(const ReconciliationEvaluation &) = delete;
  ReconciliationEvaluation & operator = (const ReconciliationEvaluation &) = delete;
  ReconciliationEvaluation(ReconciliationEvaluation &&) = delete;
  ReconciliationEvaluation & operator = (ReconciliationEvaluation &&) = delete;

  void setRates(const Parameters &parameters);


  /**
   * Get the current root of the gene tree. Return null if the tree does not have a 
   * current root (in unrooted mode)
   * This method is mostly used for rollbacking to a previous state. In most of the
   * cases you should call inferMLRoot instead.
   */
  pll_unode_t *getRoot() {return _reconciliationModel->getRoot();}
  void setRoot(pll_unode_t * root) {_reconciliationModel->setRoot(root);}

  /**
   *  @param input geneTree
   */
  double evaluate();

  bool implementsTransfers() {return Enums::accountsForTransfers(_model);} 

  /*
   *  Call this everytime that the species tree changes
   */
  void onSpeciesTreeChange();

  /**
   *  Invalidate the CLV at a given node index
   *  Must be called on the nodes affected by a move 
   */
  void invalidateCLV(unsigned int nodeIndex);
 
  void invalidateAllCLVs();
 
  pll_unode_t *inferMLRoot();
  
  void inferMLScenario(Scenario &scenario);

  RecModel getRecModel() const {return _model;}
private:
  PLLRootedTree &_speciesTree;
  PLLUnrootedTree &_initialGeneTree;
  GeneSpeciesMapping _geneSpeciesMapping;
  bool _rootedGeneTree;
  RecModel _model; 
  bool _infinitePrecision;
  std::vector<double> _dupRates;
  std::vector<double> _lossRates;
  std::vector<double> _transferRates;
  std::unique_ptr<ReconciliationModelInterface> _reconciliationModel;
private:
  
  std::unique_ptr<ReconciliationModelInterface> buildRecModelObject(RecModel recModel, bool infinitePrecision);
  pll_unode_t *computeMLRoot();
  void updatePrecision(bool infinitePrecision);
};

