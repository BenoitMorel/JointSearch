// Modified by N.Comte for Treerecs – Copyright © by INRIA – All rights reserved – 2017
#include "exODT_DL.h"
#include "fractionMissing.h"

// Bpp includes
#include <Bpp/BppString.h>
#include <Bpp/Numeric/Random/RandomTools.h>

// Treerecs includes
#include <ale/tools/IO/IO.h>
#include <ale/tools/PhyloTreeToolBox.h>

using namespace bpp;
using namespace std;

exODT_DL_model::exODT_DL_model() {
}


void exODT_DL_model::set_model_parameter(std::string name, std::string value) {
  string_parameter[name] = value;
}


void exODT_DL_model::set_model_parameter(std::string name, scalar_type value) {

  if (name == "delta" or name == "lambda") {
    scalar_type N = vector_parameter["N"][0];
    vector_parameter[name].clear();
    for (int branch = 0; branch < last_branch; branch++)
      vector_parameter[name].push_back(value);
    scalar_parameter[name + "_avg"] = value;

  } else if (name == "N" or name == "Delta_bar" or name == "Lambda_bar") {
    vector_parameter[name].clear();
    for (int rank = 0; rank < last_rank; rank++)
      vector_parameter[name].push_back(value);
  } else
    scalar_parameter[name] = value;
}


void exODT_DL_model::construct_undated(const std::string &Sstring, const std::string &fractionMissingFile) {
  daughter.clear();
  son.clear();
  name_node.clear();
  node_name.clear();
  node_ids.clear();
  S = std::shared_ptr<tree_type>(IO::newickToPhyloTree(Sstring, true));
  // For each node from the speciestree
  //std::vector<std::shared_ptr<bpp::PhyloNode>> nodes = S->getAllNodes();
  auto nodes = PhyloTreeToolBox::getNodesInPostOrderTraversalRecursive_list(*S);

  // Set each branch length to 1.
  for (auto it = nodes.begin(); it != nodes.end(); it++) {
    if (S->hasFather(*it)) {
      auto edge_to_father = S->getEdgeToFather(*it);
      if (edge_to_father) S->getEdgeToFather(*it)->setLength(1);
    }
  }

  // For each node from the speciestree, record node names.
  // name_node gives a node according to the name
  // node_name gives a name accoring to the node.
  // * For leaves, get name.
  // * For internal nodes, gives a node name according to leaves under.
  // \todo node_name is useless until PhyloNode contains a name.
  for (auto it = nodes.begin(); it != nodes.end(); it++) {
    if (S->isLeaf(*it)) {
      name_node[(*it)->getName()] = (*it);
      node_name[(*it)] = (*it)->getName();
    } else {
      std::vector <std::string> leafnames = PhyloTreeToolBox::getLeavesNames(*S, (*it));
      std::sort(leafnames.begin(), leafnames.end());
      std::stringstream name;
      for (auto leafname = leafnames.begin(); leafname != leafnames.end(); leafname++)
        name << (*leafname) << ".";

      name_node[name.str()] = (*it);
      node_name[(*it)] = name.str();
    }
  }
  daughter.resize(nodes.size());
  son.resize(nodes.size());

  // register species
  last_branch = 0;
  last_leaf = 0;

  // associate each node to an id (node_ids and i_nodes).
  // this will determines the number of leaves (last_leaf)
  // and starting to determine the number of branches.
  // \todo Use PhyloTreeToolBox to get direct post-order.
  std::set<std::shared_ptr<bpp::PhyloNode>> saw;
  
  for (auto it = name_node.begin(); it != name_node.end(); it++)
    if (S->isLeaf((*it).second)) {
      auto node = (*it).second;
      extant_species[last_branch] = node->getName();
      node_ids[node] = last_branch;
      last_branch++;
      last_leaf++;
      saw.insert(node);
      // a leaf
      daughter[last_branch] = -1;
      // a leaf
      son[last_branch] = -1;
    }

  //ad-hoc postorder, each internal node is associated to an id and an id to an internal node
  //During the node exploration, set "ID" property to each node with value as the name.
  //
  std::vector<std::shared_ptr<bpp::PhyloNode>> next_generation;
  for (auto it = name_node.begin(); it != name_node.end(); it++)
    if (S->isLeaf((*it).second)) {
      auto node = (*it).second;
      next_generation.push_back(node);
    }

  while (next_generation.size()) {
    std::vector<std::shared_ptr<bpp::PhyloNode>> new_generation;
    for (auto it = next_generation.begin(); it != next_generation.end(); it++) {
      auto node = (*it);
      if (S->hasFather(node)) {
        auto father = S->getFather(node);
        auto sons = S->getSons(father);
        decltype(node) sister;

        if (sons[0] == node) 
          sister = sons[1];
        else 
          sister = sons[0];

        if (not node_ids.count(father) and saw.count(sister)) {
          node_ids[father] = last_branch;
          last_branch++;
          saw.insert(father);
          new_generation.push_back(father);
        }
      }
    }
    next_generation.clear();
    for (auto it = new_generation.begin();
         it != new_generation.end(); it++)
      next_generation.push_back((*it));
  }

  // Fill daughter and son maps. Daughter contains son left and son son right of a node.
  for (auto it = name_node.begin(); it != name_node.end(); it++) {
    if (not S->isLeaf(it->second))//not (*it).second->isLeaf())
    {
      auto node = (*it).second;
      auto sons = S->getSons(node);//node->getSons();
      daughter[node_ids[node]] = node_ids[sons[0]];
      son[node_ids[node]] = node_ids[sons[1]];
    }
  }
  last_rank = last_branch;
  set_model_parameter("N", 1);
}

void exODT_DL_model::calculate_undatedEs() {
  scalar_type P_D = vector_parameter["delta"][0];
  scalar_type P_L = vector_parameter["lambda"][0];
  scalar_type P_S = 1;
  scalar_type sum = P_D + P_L + P_S;
  PD = P_D / sum;
  PL = P_L / sum;
  PS = P_S / sum;
  uE = vector<scalar_type>(last_branch, 0.0);
  for (int e = 0; e < last_leaf; ++e) {
    scalar_type a = PD;
    scalar_type b = -1.0;
    scalar_type c = PL;
    uE[e] = (-b - sqrt(b * b - 4 * a * c)) / (2.0 * a);
  }
  for (int e = last_leaf; e < last_branch; ++e) {
    int f = daughter[e];
    int g = son[e];
    scalar_type a = PD;
    scalar_type b = -1.0;
    scalar_type c = PL + PS * uE[f]  * uE[g];
    uE[e] = (-b - sqrt(b * b - 4 * a * c)) / (2.0 * a);
  }
}

void  exODT_DL_model::step_one(std::shared_ptr<approx_posterior> ale) {
  for (auto it = ale->size_ordered_bips.begin(); it != ale->size_ordered_bips.end(); it++)
    for (auto jt = (*it).second.begin(); jt != (*it).second.end(); jt++) {
      g_ids.push_back((*jt));
      g_id_sizes.push_back((*it).first);
    }
  //root bipartition needs to be handled separately
  g_ids.push_back(-1);
  g_id_sizes.push_back(ale->Gamma_size);
}

void exODT_DL_model::gene_secies_mapping(std::shared_ptr<approx_posterior> ale)
{

  // gene<->species mapping
  if (gid_sps.size() == 0) // If the mapping has not been done yet
  {
    //Test that the species associated to genes are really in the species tree
    std::set<std::string> species_set;
    for (auto iter = extant_species.begin(); iter != extant_species.end(); ++iter) {
      species_set.insert(iter->second);
    }

    for (int i = 0; i < (int) g_ids.size(); i++) {
      long int g_id = g_ids[i];

      if (g_id_sizes[i] == 1) {
        int id = 0;
        for (auto i = 0; i < ale->Gamma_size + 1; ++i) {
          if (ale->id_sets[g_id][i]) {
            id = i;
            break;
          }
        }

        std::string gene_name = ale->id_leaves[id];
        // Set associated species in gid_sps[g_id]
        std::string species_name = speciesGeneMap.getAssociatedSpecies(gene_name);
        gid_sps[g_id] = species_name;
        if (species_set.find(species_name) == species_set.end()) {
          std::cout << "Error: gene name " << gene_name << " is associated to species name " << species_name
                    << " that cannot be found in the species tree." << std::endl;
          exit(-1);
        }
      }
    }
  }
}

void exODT_DL_model::inner_loop(std::shared_ptr<approx_posterior> ale, bool g_is_a_leaf,
                             long int &g_id,
                             std::vector<int> &gp_is,
                             std::vector<long int> &gpp_is,
                             int i) {
  for (int e = 0; e < last_branch; e++) {
    bool s_is_leaf = (e < last_leaf);
    int f = 0;
    int g = 0;
    if (not s_is_leaf) {
      f = daughter[e];
      g = son[e];
    }
    scalar_type uq_sum = 0;
    // S leaf and G leaf
    if (e < last_leaf and g_is_a_leaf and extant_species[e] == gid_sps[g_id]) {
      // present
      uq_sum += PS;
    }
    // G internal
    if (not g_is_a_leaf) {
      int N_parts = gp_is.size();
      for (int i = 0; i < N_parts; i++) {
        int gp_i = gp_is[i];
        int gpp_i = gpp_is[i];
        if (not s_is_leaf) {
          uq_sum += PS * (uq[gp_i][f] * uq[gpp_i][g] + uq[gp_i][g] * uq[gpp_i][f]);
        }
        // D event
        uq_sum += PD * (uq[gp_i][e] * uq[gpp_i][e] * 2);
      }
    }
    if (not s_is_leaf) {
      // SL event
      uq_sum += PS * (uq[i][f] * uE[g] + uq[i][g] * uE[f]);
    }
    uq[i][e] = uq_sum / (1.0 - 2.0 * PD * uE[e]);
  }
}

scalar_type exODT_DL_model::pun(std::shared_ptr<approx_posterior> ale, bool verbose) {
  scalar_type survive = 0;
  scalar_type root_sum = 0;
  scalar_type O_norm = 0;
  
  g_ids.clear();
  g_id_sizes.clear();
  step_one(ale);
  gene_secies_mapping(ale);

  for (int i = 0; i < (int) g_ids.size(); i++) {
    long int g_id = g_ids[i];
    g_id2i[g_id] = i;
  }
  vector<scalar_type> zeros(last_branch, 0.0);
  uq = vector<vector<scalar_type>>(g_ids.size(),zeros);

  for (int i = 0; i < (int) g_ids.size(); i++) {
    // directed partition (dip) gamma's id
    long int g_id = g_ids[i];
    bool is_a_leaf = (g_id_sizes[i] == 1);
    std::vector<int> gp_is;
    std::vector<long int> gpp_is;
    if (g_id != -1) {
      for (std::unordered_map<std::pair<long int, long int>, scalar_type>::iterator kt = ale->Dip_counts[g_id].begin();
          kt != ale->Dip_counts[g_id].end(); kt++) {
        std::pair<long int, long int> parts = (*kt).first;
        long int gp_id = parts.first;
        long int gpp_id = parts.second;
        int gp_i = g_id2i[parts.first];
        int gpp_i = g_id2i[parts.second];
        gp_is.push_back(gp_i);
        gpp_is.push_back(gpp_i);
      }
    } else {
      //XX
      //root bipartition needs to be handled separately
      std::map<std::set<long int>, int> bip_parts;
      for (std::map<long int, scalar_type>::iterator it = ale->Bip_counts.begin();
          it != ale->Bip_counts.end(); it++) {
        long int gp_id = (*it).first;
        boost::dynamic_bitset<> gamma = ale->id_sets.at(gp_id);
        boost::dynamic_bitset<> not_gamma = ~gamma;
        not_gamma[0] = 0;
        long int gpp_id = ale->set_ids.at(not_gamma);

        std::set<long int> parts;
        parts.insert(gp_id);
        parts.insert(gpp_id);
        bip_parts[parts] = 1;
      }
      for (std::map<std::set<long int>, int>::iterator kt = bip_parts.begin(); kt != bip_parts.end(); kt++) {
        std::vector<long int> parts;
        for (std::set<long int>::iterator sit = (*kt).first.begin(); sit != (*kt).first.end(); sit++) {
          parts.push_back((*sit));
        }
        long int gp_id = parts[0];
        int gp_i = g_id2i[parts[0]];
        int gpp_i = g_id2i[parts[1]];
        gp_is.push_back(gp_i);
        gpp_is.push_back(gpp_i);
      }
      bip_parts.clear();
    }
    inner_loop(ale, is_a_leaf, g_id, gp_is, gpp_is, i);
  }
  survive = 0;
  root_sum = 0;
  O_norm = 0;
  for (int e = 0; e < last_branch; e++) {
    scalar_type O_p = 1;
    if (e == (last_branch - 1)) 
      O_p = scalar_parameter["O_R"];
    O_norm += O_p;
    root_sum += uq[g_ids.size() - 1][e] * O_p;
    survive += (1 - uE[e]);
  }
  return root_sum / survive / O_norm * (last_branch);
}

exODT_DL_model::~exODT_DL_model() { }

