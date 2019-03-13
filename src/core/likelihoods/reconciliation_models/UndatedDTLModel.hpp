#pragma once

#include <likelihoods/reconciliation_models/AbstractReconciliationModel.hpp>
#include <likelihoods/LibpllEvaluation.hpp>
#include <IO/GeneSpeciesMapping.hpp>
#include <maths/ScaledValue.hpp>

using namespace std;

/*
* Implement the undated model described here:
* https://github.com/ssolo/ALE/blob/master/misc/undated.pdf
* In addition, we forbid transfers to parent species
*/
class UndatedDTLModel: public AbstractReconciliationModel {
public:
  UndatedDTLModel();
  virtual ~UndatedDTLModel();
  
  // overloaded from parent
  virtual void setRates(double dupRate, double lossRate, double transferRate = 0.0);  
protected:
  // overloaded from parent
  virtual void setInitialGeneTree(pll_utree_t *tree);
  // overloaded from parent
  virtual void updateCLV(pll_unode_t *geneNode);
  // overloaded from parent
  virtual ScaledValue getRootLikelihood(pll_unode_t *root) const;
  // overload from parent
  virtual void computeRootLikelihood(pll_unode_t *virtualRoot);
  virtual ScaledValue getRootLikelihood(pll_unode_t *root, pll_rnode_t *speciesRoot) {return _uq[root->node_index + _maxGeneId + 1][speciesRoot->node_index];}
  virtual void backtrace(pll_unode_t *geneNode, pll_rnode_t *speciesNode, 
      Scenario &scenario,
      bool isVirtualRoot = false);
private:
  // model
  vector<double> _PD; // Duplication probability, per branch
  vector<double> _PL; // Loss probability, per branch
  vector<double> _PT; // Transfer probability, per branch
  vector<double> _PS; // Speciation probability, per branch

  // SPECIES
  vector<ScaledValue> _uE; // Probability for a gene to become extinct on each brance
  ScaledValue _transferExtinctionSum;
  vector<ScaledValue> _ancestralExctinctionCorrection;  

  // CLVs
  // _uq[geneId][speciesId] = probability of a gene node rooted at a species node
  // to produce the subtree of this gene node
  vector<vector<ScaledValue> > _uq;
  vector<ScaledValue> _survivingTransferSums;
  vector<vector<ScaledValue> > _ancestralCorrection;



private:
  void computeProbability(pll_unode_t *geneNode, pll_rnode_t *speciesNode, 
      ScaledValue &proba,
      bool isVirtualRoot = false) const;
  void updateTransferSums(ScaledValue &transferExtinctionSum,
    vector<ScaledValue> &ancestralExtinctionCorrection,
    const vector<ScaledValue> &probabilities);
  void resetTransferSums(ScaledValue &transferSum,
    vector<ScaledValue> &ancestralCorrection);
  ScaledValue getCorrectedTransferExtinctionSum(int speciesNode) const;
  ScaledValue getCorrectedTransferSum(int geneId, int speciesId) const;
  pll_rnode_t *getBestTransfer(int gid, pll_rnode_t *speciesNode); 
};
