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
    while (changed)
    {
        changed = false;

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
    for (int x = 0; x < colDimension; x++)
    {
        for (int y = 0; y < rowDimension; y++)
        {
            if (board[x][y] == -2)
            {
                return {x, y};
            }
        }
    }

    return {-1, -1};
}
