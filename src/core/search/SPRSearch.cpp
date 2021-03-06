#include <search/SPRSearch.hpp>
#include <trees/JointTree.hpp>
#include <search/Moves.hpp>
#include <search/SearchUtils.hpp>
#include <IO/Logger.hpp>
#include <parallelization/ParallelContext.hpp>

#include <unordered_set>
#include <array>

struct SPRMoveDesc {
  SPRMoveDesc(unsigned int prune, unsigned int regraft, const std::vector<unsigned int> &edges):
    pruneIndex(prune), regraftIndex(regraft), path(edges) {}
  unsigned int pruneIndex;
  unsigned int regraftIndex;
  std::vector<unsigned int> path;
};

static void queryPruneIndicesRec(pll_unode_t * node,
                               std::vector<unsigned int> &buffer)
{
  assert(node);
  if (node->next) {
    queryPruneIndicesRec(node->next->back, buffer);
    queryPruneIndicesRec(node->next->next->back, buffer);
    buffer.push_back(node->node_index);
    if (node->back->next) {
      buffer.push_back(node->back->node_index);
    }
  }
}

static void getAllPruneIndices(JointTree &tree, std::vector<unsigned int> &allNodeIndices) {
  auto treeinfo = tree.getTreeInfo();
  for (unsigned int i = 0; i < treeinfo->subnode_count; ++i) {
    if (treeinfo->subnodes[i]->next) {
      allNodeIndices.push_back(treeinfo->subnodes[i]->node_index);
    }
  }
}


static bool sprYeldsSameTree(pll_unode_t *p, pll_unode_t *r)
{
  assert(p);
  assert(r);
  return (r == p) || (r == p->next) || (r == p->next->next)
    || (r == p->back) || (r == p->next->back) || (r == p->next->next->back);
}

static bool isValidSPRMove(pll_unode_s *prune, pll_unode_s *regraft) {
  assert(prune);
  assert(regraft);
  return !sprYeldsSameTree(prune, regraft);
}



static void getRegraftsRec(unsigned int pruneIndex, 
    pll_unode_t *regraft, 
    int maxRadius, 
    double supportThreshold, 
    std::vector<unsigned int> &path, 
    std::vector<SPRMoveDesc> &moves)
{
  assert(regraft);
  double bootstrapValue = (nullptr == regraft->label) ? 0.0 : std::atof(regraft->label);
  if (supportThreshold >= 0.0 && bootstrapValue > supportThreshold) {
    return;
  }
  if (path.size()) {
    moves.push_back(SPRMoveDesc(pruneIndex, regraft->node_index, path));
  }
  if (static_cast<int>(path.size()) < maxRadius && regraft->next) {
    path.push_back(regraft->node_index);
    getRegraftsRec(pruneIndex, regraft->next->back, maxRadius, supportThreshold, path, moves);
    getRegraftsRec(pruneIndex, regraft->next->next->back, maxRadius, supportThreshold, path, moves);
    path.pop_back();
  }
}

static void getRegrafts(JointTree &jointTree, unsigned int pruneIndex, int maxRadius, std::vector<SPRMoveDesc> &moves) 
{
  pll_unode_t *pruneNode = jointTree.getNode(pruneIndex);
  std::vector<unsigned int> path;
  auto supportThreshold = jointTree.getSupportThreshold();
  getRegraftsRec(pruneIndex, pruneNode->next->back, maxRadius, supportThreshold, path, moves);
  getRegraftsRec(pruneIndex, pruneNode->next->next->back, maxRadius, supportThreshold, path, moves);
}

bool SPRSearch::applySPRRound(JointTree &jointTree, int radius, double &bestLoglk, bool blo) {
  std::vector<unsigned int> allNodes;
  getAllPruneIndices(jointTree, allNodes);
  std::vector<SPRMoveDesc> potentialMoves;
  std::vector<std::unique_ptr<Move> > allMoves;
  for (unsigned int i = 0; i < allNodes.size(); ++i) {
      auto pruneIndex = allNodes[i];
      getRegrafts(jointTree, pruneIndex, radius, potentialMoves);
  }
  std::vector<std::array<bool, 2> > redundantNNIMoves(
      jointTree.getTreeInfo()->subnode_count, 
      std::array<bool, 2>{{false,false}});
  for (auto &move: potentialMoves) {
    auto pruneIndex = move.pruneIndex;
    auto regraftIndex = move.regraftIndex;
    if (!isValidSPRMove(jointTree.getNode(pruneIndex), jointTree.getNode(regraftIndex))) {
      continue;
    }
    // in case of a radius 1, SPR moves are actually NNI moves
    // the traversal algorithm produces redundant moves, so we 
    // get rid of them here
    if (move.path.size() == 1) { 
      auto nniEdge = jointTree.getNode(move.path[0]);
      bool isPruneNext = nniEdge->back->next->node_index == pruneIndex;
      bool isRegraftNext = nniEdge->next->back->node_index == regraftIndex;
      auto nniType = static_cast<unsigned int>(isPruneNext == isRegraftNext);
      auto nniBranchIndex = std::min(nniEdge->node_index, nniEdge->back->node_index);
      if (redundantNNIMoves[nniBranchIndex][nniType]) {
        continue;
      }
      redundantNNIMoves[nniBranchIndex][nniType] = true; 
    }

    allMoves.push_back(std::move(Move::createSPRMove(pruneIndex, regraftIndex, move.path)));
  }
  
  Logger::info << "Start SPR round " 
    << "(std::hash=" << jointTree.getUnrootedTreeHash() << ", (best ll=" 
    << bestLoglk << ", radius=" << radius << ", possible moves: " << allMoves.size() << ")"
    << std::endl;
  unsigned int bestMoveIndex = static_cast<unsigned int>(-1);
  auto foundBetterMove = SearchUtils::findBestMove(jointTree, 
      allMoves, 
      bestLoglk, 
      bestMoveIndex, 
      blo, 
      jointTree.isSafeMode()); 
  if (foundBetterMove) {
    jointTree.applyMove(*allMoves[bestMoveIndex]);
    if (blo) {
      jointTree.optimizeMove(*allMoves[bestMoveIndex]);
    }
    double ll = jointTree.computeJointLoglk();
    double error = fabs(ll - bestLoglk);
    if (error > 0.01) {
      Logger::info << "Warning, potential numerical issue in SPRSearch::applySPRRound " << error << std::endl;
    }
  }
  return foundBetterMove;
}


void SPRSearch::applySPRSearch(JointTree &jointTree)
{ 
  jointTree.printLoglk();
  double startingLoglk = jointTree.computeJointLoglk();
  double bestLoglk = startingLoglk;
  while (applySPRRound(jointTree, 1, bestLoglk)) {}
  jointTree.optimizeParameters();
  bestLoglk = jointTree.computeJointLoglk();
  while (applySPRRound(jointTree, 1, bestLoglk)) {}
  jointTree.optimizeParameters(true, false);
  bestLoglk = jointTree.computeJointLoglk();
  while (applySPRRound(jointTree, 2, bestLoglk)) {}
  jointTree.optimizeParameters(true, false);
  bestLoglk = jointTree.computeJointLoglk();
  while (applySPRRound(jointTree, 3, bestLoglk)) {}
  jointTree.optimizeParameters(true, false);
  bestLoglk = jointTree.computeJointLoglk();
  while (applySPRRound(jointTree, 5, bestLoglk)) {}
}

