#include "board.hpp"
#include "alphabeta.hpp"
#include "mcts.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>

struct TestPosition {
    std::array<int,14> board;
    int side;
};

static std::vector<TestPosition> loadPositions(const std::string& filename) {
    std::vector<TestPosition> positions;
    std::ifstream f(filename);
    if (!f.is_open()) {
        // Use default test positions
        // Starting position
        TestPosition tp;
        tp.board = {4,4,4,4,4,4,0,4,4,4,4,4,4,0};
        tp.side  = 0;
        positions.push_back(tp);

        // Mid-game position
        tp.board = {0,2,6,0,3,1,8,0,4,2,0,5,3,12};
        tp.side  = 1;
        positions.push_back(tp);

        // Late-game position
        tp.board = {0,0,1,0,0,2,20,0,0,3,0,0,1,21};
        tp.side  = 0;
        positions.push_back(tp);

        return positions;
    }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        TestPosition tp;
        std::istringstream ss(line);
        for (int i = 0; i < 14; ++i) ss >> tp.board[i];
        ss >> tp.side;
        positions.push_back(tp);
    }
    return positions;
}

static void printTable(const std::string& algo,
                       const std::vector<int>& threads,
                       const std::vector<std::vector<double>>& times,
                       const std::vector<std::vector<long long>>& extra1,
                       const std::vector<std::vector<long long>>& extra2,
                       const std::string& e1name, const std::string& e2name)
{
    std::cout << "\n=== " << algo << " ===\n";
    std::cout << std::setw(8) << "Threads"
              << std::setw(12) << "T(p) [ms]"
              << std::setw(12) << "S(p)"
              << std::setw(12) << "E(p)"
              << std::setw(14) << e1name
              << std::setw(14) << e2name << "\n";
    std::cout << std::string(72, '-') << "\n";

    for (int i = 0; i < (int)threads.size(); ++i) {
        double avgTime = 0;
        for (double t : times[i]) avgTime += t;
        avgTime /= times[i].size();

        double t1 = 0;
        for (double t : times[0]) t1 += t;
        t1 /= times[0].size();

        double sp = t1 / avgTime;
        double ep = sp / threads[i];

        long long avgE1 = 0, avgE2 = 0;
        for (auto v : extra1[i]) avgE1 += v;
        for (auto v : extra2[i]) avgE2 += v;
        if (!extra1[i].empty()) avgE1 /= extra1[i].size();
        if (!extra2[i].empty()) avgE2 /= extra2[i].size();

        std::cout << std::setw(8)  << threads[i]
                  << std::setw(12) << std::fixed << std::setprecision(1) << avgTime
                  << std::setw(12) << std::setprecision(3) << sp
                  << std::setw(12) << std::setprecision(3) << ep
                  << std::setw(14) << avgE1
                  << std::setw(14) << avgE2 << "\n";
    }
}

int main(int argc, char* argv[]) {
    std::string algo = "alphabeta";
    int depth        = 8;
    int sims         = 10000;
    std::string posFile = "";

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--algo")        && i+1 < argc) algo   = argv[++i];
        if (!strcmp(argv[i], "--depth")       && i+1 < argc) depth  = atoi(argv[++i]);
        if (!strcmp(argv[i], "--simulations") && i+1 < argc) sims   = atoi(argv[++i]);
        if (!strcmp(argv[i], "--positions")   && i+1 < argc) posFile= argv[++i];
    }

    auto positions = loadPositions(posFile);
    std::cout << "Loaded " << positions.size() << " test positions\n";
    std::cout << "Algorithm: " << algo << "\n";

    std::vector<int> threadCounts = {1, 2, 4, 8};

    if (algo == "alphabeta") {
        std::vector<int> depths = {depth};
        if (depth != 12) depths.push_back(12);

        for (int d : depths) {
            std::cout << "\n--- Alpha-Beta depth=" << d << " ---\n";
            std::vector<std::vector<double>>    times(threadCounts.size());
            std::vector<std::vector<long long>> nodes(threadCounts.size());
            std::vector<std::vector<long long>> prunes(threadCounts.size());

            for (int ti = 0; ti < (int)threadCounts.size(); ++ti) {
                int T = threadCounts[ti];
                omp_set_num_threads(T);
                for (auto& pos : positions) {
                    Board b(pos.board, pos.side);
                    ABResult r = alphaBetaRoot(b, d, T, T == 1);
                    times[ti].push_back(r.stats.elapsed * 1000.0);
                    nodes[ti].push_back(r.stats.nodes);
                    prunes[ti].push_back(r.stats.prunes);
                    if (T == 1) {
                        // Verify: minimax must agree
                        ABResult mm = minimaxRoot(b, std::min(d, 6));
                        ABResult ab = alphaBetaSeqRoot(b, std::min(d, 6));
                        if (mm.bestMove != ab.bestMove) {
                            std::cerr << "[WARN] Minimax/AlphaBeta disagree at depth "
                                      << std::min(d,6) << ": mm=" << mm.bestMove
                                      << " ab=" << ab.bestMove << "\n";
                        }
                    }
                }
            }
            printTable("Alpha-Beta (depth=" + std::to_string(d) + ")",
                       threadCounts, times, nodes, prunes, "Nodes", "Prunes");
        }
    } else {
        // MCTS
        std::vector<int> simsList = {sims};
        if (sims != 100000) simsList.push_back(100000);

        // Get reference Alpha-Beta moves for coincidence rate
        std::vector<int> abMoves(positions.size(), -1);
        for (int pi = 0; pi < (int)positions.size(); ++pi) {
            Board b(positions[pi].board, positions[pi].side);
            ABResult r = alphaBetaSeqRoot(b, 8);
            abMoves[pi] = r.bestMove;
        }

        for (int s : simsList) {
            std::cout << "\n--- MCTS simulations=" << s << " ---\n";
            std::vector<std::vector<double>>    times(threadCounts.size());
            std::vector<std::vector<long long>> rollouts(threadCounts.size());
            std::vector<std::vector<long long>> depths(threadCounts.size());

            for (int ti = 0; ti < (int)threadCounts.size(); ++ti) {
                int T = threadCounts[ti];
                int matches = 0;
                for (int pi = 0; pi < (int)positions.size(); ++pi) {
                    Board b(positions[pi].board, positions[pi].side);
                    MCTSResult r = mcts(b, s, T, false);
                    times[ti].push_back(r.stats.elapsed * 1000.0);
                    rollouts[ti].push_back((long long)r.stats.rollouts);
                    int avgD = r.stats.treeDepthN > 0 ?
                        (int)(r.stats.treeDepthSum / r.stats.treeDepthN) : 0;
                    depths[ti].push_back((long long)avgD);
                    if (r.bestMove == abMoves[pi]) matches++;
                }
                double coinc = 100.0 * matches / positions.size();
                std::cout << "  T=" << T << " coincidence with AB: "
                          << std::fixed << std::setprecision(1) << coinc << "%\n";
            }
            printTable("MCTS (sims=" + std::to_string(s) + ")",
                       threadCounts, times, rollouts, depths,
                       "Rollouts", "AvgDepth");
        }
    }

    return 0;
}
