#pragma once
#include <array>
#include <vector>
#include <string>
#include <stdexcept>
#include <sstream>
#include <algorithm>
#include <random>


// Kalah(6,4): 6 pits per side, 4 seeds per pit

struct Board {
    std::array<int, 14> pits;
    int side; // 0 or 1: whose turn

    static constexpr int KALAHA_0 = 6;
    static constexpr int KALAHA_1 = 13;

    Board() : side(0) {
        pits.fill(4);
        pits[KALAHA_0] = 0;
        pits[KALAHA_1] = 0;
    }

    Board(const std::array<int, 14>& p, int s) : pits(p), side(s) {}

    bool isTerminal() const {
        int sum0 = 0, sum1 = 0;
        for (int i = 0; i < 6; ++i)  sum0 += pits[i];
        for (int i = 7; i < 13; ++i) sum1 += pits[i];
        return sum0 == 0 || sum1 == 0;
    }

    int pitStart(int player) const {
        return player == 0 ? 0 : 7;
    }

    int kalaha(int player) const {
        return player == 0 ? KALAHA_0 : KALAHA_1;
    }

    int oppPit(int pit) const {
        if ((pit >= 0 && pit <= 5) || (pit >= 7 && pit <= 12))
            return 12 - pit;
        return -1;
    }

    // Randomized legal moves
    std::vector<int> legalMoves() const {

        std::vector<int> moves;

        int start = pitStart(side);

        for (int i = 0; i < 6; ++i) {

            int pit = start + i;

            if (pits[pit] > 0)
                moves.push_back(pit);
        }

        std::sort(moves.begin(), moves.end());

        return moves;
    }

    Board applyMove(int pit) const {
        Board next = *this;

        int seeds = next.pits[pit];
        next.pits[pit] = 0;

        int myKalaha = kalaha(side);
        int oppKalaha = (side == 0) ? KALAHA_1 : KALAHA_0;

        int idx = pit;

        while (seeds > 0) {
            idx = (idx + 1) % 14;

            if (idx == oppKalaha)
                continue;

            next.pits[idx]++;
            seeds--;
        }

        bool extraTurn = (idx == myKalaha);

        int pitStart_ = pitStart(side);

        bool inOwnSide =
            (idx >= pitStart_ && idx < pitStart_ + 6);

        // Capture
        if (!extraTurn && inOwnSide && next.pits[idx] == 1) {
            int opp = oppPit(idx);

            if (opp != -1 && next.pits[opp] > 0) {
                next.pits[myKalaha] += next.pits[opp] + 1;

                next.pits[idx] = 0;
                next.pits[opp] = 0;
            }
        }

        // Terminal collection
        if (next.isTerminal()) {
            for (int i = 0; i < 6; ++i) {
                next.pits[KALAHA_0] += next.pits[i];
                next.pits[i] = 0;
            }

            for (int i = 7; i < 13; ++i) {
                next.pits[KALAHA_1] += next.pits[i];
                next.pits[i] = 0;
            }
        }

        if (extraTurn)
            next.side = side;
        else
            next.side = 1 - side;

        return next;
    }

    // Improved heuristic
    double evaluate(int forSide, double alpha = 0.3) const {

        int myK   = pits[kalaha(forSide)];
        int oppK  = pits[kalaha(1 - forSide)];

        int myStart  = pitStart(forSide);
        int oppStart = pitStart(1 - forSide);

        int mySeeds = 0;
        int oppSeeds = 0;

        for (int i = myStart; i < myStart + 6; ++i)
            mySeeds += pits[i];

        for (int i = oppStart; i < oppStart + 6; ++i)
            oppSeeds += pits[i];

        double score =
            (myK - oppK)
            + alpha * (mySeeds - oppSeeds);

        // Mobility bonus
        score += 0.2 * legalMoves().size();

        return score;
    }

    int winner() const {
        int k0 = pits[KALAHA_0];
        int k1 = pits[KALAHA_1];

        if (k0 > k1) return 0;
        if (k1 > k0) return 1;

        return -1;
    }

    std::string toString() const {
        std::ostringstream oss;

        oss << "P1: [";

        for (int i = 12; i >= 7; --i)
            oss << pits[i] << (i > 7 ? "," : "");

        oss << "] K1=" << pits[KALAHA_1] << "\n";

        oss << "P0: [";

        for (int i = 0; i < 6; ++i)
            oss << pits[i] << (i < 5 ? "," : "");

        oss << "] K0=" << pits[KALAHA_0] << "\n";

        oss << "Turn: Player " << side;

        return oss.str();
    }
};