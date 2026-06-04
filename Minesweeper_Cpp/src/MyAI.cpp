#include "MyAI.hpp"

MyAI::MyAI(int _rowDimension, int _colDimension, int _totalMines, int _agentX, int _agentY) : Agent()
{
    rowDimension = _rowDimension;
    colDimension = _colDimension;
    totalMines = _totalMines;

    lastX = _agentX;
    lastY = _agentY;
    firstMove = true;
    uncoveredCount = 0;

    board = vector<vector<int>>(colDimension, vector<int>(rowDimension, -2));
}

Agent::Action MyAI::getAction(int number)
{
    if (inBounds(lastX, lastY) && board[lastX][lastY] == -2)
    {
        board[lastX][lastY] = number;
        uncoveredCount++;

        if (number == 0)
        {
            addZeroNeighbors(lastX, lastY);
        }
    }

    bool changed = true;
    int maxIterations = 100; // prevent infinite loops in case of bugs
    while (changed && maxIterations > 0)
    {
        changed = false;
        maxIterations--;

        for (int x = 0; x < colDimension; x++)
        {
            for (int y = 0; y < rowDimension; y++)
            {
                if (board[x][y] < 0)
                {
                    continue;
                }

                int marked = 0;
                vector<pair<int,int>> covered;

                for (int dx = -1; dx <= 1; dx++)
                {
                    for (int dy = -1; dy <= 1; dy++)
                    {
                        if (dx == 0 && dy == 0)
                        {
                            continue;
                        }

                        int nx = x + dx;
                        int ny = y + dy;

                        if (!inBounds(nx, ny))
                        {
                            continue;
                        }

                        if (board[nx][ny] == -1)
                        {
                            marked++;
                        }
                        else if (board[nx][ny] == -2)
                        {
                            covered.push_back({nx, ny});
                        }
                    }
                }

                int effective = board[x][y] - marked;

                if (effective == 0)
                {
                    for (auto p : covered)
                    {
                        addSafeMove(p.first, p.second);
                    }
                }

                if (effective > 0 && effective == (int)covered.size())
                {
                    for (auto p : covered)
                    {
                        if (board[p.first][p.second] == -2)
                        {
                            board[p.first][p.second] = -1;
                            changed = true;
                        }
                    }
                }
            }
        }

        if (!changed && CSPCheck()) {
            changed = true;
        }
    }

    if (uncoveredCount >= rowDimension * colDimension - totalMines)
    {
        return {LEAVE, -1, -1};
    }

    while (!safeMoves.empty())
    {
        pair<int,int> move = safeMoves.back();
        safeMoves.pop_back();

        if (isCovered(move.first, move.second))
        {
            lastX = move.first;
            lastY = move.second;
            return {UNCOVER, lastX, lastY};
        }
    }

    pair<int,int> guess = chooseGuess();

    if (guess.first == -1)
    {
        return {LEAVE, -1, -1};
    }

    lastX = guess.first;
    lastY = guess.second;
    return {UNCOVER, lastX, lastY};
}

bool MyAI::inBounds(int x, int y)
{
    return x >= 0 && x < colDimension && y >= 0 && y < rowDimension;
}

bool MyAI::isCovered(int x, int y)
{
    return inBounds(x, y) && board[x][y] == -2;
}

void MyAI::addSafeMove(int x, int y)
{
    if (!isCovered(x, y))
    {
        return;
    }

    for (auto p : safeMoves)
    {
        if (p.first == x && p.second == y)
        {
            return;
        }
    }

    safeMoves.push_back({x, y});
}

void MyAI::addZeroNeighbors(int x, int y)
{
    for (int dx = -1; dx <= 1; dx++)
    {
        for (int dy = -1; dy <= 1; dy++)
        {
            if (dx == 0 && dy == 0)
            {
                continue;
            }

            int nx = x + dx;
            int ny = y + dy;

            if (isCovered(nx, ny))
            {
                addSafeMove(nx, ny);
            }
        }
    }
}

pair<int,int> MyAI::chooseGuess()
{
    //{(3,3): 0.5} - (3,3) has 50% chance of being a mine. 
    map<pair<int,int>, double> probabilities;

    //calculate probabilities for each covered tile based on neighboring uncovered tiles
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            //skips covered/flagged tiles as we only care about hint numbers.
            if (board[x][y] <= 0) { 
                continue;
            }
            int flag = 0;
            vector<pair<int, int>> covered;

            for (int dx = -1; dx <= 1; dx++){
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0){
                        continue;
                    }

                    int nx = x + dx;
                    int ny = y + dy;

                    if (!inBounds(nx, ny)){
                        continue;
                    }

                    if (board[nx][ny] == -1) {
                        flag++;
                    }
                    else if (board[nx][ny] == -2) {
                        covered.push_back({nx, ny});
                    }
                }
            }

            if (covered.empty()) {
                continue;
            }

            //Calculate probability for that tile's neighbors 
            //e.g. tile says 2, one neighbor already flagged -> 2-1 = 1 remanining left among 3 covered neighbors 
            //-> 1/3 = 33% chance each neighbor is a mine
            int remainingMines = board[x][y] - flag;
            if (remainingMines <= 0) {
                continue;
            }

            double prob = (double)remainingMines / covered.size();

            for (auto& p : covered) {
                //keep the highest probability seen for each tile
                if (probabilities.find(p) == probabilities.end()) {
                    probabilities[p] = prob;
                } else {
                    //if multiple uncovered tiles point to the same covered tile,
                    //take the highest probability (most likely to be a mine) to be safe.
                    probabilities[p] = max(probabilities[p], prob);
                }
            }
        }
    }

    //find covered tiles with no numbered neighbors - use global probability 
    int totalCovered = 0;
    int totalFlags = 0;
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            if (board[x][y] == -2) {
                totalCovered++;
            } else if (board[x][y] == -1) {
                totalFlags++;
            }
        }
    }

    // If there are covered tiles with no information, assign them the global probability of being a mine
    //e.g. if 10 mines left and 20 covered tiles with no info, each has 50% chance of being a mine.
    double globalProb = (totalCovered > 0) ? (double)(totalMines - totalFlags) / totalCovered : 1.0;

    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            if (board[x][y] == -2 && probabilities.find({x, y}) == probabilities.end()) {
                probabilities[{x, y}] = globalProb;
            }
        }
    }

    if (probabilities.empty()) {
        return {-1, -1};
    }

    pair<pair<int,int>, double> best = {make_pair(-1,-1), 2.0};
    for (auto it = probabilities.begin(); it != probabilities.end(); ++it)
    {
        //find the tile with the lowest probability of being a mine
        if (it->second < best.second)
        {
            best.first = it->first;
            best.second = it->second;
        }
    }

    if (best.first.first == -1) return {-1, -1};

    return best.first;
}

bool MyAI::CSPCheck() {
    bool changed = false;

    //constraint = {covered neighbors, remaining mines}
    vector<pair<vector<pair<int,int>>, int>> constraints;
    
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            //only care about uncovered tiles with numbers, as they provide constraints on their covered neighbors
            if (board[x][y] <= 0) {
                continue;
            }

            int flag = 0;
            vector<pair<int, int>> covered;

            for (int dx = -1; dx <= 1; dx++) {
                for (int dy = -1; dy <= 1; dy++) {
                    if (dx == 0 && dy == 0){
                        continue;
                    }

                    int nx = x + dx;
                    int ny = y + dy;

                    if (!inBounds(nx, ny)){
                        continue;
                    }

                    if (board[nx][ny] == -1) {
                        flag++;
                    }
                    else if (board[nx][ny] == -2) {
                        covered.push_back({nx, ny});
                    }
                }
            }

            int remainingMines = board[x][y] - flag;
            if (remainingMines > 0 && !covered.empty()) {
                constraints.push_back({covered, remainingMines});
            }
        }
    }

    for (int i = 0; i < (int)constraints.size(); i++) {
        for (int j = 0; j < (int)constraints.size(); j++) {
            if (i == j) continue;

            vector<pair<int, int>>& setA = constraints[i].first;
            vector<pair<int, int>>& setB = constraints[j].first;
            int minesA = constraints[i].second;
            int minesB = constraints[j].second;

            bool isSubset = true;
            for (auto& p : setA) {
                bool found = false;
                for (auto& t : setB) {
                    if (t == p) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    isSubset = false;
                    break;
                }
            }

            if (!isSubset) {
                continue;
            }

            //setA is a subset of setB, so the difference setB - setA must contain minesB - minesA mines
            vector<pair<int, int>> diff;
            for (auto& p : setB) {
                bool inA = false;
                for (auto& t: setA) {
                    if (t == p) {
                        inA = true;
                        break;
                    }
                }
                if (!inA) {
                    diff.push_back(p);
                }
            }

            if (diff.empty()) {
                continue;
            }

            int remainingMines = minesB - minesA;

            //if difference has 0 mines -> all safe
            if (remainingMines == 0) {
                for (auto& p : diff) {
                    addSafeMove(p.first, p.second);
                    changed = true;
                }
            }

            //if difference tiles == remaining mines -> all mines
            if (remainingMines == (int)diff.size()) {
                for (auto& p : diff) {
                    if (board[p.first][p.second] == -2) {
                        board[p.first][p.second] = -1;
                        changed = true;
                    }
                }
            }


        }
    }

    return changed;
}