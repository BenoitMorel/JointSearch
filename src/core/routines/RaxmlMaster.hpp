#pragma once

#include <string>
#include <vector>
#include <IO/FamiliesFileParser.hpp>

class RaxmlMaster {
public:
  RaxmlMaster() = delete;
  static void runRaxmlOptimization(Families &families,
    const std::string &output,
    const std::string &execPath,
    int iteration,
    bool splitImplem,
    long &sumElapsed);

};
