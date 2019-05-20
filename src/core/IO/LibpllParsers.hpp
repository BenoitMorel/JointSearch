#pragma once

#include <vector>
#include <string>
#include <IO/FamiliesFileParser.hpp>

typedef struct pll_utree_s pll_utree_t;
typedef struct pll_unode_s pll_unode_t;
typedef struct pll_rtree_s pll_rtree_t;


class LibpllException: public std::exception {
public:
  LibpllException(const std::string &s): msg_(s) {}
  LibpllException(const std::string &s1, 
      const std::string s2): msg_(s1 + s2) {}
  virtual const char* what() const throw() { return msg_.c_str(); }
  void append(const std::string &str) {msg_ += str;}

private:
  std::string msg_;
};

class LibpllParsers {
public:
  static pll_utree_t *readNewickFromFile(const std::string &newickFile);
  static pll_utree_t *readNewickFromStr(const std::string &newickSTring);
  static pll_rtree_t *readRootedFromFile(const std::string &newickFile);
  static std::vector<unsigned int> parallelGetTreeSizes(const std::vector<FamiliesFileParser::FamilyInfo> &families);
  static void saveUtree(pll_unode_t *utree, 
    const std::string &fileName, 
    bool append = false);
};
