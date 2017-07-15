/*
 agent.hpp
 Katsuki Ohto
 */

#ifndef DAB_AGENT_HPP_
#define DAB_AGENT_HPP_

#include "dab.hpp"

namespace DotsAndBoxes{
    
    /**************************完全ランダム**************************/
    
    template<class board_t>
    std::tuple<Move, Value> randomMove(const board_t& bd){
        Move buffer[MAX_MOVES];
        int moves = genAllMoves(buffer, bd);
        return std::make_tuple(buffer[dice() % moves], Value(0));
    }
    
    /**************************貪欲alpha-beta**************************/
    
    template<bool ROOT, class board_t>
    std::tuple<Move, Value> greedyMoveSub(board_t& bd, int depth,
                                          Value alpha, Value beta){
        Color turnColor = bd.turnColor();
        if(depth <= 0 || bd.filled()){
            return std::make_tuple(MOVE_NONE, Value(bd.areaDiff(turnColor)));
        }
        std::array<Move, MAX_MOVES> buffer;
        const int moves = genAllMoves(buffer.data(), bd);
        if(ROOT){
            std::shuffle(buffer.begin(), buffer.begin() + moves, dice);
        }
        Value bestValue = Value(0);
        Move bestMove = buffer[0];
        for(int m = 0; m < moves; ++m){
            Move move = buffer[m];
            Value value;
            int newArea = bd.move(move);
            if(newArea){ // same
                auto result = greedyMoveSub<false>(bd, depth, alpha, beta);
                value = std::get<1>(result);
            }else{ // opponent
                auto result = greedyMoveSub<false>(bd, depth - 1, -beta, -alpha);
                value = -std::get<1>(result);
            }
            bd.unmove(move);
            
            if(value > bestValue){
                if(!ROOT && value >= beta){
                    return std::make_tuple(move, value);
                }
                bestValue = value;
                bestMove = move;
            }
            alpha = std::max(alpha, value);
        }
        return std::make_tuple(bestMove, bestValue);
    }
    template<class board_t>
    std::tuple<Move, Value> greedyMove(const board_t& bd, int depth){
        board_t tbd = bd;
        return greedyMoveSub<true>(tbd, depth, -VALUE_INFINITE, VALUE_INFINITE);
    }
    
    /**************************真面目な探索**************************/
    
    struct RootMove{
        Move move;
        Value value, previousValue;
        int64_t nodes;
        int searchDepth;
        void set(VH avh, int amz){
            move.set(avh, amz);
            nodes = 0;
            value = previousValue= -VALUE_INFINITE;
            searchDepth = 0;
        }
        bool operator <(const RootMove& rm)const noexcept{ // ソート用
            return value > rm.value;
        }
        bool operator ==(const Move& m)const{ // 検索用
            return move == m;
        }
    };
    
    // ルート着手情報
    std::array<RootMove, MAX_MOVES> rootBuffer;
    int rootMoves;
    
    // 置換表
    struct HashEntry{
        Move move[2];
        uint64_t key;
        int32_t value, depth;
        
        //void clear()noexcept{}
        bool any()const noexcept{
            return bool(key);
        }
        void set(uint64_t akey, Move amove,
                 Value avalue, int adepth)noexcept{
            key = akey;
            move[0] = amove;
            value = avalue;
            depth = adepth;
        }
    };
    struct HashBucket{
        static constexpr size_t BUCKET_SIZE = 4;
        std::array<HashEntry, BUCKET_SIZE> entry_;
        
        template<class board_t>
        HashEntry* find(const board_t& bd, uint64_t key){
            for(size_t i = 0; i < BUCKET_SIZE; ++i){
                if(!entry_[i].any()){ return nullptr; }
                if(entry_[i].key == key){
                    return &entry_[i];
                }
            }
            return nullptr;
        }
        template<class board_t>
        bool insert(const board_t& bd, uint64_t key, Move move, Value value, int depth){
            for(size_t i = 0; i < BUCKET_SIZE; ++i){
                if(!entry_[i].any()){
                    entry_[i].set(key, move, value, depth);
                    return true;
                }
                if(depth >= entry_[i].depth){
                    for(int j = BUCKET_SIZE - 2; j >= (int)i; --j){
                        entry_[j + 1] = entry_[j];
                    }
                    entry_[i].set(key, move, value, depth);
                    return false;
                }
            }
            return false;
        }
    };
    struct HashTable{
        static constexpr size_t SIZE = (1 << 22) - 3;
        
        std::array<HashBucket, SIZE> table_;
        size_t filled_;
        
        HashTable(){ clear(); }
        
        template<class board_t>
        HashEntry* find(const board_t& bd){
            uint64_t key = bd.key();
            size_t index = key % SIZE;
            return table_[index].find(bd, key);
        }
        template<class board_t>
        bool insert(const board_t& bd, Move move, Value value, int depth){
            uint64_t key = bd.key();
            size_t index = key % SIZE;
            return table_[index].insert(bd, key, move, value, depth);
        }
        
        double filled()const{
            return filled_ / (double)(SIZE * HashBucket::BUCKET_SIZE);
        }
        
        void clear(){
            memset(table_.data(), 0, sizeof(table_));
            filled_ = 0;
        }
    };
    
    struct MovePicker{
        Move buffer_[MAX_MOVES];
        const Board& bd_;
        Move ttMove_;
        int r_, m_;
        int state_;
        int moves_;
        int z_;
        
        MovePicker(const Board& bd, Move ttMove):
        bd_(bd), ttMove_(ttMove), r_(0), m_(0), state_(0){}
        
#define BEGIN_CR       switch(state_) { case 0:
#define END_CR         state_ == __LINE__; case __LINE__:; }
#define YIELD(move)    { state_ = __LINE__; ASSERT(bd_.valid(move), cerr << move << " is not valid.";);\
                       return (move); case __LINE__:; }
        Move next(){
            Move move;
            BEGIN_CR; // コルーチン開始
            if(ttMove_ != MOVE_NONE){
                if(bd_.valid(ttMove_)){
                    YIELD(ttMove_);
                }
            }
            while(r_ < bd_.numReaches){
                z_ = bd_.reachInfo[r_].z;
                move = fillMove(bd_.cell[z_], z_);
                r_ += 1;
                if(move != ttMove_){
                    YIELD(move);
                }
            }
            moves_ = genAllMoves(buffer_, bd_);
            while(m_ < moves_){
                move = buffer_[m_];
                m_ += 1;
                if(move != ttMove_
                   /*&& !bd_.reach(move.mz)
                   && !bd_.reach(move.mz + dirDZ[besideLineDirection[move.vh][1]])*/){
                    YIELD(move);
                }
            }
            END_CR;
            return MOVE_NONE;
        }
#undef BEGIN
#undef END
#undef YIELD
    };
    
    struct SearchAgent{
        HashTable tt;
        int64_t hashCut;
        int64_t nodes;
        ClockMicS clock;
        int64_t timeLimit;
        
        void initSearch(){
            clock.start();
            nodes = 0;
            hashCut = 0;
        }
        void initialize(){
            tt.clear();
        }
        
        SearchAgent(int tl){
            timeLimit = tl * 1000;
        }
        
        template<bool PV, bool ROOT, class board_t>
        std::tuple<Move, Value> search(board_t& bd, int depth,
                                       Value alpha, Value beta){
            Color turnColor = bd.turnColor();
            Move ttMove = MOVE_NONE;
            Value ttValue;
            
            if(!ROOT && !PV){
                const HashEntry *const pentry = tt.find(bd);
                if(pentry != nullptr){
                    hashCut += 1;
                    ttValue = Value(pentry->value);
                    if(pentry->depth >= depth){
                        return std::make_tuple(pentry->move[0], ttValue);
                    }
                }
            }
            if(!ROOT && bd.mate()){
                return std::make_tuple(MOVE_NONE, Value((int)VALUE_MATE + bd.areaDiff(turnColor) \
                                                        + bd.immediateReachEffect));
            }
            if(depth <= 0 && bd.numReaches <= 0){
                return std::make_tuple(MOVE_NONE, Value(bd.areaDiff(turnColor)));
            }
            //std::array<Move, MAX_MOVES> buffer;
            //const int moves = genAllMoves(buffer.data(), bd);
            Value bestValue = -VALUE_INFINITE;
            Move bestMove = MOVE_NONE;
            int moveCount = 0;
            Move move;
            
            MovePicker mp(bd, ttMove);
            
            while(bestValue < std::min(beta, VALUE_MATE) && (move = mp.next()) != MOVE_NONE){
                //Move move = buffer[m];
                // ルートの候補手として指定されていないものは探索しない
                if(ROOT && !std::count(rootBuffer.cbegin(), rootBuffer.cbegin() + rootMoves, move)){
                    continue;
                }
                Value value;
                int newArea = bd.move(move); nodes += 1;
                //cerr << bd.areaDiff(turnColor) << endl;
                if(moveCount == 0){ // pv
                    if(newArea){ // same
                        auto result = search<PV, false>(bd, depth, alpha, beta);
                        value = std::get<1>(result);
                    }else{ // opponent
                        auto result = search<PV, false>(bd, depth - 1, -beta, -alpha);
                        value = -std::get<1>(result);
                    }
                }else{
                    if(newArea){ // same
                        auto result = search<false, false>(bd, depth, alpha, beta);
                        value = std::get<1>(result);
                    }else{ // opponent
                        auto result = search<false, false>(bd, depth - 1, -beta, -alpha);
                        value = -std::get<1>(result);
                    }
                }
                bd.unmove(move);
                
                // 時間チェック
                if(value == VALUE_NONE || clock.stop() >= timeLimit){
                    return std::make_tuple(MOVE_NONE, VALUE_NONE);
                }
                
                if(ROOT){
                    // root move に結果を保存
                    RootMove& rm = *std::find(rootBuffer.begin(), rootBuffer.begin() + rootMoves, move);
                    //cerr << move << " " << value;
                    if(moveCount == 0 || value > alpha){
                        rm.value = value;
                    }else{
                        rm.value = -VALUE_INFINITE;
                    }
                }
                if(value > bestValue){
                    if(!ROOT && value > beta){
                        tt.insert(bd, move, value, depth);
                        return std::make_tuple(move, value);
                    }
                    bestValue = value;
                    bestMove = move;
                }
                alpha = std::max(alpha, value);
                moveCount += 1;
            }
            ASSERT(bestValue > -VALUE_INFINITE, cerr << bestValue << endl;);
            tt.insert(bd, bestMove, bestValue, depth);
            return std::make_tuple(bestMove, bestValue);
        }
        template<class oboard_t>
        std::tuple<Move, Value> searchMove(const oboard_t& obd, int depth){
            if(obd.ply == 0){
                tt.clear();
            }
            initSearch();
            Board bd = obd;
            //cerr << bd;
            // root move の用意
            rootMoves = genAllMoves(rootBuffer.data(), bd);
            std::shuffle(rootBuffer.begin(), rootBuffer.begin() + rootMoves, dice);
            
            MovePicker mp(bd, MOVE_NONE);
            Move move;
            while((move = mp.next()) != MOVE_NONE){
                cerr << move << " ";
            }
            
            Move bestMove;
            Value bestValue;
            for(int iteration = 1; iteration <= depth && clock.stop() < timeLimit; ++iteration){
                auto result = search<true, true>(bd, iteration, -VALUE_INFINITE, VALUE_INFINITE);
                // root move の並べ替え
                std::stable_sort(rootBuffer.begin(), rootBuffer.begin() + rootMoves);
                // previous value を保存
                for(int m = 0; m < rootMoves; ++m){
                    rootBuffer[m].previousValue = rootBuffer[m].value;
                }
                bestMove = rootBuffer[0].move;
                bestValue = rootBuffer[0].value;
                cerr << "iteration " << iteration << " move " << bestMove << " value " << bestValue;
                cerr << " time " << clock.stop() / 1000 << " nodes " << nodes;
                cerr << " hashcut " << hashCut << " hashfull " << tt.filled() << endl;
            }
            return std::make_tuple(bestMove, bestValue);
        }
    };
}

#endif // DAB_AGENT_HPP_