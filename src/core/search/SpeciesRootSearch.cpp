#include "SpeciesRootSearch.hpp"
#include <trees/SpeciesTree.hpp>
#include <trees/PLLRootedTree.hpp>



static void rootSearchAux(SpeciesTree &speciesTree, 
    SpeciesTreeLikelihoodEvaluatorInterface &evaluator,
    std::vector<unsigned int> &movesHistory, 
    std::vector<unsigned int> &bestMovesHistory, 
    double &bestLL, 
    unsigned int &visits,
    unsigned int maxDepth, 
    RootLikelihoods *rootLikelihoods,
    TreePerFamLLVec *treePerFamLLVec) 
{
  if (movesHistory.size() > maxDepth) {
    return;
  }
  std::vector<unsigned int> moves;
  moves.push_back(movesHistory.back() % 2);
  moves.push_back(2 + (movesHistory.back() % 2));
  for (auto direction: moves) {
    if (!SpeciesTreeOperator::canChangeRoot(speciesTree, direction)) {
      continue;
    }
    movesHistory.push_back(direction);
    evaluator.pushRollback();
    SpeciesTreeOperator::changeRoot(speciesTree, direction);
    double ll = evaluator.computeLikelihood();
    if (treePerFamLLVec) {
      auto newick = speciesTree.getTree().getNewickString();
      treePerFamLLVec->push_back({newick, PerFamLL()});
      auto &perFamLL = treePerFamLLVec->back().second;
      evaluator.fillPerFamilyLikelihoods(perFamLL);
    }
    auto root = speciesTree.getRoot();
    if (rootLikelihoods) {
      rootLikelihoods->saveValue(root->left, ll);
      rootLikelihoods->saveValue(root->right, ll);
    }
    visits++;
    unsigned int additionalDepth = 0;
    if (ll > bestLL) {
      bestLL = ll;
      bestMovesHistory = movesHistory; 
      Logger::info << "Found better root " << ll << std::endl;
      additionalDepth = 3;
    }
    rootSearchAux(speciesTree, 
        evaluator,
        movesHistory, 
        bestMovesHistory, 
        bestLL, 
        visits,
        maxDepth + additionalDepth,
        rootLikelihoods,
        treePerFamLLVec);
    SpeciesTreeOperator::revertChangeRoot(speciesTree, direction);
    evaluator.popAndApplyRollback();
    movesHistory.pop_back();
  }
}

double SpeciesRootSearch::rootSearch(
    SpeciesTree &speciesTree,
    SpeciesTreeLikelihoodEvaluatorInterface &evaluator,
    unsigned int maxDepth,
    RootLikelihoods *rootLikelihoods,
    TreePerFamLLVec *treePerFamLLVec)
{
  Logger::timed << "[Species search] Root search with depth=" << maxDepth << std::endl;
  std::vector<unsigned int> movesHistory;
  std::vector<unsigned int> bestMovesHistory;
  double bestLL = evaluator.computeLikelihood();
  if (treePerFamLLVec) {
    treePerFamLLVec->clear();
    auto newick = speciesTree.getTree().getNewickString();
    treePerFamLLVec->push_back({newick, PerFamLL()});
    auto &perFamLL = treePerFamLLVec->back().second;
    evaluator.fillPerFamilyLikelihoods(perFamLL);
  }
  auto root = speciesTree.getRoot();
  if (rootLikelihoods) {
    rootLikelihoods->saveValue(root->left, bestLL);
    rootLikelihoods->saveValue(root->right, bestLL);
  }
  unsigned int visits = 1;
  movesHistory.push_back(1);
  rootSearchAux(speciesTree,
      evaluator,
      movesHistory, 
      bestMovesHistory, 
      bestLL, 
      visits, 
      maxDepth,
      rootLikelihoods,
      treePerFamLLVec);
  movesHistory[0] = 0;
  rootSearchAux(speciesTree, 
      evaluator,
      movesHistory, 
      bestMovesHistory, 
      bestLL, 
      visits,
      maxDepth,
      rootLikelihoods,
      treePerFamLLVec);
  for (unsigned int i = 1; i < bestMovesHistory.size(); ++i) {
    SpeciesTreeOperator::changeRoot(speciesTree, bestMovesHistory[i]);
  }
  if (rootLikelihoods) {
    auto newick = speciesTree.getTree().getNewickString();
    PLLRootedTree tree(newick, false); 
    rootLikelihoods->fillTree(tree);
  }
  Logger::timed << "[Species search] After root search: LL=" << bestLL << std::endl;
  return bestLL;
}





