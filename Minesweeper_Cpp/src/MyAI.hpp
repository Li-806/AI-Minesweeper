// ======================================================================
// FILE:        MyAI.hpp
// DESCRIPTION: Minesweeper AI agent.
//   Strategy:
//     1. Single-point constraint logic (trivially safe / trivially mine).
//     2. Subset (constraint-difference) reduction.
//     3. CSP enumeration over connected frontier components -> exact
//        per-cell mine probabilities, coupled by the global mine count
//        and the unconstrained "sea" of interior tiles.
//     4. When no provably-safe tile exists, uncover the tile with the
//        lowest mine probability (with progress-oriented tie-breaks).
//   Everything lives inside the MyAI class and uses only the STL.
// ======================================================================

#ifndef MINE_SWEEPER_CPP_SHELL_MYAI_HPP
#define MINE_SWEEPER_CPP_SHELL_MYAI_HPP

#include "Agent.hpp"
#include <vector>
#include <utility>

using namespace std;

class MyAI : public Agent
{
public:
    MyAI(int _rowDimension, int _colDimension, int _totalMines, int _agentX, int _agentY);

    Action getAction(int number) override;

    // true iff the most recent UNCOVER was a probabilistic guess rather than a
    // logically-certain safe move (used only by the offline verifier).
    bool lastWasGuess = false;

private:
    // ---- board geometry / state ----
    int rowDimension;   // number of rows    (y range)
    int colDimension;   // number of columns (x range)
    int totalMines;

    int lastX, lastY;       // coordinates of the previous action
    int revealedCount;      // # of safe tiles uncovered so far
    int targetReveal;       // colDimension*rowDimension - totalMines
    int knownMines;         // # of tiles proven to be mines

    vector<int>  board;     // size N: -2 covered/unknown, -1 known mine, 0..8 number
    vector<char> safeMark;  // size N: 1 if tile is proven safe (queued/known safe)
    vector<int>  safeQueue; // ids of proven-safe covered tiles to uncover

    // ---- helpers ----
    inline bool inB(int x, int y) const { return x >= 0 && x < colDimension && y >= 0 && y < rowDimension; }
    inline int  cid(int x, int y) const { return x * rowDimension + y; }
    void markSafe(int x, int y);
    void markMine(int x, int y);

    // ---- inference ----
    bool deduce();                       // single-point + subset until a change; true if changed
    bool singlePoint();                  // trivial safe / trivial mine
    bool subsetRule();                   // constraint-difference reduction
    pair<int,int> solveCSP(bool& progressed); // probability engine + best guess
    bool endgameExact(const vector<int>& candidates,
                      const vector<vector<int>>& cons,
                      const vector<int>& consNeed,
                      int R,
                      bool& progressed,
                      pair<int,int>& guess);

    // CSP component enumeration
    static bool enumerateComp(int n,
                              const vector<vector<int>>& conVars,
                              const vector<int>& conNeed,
                              int Rcap, long long nodeCap, double deadline,
                              vector<double>& cntByK,
                              vector<vector<double>>& mineByK);
};

#endif // MINE_SWEEPER_CPP_SHELL_MYAI_HPP
