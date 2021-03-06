
class GeneRaxInstance;


/**
 * All the routines GeneRax main needs
 */
class GeneRaxCore {
public:
  GeneRaxCore() = delete;
  
  /*
   * Create output directories, initialize the logger, initialize the species tree,
   * read and filter the families
   */
  static void initInstance(GeneRaxInstance &instance); 

  /*
   *  If the starting gene trees are random, generate them, update the families, 
   *  and apply an initial tree search. 
   *  Create additional trees if multiple starting trees (duplicates) option is used.
   *  Does nothing is the user provides input gene trees.
   */
  static void initRandomGeneTrees(GeneRaxInstance &instance);
  
  /**
   *  Initialize the species tree
   */
  static void initSpeciesTree(GeneRaxInstance &instance);
  
  static void generateFakeAlignments(GeneRaxInstance &instance);

  static void printStats(GeneRaxInstance &instance);

  /**
   * Infer a species tree if species tree inference is enabled
   */
  static void speciesTreeSearch(GeneRaxInstance &instance);

  /**
   *  Apply a gene tree search based on the joint likelihood, 
   *  and save the results
   */
  static void geneTreeJointSearch(GeneRaxInstance &instance);

  /**
   *  Reconcile the gene trees with the species tree and save results
   */
  static void reconcile(GeneRaxInstance &instance);

  /**
   *  Estimate the branch lengths of the species tree
   */
  static void speciesTreeBLEstimation(GeneRaxInstance &instance);


  /**
   *  Estimate the support values of the species tree
   */
  static void speciesTreeSupportEstimation(GeneRaxInstance &instance);

  /**
   *  Write stats and print some last logs
   */
  static void terminate(GeneRaxInstance &instance);

private:
  /**
   *  Create a folder for each family in instance.currentFamilies
   */
  static void initFolders(GeneRaxInstance &instance); 

  /**
   * Apply an initial tree search on the gene trees.
   */
  static void initialGeneTreeSearch(GeneRaxInstance &instance);

  /*
   *  Generic gene tree search  
   */
  static void optimizeRatesAndGeneTrees(GeneRaxInstance &instance,
    bool perSpeciesDTLRates,
    bool enableLibpll,
    unsigned int sprRadius);

};
