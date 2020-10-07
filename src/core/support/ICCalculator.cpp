#include "ICCalculator.hpp"

#include <array>
#include <algorithm>
#include <unordered_map>
#include <IO/Logger.hpp>
#include <IO/GeneSpeciesMapping.hpp>
#include <IO/LibpllParsers.hpp>
#include <trees/DSTagger.hpp>

void printTaxaSet(const TaxaSet &set) {
  for (auto taxa: set) {

    Logger::info << taxa <<" ";
  }
  Logger::info << std::endl;
}

ICCalculator::ICCalculator(const std::string &referenceTreePath,
      const Families &families):
  _rootedReferenceTree(referenceTreePath),
  _referenceTree(_rootedReferenceTree),
  _taxaNumber(0)
{
  _readTrees(families);
  _computeRefBranchIndices();
  _computeQuartets();
  printNQuartets(30);
  _initScores();
  _computeScores();  

  Logger::info << "LQIC score: " << std::endl;
  Logger::info << _getNewickWithScore(_lqic, std::string("LQIC")) << std::endl;
  Logger::info << _getNewickWithScore(_qpic, std::string("QPIC")) << std::endl;
}

static double getLogScore(const std::array<unsigned int, 3> &q) 
{
  if (q[0] == 0 && q[1] == 0 && q[2] == 0) {
    Logger::info << "  HEY NO OCCURENCES => qic = 0" << std::endl;
    return 0.0;
  }
  auto qsum = std::accumulate(q.begin(), q.end(), 0);
  double qic = 1.0;
  for (unsigned int i = 0; i < 3; ++i) {
    if (q[i] != 0) {
      auto p = double(q[i]) / double(qsum);
		  qic += p * log(p) / log(3);
    }
  }
  return qic;
}


void ICCalculator::_computeRefBranchIndices()
{
  Logger::timed << "[IC computation] Assigning branch indices..." << std::endl; 
  unsigned int currBranchIndex = 0;
  auto branches = _referenceTree.getBranches();
  _refNodeIndexToBranchIndex.resize(branches.size() * 2);
  std::fill(_refNodeIndexToBranchIndex.begin(),
      _refNodeIndexToBranchIndex.end(),
      static_cast<unsigned int>(-1));
  for (auto &branch: branches) {
    _refNodeIndexToBranchIndex[branch->node_index] = currBranchIndex;
    _refNodeIndexToBranchIndex[branch->back->node_index] = currBranchIndex;
    currBranchIndex++;
  }
  assert(currBranchIndex == _taxaNumber * 2 - 3);
  for (auto v: _refNodeIndexToBranchIndex) {
    assert(v != static_cast<unsigned int>(-1));
  }
}

void ICCalculator::_initScores()
{
  Logger::timed << "[IC computation] Initializing scores..." << std::endl; 
  auto branchNumbers = _taxaNumber * 2 - 3;
  _lqic = std::vector<double>(branchNumbers, 1.0);
  _qpic = std::vector<double>(branchNumbers, 1.0);
}

void ICCalculator::_readTrees(const Families &families)
{
  Logger::timed << "[IC computation] Reading trees..." << std::endl; 
  std::unordered_map<std::string, SPID> speciesLabelToSpid;
  
  for (auto speciesLabel: _rootedReferenceTree.getLabels(true)) {
    auto spid = static_cast<SPID>(speciesLabelToSpid.size());
    speciesLabelToSpid[speciesLabel] = spid;
    _allSPID.insert(spid);
    _spidToString.push_back(speciesLabel);
  }
  for (auto &leaf: _referenceTree.getLeaves()) {
    leaf->clv_index = speciesLabelToSpid[std::string(leaf->label)]; 
  }
  _taxaNumber = _allSPID.size();
  for (auto &family: families) {
    GeneSpeciesMapping mappings;
    mappings.fill(family.mappingFile, family.startingGeneTree);
    auto evaluationTree = std::make_unique<PLLUnrootedTree>(family.startingGeneTree);
    for (auto &leaf: evaluationTree->getLeaves()) {
      leaf->clv_index = speciesLabelToSpid[mappings.getSpecies(std::string(leaf->label))];
    }
    _evaluationTrees.push_back(std::move(evaluationTree));
    DSTagger tagger(*(_evaluationTrees.back()));
  }
}


void ICCalculator::_getSpidUnderNode(pll_unode_t *node, TaxaSet &taxa)
{
  if (node->next) {
    _getSpidUnderNode(node->next->back, taxa);
    _getSpidUnderNode(node->next->next->back, taxa);
  } else {
    taxa.insert(node->clv_index);
  }
}



void ICCalculator::_computeQuartets()
{
  Logger::timed << "[IC computation] Computing quartets..." << std::endl; 
  unsigned int quartetsNumber = pow(_allSPID.size(), 4);
  _quartetCounts = std::vector<SPID>(quartetsNumber, SPID());

  unsigned int printEvery = _evaluationTrees.size() >= 1000 ?
    _evaluationTrees.size() / 10 : 1000;
  if (printEvery < 10) {
    printEvery = 999999999;
  }
  unsigned int plop = 0;
  for (auto &evaluationTree: _evaluationTrees) {
    if (++plop % printEvery == 0) {
      Logger::timed << "    Processed " << plop << "/" << _evaluationTrees.size() << " trees" << std::endl;
    }
    _computeQuartetsForTree(*evaluationTree);
  }
}
  
void ICCalculator::_computeQuartetsForTree(PLLUnrootedTree &evaluationTree)
{
  for (auto v: evaluationTree.getInnerNodes()) {
    TaxaSet subtreeTaxa[3];
    _getSpidUnderNode(v->back, subtreeTaxa[0]);
    _getSpidUnderNode(v->next->back, subtreeTaxa[1]);
    _getSpidUnderNode(v->next->next->back, subtreeTaxa[2]);
    for (unsigned int i = 0; i < 3; ++i) {
      for (auto a: subtreeTaxa[i%3]) {
        for (auto b: subtreeTaxa[i%3]) {
          if (a == b) {
            continue;
          }   
          for (auto c: subtreeTaxa[(i+1)%3]) {
            for (auto d: subtreeTaxa[(i+2)%3]) {
              // cover all equivalent quartets
              // we need to swap ab|cd-cd|ab and c-d
              // because of the travserse, we do not need to swap a-b
              _quartetCounts[_getLookupIndex(a,b,c,d)]++;
              _quartetCounts[_getLookupIndex(a,b,d,c)]++;
              _quartetCounts[_getLookupIndex(c,d,a,b)]++;
              _quartetCounts[_getLookupIndex(d,c,a,b)]++;
            }
          }
        }
      }
    }
  }
}

void ICCalculator::_computeScores()
{
  Logger::timed << "[IC computation] Computing scores..." << std::endl; 
  for (auto u: _referenceTree.getInnerNodes()) {
    for (auto v: _referenceTree.getInnerNodes()) {
      _processNodePair(u, v);
    }
  }
}
  

void ICCalculator::_processNodePair(pll_unode_t *u, pll_unode_t *v)
{
  if (u == v) {
    return;
  }
  assert(u->next && v->next);
  assert(u->next != v && u->next->next != v);
  assert(v->next != u && v->next->next != u);
  std::vector<pll_unode_t *> branchPath;
  PLLUnrootedTree::orientTowardEachOther(&u, &v, branchPath);
  std::vector<unsigned int> branchIndices;
  for (auto branch: branchPath) {
    branchIndices.push_back(_refNodeIndexToBranchIndex[branch->node_index]);
  }
  assert(u != v);
  assert(u->next && v->next);
  assert(branchPath.size()); 
  
  std::array<pll_unode_t*, 4> referenceSubtrees;
  referenceSubtrees[0] = u->next->back;
  referenceSubtrees[1] = u->next->next->back;
  referenceSubtrees[2] = v->next->back;
  referenceSubtrees[3] = v->next->next->back;
 
  std::array<SPIDSet, 4> referenceMetaQuartet;
  for (unsigned int i = 0; i < 4; ++i) {
    _getSpidUnderNode(referenceSubtrees[i], referenceMetaQuartet[i]);
  }
  std::array<unsigned int, 3> counts = {0, 0, 0};
  for (auto a: referenceMetaQuartet[0]) {
    for (auto b: referenceMetaQuartet[1]) {
      for (auto c: referenceMetaQuartet[2]) {
        for (auto d: referenceMetaQuartet[3]) {
          auto qic = _getQic(a, b, c, d); 
          counts[0] += _quartetCounts[_getLookupIndex(a,b,c,d)];
          counts[1] += _quartetCounts[_getLookupIndex(a,c,b,d)];
          counts[2] += _quartetCounts[_getLookupIndex(a,d,c,b)];
          for (auto branchIndex: branchIndices) {
            _lqic[branchIndex] = std::min(_lqic[branchIndex], qic);
          }
        }
      }
    }
  }
  if (branchIndices.size() == 1) {
    auto branchIndex = branchIndices[0];
    _qpic[branchIndex] =  getLogScore(counts);
  }
}



void ICCalculator::printNQuartets(unsigned int n) {
  unsigned int idx = 0;
  for (auto a: _allSPID) {
    for (auto b: _allSPID) {
      if (a == b) {
        continue;
      }
      for (auto c: _allSPID) {
        if (c == a || c == b) {
          continue;
        } 
        for (auto d: _allSPID) {
          if (d == a || d == b || d == c) {
            continue;
          }
          _printQuartet(a, b, c, d);
          
          if (idx++ > n) {
            Logger::info << "Number of species: " << _allSPID.size() << std::endl;
            return;
          }
        }
      }
    }
  }
}

void ICCalculator::_printQuartet(SPID a, SPID b, SPID c, SPID d)
{
  auto astr = _spidToString[a];
  auto bstr = _spidToString[b];
  auto cstr = _spidToString[c];
  auto dstr = _spidToString[d];
  Logger::info << astr << "-" << bstr << " | " << cstr << "-" << dstr;
  //Logger::info << a << "-" << b << " | " << c << "-" << d;
  unsigned int idx[3]; 
  idx[0] = _getLookupIndex(a,b,c,d);
  idx[1] = _getLookupIndex(a,c,b,d);
  idx[2] = _getLookupIndex(a,d,c,b);
  unsigned int occurences[3];
  unsigned int sum = 0;
  for (unsigned int i = 0; i < 3; ++i) {
    occurences[i] = _quartetCounts[idx[i]];
    sum += occurences[i];
  }
  double frequencies[3];
  for (unsigned int i = 0; i < 3; ++i) {
    frequencies[i] = double(occurences[i]) / double(sum);
    Logger::info << " q" << i << "=" << frequencies[i] << ",";
    //Logger::info << " q" << i << "=" << occurences[i] << ",";
  }
  Logger::info << std::endl;
}
  


double ICCalculator::_getQic(SPID a, SPID b, SPID c, SPID d)
{
  std::array <unsigned int, 3> idx;
  idx[0] = _getLookupIndex(a,b,c,d);
  idx[1] = _getLookupIndex(a,c,b,d);
  idx[2] = _getLookupIndex(a,d,c,b);
  std::array<unsigned int, 3> counts;
  for (unsigned int i = 0; i < 3; ++i) {
    counts[i] = _quartetCounts[idx[i]];
  }
  auto logScore = getLogScore(counts);
  if (counts[0] >= counts[1] && counts[0] >= counts[2]) {
    return logScore;
  } else {
    return -logScore;
  }
}

std::string ICCalculator::_getNewickWithScore(std::vector<double> &branchScores, const std::string &scoreName)
{
  for (auto node: _referenceTree.getPostOrderNodes()) {
    if (!node->next || !node->back->next) {
      continue;
    }
    auto branchIndex = _refNodeIndexToBranchIndex[node->node_index];
    auto score = branchScores[branchIndex];
    auto scoreStr = scoreName + std::string(" = ") + std::to_string(score);
    free(node->label);
    node->label = (char *)calloc(scoreStr.size() + 1, sizeof(char));
    strcpy(node->label, scoreStr.c_str());
  }
  return _referenceTree.getNewickString(); 
}



