#pragma once

#include <likelihoods/reconciliation_models/AbstractReconciliationModel.hpp>
#include <likelihoods/LibpllEvaluation.hpp>
#include <IO/GeneSpeciesMapping.hpp>
#include <IO/Logger.hpp>
#include <algorithm>
#include <util/Scenario.hpp>
#include <cmath>





template <class REAL>
class SimpleDSModel: public AbstractReconciliationModel<REAL> {
public:
  SimpleDSModel(PLLRootedTree &speciesTree, const GeneSpeciesMapping &geneSpeciesMappingp, 
      bool rootedGeneTree,
      double minGeneBranchLength,
      bool pruneSpeciesTree):
    AbstractReconciliationModel<REAL>(speciesTree, geneSpeciesMappingp, rootedGeneTree, minGeneBranchLength, pruneSpeciesTree) {}
  
  
  SimpleDSModel(const SimpleDSModel &) = delete;
  SimpleDSModel & operator = (const SimpleDSModel &) = delete;
  SimpleDSModel(SimpleDSModel &&) = delete;
  SimpleDSModel & operator = (SimpleDSModel &&) = delete;
  virtual ~SimpleDSModel();
  
  // overloaded from parent
  virtual void setRates(const RatesVector &rates);
protected:
  // overload from parent
  virtual void setInitialGeneTree(PLLUnrootedTree &tree);
  // overload from parent
  virtual void updateCLV(pll_unode_t *geneNode);
  // overload from parent
  virtual REAL getGeneRootLikelihood(pll_unode_t *root) const;
  virtual REAL getGeneRootLikelihood(pll_unode_t *root, pll_rnode_t *) {
    return _dsclvs[root->node_index].proba;
  }

  // overload from parent
  virtual REAL getLikelihoodFactor() const {return REAL(1.0);}
  // overload from parent
  virtual void recomputeSpeciesProbabilities(){};
  // overload from parent
  virtual void computeGeneRootLikelihood(pll_unode_t *virtualRoot);
  // overlead from parent
  virtual void computeProbability(pll_unode_t *geneNode, pll_rnode_t *speciesNode, 
      REAL &proba,
      bool isVirtualRoot = false,
      Scenario *scenario = nullptr,
      Scenario::Event *event = nullptr,
      bool stochastic = false);
private:
  double _PS; // Speciation probability
  double _PD; // Duplication probability
  
  struct DSCLV {
    REAL proba;
    std::set<unsigned int> clade;
    unsigned int genesCount;
    DSCLV():proba(REAL()), genesCount(0) {}
  };
  std::vector<DSCLV> _dsclvs;
};


template <class REAL>
void SimpleDSModel<REAL>::setInitialGeneTree(PLLUnrootedTree &tree)
{
  AbstractReconciliationModel<REAL>::setInitialGeneTree(tree);
  assert(this->_maxGeneId);
  _dsclvs = std::vector<DSCLV>(2 * (this->_maxGeneId + 1));
}

template <class REAL>
void SimpleDSModel<REAL>::setRates(const RatesVector &rates)
{
  assert(rates.size() == 1);
  auto &dupRates = rates[0];
  _PD = dupRates[0];
  _PS = 1.0;
  auto sum = _PD + _PS;
  _PD /= sum;
  _PS /= sum;
  this->_geneRoot = 0;
  this->invalidateAllCLVs();
}


template <class REAL>
SimpleDSModel<REAL>::~SimpleDSModel() { }

template <class REAL>
void SimpleDSModel<REAL>::updateCLV(pll_unode_t *geneNode)
{
  assert(geneNode);
  computeProbability(geneNode, 
      nullptr, 
      _dsclvs[geneNode->node_index].proba);
}


template <class REAL>
void SimpleDSModel<REAL>::computeProbability(pll_unode_t *geneNode, 
    pll_rnode_t *, 
      REAL &proba,
      bool isVirtualRoot,
      Scenario *,
      Scenario::Event *event,
      bool)
  
{
  assert(!event); // we cannot reconcile with this model
  auto gid = geneNode->node_index;
  bool isGeneLeaf = !geneNode->next;
  auto &clade = _dsclvs[gid].clade;

  if (isGeneLeaf) {
    auto speciesId = this->_geneToSpecies[gid];
    proba = REAL(_PS);
    clade.insert(speciesId);
    _dsclvs[gid].genesCount = 1;
    return;
  }
  pll_unode_t *leftGeneNode = 0;     
  pll_unode_t *rightGeneNode = 0;     
  leftGeneNode = this->getLeft(geneNode, isVirtualRoot);
  rightGeneNode = this->getRight(geneNode, isVirtualRoot);
  auto v = leftGeneNode->node_index;
  auto w = rightGeneNode->node_index;
  auto &leftClade = _dsclvs[v].clade;
  auto &rightClade = _dsclvs[w].clade;
  clade.clear();
  clade.insert(leftClade.begin(), leftClade.end());
  clade.insert(rightClade.begin(), rightClade.end());
  _dsclvs[gid].genesCount = _dsclvs[v].genesCount + _dsclvs[w].genesCount;
  proba = REAL(_dsclvs[v].proba * _dsclvs[w].proba);
  if (clade.size() == leftClade.size() + rightClade.size()) {
    proba *= _PS;
    proba /= pow(2.0, clade.size() - 1);
  } else {
    proba *= _PD;
    proba /= pow(2.0, _dsclvs[gid].genesCount - 1) - pow(2.0, clade.size() - 1);
  }
  scale(proba);
}
  
template <class REAL>
REAL SimpleDSModel<REAL>::getGeneRootLikelihood(pll_unode_t *root) const
{
  auto u = root->node_index + this->_maxGeneId + 1;
  return _dsclvs[u].proba;
}

template <class REAL>
void SimpleDSModel<REAL>::computeGeneRootLikelihood(pll_unode_t *virtualRoot)
{
  auto u = virtualRoot->node_index;
  computeProbability(virtualRoot, nullptr, _dsclvs[u].proba, true);
}



