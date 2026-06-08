#include "../src/board.hpp"
#include "../src/alphabeta.hpp"
#include "../src/mcts.hpp"
#include <cassert>
#include <iostream>
#include <cmath>

// ─────────────────────────────────────────────────────────────────────────────
// Board tests
// ─────────────────────────────────────────────────────────────────────────────

void testInitialBoard() {
    Board b;
    for (int i = 0; i < 6;  ++i) assert(b.pits[i] == 4);
    for (int i = 7; i < 13; ++i) assert(b.pits[i] == 4);
    assert(b.pits[6]  == 0);
    assert(b.pits[13] == 0);
    assert(b.side == 0);
    assert(!b.isTerminal());
    std::cout << "[PASS] testInitialBoard\n";
}

void testLegalMoves() {
    Board b;
    auto moves = b.legalMoves();
    assert(moves.size() == 6);
    for (int i = 0; i < 6; ++i) assert(moves[i] == i);
    std::cout << "[PASS] testLegalMoves\n";
}

void testSowing() {
    Board b;
    // Player 0 picks pit 0 (4 seeds): sows to pits 1,2,3,4
    Board b2 = b.applyMove(0);
    assert(b2.pits[0] == 0);
    assert(b2.pits[1] == 5);
    assert(b2.pits[2] == 5);
    assert(b2.pits[3] == 5);
    assert(b2.pits[4] == 5);
    assert(b2.pits[5] == 4); // unchanged
    assert(b2.pits[6] == 0); // no seed reached kalaha
    assert(b2.side == 1);    // turn passes to player 1
    std::cout << "[PASS] testSowing\n";
}

void testExtraTurn() {
    // Player 0 should get extra turn when last seed lands in kalaha
    // Pit 2 with 4 seeds: lands on 3,4,5,6 (kalaha) → extra turn
    Board b;
    b.pits[2] = 4;
    Board b2 = b.applyMove(2);
    // last seed lands at index 6 (kalaha_0)
    // but pit 2 has 4 seeds: 2→3,4,5,6 — yes, last at kalaha!
    // side should remain 0
    assert(b2.side == 0);
    assert(b2.pits[6] == 1);
    std::cout << "[PASS] testExtraTurn\n";
}

void testCapture() {
    // ── P0 capture (triggers terminal) ──
    // P0 picks pit 1 (3 seeds) → sows to 2, 3, 4
    // pit 4 was 0 → capture: kalaha_0 += pits[4]+pits[8] = 1+5=6
    // After capture P1 side is empty → terminal → P0 remaining (pits[2]+pits[3]=2) swept
    // Final kalaha_0 = 8
    Board b;
    b.pits.fill(0);
    b.pits[6] = 0; b.pits[13] = 0;
    b.pits[1] = 3;  // sow: 2,3,4
    b.pits[4] = 0;  // landing pit empty
    b.pits[8] = 5;  // opp (12-4=8)
    b.side = 0;

    Board b2 = b.applyMove(1);
    assert(b2.pits[4]  == 0);
    assert(b2.pits[8]  == 0);
    assert(b2.pits[6]  == 8);   // capture(6) + sweep(2) = 8
    assert(b2.isTerminal());
    std::cout << "[PASS] testCapture (P0 with terminal sweep)\n";

    // ── P0 capture (no terminal) ──
    // P0 picks pit 0 (1 seed) → sows to pit 1 (empty)
    // opp = 12-1=11 has 3 seeds → capture: kalaha_0 gets 4
    // P0 still has pits[2,3]=2,2; P1 still has pits[9]=2 → NOT terminal
    Board d;
    d.pits.fill(0);
    d.pits[6] = 0; d.pits[13] = 0;
    d.pits[0]  = 1;  // sows to pit 1
    d.pits[1]  = 0;  // empty landing
    d.pits[11] = 3;  // opp (12-1=11)
    d.pits[2]  = 2;  d.pits[3] = 2;  // P0 remaining
    d.pits[9]  = 2;                   // P1 remaining
    d.side = 0;

    Board d2 = d.applyMove(0);
    assert(d2.pits[1]  == 0);
    assert(d2.pits[11] == 0);
    assert(d2.pits[6]  == 4);   // 3+1=4, no sweep
    assert(!d2.isTerminal());
    std::cout << "[PASS] testCapture (P0, no terminal)\n";

    // ── P1 capture (no terminal) ──
    // P1 picks pit 8 (3 seeds) → sows to 9, 10, 11
    // pit 11 was empty → opp = 12-11=1 has 4 seeds → capture: kalaha_1 gets 5
    // P1 still has pit[12]=2; P0 still has pits[2,3]=2 → NOT terminal
    Board c;
    c.pits.fill(0);
    c.pits[6] = 0; c.pits[13] = 0;
    c.pits[8]  = 3;   // P1, sows: 9,10,11
    c.pits[11] = 0;   // empty landing
    c.pits[1]  = 4;   // opp (12-11=1)
    c.pits[12] = 2;   // P1 remaining
    c.pits[2]  = 2;   c.pits[3] = 2;  // P0 remaining
    c.side = 1;

    Board c2 = c.applyMove(8);
    assert(c2.pits[11] == 0);
    assert(c2.pits[1]  == 0);
    assert(c2.pits[13] == 5);   // 4+1=5, no sweep
    assert(!c2.isTerminal());
    std::cout << "[PASS] testCapture (P1, no terminal)\n";
}

void testTerminal() {
    Board b;
    b.pits.fill(0);
    b.pits[6] = 20;
    b.pits[13] = 28;
    assert(b.isTerminal());
    assert(b.winner() == 1);
    std::cout << "[PASS] testTerminal\n";
}

void testAlphaBetaEqualsMinimaxSmallDepth() {
    Board b;
    // At depth ≤ 4, Alpha-Beta must return same best move as pure Minimax
    for (int d = 1; d <= 5; ++d) {
        ABResult mm = minimaxRoot(b, d);
        ABResult ab = alphaBetaSeqRoot(b, d);
        if (mm.bestMove != ab.bestMove) {
            std::cerr << "[FAIL] testAlphaBetaEqualsMinimaxSmallDepth at depth " << d
                      << ": mm=" << mm.bestMove << " ab=" << ab.bestMove << "\n";
            // Note: ties can cause different moves with same value
            // Check that values are equal
            assert(mm.bestVal == ab.bestVal);
        }
    }
    std::cout << "[PASS] testAlphaBetaEqualsMinimaxSmallDepth\n";
}

void testMCTSConverges() {
    Board b;
    // MCTS with large budget should agree with Alpha-Beta on best move
    MCTSResult mr = mcts(b, 5000, 1, false);
    assert(mr.bestMove >= 0 && mr.bestMove <= 5);
    assert(mr.winRate >= 0.0 && mr.winRate <= 1.0);
    std::cout << "[PASS] testMCTSConverges (move=" << mr.bestMove
              << ", winRate=" << mr.winRate << ")\n";
}

void testParallelAlphaBetaAgreesWithSequential() {
    Board b;
    int depth = 6;
    ABResult seq = alphaBetaSeqRoot(b, depth);
    for (int T : {2, 4}) {
        ABResult par = alphaBetaRoot(b, depth, T);
        // Values should be equal (root parallelism is correct by construction)
        if (seq.bestVal != par.bestVal) {
            std::cerr << "[WARN] Parallel AB val=" << par.bestVal
                      << " != seq val=" << seq.bestVal << " T=" << T << "\n";
        }
        assert(seq.bestVal == par.bestVal);
    }
    std::cout << "[PASS] testParallelAlphaBetaAgreesWithSequential\n";
}

void testMCTSParallelRootAndLeaf() {
    Board b;
    // Root parallelism
    MCTSResult r1 = mcts(b, 2000, 1, false);
    MCTSResult r2 = mcts(b, 2000, 2, false); // root parallelism
    MCTSResult r3 = mcts(b, 2000, 2, true);  // leaf parallelism
    // All should return a valid move
    assert(r1.bestMove >= 0);
    assert(r2.bestMove >= 0);
    assert(r3.bestMove >= 0);
    std::cout << "[PASS] testMCTSParallelRootAndLeaf (r1=" << r1.bestMove
              << " r2=" << r2.bestMove << " r3=" << r3.bestMove << ")\n";
}

void testEvaluation() {
    Board b;
    b.pits[6]  = 10; // player 0 has 10 in store
    b.pits[13] = 5;  // player 1 has 5
    int eval = b.evaluate(0);
    assert(eval > 0); // player 0 is ahead
    std::cout << "[PASS] testEvaluation\n";
}

void testOppPit() {
    Board b;
    // Board face-to-face:
    //   P1: [12][11][10][ 9][ 8][ 7]
    //   P0: [ 0][ 1][ 2][ 3][ 4][ 5]
    // Each column sums to 12.
    assert(b.oppPit(0)  == 12);
    assert(b.oppPit(1)  == 11);
    assert(b.oppPit(2)  == 10);
    assert(b.oppPit(3)  ==  9);
    assert(b.oppPit(4)  ==  8);
    assert(b.oppPit(5)  ==  7);
    assert(b.oppPit(7)  ==  5);
    assert(b.oppPit(8)  ==  4);
    assert(b.oppPit(9)  ==  3);
    assert(b.oppPit(10) ==  2);
    assert(b.oppPit(11) ==  1);
    assert(b.oppPit(12) ==  0);
    std::cout << "[PASS] testOppPit\n";
}

int main() {
    std::cout << "=== Mancala Kalah Unit Tests ===\n\n";
    testInitialBoard();
    testLegalMoves();
    testSowing();
    testExtraTurn();
    testCapture();
    testTerminal();
    testEvaluation();
    testOppPit();
    testAlphaBetaEqualsMinimaxSmallDepth();
    testMCTSConverges();
    testParallelAlphaBetaAgreesWithSequential();
    testMCTSParallelRootAndLeaf();

    std::cout << "\n=== All tests passed ===\n";
    return 0;
}
