
#include "GeneRaxCore.hpp"
#include "GeneRaxInstance.hpp"
#include <parallelization/ParallelContext.hpp>
#include <IO/FamiliesFileParser.hpp>
#include <IO/Logger.hpp>
#include <IO/LibpllParsers.hpp>
#include <algorithm>
#include <limits>
#include <trees/PerCoreGeneTrees.hpp>
#include <optimizers/DTLOptimizer.hpp>
#include <maths/Parameters.hpp>
#include <IO/FileSystem.hpp>
#include <IO/ParallelOfstream.hpp>
#include <routines/GeneRaxSlaves.hpp>
#include <parallelization/Scheduler.hpp>
#include <routines/RaxmlMaster.hpp>
#include <routines/GeneTreeSearchMaster.hpp>
#include <routines/Routines.hpp>
#include <optimizers/SpeciesTreeOptimizer.hpp>
#include <trees/SpeciesTree.hpp>



void GeneRaxCore::initInstance(GeneRaxInstance &instance) 
{
  srand(instance.args.seed);
  FileSystem::mkdir(instance.args.output, true);
  Logger::initFileOutput(FileSystem::joinPaths(instance.args.output, "generax"));
  instance.args.printCommand();
  instance.args.printSummary();
  instance.initialFamilies = FamiliesFileParser::parseFamiliesFile(instance.args.families);
  if (instance.args.speciesTree == "random") {
    Logger::info << "Generating random starting species tree" << std::endl;
    SpeciesTree speciesTree(instance.initialFamilies);
    instance.speciesTree = FileSystem::joinPaths(instance.args.output, "randomSpeciesTree.newick");
    Logger::info << "before save to " << std::endl;
    speciesTree.saveToFile(instance.speciesTree, true);
    ParallelContext::barrier();
  } else {
    instance.speciesTree = FileSystem::joinPaths(instance.args.output, "labelled_species_tree.newick");
    LibpllParsers::labelRootedTree(instance.args.speciesTree, instance.speciesTree);
    ParallelContext::barrier();
  }
  Logger::info << "Filtering invalid families..." << std::endl;
  filterFamilies(instance.initialFamilies, instance.speciesTree);
  if (!instance.initialFamilies.size()) {
    Logger::info << "[Error] No valid families! Aborting GeneRax" << std::endl;
    ParallelContext::abort(10);
  }
  Logger::timed << "Number of gene families: " << instance.initialFamilies.size() << std::endl;
  instance.currentFamilies = instance.initialFamilies;
  initFolders(instance);
}

void GeneRaxCore::initRandomGeneTrees(GeneRaxInstance &instance)
{
  unsigned int duplicates = instance.args.duplicates;
  if (duplicates > 1) {
    duplicatesFamilies(instance.initialFamilies, instance.currentFamilies, duplicates);
    initFolders(instance);
    ParallelContext::barrier();
  } else {
    instance.currentFamilies = instance.initialFamilies;
  }
  bool randoms = Routines::createRandomTrees(instance.args.output, instance.currentFamilies); 
  if (!randoms && duplicates > 1) {
    Logger::info << "Error: multiple starting trees (duplicates option) is only compatible with random starting trees" << std::endl;
    ParallelContext::abort(42);
  }
  if (randoms) {
    initialGeneTreeSearch(instance);
  }
}

void GeneRaxCore::speciesTreeSearch(GeneRaxInstance &instance)
{
  if (!instance.args.optimizeSpeciesTree) {
    return;
  }
  ParallelContext::barrier();
  SpeciesTreeOptimizer speciesTreeOptimizer(instance.speciesTree, instance.currentFamilies, UndatedDL, instance.args.output, instance.args.exec);
  speciesTreeOptimizer.setPerSpeciesRatesOptimization(instance.args.perSpeciesDTLRates); 
  for (unsigned int radius = 1; radius < 5; ++radius) {
    if (radius == 5) {
      speciesTreeOptimizer.setModel(instance.recModel);
    }
    speciesTreeOptimizer.ratesOptimization();
    speciesTreeOptimizer.sprSearch(radius, false);
    speciesTreeOptimizer.rootExhaustiveSearch(false);
    Logger::info << "RecLL = " << speciesTreeOptimizer.getReconciliationLikelihood() << std::endl;
  }
  if (ParallelContext::getRank() == 0) {
    speciesTreeOptimizer.saveCurrentSpeciesTree(instance.speciesTree, true);
  }
  ParallelContext::barrier();
}

void GeneRaxCore::geneTreeJointSearch(GeneRaxInstance &instance)
{
  for (unsigned int i = 1; i <= instance.args.recRadius; ++i) { 
    bool enableLibpll = false;
    bool perSpeciesDTLRates = false;
    optimizeRatesAndGeneTrees(instance, perSpeciesDTLRates, enableLibpll, i);
  }
  for (int i = 0; i <= instance.args.maxSPRRadius; ++i) {
    bool enableLibpll = true;
    bool perSpeciesDTLRates = instance.args.perSpeciesDTLRates && (i >= instance.args.maxSPRRadius - 1); // only apply per-species optimization at the two last rounds
    optimizeRatesAndGeneTrees(instance, perSpeciesDTLRates, enableLibpll, i);
  }
}


void GeneRaxCore::postProcessGeneTrees(GeneRaxInstance &instance)
{
  if (instance.args.duplicates > 1) {
    Families contracted = instance.initialFamilies;
    contractFamilies(instance.currentFamilies, contracted);
    instance.currentFamilies = contracted;
    bool perSpeciesDTLRates = false;
    bool enableLibpll = true;
    optimizeRatesAndGeneTrees(instance, perSpeciesDTLRates, enableLibpll, 0);
  }
}

void GeneRaxCore::reconcile(GeneRaxInstance &instance)
{
  Logger::timed << "Reconciling gene trees with the species tree..." << std::endl;
  Routines::inferReconciliation(instance.speciesTree, instance.currentFamilies, instance.recModel, instance.rates, instance.args.output);
}
  
void GeneRaxCore::terminate(GeneRaxInstance &instance)
{
  ParallelOfstream os(FileSystem::joinPaths(instance.args.output, "stats.txt"));
  os << "JointLL: " << instance.totalLibpllLL + instance.totalRecLL << std::endl;
  os << "LibpllLL: " << instance.totalLibpllLL << std::endl;
  os << "RecLL: " << instance.totalRecLL;
  if (instance.elapsedRaxml) {
    Logger::info << "Initial time spent on optimizing random trees: " << instance.elapsedRaxml << "s" << std::endl;
  }
  Logger::info << "Time spent on optimizing rates: " << instance.elapsedRates << "s" << std::endl;
  Logger::info << "Time spent on optimizing gene trees: " << instance.elapsedSPR << "s" << std::endl;
  Logger::timed << "End of GeneRax execution" << std::endl;
}


void GeneRaxCore::initFolders(GeneRaxInstance &instance) 
{
  std::string results = FileSystem::joinPaths(instance.args.output, "results");
  FileSystem::mkdir(results, true);
  for (auto &family: instance.currentFamilies) {
    FileSystem::mkdir(FileSystem::joinPaths(results, family.name), true);
  }
}

void GeneRaxCore::initialGeneTreeSearch(GeneRaxInstance &instance)
{
  unsigned int duplicates = instance.args.duplicates;
  Logger::info << std::endl;
  Logger::timed << "[Initialization] Initial optimization of the starting random gene trees" << std::endl;
  if (duplicates == 1 || instance.args.initStrategies == 1) {
    Logger::timed << "[Initialization] All the families will first be optimized with sequences only" << std::endl;
    Logger::mute();
    RaxmlMaster::runRaxmlOptimization(instance.currentFamilies, instance.args.output, instance.args.execPath, instance.currentIteration++, ParallelContext::allowSchedulerSplitImplementation(), instance.elapsedRaxml);
    Logger::unmute();
    Routines::gatherLikelihoods(instance.currentFamilies, instance.totalLibpllLL, instance.totalRecLL);
  } else {
    std::vector<Families> splitFamilies;
    
    ParallelContext::barrier();
    unsigned int splits = instance.args.initStrategies;
    unsigned int recRadius = 5;
    splitInitialFamilies(instance.currentFamilies, splitFamilies, splits);
    Families initialCurrentFamilies = instance.currentFamilies;
    ParallelContext::barrier();
    // only raxml
    Logger::timed << "[Initialization] Optimizing some of the duplicated families with sequences only" << std::endl;
    Logger::mute();
    instance.currentFamilies = splitFamilies[0];
    RaxmlMaster::runRaxmlOptimization(instance.currentFamilies, instance.args.output, instance.args.execPath, instance.currentIteration++, ParallelContext::allowSchedulerSplitImplementation(), instance.elapsedRaxml);
    Logger::unmute();
    if (splits > 1) {
      // raxml and rec
      Logger::timed << "[Initialization] Optimizing some of the duplicated families with sequences only and then species tree only" << std::endl;
      Logger::mute();
      instance.currentFamilies = splitFamilies[1];
      RaxmlMaster::runRaxmlOptimization(instance.currentFamilies, instance.args.output, instance.args.execPath, instance.currentIteration++, ParallelContext::allowSchedulerSplitImplementation(), instance.elapsedRaxml);
      for (unsigned int i = 1; i <= recRadius; ++i) {
        bool enableLibpll = false;
        bool perSpeciesDTLRates = false;
        optimizeRatesAndGeneTrees(instance, perSpeciesDTLRates, enableLibpll, i);
      }
      Logger::unmute();
    }
    if (splits > 2) {
      // rec and raxml
      Logger::timed << "[Initialization] Optimizing some of the duplicated families with species only and then sequences tree only" << std::endl;
      for (unsigned int i = 1; i <= recRadius; ++i) {
        Logger::mute();
        bool enableLibpll = false;
        bool perSpeciesDTLRates = false;
        instance.currentFamilies = splitFamilies[2];
        optimizeRatesAndGeneTrees(instance, perSpeciesDTLRates, enableLibpll, i);
      }
      RaxmlMaster::runRaxmlOptimization(instance.currentFamilies, instance.args.output, instance.args.execPath, instance.currentIteration++, ParallelContext::allowSchedulerSplitImplementation(), instance.elapsedRaxml);
      Logger::unmute();
    }
    instance.currentFamilies = initialCurrentFamilies;
    mergeSplitFamilies(splitFamilies, instance.currentFamilies, splits);
  }
  Logger::timed << "[Initialization] Finished optimizing some of the gene trees" << std::endl;
  Logger::info << std::endl;
}

void GeneRaxCore::optimizeRatesAndGeneTrees(GeneRaxInstance &instance,
    bool perSpeciesDTLRates,
    bool enableLibpll,
    unsigned int sprRadius)
{
  long elapsed = 0;
  if (perSpeciesDTLRates) {
    Logger::timed << "Optimizing per species DTL rates... " << std::endl;
  } else {
    Logger::timed << "Optimizing global DTL rates... " << std::endl;
  }
  Routines::optimizeRates(instance.args.userDTLRates, instance.speciesTree, instance.recModel, instance.currentFamilies, perSpeciesDTLRates, instance.rates, instance.elapsedRates);
  if (instance.rates.dimensions() <= 3) {
    Logger::info << instance.rates << std::endl;
  } else {
    Logger::info << " RecLL=" << instance.rates.getScore();
  }
  Logger::info << std::endl;
  Logger::timed << "Optimizing gene trees with radius=" << sprRadius << "... " << std::endl; 
  GeneTreeSearchMaster::optimizeGeneTrees(instance.currentFamilies, instance.recModel, instance.rates, instance.args.output, "results",
      instance.args.execPath, instance.speciesTree, instance.args.reconciliationOpt, instance.args.perFamilyDTLRates, instance.args.rootedGeneTree, 
     instance.args.pruneSpeciesTree, instance.args.recWeight, true, enableLibpll, sprRadius, instance.currentIteration++, ParallelContext::allowSchedulerSplitImplementation(), elapsed);
  instance.elapsedSPR += elapsed;
  Routines::gatherLikelihoods(instance.currentFamilies, instance.totalLibpllLL, instance.totalRecLL);
  Logger::info << "\tJointLL=" << instance.totalLibpllLL + instance.totalRecLL << " RecLL=" << instance.totalRecLL << " LibpllLL=" << instance.totalLibpllLL << std::endl;
  Logger::info << std::endl;
}
