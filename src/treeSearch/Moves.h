#ifndef _MOVES_H_
#define _MOVES_H_

#include <likelihoods/LibpllEvaluation.hpp>
#include <treeSearch/Rollbacks.hpp>
#include <memory>

class JointTree;

class Move {
public:
  virtual std::shared_ptr<Rollback> applyMove(JointTree &tree) = 0;
  static std::shared_ptr<Move> createSPRMove(int pruneIndex, int regraftIndex, const vector<int> &path);
    
  friend std::ostream & operator <<( std::ostream &os, const Move &move ) {
    return move.print(os);
  }
  
  virtual std::ostream& print(std::ostream & os) const = 0;
};

class SPRMove: public Move {
public:
  SPRMove(int pruneIndex, int regraftIndex, const vector<int> &path);
  virtual std::shared_ptr<Rollback> applyMove(JointTree &tree);
  virtual std::ostream& print(std::ostream & os) const;
private:
  int pruneIndex_;
  int regraftIndex_;
  vector<int> path_;
};

#endif
