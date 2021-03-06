#pragma once

#include <util/enums.hpp>
#include <maths/Parameters.hpp>

struct RecModelInfo {
  // reconciliation model (UndatedDTL, UndatedDL, etc)
  RecModel model;
  // if set to true, each family can have different set of rates
  bool perFamilyRates;
  // if set to true,  for each family, we prune from the species
  // tree the taxa that are not covered in this family
  bool pruneSpeciesTree;
  bool rootedGeneTree;
  // if the reconciliaiton model accounts for polytomies, branches
  // with a lenghts <= branchLengthThreshold are be contracted
  double branchLengthThreshold;
 
  // disable duplications
  bool noDup;

  std::string fractionMissingFile;
  
  
  RecModelInfo():
    model(RecModel::UndatedDTL),
    perFamilyRates(true),
    pruneSpeciesTree(true),
    rootedGeneTree(true),
    branchLengthThreshold(-1.0),
    noDup(false)
  {

  }

  RecModelInfo(RecModel model,
      bool perFamilyRates,
      bool pruneSpeciesTree,
      bool rootedGeneTree,
      double branchLengthThreshold,
      bool noDup,
      const std::string &fractionMissingFile):
    model(model),
    perFamilyRates(perFamilyRates),
    pruneSpeciesTree(pruneSpeciesTree),
    rootedGeneTree(rootedGeneTree),
    branchLengthThreshold(branchLengthThreshold),
    noDup(noDup),
    fractionMissingFile(fractionMissingFile)
  {

  }

  void readFromArgv(char** argv, int &i)
  {
    model = RecModel(atoi(argv[i++]));  
    perFamilyRates = bool(atoi(argv[i++]));
    pruneSpeciesTree = bool(atoi(argv[i++]));
    branchLengthThreshold = double(atof(argv[i++]));
    rootedGeneTree = bool(atoi(argv[i++]));
    noDup = bool(atoi(argv[i++]));
    fractionMissingFile = std::string(argv[i++]);
    if (fractionMissingFile == "NONE") {
      fractionMissingFile = std::string();
    }
  }

  std::vector<std::string> getArgv() const
  {
    std::vector<std::string> argv;
    argv.push_back(std::to_string(static_cast<int>(model)));
    argv.push_back(std::to_string(static_cast<int>(perFamilyRates)));
    argv.push_back(std::to_string(static_cast<int>(pruneSpeciesTree)));
    argv.push_back(std::to_string(static_cast<int>(rootedGeneTree)));
    argv.push_back(std::to_string(static_cast<int>(noDup)));
    argv.push_back(std::to_string(branchLengthThreshold));
    if (fractionMissingFile.size()) {
      argv.push_back(fractionMissingFile);
    } else {
      argv.push_back(std::string("NONE"));
    }
    return argv;
  }

  static int getArgc() 
  {
    return 7;
  }

  unsigned int modelFreeParameters() const {
    return Enums::freeParameters(model);
  }
 
  /*
   *  Return global parameters with the appropriate 
   *  number of values (all set to 0.1)
   */
  Parameters getDefaultGlobalParameters() const {
    Parameters res(modelFreeParameters());
    for (unsigned int i = 0; i < res.dimensions(); ++i) {
      res[i] = 0.1;
    }
    if (noDup) {
      res[0] = 0.0;
    }
    return res;
  }

  /*
   * Takes user-define parameters and return parameters
   * with the appropriate dimensions. If the input parameters
   * have too many values, the last ones are discarded, and if
   * it does not have enough values, they are completed with 0.1
   */
  Parameters getParametersFromUser(const Parameters &user) const {
    Parameters res(modelFreeParameters());
    for (unsigned int i = 0; i < res.dimensions(); ++i) {
      if (user.dimensions() > i) {
        res[i] = user[i];
      } else {
        res[i] = 0.1;
      }
    }
    return res;
  }
};
