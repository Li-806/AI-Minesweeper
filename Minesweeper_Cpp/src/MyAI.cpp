#include "MyAI.hpp"
#include <cmath>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <chrono>

static double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

static const int DX[8] = {-1,-1,-1, 0, 0, 1, 1, 1};
static const int DY[8] = {-1, 0, 1,-1, 1,-1, 0, 1};

MyAI::MyAI(int _rowDimension, int _colDimension, int _totalMines, int _agentX, int _agentY) : Agent() {
    rowDimension = _rowDimension;
    colDimension = _colDimension;
    totalMines   = _totalMines;

    lastX = _agentX;
    lastY = _agentY;

    int N = rowDimension * colDimension;
    board = vector<int>(N, -2);
    safeMark = vector<char>(N, 0);

    revealedCount = 0;
    knownMines = 0;
    targetReveal = N - totalMines;
}

void MyAI::markSafe(int x, int y) {
    int id = cid(x, y);
    if (board[id] == -2 && !safeMark[id]) {
        safeMark[id] = 1;
        safeQueue.push_back(id);
    }
}

void MyAI::markMine(int x, int y) {
    int id = cid(x, y);
    if (board[id] == -2 && !safeMark[id]) {
        board[id] = -1;
        knownMines++;
    }
}

//main decision loop.
Agent::Action MyAI::getAction(int number) {
    // 1. record the percept for the previous UNCOVER.
    if (inB(lastX, lastY)) {
        int id = cid(lastX, lastY);
        if (board[id] == -2) {
            board[id] = number; // a number 0-8 (this tile was safe)
            revealedCount++;
        }
    }

    // 2. inference/action 
    while (true) {
        while (deduce()) { /* repeat while we keep making progress */ }

        if (revealedCount >= targetReveal) {
            return {LEAVE, -1, -1};
        }

        //uncover a proven-safe tile if we have one queued.
        while (!safeQueue.empty()) {
            int id = safeQueue.back();
            safeQueue.pop_back();
            if (board[id] == -2) {
                lastX = id / rowDimension;
                lastY = id % rowDimension;
                lastWasGuess = false;
                return {UNCOVER, lastX, lastY};
            }
        }

        //no certain move: run the probability engine.
        bool progressed = false;
        pair<int,int> slot = solveCSP(progressed);
        if (progressed) {
            continue; //newly proven safe/mine tiles -> re-loop
        }                  

        if (slot.first == -1) {
            return {LEAVE, -1, -1};
        }

        lastX = slot.first;
        lastY = slot.second;
        lastWasGuess = true;
        return {UNCOVER, lastX, lastY};
    }
}

bool MyAI::deduce() {
    if (singlePoint()) return true;
    if (subsetRule()) return true;
    return false;
}

//trivial deductions from each revealed number.
bool MyAI::singlePoint() {
    bool changed = false;
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            int v = board[cid(x, y)];
            if (v < 0) continue;       //only revealed numbers

            int mines = 0, covered = 0;
            int cx[8], cy[8], cc = 0;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!inB(nx, ny)) continue;
                int nv = board[cid(nx, ny)];
                if (nv == -1) mines++;
                else if (nv == -2 && !safeMark[cid(nx, ny)]) {
                    cx[cc] = nx; cy[cc] = ny; cc++;
                    covered++;
                }
            }
            if (covered == 0) continue;

            int need = v - mines;
            if (need == 0) {
                for (int i = 0; i < cc; i++) { 
                    markSafe(cx[i], cy[i]); changed = true; 
                }
            }
            else if (need == covered) {
                for (int i = 0; i < cc; i++) { 
                    markMine(cx[i], cy[i]); changed = true; 
                }
            }
        }
    }
    return changed;
}

//subset/constraint-difference reduction (e.g. the 1-2-1 pattern).
bool MyAI::subsetRule() {
    int N = rowDimension * colDimension;

    //build constraints from revealed numbers over unknown (non-safe) tiles.
    vector<vector<int>> vars;  //candidate tile ids per constraint
    vector<int> need;
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            int v = board[cid(x, y)];
            if (v < 0) continue;
            int mines = 0;
            vector<int> cov;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!inB(nx, ny)) continue;
                int id = cid(nx, ny);
                if (board[id] == -1) mines++;
                else if (board[id] == -2 && !safeMark[id]) cov.push_back(id);
            }
            if (cov.empty()) continue;
            int nd = v - mines;
            if (nd < 0) continue;
            sort(cov.begin(), cov.end());
            vars.push_back(std::move(cov));
            need.push_back(nd);
        }
    }

    int C = (int)vars.size();
    if (C < 2) return false;

    //cell id -> constraints containing it.
    vector<vector<int>> byCell(N);
    for (int i = 0; i < C; i++) {
        for (int id : vars[i]) {
            byCell[id].push_back(i);
        }
    }

    bool changed = false;
    //for each ordered pair (a subset of b) sharing a cell, derive the difference.
    for (int a = 0; a < C; a++) {
        if (vars[a].empty()) continue;
        int anchor = vars[a][0];
        for (int b : byCell[anchor]) {
            if (b == a) continue;
            if (vars[b].size() <= vars[a].size()) continue;
            //is vars[a] subset of vars[b]? (both sorted)
            bool sub = true;
            size_t ia = 0, ib = 0;
            while (ia < vars[a].size() && ib < vars[b].size()) {
                if (vars[a][ia] == vars[b][ib]) { 
                    ia++; ib++; 
                }
                else if (vars[b][ib] < vars[a][ia]) {
                    ib++;
                }
                else { 
                    sub = false; break; 
                }
            }
            if (ia != vars[a].size()) sub = false;
            if (!sub) continue;

            int dneed = need[b] - need[a];
            int dsize = (int)vars[b].size() - (int)vars[a].size();
            if (dneed < 0 || dneed > dsize) continue;
            if (dneed != 0 && dneed != dsize) continue;

            //difference = vars[b] \ vars[a]
            ia = 0; ib = 0;
            while (ib < vars[b].size()) {
                if (ia < vars[a].size() && vars[a][ia] == vars[b][ib]) { 
                    ia++; ib++; continue; 
                }
                int id = vars[b][ib];
                int x = id / rowDimension, y = id % rowDimension;
                if (board[id] == -2 && !safeMark[id]) {
                    if (dneed == 0) { 
                        markSafe(x, y); changed = true; 
                    }
                    else { 
                        markMine(x, y); changed = true; 
                    }
                }
                ib++;
            }
        }
    }
    return changed;
}

//backtracking enumeration of one frontier component.
//cntByK[k] = # of consistent assignments using exactly k mines
//mineByK[k][j] = # of those assignments where local var j is a mine
//returns false if the node budget was exceeded (component left unsolved).
bool MyAI::enumerateComp(int n,
                         const vector<vector<int>>& conVars,
                         const vector<int>& conNeed,
                         int Rcap, long long nodeCap, double deadline,
                         vector<double>& cntByK,
                         vector<vector<double>>& mineByK)
{
    int C = (int)conVars.size();

    //var -> constraints containing it; and variable ordering by degree.
    vector<vector<int>> varCons(n);
    for (int c = 0; c < C; c++) {
        for (int v : conVars[c]) {
            varCons[v].push_back(c);
        }
    }

    vector<int> order(n);
    for (int i = 0; i < n; i++) {
        order[i] = i;
    }
    sort(order.begin(), order.end(),
         [&](int a, int b){ return varCons[a].size() > varCons[b].size(); });

    cntByK.assign(n + 1, 0.0);
    mineByK.assign(n + 1, vector<double>(n, 0.0));

    vector<int> conCur(C, 0), conRem(C, 0);
    for (int c = 0; c < C; c++) {
        conRem[c] = (int)conVars[c].size();
    }

    vector<char> assign(n, 0);
    long long nodes = 0;
    bool overflow = false;

    //iterative-style recursion via explicit lambda
    //(depth bounded by n which is small; native recursion is fine)
    struct Rec {
        int n, Rcap; long long nodeCap; double deadline;
        const vector<vector<int>>* conVars;
        const vector<int>* conNeed;
        vector<int>* order; vector<vector<int>>* varCons;
        vector<int>* conCur; vector<int>* conRem;
        vector<char>* assign;
        vector<double>* cntByK; vector<vector<double>>* mineByK;
        long long* nodes; bool* overflow;

        void go(int idx, int curMines) {
            if (*overflow) return;
            if (++(*nodes) > nodeCap) { 
                *overflow = true; return; 
            }
            if (((*nodes) & 0x3FFFF) == 0 && nowSeconds() > deadline) {
                *overflow = true; return;
            }

            if (idx == n) {
                (*cntByK)[curMines] += 1.0;
                for (int v = 0; v < n; v++) {
                    if ((*assign)[v]) (*mineByK)[curMines][v] += 1.0;
                }
                return;
            }

            int v = (*order)[idx];
            for (int val = 0; val <= 1; val++) {
                if (val == 1 && curMines + 1 > Rcap) continue;

                bool ok = true;
                for (int c : (*varCons)[v]) {
                    (*conRem)[c]--;
                    if (val) (*conCur)[c]++;
                }
                for (int c : (*varCons)[v]) {
                    int cur = (*conCur)[c], rem = (*conRem)[c], nd = (*conNeed)[c];
                    if (cur > nd || cur + rem < nd) { 
                        ok = false; break;
                    }
                }
                if (ok) {
                    (*assign)[v] = (char)val;
                    go(idx + 1, curMines + val);
                    (*assign)[v] = 0;
                }
                for (int c : (*varCons)[v]) {
                    (*conRem)[c]++;
                    if (val) (*conCur)[c]--;
                }
                if (*overflow) return;
            }
        }
    } rec;

    rec.n = n; rec.Rcap = Rcap; rec.nodeCap = nodeCap; rec.deadline = deadline;
    rec.conVars = &conVars; rec.conNeed = &conNeed;
    rec.order = &order; rec.varCons = &varCons;
    rec.conCur = &conCur; rec.conRem = &conRem;
    rec.assign = &assign;
    rec.cntByK = &cntByK; rec.mineByK = &mineByK;
    rec.nodes = &nodes; rec.overflow = &overflow;

    rec.go(0, 0);
    return !overflow;
}

//late endgame full-board enumeration. This is intentionally narrow: when
//all remaining covered cells are small enough, enumerate every complete
//mine layout satisfying the local constraints and total mine count. If no
//forced tile exists, choose among near-min-risk guesses by expected posterior
//ambiguity after the revealed number.
bool MyAI::endgameExact(const vector<int>& candidates,
                        const vector<vector<int>>& cons,
                        const vector<int>& consNeed,
                        int R,
                        bool& progressed,
                        pair<int,int>& guess)
{
    progressed = false;
    guess = { -1, -1 };

    int n = (int)candidates.size();
    if (n <= 0 || n > 20) return false;
    if (R < 0 || R > n) return false;

    int N = rowDimension * colDimension;
    vector<int> local(N, -1);
    for (int i = 0; i < n; i++) {
        local[candidates[i]] = i;
    }

    vector<vector<int>> conVars;
    conVars.reserve(cons.size());
    for (const vector<int>& c : cons) {
        vector<int> lv;
        lv.reserve(c.size());
        for (int id : c) {
            int j = local[id];
            if (j >= 0) {
                lv.push_back(j);
            }
        }
        conVars.push_back(lv);
    }

    int C = (int)conVars.size();
    vector<vector<int>> varCons(n);
    for (int c = 0; c < C; c++) {
        for (int v : conVars[c]) {
            varCons[v].push_back(c);
        }
    }

    vector<int> order(n);
    for (int i = 0; i < n; i++) {
        order[i] = i;
    }
    sort(order.begin(), order.end(),
         [&](int a, int b){ return varCons[a].size() > varCons[b].size(); });

    vector<int> conCur(C, 0), conRem(C, 0);
    for (int c = 0; c < C; c++) {
        conRem[c] = (int)conVars[c].size();
    }

    vector<unsigned long long> layouts;
    layouts.reserve(4096);
    vector<char> assign(n, 0);
    long long nodes = 0;
    bool overflow = false;
    const long long NODECAP = 2000000LL;
    const size_t LAYOUTCAP = 200000;
    const double deadline = nowSeconds() + 0.25;

    struct Rec {
        int n, R; long long nodeCap; size_t layoutCap; double deadline;
        const vector<int>* order;
        const vector<vector<int>>* varCons;
        const vector<int>* conNeed;
        vector<int>* conCur;
        vector<int>* conRem;
        vector<char>* assign;
        vector<unsigned long long>* layouts;
        long long* nodes;
        bool* overflow;

        void go(int idx, int curMines) {
            if (*overflow) return;
            if (++(*nodes) > nodeCap) { 
                *overflow = true; return; 
            }
            if (((*nodes) & 0x3FFF) == 0 && nowSeconds() > deadline) { 
                *overflow = true; return; 
            }
            if (curMines > R || curMines + (n - idx) < R) return;

            if (idx == n) {
                if (curMines != R) return;
                unsigned long long mask = 0ULL;
                for (int v = 0; v < n; v++) {
                    if ((*assign)[v]) {
                        mask |= (1ULL << v);
                    }
                }
                layouts->push_back(mask);
                if (layouts->size() > layoutCap) {
                    *overflow = true;
                }
                return;
            }

            int v = (*order)[idx];
            for (int val = 0; val <= 1; val++) {
                bool ok = true;
                for (int c : (*varCons)[v]) {
                    (*conRem)[c]--;
                    if (val) (*conCur)[c]++;
                }
                for (int c : (*varCons)[v]) {
                    int cur = (*conCur)[c], rem = (*conRem)[c], nd = (*conNeed)[c];
                    if (cur > nd || cur + rem < nd) { ok = false; break; }
                }
                if (ok) {
                    (*assign)[v] = (char)val;
                    go(idx + 1, curMines + val);
                    (*assign)[v] = 0;
                }
                for (int c : (*varCons)[v]) {
                    (*conRem)[c]++;
                    if (val) (*conCur)[c]--;
                }
                if (*overflow) return;
            }
        }
    } rec;

    rec.n = n; rec.R = R; rec.nodeCap = NODECAP; rec.layoutCap = LAYOUTCAP; rec.deadline = deadline;
    rec.order = &order; rec.varCons = &varCons; rec.conNeed = &consNeed;
    rec.conCur = &conCur; rec.conRem = &conRem; rec.assign = &assign;
    rec.layouts = &layouts; rec.nodes = &nodes; rec.overflow = &overflow;
    rec.go(0, 0);

    if (overflow || layouts.empty()) return false;

    vector<int> mineCnt(n, 0);
    for (unsigned long long m : layouts) {
        for (int i = 0; i < n; i++) {
            if (m & (1ULL << i)) mineCnt[i]++;
        }
    }

    bool forced = false;
    for (int i = 0; i < n; i++) {
        int id = candidates[i];
        if (mineCnt[i] == 0) { markSafe(id / rowDimension, id % rowDimension); forced = true; }
        else if (mineCnt[i] == (int)layouts.size()) { markMine(id / rowDimension, id % rowDimension); forced = true; }
    }
    if (forced) {
        progressed = true;
        return true;
    }

    double minP = 2.0;
    for (int i = 0; i < n; i++) {
        minP = min(minP, (double)mineCnt[i] / (double)layouts.size());
    }

    int best = -1;
    double bestP = 2.0;
    double bestAmbiguity = 1e100;
    int bestZero = -1;
    const double BAND = 0.005;

    for (int i = 0; i < n; i++) {
        double p = (double)mineCnt[i] / (double)layouts.size();
        if (p > minP + BAND + 1e-12) continue;

        int id = candidates[i];
        int x = id / rowDimension, y = id % rowDimension;
        int buckets[9] = {0,0,0,0,0,0,0,0,0};
        int zeroSafe = 0;
        for (unsigned long long m : layouts) {
            if (m & (1ULL << i)) continue;
            int clue = 0;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!inB(nx, ny)) continue;
                int nid = cid(nx, ny);
                if (board[nid] == -1) clue++;
                else {
                    int lj = local[nid];
                    if (lj >= 0 && (m & (1ULL << lj))) clue++;
                }
            }
            buckets[clue]++;
            if (clue == 0) zeroSafe++;
        }

        double ambiguity = 0.0;
        for (int b = 0; b <= 8; b++) {
            ambiguity += (double)buckets[b] * (double)buckets[b];
        }
        if (p < bestP - 1e-12 ||
            (fabs(p - bestP) <= 1e-12 &&
             (ambiguity < bestAmbiguity - 1e-9 ||
              (fabs(ambiguity - bestAmbiguity) <= 1e-9 && zeroSafe > bestZero))))
        {
            best = i;
            bestP = p;
            bestAmbiguity = ambiguity;
            bestZero = zeroSafe;
        }
    }

    if (best < 0) return false;
    int id = candidates[best];
    guess = { id / rowDimension, id % rowDimension };
    return true;
}

//probability engine: compute mine probabilities, mark forced tiles,
//otherwise return the lowest-probability tile to uncover.
pair<int,int> MyAI::solveCSP(bool& progressed) {
    progressed = false;
    int N = rowDimension * colDimension;

    //collect covered, non-safe candidate tiles.
    vector<int> candidates;
    vector<char> isCand(N, 0);
    for (int id = 0; id < N; id++) {
        if (board[id] == -2 && !safeMark[id]) { 
            candidates.push_back(id); isCand[id] = 1; 
        }
    }

    if (candidates.empty()) {
        return { -1, -1 };
    }

    int R = totalMines - knownMines;   //mines still hidden among candidates

    //global trivial cases.
    if (R <= 0) {
        for (int id : candidates) {
            markSafe(id / rowDimension, id % rowDimension);
        }
        progressed = true; return { -1, -1 };
    }
    if (R >= (int)candidates.size()) {
        for (int id : candidates) {
            markMine(id / rowDimension, id % rowDimension);
        }
        progressed = true; return { -1, -1 };
    }

    //build constraints; record which candidates are on the frontier.
    vector<vector<int>> cons;    //candidate ids per constraint
    vector<int> consNeed;
    vector<char> isFront(N, 0);
    for (int x = 0; x < colDimension; x++) {
        for (int y = 0; y < rowDimension; y++) {
            int v = board[cid(x, y)];
            if (v < 0) continue;
            int mines = 0;
            vector<int> cov;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!inB(nx, ny)) continue;
                int id = cid(nx, ny);
                if (board[id] == -1) mines++;
                else if (isCand[id]) cov.push_back(id);
            }
            if (cov.empty()) continue;
            int nd = v - mines;
            if (nd < 0) continue;
            for (int id : cov) {
                isFront[id] = 1;
            }
            cons.push_back(cov);
            consNeed.push_back(nd);
        }
    }

    pair<int,int> exactGuess;
    if (endgameExact(candidates, cons, consNeed, R, progressed, exactGuess)) {
        if (progressed) {
            return { -1, -1 };
        }
        return exactGuess;
    }

    //frontier and sea partition.
    vector<int> frontier, sea;
    for (int id : candidates) {
        (isFront[id] ? frontier : sea).push_back(id);
    }
    int seaSize = (int)sea.size();

    //no frontier: pure sea guess (corner-preferred). 
    if (frontier.empty()) {
        //every sea tile has equal probability R/seaSize
        int best = -1, bestScore = 1e9;
        for (int id : sea) {
            int x = id / rowDimension, y = id % rowDimension;
            int nb = 0;
            for (int k = 0; k < 8; k++) {
                if (inB(x + DX[k], y + DY[k])) nb++;
            }
            if (nb < bestScore) { 
                bestScore = nb; 
                best = id;
            }
        }
        if (best < 0) best = sea[0];
        return { best / rowDimension, best % rowDimension };
    }

    //union-find over frontier tiles linked by shared constraints.
    vector<int> parent(N);
    for (int id : frontier) {
        parent[id] = id;
    }
    function<int(int)> find = [&](int a){ while (parent[a] != a){ parent[a] = parent[parent[a]]; a = parent[a]; } return a; };
    auto uni = [&](int a, int b){ a = find(a); b = find(b); if (a != b) parent[a] = b; };
    for (auto& cv : cons) {
        for (size_t i = 1; i < cv.size(); i++) uni(cv[0], cv[i]);
    }

    //group frontier vars by component root.
    vector<int> roots;
    vector<vector<int>> compVars; //ids per component
    vector<int> rootIndex(N, -1);
    for (int id : frontier) {
        int r = find(id);
        if (rootIndex[r] == -1) { 
            rootIndex[r] = (int)roots.size(); roots.push_back(r); compVars.push_back({}); 
        }
        compVars[rootIndex[r]].push_back(id);
    }
    int numComp = (int)roots.size();

    //local index of each frontier id within its component
    vector<int> localIdx(N, -1);
    for (int c = 0; c < numComp; c++) {
        for (size_t j = 0; j < compVars[c].size(); j++) {
            localIdx[compVars[c][j]] = (int)j;
        }
    }

    //distribute constraints to components (translate ids -> local indices).
    vector<vector<vector<int>>> compCons(numComp);
    vector<vector<int>> compNeed(numComp);
    for (size_t ci = 0; ci < cons.size(); ci++) {
        int comp = rootIndex[find(cons[ci][0])];
        vector<int> lv;
        for (int id : cons[ci]) {
            lv.push_back(localIdx[id]);
        }
        compCons[comp].push_back(lv);
        compNeed[comp].push_back(consNeed[ci]);
    }

    //enumerate each component.
    //caps keep worst-case effort bounded; the wall-clock deadline is a hard
    //safety net so a single pathological board can never blow a time limit.
    const int SIZECAP = 55;
    const long long NODECAP = 80000000LL;
    const double deadline = nowSeconds() + 2.5; // <= ~2.5s per decision
    vector<vector<double>> compCnt(numComp); // cntByK
    vector<vector<vector<double>>>  compMine(numComp); //mineByK
    vector<char> solved(numComp, 0);
    vector<vector<double>> heurP(numComp); //fallback per-var prob

    for (int c = 0; c < numComp; c++) {
        int n = (int)compVars[c].size();
        bool ok = false;
        if (n <= SIZECAP) {
            ok = enumerateComp(n, compCons[c], compNeed[c], R, NODECAP, deadline, compCnt[c], compMine[c]);
        }

        if (ok) {
            solved[c] = 1;
        }
        else {
            //heuristic per-var probability from local constraints
            heurP[c].assign(n, 0.0);
            vector<int> deg(n, 0);
            for (size_t k = 0; k < compCons[c].size(); k++) {
                double frac = (double)compNeed[c][k] / (double)compCons[c][k].size();
                for (int lv : compCons[c][k]) {
                    heurP[c][lv] += frac; deg[lv]++;
                }
            }
            for (int j = 0; j < n; j++) {
                heurP[c][j] = deg[j] ? heurP[c][j] / deg[j] : (double)R / candidates.size();
            }
        }
    }

    //probabilities to be filled in.
    vector<double> probId(N, -1.0);   //mine probability per frontier id
    double seaProb = (seaSize > 0) ? (double)R / candidates.size() : -1.0;

    bool allSolved = true;
    for (int c = 0; c < numComp; c++) {
        if (!solved[c]) allSolved = false;
    }

    if (allSolved) {
        //exact global coupling via mine-count convolution.
        //prefix/suffix products of the per-component mine-count polynomials.
        vector<vector<double>> pre(numComp + 1), suf(numComp + 1);
        pre[0] = {1.0};
        for (int c = 0; c < numComp; c++) {
            const vector<double>& a = pre[c];
            const vector<double>& b = compCnt[c];
            vector<double> r(a.size() + b.size() - 1, 0.0);
            for (size_t i = 0; i < a.size(); i++) if (a[i] != 0) {
                for (size_t j = 0; j < b.size(); j++) {
                    r[i + j] += a[i] * b[j];
                }
            }
            pre[c + 1] = std::move(r);
        }
        suf[numComp] = {1.0};
        for (int c = numComp - 1; c >= 0; c--) {
            const vector<double>& a = suf[c + 1];
            const vector<double>& b = compCnt[c];
            vector<double> r(a.size() + b.size() - 1, 0.0);
            for (size_t i = 0; i < a.size(); i++) if (a[i] != 0) {
                for (size_t j = 0; j < b.size(); j++) {
                    r[i + j] += a[i] * b[j];
                }
            }
            suf[c] = std::move(r);
        }
        vector<double>& total = pre[numComp];   //total[T] = #combos with T frontier mines

        //normalised binomial weights for choosing (R-T) mines in the sea.
        //logBin[m] = log C(seaSize, m), m in [0, seaSize].
        vector<double> logBin;
        double maxLog = 0.0;
        if (seaSize > 0) {
            logBin.assign(seaSize + 1, 0.0);
            double lf_n = lgamma((double)seaSize + 1.0);
            bool first = true;
            for (int m = 0; m <= seaSize; m++) {
                logBin[m] = lf_n - lgamma((double)m + 1.0) - lgamma((double)(seaSize - m) + 1.0);
                if (first || logBin[m] > maxLog) { 
                    maxLog = logBin[m]; first = false; 
                }
            }
        }
        auto binW = [&](int m) -> double {
            if (seaSize == 0) return (m == 0) ? 1.0 : 0.0;
            if (m < 0 || m > seaSize) return 0.0;
            return exp(logBin[m] - maxLog);
        };

        //partition function Z.
        double Z = 0.0;
        for (int T = 0; T < (int)total.size(); T++) {
            if (total[T] != 0) {
                Z += total[T] * binW(R - T);
            }
        }

        if (Z <= 0.0) {
            //numerical fallback: pick any candidate
            int id = candidates[0];
            return { id / rowDimension, id % rowDimension };
        }

        //sea probability.
        if (seaSize > 0) {
            double num = 0.0;
            for (int T = 0; T < (int)total.size(); T++) 
                if (total[T] != 0) {
                    int s = R - T;
                    if (s < 0 || s > seaSize) continue;
                    num += total[T] * binW(s) * ((double)s / seaSize);
                }
            seaProb = num / Z;
        }

        //per-component, per-var probabilities.
        for (int c = 0; c < numComp; c++) {
            //G = product of all components except c
            const vector<double>& A = pre[c];
            const vector<double>& B = suf[c + 1];
            vector<double> G(A.size() + B.size() - 1, 0.0);
            for (size_t i = 0; i < A.size(); i++) if (A[i] != 0) {
                for (size_t j = 0; j < B.size(); j++) {
                    G[i + j] += A[i] * B[j];
                }
            }

            int n = (int)compVars[c].size();
            //W[k] = sum_u G[u] * binW(R - k - u)
            vector<double> W(n + 1, 0.0);
            for (int k = 0; k <= n; k++) {
                double w = 0.0;
                for (int u = 0; u < (int)G.size(); u++) {
                    if (G[u] != 0) {
                        w += G[u] * binW(R - k - u);
                    }
                }
                W[k] = w;
            }
            for (int j = 0; j < n; j++) {
                double num = 0.0;
                for (int k = 0; k <= n; k++) {
                    if (compMine[c][k][j] != 0) {
                        num += compMine[c][k][j] * W[k];
                    }
                }
                probId[compVars[c][j]] = num / Z;
            }
        }
    }
    else {
        //fallback: per-component local probabilities (no global coupling).
        double expFrontierMines = 0.0;
        for (int c = 0; c < numComp; c++) {
            int n = (int)compVars[c].size();
            if (solved[c]) {
                double tot = 0.0, em = 0.0;
                for (int k = 0; k < (int)compCnt[c].size(); k++) {
                    tot += compCnt[c][k]; em += k * compCnt[c][k]; 
                }
                if (tot <= 0) tot = 1.0;
                for (int j = 0; j < n; j++) {
                    double num = 0.0;
                    for (int k = 0; k < (int)compCnt[c].size(); k++) {
                        num += compMine[c][k][j];
                    }
                    probId[compVars[c][j]] = num / tot;
                }
                expFrontierMines += em / tot;
            }
            else {
                for (int j = 0; j < n; j++) {
                    probId[compVars[c][j]] = heurP[c][j];
                    expFrontierMines += heurP[c][j];
                }
            }
        }
        if (seaSize > 0) {
            double s = ((double)R - expFrontierMines) / seaSize;
            if (s < 0) s = 0; 
            if (s > 1) s = 1;
            seaProb = s;
        }
    }

    //mark forced tiles (probability 0 -> safe, 1 -> mine). 
    const double EPS = 1e-9;
    bool forced = false;
    for (int id : frontier) {
        double p = probId[id];
        if (p < 0) continue;
        if (p <= EPS) {
            markSafe(id / rowDimension, id % rowDimension); forced = true;
        }
        else if (p >= 1.0 - EPS) { 
            markMine(id / rowDimension, id % rowDimension); forced = true; 
        }
    }
    if (seaSize > 0) {
        if (seaProb <= EPS) { 
            for (int id : sea) {
                markSafe(id / rowDimension, id % rowDimension); 
                forced = true; 
            }
        }
        else if (seaProb >= 1.0 - EPS) { for (int id : sea) markMine(id / rowDimension, id % rowDimension); forced = true; }
    }
    if (forced) { 
        progressed = true; return { -1, -1 }; 
    }

    //no certainty: choose the guess. 
    //primary objective is the minimum mine probability. Among candidates whose
    //probability is within GUESS_MARGIN of the minimum (an effective tie), we
    //prefer the most *informative* tile -- the one bordering the most revealed
    //numbers -- since its revealed value constrains the most cells and tends to
    //reduce the number of future guesses. Measured effect: Expert ~+0.8% over a
    //5000-world set with no regression on Beginner/Intermediate. A wider margin
    //or a "maximise blank-cascade" objective both tested *worse* (they trade
    //away too much immediate safety), so the band is kept tight.
    const double GUESS_MARGIN = 0.01;

    double minP = 2.0;
    double frontierMin = 2.0;
    for (int id : frontier) {
        double p = probId[id];
        if (p < 0) continue;
        if (p < frontierMin) frontierMin = p;
        if (p < minP) minP = p;
    }
    if (seaSize > 0 && seaProb >= 0 && seaProb < minP) minP = seaProb;

    auto revealedNeighbours = [&](int id) {
        int x = id / rowDimension, y = id % rowDimension, r = 0;
        for (int k = 0; k < 8; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (inB(nx, ny) && board[cid(nx, ny)] >= 0) r++;
        }
        return r;
    };

    int bestId = -1; double bestP = 2.0; int bestInfo = -1;
    auto consider = [&](int id, double p, int info) {
        if (p > minP + GUESS_MARGIN + 1e-12) return;          // outside the tie band
        if (info > bestInfo || (info == bestInfo && p < bestP - 1e-12))
        { bestInfo = info; bestP = p; bestId = id; }
    };

    for (int id : frontier) {
        double p = probId[id];
        if (p < 0) continue;
        consider(id, p, revealedNeighbours(id));
    }
    if (seaSize > 0 && seaProb >= 0) {
        //representative sea tile. If sea is strictly safer than the frontier,
        //prefer a cell deep in the unconstrained area over a boundary cell.
        int rep = -1, 
        bestSeaScore = -1000000000;
        for (int id : sea) {
            int x = id / rowDimension, y = id % rowDimension;
            int nb = 0, adjFront = 0, adjSea = 0, nearestReveal = 1000;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!inB(nx, ny)) continue;
                nb++;
                int nid = cid(nx, ny);
                if (isFront[nid]) adjFront++;
                else if (board[nid] == -2 && !safeMark[nid]) adjSea++;
            }
            for (int rx = 0; rx < colDimension; rx++) {
                for (int ry = 0; ry < rowDimension; ry++){
                    if (board[cid(rx, ry)] >= 0) {
                        int d = max(abs(rx - x), abs(ry - y));
                        if (d < nearestReveal) nearestReveal = d;
                    }
                }
            }
            int score = nearestReveal * 32 + adjSea * 4 - adjFront * 24 - nb;
            if (score > bestSeaScore) {
                bestSeaScore = score; rep = id;
            }
        }
        int seaInfo = (seaProb < frontierMin - 1e-12) ? bestSeaScore : 0;
        if (rep >= 0) consider(rep, seaProb, seaInfo); //sea borders no numbers
    }

    if (bestId < 0) {
        int id = candidates[0];
        return { id / rowDimension, id % rowDimension };
    }
    return { bestId / rowDimension, bestId % rowDimension };
}
