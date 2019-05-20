#pragma once

#include "enums.hpp"
#include <string>


  

class SpeciesRaxArguments {
public:
   SpeciesRaxArguments(int argc, char * argv[]);
   void checkInputs();
   void printHelp();
   void printCommand();
   void printSummary();
public:
   int argc;
   char ** argv;
   std::string families;
   std::string speciesTree;
   RecModel reconciliationModel;
   RecOpt reconciliationOpt;
   std::string output;
   bool rootedGeneTree;
   bool userDTLRates;
   double dupRate;
   double lossRate;
   double transferRate;
};
