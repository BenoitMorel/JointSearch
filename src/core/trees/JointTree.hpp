#pragma once

#include <likelihoods/LibpllEvaluation.hpp>
#include <likelihoods/ReconciliationEvaluation.hpp>
#include <IO/Logger.hpp>
#include <search/Moves.hpp>
#include <IO/GeneSpeciesMapping.hpp>
#include <enums.hpp>

#include <sstream>
#include <stack>



void printLibpllNode(pll_unode_s *node, std::ostream &os, bool isRoot);
void printLibpllTreeRooted(pll_unode_t *root, std::ostream &os);

class JointTree {
public:
    JointTree(const std::string &newick_string,
              const std::string &alignment_file,
              const std::string &speciestree_file,
              const std::string &geneSpeciesMap_file,
              const std::string &substitutionModel,
              RecModel reconciliationModel,
              RecOpt reconciliationOpt,
              bool rootedGeneTree,
              double recWeight,
              bool safeMode,
              bool optimizeDTLRates,
              double dupRate,
              double lossRate,
              double transRate);

    virtual ~JointTree();
    void printLibpllTree() const;
    void optimizeParameters(bool felsenstein = true, bool reconciliation = true);
    double computeLibpllLoglk(bool incremental = false);
    double computeReconciliationLoglk ();
    double computeJointLoglk();
    void printLoglk(bool libpll = true, bool rec = true, bool joint = true, Logger &os = Logger::info);
    pll_unode_t *getNode(unsigned int index);
    void applyMove(std::shared_ptr<Move> move);
    void optimizeMove(std::shared_ptr<Move> move);
  
    void invalidateCLV(pll_unode_s *node);
    void printAllNodes(std::ostream &os);
    void printInfo();
    void rollbackLastMove();
    void save(const std::string &fileName, bool append);
    std::shared_ptr<pllmod_treeinfo_t> getTreeInfo();
    void setRates(double dup, double loss, double trans = 0);
    pll_rtree_t *getSpeciesTree() {return pllSpeciesTree_;}
    size_t getUnrootedTreeHash();
    std::shared_ptr<ReconciliationEvaluation> getReconciliationEvaluation() {return reconciliationEvaluation_;}
    
    pll_unode_t *getRoot() {return reconciliationEvaluation_->getRoot();}
    void setRoot(pll_unode_t * root) {reconciliationEvaluation_->setRoot(root);}
    double getDupRate() const {return dupRate_;}
    double getLossRate() const {return lossRate_;}
    double getTransferRate() const {return transRate_;}
    void inferMLScenario(Scenario &scenario) {
      reconciliationEvaluation_->inferMLScenario(scenario);
    }
    bool isSafeMode() {return safeMode_;}
    void enableReconciliation(bool enable) {enableReconciliation_ = enable;}
    unsigned int getGeneTaxaNumber() {return getTreeInfo()->tip_count;}
private:
    std::shared_ptr<LibpllEvaluation> libpllEvaluation_;
    std::shared_ptr<ReconciliationEvaluation> reconciliationEvaluation_;
    pll_rtree_t *pllSpeciesTree_;
    GeneSpeciesMapping geneSpeciesMap_;
    LibpllAlignmentInfo info_;
    double dupRate_;
    double lossRate_;
    double transRate_;
    std::stack<std::shared_ptr<Rollback> > rollbacks_;
    double reconciliationLL_;
    bool optimizeDTLRates_;
    bool safeMode_;
    bool enableReconciliation_;
    RecOpt recOpt_;
    double _recWeight;
};

