#pragma once

#include "board.hpp"

#include <cmath>
#include <memory>
#include <vector>
#include <random>
#include <atomic>
#include <omp.h>
#include <limits>
#include <numeric>
#include <algorithm>

static constexpr double UCT_C = 1.41421356;

// ─────────────────────────────────────────────────────────────────────────────
// Node
// ─────────────────────────────────────────────────────────────────────────────
struct MCTSNode {

    Board board;

    int move;

    MCTSNode* parent;

    std::vector<std::unique_ptr<MCTSNode>> children;

    std::vector<int> untriedMoves;

    std::atomic<double> wins{0.0};

    std::atomic<int> visits{0};

    int forSide;

    MCTSNode(
        const Board& b,
        int mv,
        MCTSNode* par,
        int fs
    )
        : board(b),
          move(mv),
          parent(par),
          forSide(fs)
    {
        untriedMoves = b.legalMoves();
    }

    bool isFullyExpanded() const {
        return untriedMoves.empty();
    }

    bool isLeaf() const {
        return children.empty();
    }

    double uct(double parentVisits) const {

        int v = visits.load();

        if (v == 0)
            return std::numeric_limits<double>::infinity();

        double w = wins.load();

        return
            (w / v)
            + UCT_C * std::sqrt(std::log(parentVisits) / v);
    }

    MCTSNode* bestChild() const {

        MCTSNode* best = nullptr;

        double bestU = -1e18;

        static thread_local std::mt19937 rng(
            std::random_device{}()
        );

        for (auto& c : children) {

            double u = c->uct((double)visits.load());

            if (u > bestU) {

                bestU = u;

                best = c.get();
            }
            else if (std::abs(u - bestU) < 1e-9) {

                std::uniform_int_distribution<int> d(0,1);

                if (d(rng))
                    best = c.get();
            }
        }

        return best;
    }

    MCTSNode* mostVisitedChild() const {

        MCTSNode* best = nullptr;

        int bestV = -1;

        for (auto& c : children) {

            int v = c->visits.load();

            if (v > bestV) {

                bestV = v;

                best = c.get();
            }
        }

        return best;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Rollout
// ─────────────────────────────────────────────────────────────────────────────
static int rollout(Board b, std::mt19937& rng)
{
    int steps = 0;

    while (!b.isTerminal() && steps < 200) {

        auto moves = b.legalMoves();

        if (moves.empty())
            break;

        std::uniform_int_distribution<int> dist(
            0,
            (int)moves.size() - 1
        );

        b = b.applyMove(moves[dist(rng)]);

        ++steps;
    }

    return b.winner();
}

// ─────────────────────────────────────────────────────────────────────────────
// Stats
// ─────────────────────────────────────────────────────────────────────────────
struct MCTSStats {

    int rollouts = 0;

    double treeDepthSum = 0;

    int treeDepthN = 0;

    int threads = 1;

    double elapsed = 0.0;

    double winRate = 0.0;
};

struct MCTSResult {

    int bestMove = -1;

    double winRate = 0.0;

    MCTSStats stats;
};

// ─────────────────────────────────────────────────────────────────────────────
// Backprop
// ─────────────────────────────────────────────────────────────────────────────
static void backprop(
    MCTSNode* node,
    int winner,
    int forSide
)
{
    while (node) {

        node->visits.fetch_add(1);

        if (winner == forSide) {

            node->wins.store(
                node->wins.load() + 1.0
            );
        }
        else if (winner == -1) {

            node->wins.store(
                node->wins.load() + 0.5
            );
        }

        node = node->parent;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// One iteration
// ─────────────────────────────────────────────────────────────────────────────
static void mctsIteration(
    MCTSNode* root,
    std::mt19937& rng,
    MCTSStats& stats
)
{
    MCTSNode* node = root;

    int depth = 0;

    // Selection
    while (
        !node->board.isTerminal()
        && node->isFullyExpanded()
        && !node->isLeaf()
    ) {
        node = node->bestChild();

        ++depth;
    }

    // Expansion
    if (
        !node->board.isTerminal()
        && !node->untriedMoves.empty()
    ) {

        std::uniform_int_distribution<int> d(
            0,
            (int)node->untriedMoves.size() - 1
        );

        int idx = d(rng);

        int mv = node->untriedMoves[idx];

        node->untriedMoves.erase(
            node->untriedMoves.begin() + idx
        );

        Board nextBoard = node->board.applyMove(mv);

        node->children.push_back(
            std::make_unique<MCTSNode>(
                nextBoard,
                mv,
                node,
                root->forSide
            )
        );

        node = node->children.back().get();

        ++depth;
    }

    stats.treeDepthSum += depth;

    stats.treeDepthN++;

    // Simulation
    int winner = rollout(node->board, rng);

    stats.rollouts++;

    // Backprop
    backprop(node, winner, root->forSide);
}

// ─────────────────────────────────────────────────────────────────────────────
// Main MCTS
// ─────────────────────────────────────────────────────────────────────────────
MCTSResult mcts(
    const Board& rootBoard,
    int simulations,
    int numThreads,
    bool leafParallel = false
)
{
    int forSide = rootBoard.side;

    double t0 = omp_get_wtime();

    if (rootBoard.isTerminal())
        return {};

    MCTSNode root(
        rootBoard,
        -1,
        nullptr,
        forSide
    );

    MCTSStats stats;

    stats.threads = numThreads;

    omp_set_num_threads(numThreads);

    #pragma omp parallel
    {
        std::mt19937 rng(
            std::random_device{}()
            + omp_get_thread_num()
        );

        #pragma omp for
        for (int i = 0; i < simulations; ++i) {

            #pragma omp critical
            {
                mctsIteration(
                    &root,
                    rng,
                    stats
                );
            }
        }
    }

    double elapsed = omp_get_wtime() - t0;

    stats.elapsed = elapsed;

    MCTSNode* best = root.mostVisitedChild();

    MCTSResult res;

    if (best) {

        res.bestMove = best->move;

        int v = best->visits.load();

        res.winRate =
            v > 0
            ? best->wins.load() / v
            : 0.0;
    }

    res.stats = stats;

    res.stats.winRate = res.winRate;

    return res;
}