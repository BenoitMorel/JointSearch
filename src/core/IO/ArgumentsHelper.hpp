#pragma once

#include <string>
#include <util/enums.hpp>
#include <IO/Logger.hpp>



/**
 * Static methods for helping argument parsing
 */
class ArgumentsHelper {
public:
  ArgumentsHelper() = delete;

  static std::string strategyToStr(Strategy s)
  {
    switch(s) {
    case Strategy::SPR:
      return "SPR";
    case Strategy::EVAL:
      return "EVAL";
    }
    exit(41);
  }

  static Strategy strToStrategy(const std::string &str) 
  {
    if (str == "SPR") {
      return Strategy::SPR;
    }  else if (str == "EVAL") {
      return Strategy::EVAL;
    } else {
      Logger::info << "Invalid strategy " << str << std::endl;
      exit(41);
    }
  }
 
  static std::string recModelToStr(RecModel recModel)
  {
    switch(recModel) {
    case RecModel::UndatedDL:
      return "UndatedDL";
    case RecModel::UndatedDTL:
      return "UndatedDTL";
    };
    exit(41);
  }

  static bool isValidRecModel(const std::string &str)
  {
    return (str == "UndatedDL") || (str == "UndatedDTL");
  }

  static RecModel strToRecModel(const std::string &str)
  {
    if (str == "UndatedDL") {
      return RecModel::UndatedDL;
    } else if (str == "UndatedDTL") {
      return RecModel::UndatedDTL;
    } else {
      Logger::info << "Invalid reconciliation model " << str << std::endl;
      exit(41);
    }
  }

  static std::string recOptToStr(RecOpt recOpt) {
    switch(recOpt) {
    case RecOpt::Grid:
      return "grid";
    case RecOpt::Simplex:
      return "simplex";
    case RecOpt::Gradient:
      return "gradient";
    }
    exit(41);
  }

  static RecOpt strToRecOpt(const std::string &str) {
    if (str == "grid") {
      return RecOpt::Grid;
    } else if (str == "simplex") {
      return RecOpt::Simplex;
    } else if (str == "gradient") {
      return RecOpt::Gradient;
    } else {
      Logger::info << "Invalid reconciliation optimization method " << str << std::endl;
      exit(41);
    }
  }

};
