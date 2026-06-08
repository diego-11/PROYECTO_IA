#pragma once

#include "board.hpp"

#include <limits>
#include <atomic>
#include <omp.h>
#include <random>
#include <vector>
#include <algorithm>

struct AlphaBetaStats {
    long long nodes   = 0;
    long long prunes  = 0;
    int       threads = 1;
    double    elapsed = 0.0;
};

// ─────────────────────────────────────────────────────────────────────────────
// Sequential Minimax (no pruning)
// ─────────────────────────────────────────────────────────────────────────────
static double minimaxSeq(const Board& b, int depth, int forSide,
                         long long& nodes)
{
    ++nodes;

    if (depth == 0 || b.isTerminal())
        return b.evaluate(forSide);

    auto moves = b.legalMoves();

    if (moves.empty())
        return b.evaluate(forSide);

    bool isMax = (b.side == forSide);

    double best =
        isMax
        ? -1e18
        :  1e18;

    for (int m : moves) {

        Board next = b.applyMove(m);

        double val =
            minimaxSeq(
                next,
                depth - 1,
                forSide,
                nodes
            );

        if (isMax)
            best = std::max(best, val);
        else
            best = std::min(best, val);
    }

    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sequential Alpha-Beta
// ─────────────────────────────────────────────────────────────────────────────
static double alphaBetaSeq(
    const Board& b,
    int depth,
    double alpha,
    double beta,
    int forSide,
    long long& nodes,
    long long& prunes
)
{
    ++nodes;

    if (depth == 0 || b.isTerminal())
        return b.evaluate(forSide);

    auto moves = b.legalMoves();

    if (moves.empty())
        return b.evaluate(forSide);

    bool isMax = (b.side == forSide);

    if (isMax) {

        double best = -1e18;

        for (int m : moves) {

            Board next = b.applyMove(m);

            double val =
                alphaBetaSeq(
                    next,
                    depth - 1,
                    alpha,
                    beta,
                    forSide,
                    nodes,
                    prunes
                );

            best  = std::max(best, val);
            alpha = std::max(alpha, best);

            if (beta <= alpha) {
                ++prunes;
                break;
            }
        }

        return best;
    }
    else {

        double best = 1e18;

        for (int m : moves) {

            Board next = b.applyMove(m);

            double val =
                alphaBetaSeq(
                    next,
                    depth - 1,
                    alpha,
                    beta,
                    forSide,
                    nodes,
                    prunes
                );

            best = std::min(best, val);
            beta = std::min(beta, best);

            if (beta <= alpha) {
                ++prunes;
                break;
            }
        }

        return best;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Result
// ─────────────────────────────────────────────────────────────────────────────
struct ABResult {

    int bestMove = -1;

    double bestVal = -1e18;

    AlphaBetaStats stats;
};

// ─────────────────────────────────────────────────────────────────────────────
// Root Parallel Alpha-Beta
// ─────────────────────────────────────────────────────────────────────────────
ABResult alphaBetaRoot(
    const Board& root,
    int depth,
    int numThreads,
    bool sequential = false
)
{
    auto moves = root.legalMoves();

    if (moves.empty())
        return {};

    int forSide = root.side;

    int n = (int)moves.size();

    std::vector<double> vals(n, -1e18);

    std::vector<long long> nodesCnt(n, 0);
    std::vector<long long> prunesCnt(n, 0);

    double t0 = omp_get_wtime();

    if (sequential || numThreads == 1) {

        for (int i = 0; i < n; ++i) {

            Board next = root.applyMove(moves[i]);

            double alpha = -1e18;
            double beta  =  1e18;

            vals[i] =
                alphaBetaSeq(
                    next,
                    depth - 1,
                    alpha,
                    beta,
                    forSide,
                    nodesCnt[i],
                    prunesCnt[i]
                );

            nodesCnt[i]++;
        }
    }
    else {

        omp_set_num_threads(numThreads);

        #pragma omp parallel for schedule(dynamic, 1)
        for (int i = 0; i < n; ++i) {

            Board next = root.applyMove(moves[i]);

            double alpha = -1e18;
            double beta  =  1e18;

            vals[i] =
                alphaBetaSeq(
                    next,
                    depth - 1,
                    alpha,
                    beta,
                    forSide,
                    nodesCnt[i],
                    prunesCnt[i]
                );

            nodesCnt[i]++;
        }
    }

    double elapsed = omp_get_wtime() - t0;

    ABResult res;

    res.stats.elapsed = elapsed;

    res.stats.threads =
        sequential
        ? 1
        : numThreads;

    std::vector<int> bestMoves;

    for (int i = 0; i < n; ++i) {

        res.stats.nodes  += nodesCnt[i];
        res.stats.prunes += prunesCnt[i];

        if (vals[i] > res.bestVal) {

            res.bestVal = vals[i];

            bestMoves.clear();

            bestMoves.push_back(moves[i]);
        }
        else if (std::abs(vals[i] - res.bestVal) < 1e-9) {

            bestMoves.push_back(moves[i]);
        }
    }

    if (!bestMoves.empty()) {

        std::random_device rd;

        std::mt19937 rng(rd());

        std::uniform_int_distribution<int> dist(
            0,
            (int)bestMoves.size() - 1
        );

        res.bestMove = bestMoves[dist(rng)];
    }

    return res;
}

// ─────────────────────────────────────────────────────────────────────────────
// Convenience wrappers
// ─────────────────────────────────────────────────────────────────────────────
ABResult alphaBetaSeqRoot(const Board& root, int depth)
{
    return alphaBetaRoot(root, depth, 1, true);
}

ABResult minimaxRoot(const Board& root, int depth)
{
    auto moves = root.legalMoves();

    if (moves.empty())
        return {};

    int forSide = root.side;

    int n = (int)moves.size();

    ABResult res;

    res.stats.threads = 1;

    double t0 = omp_get_wtime();

    for (int i = 0; i < n; ++i) {

        Board next = root.applyMove(moves[i]);

        long long nodes = 0;

        double val =
            minimaxSeq(
                next,
                depth - 1,
                forSide,
                nodes
            );

        res.stats.nodes += nodes + 1;

        if (val > res.bestVal) {

            res.bestVal = val;

            res.bestMove = moves[i];
        }
    }

    res.stats.elapsed = omp_get_wtime() - t0;

    return res;
}