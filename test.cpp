#include <cstdint>
#include <vector>
#include <iostream>
#include <array>
#include <chrono>
//TODO: missing king movement, enpassant capturing, multithread, opening and endgame, pruning up to a stable position, impprove hueristic

using namespace std;

struct Benchmark {
    double positionsEvaluated = 0;

    double minimaxTime = 0;
    double genLegalMovesTime = 0;
    double evalTime = 0;

    double pawnTime = 0;
    double knightTime = 0;
    double slideTime = 0;
};

Benchmark bench;
bool debug=false; 

ostream& operator<<(ostream& os, const Benchmark& b) {
    os << "positionsEvaluated: " << b.positionsEvaluated << "\n";
    os << "minimaxTime: " << b.minimaxTime << "\n";
    os << "genLegalMovesTime: " << b.genLegalMovesTime << "\n";
    os << "evalTime: " << b.evalTime << "\n";
    os << "pawnTime: " << b.pawnTime << "\n";
    os << "knightTime: " << b.knightTime << "\n";
    os << "slideTime: " << b.slideTime << "\n";
    return os;
}


// inline double now() {
//     return std::chrono::high_resolution_clock::now()
//         .time_since_epoch().count();
// }
inline double now() {
    return std::chrono::duration<double>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

using Bitboard = uint64_t;

enum Color { WHITE = 0, BLACK = 1 };

struct Move {
    int from;
    int to;
};

ostream& operator<<(ostream& os, const Move& m) {
    os << "from: " << m.from;// << "\n";
    os << " to: " << m.to; //<< "\n";
    return os;
}

struct MinimaxResult {
    Move bestMove;
    double score;
};


struct Board {
    uint64_t black_pawn;
    uint64_t black_knight;
    uint64_t black_bishop;
    uint64_t black_rook;
    uint64_t black_queen;
    uint64_t black_king;

    uint64_t white_pawn;
    uint64_t white_knight;
    uint64_t white_bishop;
    uint64_t white_rook;
    uint64_t white_queen;
    uint64_t white_king;
    
    uint64_t white_occ;
    uint64_t black_occ;
    uint64_t all_occ;
};

Board setupBoard() {
    Board b{};

    // Black pieces (rank 8 and rank 7)
    b.black_pawn   = 0x00FF000000000000ULL;
    b.black_knight = 0x4200000000000000ULL;
    b.black_bishop = 0x2400000000000000ULL;
    b.black_rook   = 0x8100000000000000ULL;
    b.black_queen  = 0x1000000000000000ULL;
    b.black_king   = 0x0800000000000000ULL;

    // White pieces (rank 1 and rank 2)
    b.white_pawn   = 0x000000000000FF00ULL;
    b.white_knight = 0x0000000000000042ULL;
    b.white_bishop = 0x0000000000000024ULL;
    b.white_rook   = 0x0000000000000081ULL;
    b.white_queen  = 0x0000000000000010ULL;
    b.white_king   = 0x0000000000000008ULL;

    b.white_occ = b.white_pawn | b.white_knight | b.white_bishop |
                  b.white_rook | b.white_queen | b.white_king;

    b.black_occ = b.black_pawn | b.black_knight | b.black_bishop |
                  b.black_rook | b.black_queen | b.black_king;

    b.all_occ = b.white_occ | b.black_occ;

    return b;
}

Board testBoard() {
    Board b{};

    // Black pieces (rank 8 and rank 7)
    b.black_pawn   = 0x00F0000000000000ULL;
    b.black_knight = 0x0000000000000000ULL;
    b.black_bishop = 0x0000000000000000ULL;
    b.black_rook   = 0x0000000000000000ULL;
    b.black_queen  = 0x0000000000000000ULL;
    b.black_king   = 0x0800000000000000ULL;

    // White pieces (rank 1 and rank 2)
    b.white_pawn   = 0x0000000000000000ULL;
    b.white_knight = 0x0000000000000000ULL;
    b.white_bishop = 0x0000000000000024ULL;
    b.white_rook   = 0x0000000000000000ULL;
    b.white_queen  = 0x0000000000000000ULL;
    b.white_king   = 0x0000000000000008ULL;

    b.white_occ = b.white_pawn | b.white_knight | b.white_bishop |
                  b.white_rook | b.white_queen | b.white_king;

    b.black_occ = b.black_pawn | b.black_knight | b.black_bishop |
                  b.black_rook | b.black_queen | b.black_king;

    b.all_occ = b.white_occ | b.black_occ;

    return b;
}


inline bool bitAt(uint64_t bb, int sq) {
    return (bb >> sq) & 1ULL;
}
inline void clearBit(uint64_t &bb, int sq) {
    bb &= ~(1ULL << sq);
}
inline void setBit(uint64_t &bb, int sq) {
    bb |= (1ULL << sq);
}

void displayBoard(const Board& b) {
    cout << "  a b c d e f g h\n";

    for (int rank = 7; rank >= 0; rank--) {
        cout << rank + 1 << " ";

        for (int file = 0; file < 8; file++) {
            int sq = rank * 8 + file;

            if (bitAt(b.white_occ, sq)) {
                if (bitAt(b.white_pawn, sq))   cout << "P ";
                else if (bitAt(b.white_knight, sq)) cout << "N ";
                else if (bitAt(b.white_bishop, sq)) cout << "B ";
                else if (bitAt(b.white_rook, sq))   cout << "R ";
                else if (bitAt(b.white_queen, sq))  cout << "Q ";
                else if (bitAt(b.white_king, sq))   cout << "K ";
            }
            else if (bitAt(b.black_occ, sq)) {
                if (bitAt(b.black_pawn, sq))   cout << "p ";
                else if (bitAt(b.black_knight, sq)) cout << "n ";
                else if (bitAt(b.black_bishop, sq)) cout << "b ";
                else if (bitAt(b.black_rook, sq))   cout << "r ";
                else if (bitAt(b.black_queen, sq))  cout << "q ";
                else if (bitAt(b.black_king, sq))   cout << "k ";
            }
            else {
                cout << ". ";
            }
        }

        cout << "\n";
    }
}


// masks
constexpr uint64_t FILE_A    = 0x8080808080808080ULL;
constexpr uint64_t FILE_H    = 0x0101010101010101ULL;
constexpr uint64_t FILE_Anot = ~FILE_A;
constexpr uint64_t FILE_Hnot = ~FILE_H;
constexpr uint64_t RANK_2 = 0x000000000000FF00ULL;
constexpr uint64_t RANK_7 = 0x00FF000000000000ULL;

// Knight mask
array<uint64_t, 64> KnightMask;

void genKnightMasks() {
    for (int sq = 0; sq < 64; sq++) {
        uint64_t mask = 0ULL;

        int r = sq / 8;
        int f = sq % 8;

        const int moves[8][2] = {
            {r+2, f+1}, {r+2, f-1},
            {r-2, f+1}, {r-2, f-1},
            {r+1, f+2}, {r+1, f-2},
            {r-1, f+2}, {r-1, f-2}
        };

        for (auto& m : moves) {
            int rr = m[0];
            int ff = m[1];
            if (rr >= 0 && rr < 8 && ff >= 0 && ff < 8) {
                mask |= (1ULL << (rr * 8 + ff));
            }
        }

        KnightMask[sq] = mask;
    }
}

struct DirMask {
    int shift;
    uint64_t mask;
};

constexpr DirMask BISHOP_DIRS[4] = {
    { +9, FILE_Anot }, // NE//switched
    { +7, FILE_Hnot }, // NW
    { -7, FILE_Anot }, // SE
    { -9, FILE_Hnot }  // SW
};

constexpr int Depth=5;
constexpr int turns=3;

constexpr uint64_t ALL_MASK = 0xFFFFFFFFFFFFFFFFULL;

constexpr DirMask ROOK_DIRS[4] = {
    { +8, ALL_MASK },  // North
    { -8, ALL_MASK },  // South
    { +1, FILE_Hnot }, // East
    { -1, FILE_Anot }  // West
};

constexpr DirMask QUEEN_DIRS[8] = {
    BISHOP_DIRS[0], BISHOP_DIRS[1], BISHOP_DIRS[2], BISHOP_DIRS[3],
    ROOK_DIRS[0],   ROOK_DIRS[1],   ROOK_DIRS[2],   ROOK_DIRS[3]
};

struct SlideResult {
    int from;
    uint64_t moves;
};

vector<SlideResult> slideMoves(uint64_t piece_bb, uint64_t occ_all, uint64_t occ_enemy, const DirMask* dirs, int dirCount){
    vector<SlideResult> result;

    // iterate through all bits in piece_bb
    uint64_t bb = piece_bb;
    while (bb) {
        int sq = __builtin_ctzll(bb);   // get least significant 1-bit
        bb &= bb - 1;                   // clear that bit

        uint64_t moves = 0ULL;

        for (int i = 0; i < dirCount; i++) {
            int shift = dirs[i].shift;
            uint64_t mask = dirs[i].mask;

            int cur = sq;
            while (true) {
                int nxt = cur + shift;

                if (nxt < 0 || nxt >= 64)
                    break;

                // file wrap prevention
                if (((mask >> cur) & 1ULL) == 0)
                    break;

                moves |= (1ULL << nxt);

                if ((occ_all >> nxt) & 1ULL)
                    break;

                cur = nxt;
            }
        }

        // remove own pieces from moves
        moves &= ~occ_all | occ_enemy;

        result.push_back({sq, moves});
    }

    return result;
}

vector<Move> slidingMoveList(const vector<SlideResult>& slideResults) {
    vector<Move> moves;

    for (const auto& sr : slideResults) {
        uint64_t bb = sr.moves;

        while (bb) {
            int to = __builtin_ctzll(bb);
            bb &= bb - 1;
            moves.push_back({sr.from, to});
        }
    }

    return moves;
}

vector<Move> bb_to_moves(uint64_t from_bb, uint64_t to_bb, int shift) {
    vector<Move> moves;

    uint64_t bb = to_bb;
    while (bb) {
        int to_sq = __builtin_ctzll(bb);   // index of least significant 1-bit
        bb &= bb - 1;                      // clear that bit

        int from_sq = to_sq - shift;

        if (from_sq >= 0 && from_sq < 64) {
            if ((from_bb >> from_sq) & 1ULL) {
                moves.push_back({from_sq, to_sq});
            }
        }
    }

    return moves;
}

inline int pop(uint64_t bb) {
    return __builtin_popcountll(bb);
}

double evaluation(const Board& b, int turn) {
    double start = now();

    double white_score =
        1.0 * pop(b.white_pawn) +
        2.8 * pop(b.white_knight) +
        3.0 * pop(b.white_bishop) +
        5.0 * pop(b.white_rook) +
        9.0 * pop(b.white_queen);

    double black_score =
        1.0 * pop(b.black_pawn) +
        2.8 * pop(b.black_knight) +
        3.0 * pop(b.black_bishop) +
        5.0 * pop(b.black_rook) +
        9.0 * pop(b.black_queen);

    double score = white_score - black_score;

    bench.evalTime += now() - start;
    ++bench.positionsEvaluated;
    return (turn == 0 ? score : -score);
}

void changeBoard(Board &b, int from, int to, int turn) {
    if (turn == 0) {  // WHITE TO MOVE
        // Move white piece
        if (bitAt(b.white_pawn, from)) {
            clearBit(b.white_pawn, from);
            setBit(b.white_pawn, to);
        }
        else if (bitAt(b.white_knight, from)) {
            clearBit(b.white_knight, from);
            setBit(b.white_knight, to);
        }
        else if (bitAt(b.white_bishop, from)) {
            clearBit(b.white_bishop, from);
            setBit(b.white_bishop, to);
        }
        else if (bitAt(b.white_rook, from)) {
            clearBit(b.white_rook, from);
            setBit(b.white_rook, to);
        }
        else if (bitAt(b.white_queen, from)) {
            clearBit(b.white_queen, from);
            setBit(b.white_queen, to);
        }

        // Update white occupancy
        clearBit(b.white_occ, from);
        setBit(b.white_occ, to);

        // Capture black piece if present
        if (bitAt(b.black_occ, to)) {
            clearBit(b.black_occ, to);
            clearBit(b.black_pawn, to);
            clearBit(b.black_knight, to);
            clearBit(b.black_bishop, to);
            clearBit(b.black_rook, to);
            clearBit(b.black_queen, to);
        }
    }
    else {  // BLACK TO MOVE
        // Move black piece
        if (bitAt(b.black_pawn, from)) {
            clearBit(b.black_pawn, from);
            setBit(b.black_pawn, to);
        }
        else if (bitAt(b.black_knight, from)) {
            clearBit(b.black_knight, from);
            setBit(b.black_knight, to);
        }
        else if (bitAt(b.black_bishop, from)) {
            clearBit(b.black_bishop, from);
            setBit(b.black_bishop, to);
        }
        else if (bitAt(b.black_rook, from)) {
            clearBit(b.black_rook, from);
            setBit(b.black_rook, to);
        }
        else if (bitAt(b.black_queen, from)) {
            clearBit(b.black_queen, from);
            setBit(b.black_queen, to);
        }

        // Update black occupancy
        clearBit(b.black_occ, from);
        setBit(b.black_occ, to);

        // Capture white piece if present
        if (bitAt(b.white_occ, to)) {
            clearBit(b.white_occ, to);
            clearBit(b.white_pawn, to);
            clearBit(b.white_knight, to);
            clearBit(b.white_bishop, to);
            clearBit(b.white_rook, to);
            clearBit(b.white_queen, to);
        }
    }

    // Update all occupancy
    clearBit(b.all_occ, from);
    setBit(b.all_occ, to);
}

vector<Move> genLegalMoves(const Board& b, int turn) {
    vector<Move> legal_moves;

    uint64_t empty = ~b.all_occ;

    if (turn == 0) {  // WHITE TO MOVE
        // -------------------------
        // White pawn moves
        // -------------------------
        double start = now();

        uint64_t wp = b.white_pawn;

        uint64_t single = (wp << 8) & empty;
        uint64_t doublePush = ((wp & RANK_2) << 16) & empty & (empty << 8);

        uint64_t captures_left  = (wp << 7) & b.black_occ & FILE_Hnot;
        uint64_t captures_right = (wp << 9) & b.black_occ & FILE_Anot;

        auto add1 = bb_to_moves(wp, single, 8);
        auto add2 = bb_to_moves(wp, doublePush, 16);
        auto add3 = bb_to_moves(wp, captures_left, 7);
        auto add4 = bb_to_moves(wp, captures_right, 9);

        legal_moves.insert(legal_moves.end(), add1.begin(), add1.end());
        legal_moves.insert(legal_moves.end(), add2.begin(), add2.end());
        legal_moves.insert(legal_moves.end(), add3.begin(), add3.end());
        legal_moves.insert(legal_moves.end(), add4.begin(), add4.end());
        
        bench.pawnTime += now() - start;

        // -------------------------
        // White knights
        // -------------------------
        
        start = now();

        uint64_t wkn = b.white_knight;

        uint64_t bb = wkn;
        while (bb) {
            int sq = __builtin_ctzll(bb);
            bb &= bb - 1;

            uint64_t attacks = KnightMask[sq] & ~b.white_occ;

            uint64_t atk = attacks;
            while (atk) {
                int to = __builtin_ctzll(atk);
                atk &= atk - 1;
                legal_moves.push_back({sq, to});
            }
        }

        bench.knightTime += now() - start;

        start = now();

        // -------------------------
        // White sliding pieces
        // -------------------------
        auto bsl = slideMoves(b.white_bishop, b.all_occ, b.black_occ, BISHOP_DIRS, 4);
        auto rsl = slideMoves(b.white_rook,   b.all_occ, b.black_occ, ROOK_DIRS,   4);
        auto qsl = slideMoves(b.white_queen,  b.all_occ, b.black_occ, QUEEN_DIRS,  8);

        auto bmv = slidingMoveList(bsl);
        auto rmv = slidingMoveList(rsl);
        auto qmv = slidingMoveList(qsl);

        legal_moves.insert(legal_moves.end(), bmv.begin(), bmv.end());
        legal_moves.insert(legal_moves.end(), rmv.begin(), rmv.end());
        legal_moves.insert(legal_moves.end(), qmv.begin(), qmv.end());
        
        bench.slideTime += now() - start;
        
    }
    else {  // BLACK TO MOVE
        // -------------------------
        // Black pawn moves
        // -------------------------
        double start = now();

        
        uint64_t bp = b.black_pawn;

        uint64_t single = (bp >> 8) & empty;
        uint64_t doublePush = ((bp & RANK_7) >> 16) & empty & (empty >> 8);

        uint64_t captures_left  = (bp >> 7) & b.white_occ & FILE_Hnot;
        uint64_t captures_right = (bp >> 9) & b.white_occ & FILE_Anot;

        auto add1 = bb_to_moves(bp, single, -8);
        auto add2 = bb_to_moves(bp, doublePush, -16);
        auto add3 = bb_to_moves(bp, captures_left,- 7);
        auto add4 = bb_to_moves(bp, captures_right,- 9);

        legal_moves.insert(legal_moves.end(), add1.begin(), add1.end());
        legal_moves.insert(legal_moves.end(), add2.begin(), add2.end());
        legal_moves.insert(legal_moves.end(), add3.begin(), add3.end());
        legal_moves.insert(legal_moves.end(), add4.begin(), add4.end());
        
        bench.pawnTime += now() - start;

        // -------------------------
        // Black knights
        // -------------------------
        start=now();

        uint64_t bkn = b.black_knight;

        uint64_t bb2 = bkn;
        while (bb2) {
            int sq = __builtin_ctzll(bb2);
            bb2 &= bb2 - 1;

            uint64_t attacks = KnightMask[sq] & ~b.black_occ;

            uint64_t atk = attacks;
            while (atk) {
                int to = __builtin_ctzll(atk);
                atk &= atk - 1;
                legal_moves.push_back({sq, to});
            }
        }
        bench.knightTime += now() - start;

        // -------------------------
        // Black sliding pieces
        // -------------------------
        start=now();
        
        auto bsl = slideMoves(b.black_bishop, b.all_occ, b.white_occ, BISHOP_DIRS, 4);
        auto rsl = slideMoves(b.black_rook,   b.all_occ, b.white_occ, ROOK_DIRS,   4);
        auto qsl = slideMoves(b.black_queen,  b.all_occ, b.white_occ, QUEEN_DIRS,  8);

        auto bmv = slidingMoveList(bsl);
        auto rmv = slidingMoveList(rsl);
        auto qmv = slidingMoveList(qsl);

        legal_moves.insert(legal_moves.end(), bmv.begin(), bmv.end());
        legal_moves.insert(legal_moves.end(), rmv.begin(), rmv.end());
        legal_moves.insert(legal_moves.end(), qmv.begin(), qmv.end());
        
        bench.slideTime += now() - start;
    }
        if(debug){
            cout<<"legalmoves.size()"<<legal_moves.size()<<endl;
            for(int i = 0; i< legal_moves.size();++i){
                cout<<"legalmoves: "<< i<<" "<<legal_moves[i]<<endl;
            }
        }
        
        // if (legal_moves.empty()) { //code is commented out for optimized performance
    //     throw runtime_error("usererror1: stalemate");
    // }

    return legal_moves;
}


MinimaxResult minimax(const Board& board, int turn, int depth) {
    
    // Generate legal moves
    vector<Move> moves;
    
    double start = now(); 
    
    //try { //code is commented out fro optimal performance
        moves = genLegalMoves(board, turn);
    // } catch (...) {
    //     throw runtime_error("usererror1: stalemate");
    // }
    
    bench.genLegalMovesTime += now() - start;
    
    // Base case
    if (depth == 0) {
        return {{-1, -1}, evaluation(board, turn)};
    }

    double bestScore = (turn == 0 ? -1e9 : 1e9);
    Move bestMove = {-1, -1};

    // Loop through moves
    for (const Move& mv : moves) {
        Board newBoard = board;               // cloneBoard
        changeBoard(newBoard, mv.from, mv.to, turn);

        MinimaxResult child = minimax(newBoard, turn ^ 1, depth - 1);
        double score = child.score;

        if (turn == 0) {  // maximizing (white)
            if (score > bestScore) {
                bestScore = score;
                bestMove = mv;
            }
        } else {          // minimizing (black)
            if (score < bestScore) {
                bestScore = score;
                bestMove = mv;
            }
        }
    }

    return {bestMove, bestScore};
}

Board move(Board board, int turn) {
    // Run minimax with depth 3
    double start = now();
    
    MinimaxResult result = minimax(board, turn, Depth);
    
    bench.minimaxTime += now() - start;
    
    Move best = result.bestMove;

    // Apply the move
    changeBoard(board, best.from, best.to, turn);

    // Display board after move
    displayBoard(board);

    return board;
}



int main() {
    vector<Move> moves;
    Color turn = WHITE;
    cout<<"Test"<<"\n";

    genKnightMasks();
    Board board=setupBoard();
    // board=testBoard();
    displayBoard(board);

    cout<<"turn: "<<turn<<endl;
    for(int i=0;i<turns;++i){
        cout<<"moveMade"<<endl;
        board=move(board,turn==BLACK?1:0);
        turn= turn==WHITE?turn=BLACK :turn=WHITE;
        // if(i==0) {debug=true;}
    }

    cout<<bench;
}
