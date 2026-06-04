#ifndef MINE_SWEEPER_CPP_SHELL_MYAI_HPP
#define MINE_SWEEPER_CPP_SHELL_MYAI_HPP

#include "Agent.hpp"
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <algorithm>

using namespace std;

class MyAI : public Agent
{
public:
    MyAI(int _rowDimension, int _colDimension, int _totalMines, int _agentX, int _agentY);

    Action getAction(int number) override;

private:
    int rowDimension;
    int colDimension;
    int totalMines;

    int lastX;
    int lastY;
    bool firstMove;

    int uncoveredCount;

    vector<vector<int>> board;
    vector<pair<int,int>> safeMoves;

    bool inBounds(int x, int y);
    bool isCovered(int x, int y);
    void addSafeMove(int x, int y);
    void addZeroNeighbors(int x, int y);
    pair<int,int> chooseGuess();
    bool CSPCheck();
};

#endif
