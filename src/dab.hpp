/*
 dab.hpp
 Katsuki Ohto
 */

#ifndef DAB_DAB_HPP_
#define DAB_DAB_HPP_

#include <cstring>
#include <unistd.h>
#include <sys/time.h>
#include <ctime>

#include <cmath>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cassert>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <array>
#include <vector>
#include <queue>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <random>
#include <algorithm>
#include <string>
#include <bitset>
#include <numeric>

#ifdef _WIN32

#include <winsock2.h>
#include <ws2tcpip.h>

#else

#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>

#endif

// 設定

#if !defined(MINIMUM)
#define USE_ANALYZER
#endif

// 相手手番中の先読み
#define PONDER
// ストレートデータ構造を使う
//#define USE_STRAIGHT

constexpr std::size_t N_THREADS = 8;

// 基本的定義、ユーティリティ
// 使いそうなものは全て読み込んでおく

// 出力
#include "../CppCommon/src/util/io.hpp"
#include "../CppCommon/src/util/string.hpp"
#include "../CppCommon/src/util/xorShift.hpp"
#include "../CppCommon/src/util/random.hpp"
#include "../CppCommon/src/util/bitOperation.hpp"
#include "../CppCommon/src/util/container.hpp"
#include "../CppCommon/src/util/bitSet.hpp"
#include "../CppCommon/src/util/bitArray.hpp"
#include "../CppCommon/src/util/longBitSet.hpp"
#include "../CppCommon/src/util/bitPartition.hpp"
#include "../CppCommon/src/util/math.hpp"
#include "../CppCommon/src/util/pd.hpp"
#include "../CppCommon/src/util/search.hpp"
#include "../CppCommon/src/hash/hashFunc.hpp"
#include "../CppCommon/src/hash/hashBook.hpp"
#include "../CppCommon/src/util/softmaxPolicy.hpp"
#include "../CppCommon/src/util/lock.hpp"

namespace DotsAndBoxes{
    
    /**************************設定**************************/
    
    constexpr int LENGTH_X = 5;
    constexpr int LENGTH_Y = 5;
    
    /**************************基本的定義**************************/
    
    std::mt19937 dice((unsigned int)time(NULL));
    
    // LX, LY をマス目の数とする
    //  _ _ _ _
    // |_|_|_|_|
    // |_|_|_|_|
    // |_|_|_|_|
    
    //  __
    // |  |
    // |_a|
    constexpr int SIZE = LENGTH_X * LENGTH_Y;
    
    constexpr int LX = LENGTH_X + 2;
    constexpr int LY = LENGTH_Y + 2;
    
    constexpr int CELLS = LX * LY;
    
    constexpr int VX = LENGTH_X;
    constexpr int VY = LENGTH_Y + 1;
    
    constexpr int VLINES = VX * VY;
    
    constexpr int HX = LENGTH_X + 1;
    constexpr int HY = LENGTH_Y;
    
    constexpr int HLINES = HX * HY;
    
    constexpr int MAX_PLY = VLINES + HLINES;
    constexpr int MAX_MOVES = VLINES + HLINES;
    
    constexpr int z2x(int z)noexcept{ return z / LY; }
    constexpr int z2y(int z)noexcept{ return z % LY; }
    constexpr int xy2z(int x, int y)noexcept{ return x * LY + y; }
    
    enum Color{
        B = 0, W = 1
    };
    Color operator ~(Color c)noexcept{ return Color(1 - (int)c); }
    
    enum VH : int16_t{
        V = 0, H = 1
    };
    constexpr VH operator ++(VH& vh)noexcept{
        vh = VH((int16_t)vh + 1); return vh;
    }
    const char *vhChar = "vh";
    
    enum Value{
        VALUE_INFINITE = 31000,
        VALUE_MATE = 30000,
        VALUE_NONE = -32000,
    };
    Value operator -(Value v)noexcept{ return Value(-(int)v); }
    
    enum Direction{
        DIR_0, DIR_1, DIR_2, DIR_3
    };
    constexpr Direction operator ++(Direction& dir)noexcept{
        dir = Direction((int)dir + 1); return dir;
    }
    
    Direction opposite(Direction dir)noexcept{
        return Direction(((int)dir + 2) % 4);
    }
    
    constexpr int dirDZ[4] = {-LY, -1, LY, 1};
    constexpr int move2cellDZ[4] = {0, 0, LY, 1};
    constexpr int cell2moveDZ[4] = {-LY, -1, 0, 0};
    
    Direction besideLineDirection[2][2] = {
        {DIR_1, DIR_3},
        {DIR_0, DIR_2}
    };
    
    VH dir2vh(Direction dir)noexcept{ return VH(1 - (dir & 1)); }
    
    std::string z2string(int z){
        return std::string(1, char(z2x(z) + 'A' - 1)) + std::to_string(int(z2y(z)));
    }
    int string2z(const std::string& str){
        return xy2z(int(str[0]) - int('A') + 1, int(str[1]) - int('0'));
    }
    
    /**************************マスの情報**************************/
    
    struct CellInfo{ // マス単位の情報
        uint8_t occupied; // 周囲4方向ですでに埋まった位置のビット
        uint8_t num; // 埋まった数
        int16_t reachIndex; // リーチ配列中の番号
        int count()const{ return num; }
        void addLine(Direction dir)noexcept{
            occupied |= 1 << dir;
            num += 1;
        }
        void removeLine(Direction dir)noexcept{
            occupied &= ~(1 << dir);
            num -= 1;
        }
        void setOccupied(uint8_t aoccupied)noexcept{
            occupied = aoccupied;
            num = countBits(occupied);
        }
        bool reach()const noexcept{ return num == 3; }
        bool full()const noexcept{ return num == 4; }
        void clear()noexcept{
            occupied = 0; num = 0;
            reachIndex = -1;
        }
        bool exam()const{
            if(countBits32(occupied) != num){
                cerr << "inconsistent num of occupied " << countBits32(occupied) << " <-> " << num << endl;
                return false;
            }
            return true;
        }
    };
    
    /**************************着手**************************/
    
    union Move{
        struct{
            VH vh; int16_t mz;
        };
        int32_t data_;
        void set(VH avh, int amz)noexcept{
            vh = avh; mz = amz;
        }
        bool operator ==(const Move& move)const noexcept{
            return data_ == move.data_;
        }
        bool operator !=(const Move& move)const noexcept{
            return data_ != move.data_;
        }
        Move() = default;
        Move(VH avh, int amz):vh(avh), mz(amz){}
    };

    const Move MOVE_NONE = Move(V, -1);
    
    std::ostream& operator <<(std::ostream& ost, const Move& move){
        if(move == MOVE_NONE){
            ost << "NoneMove";
        }else{
            ost << z2string(move.mz) << vhChar[move.vh];
        }
        return ost;
    }
    Move string2move(const std::string& str){
        int mz = string2z(str.substr(0, 2));
        VH vh = (((str[2] == 'v') || (str[2] == 'V')) ? V : H);
        return Move(vh, mz);
    }
    
    Move fillMove(const CellInfo& cell, int z){
        Direction dir = Direction(bsf32(~cell.occupied));
        int mz = z + cell2moveDZ[dir];
        VH vh = dir2vh(dir);
        return Move(vh, mz);
    }
    
    uint64_t maskBB[64] = {0}; // z -> lines
    uint64_t lineBB[64] = {0}; // line -> zs
    
    /*struct BitBoard{
        
        uint64_t l;
        static int XYtoZ(int x, int y)noexcept{
            return x * LY + y;
        }
        int count(int z)const{
            return countBits64(l & maskBB[z]);
        }
        void clear()noexcept{ l = 0; }
        int move(VH vh, int ez){
            l |= 1ULL << (VH * 32 + ez);
            int diff = 0;
            if(l & maskBB[ez - L])
        }
    };*/
    
    /*union ExtMove{
        struct{
            Move move;
            Value value;
        };
        uint64_t data_;
        
        ExtMove() = default;
        ExtMove(Move amove, Value avalue):
        move(amove), value(avalue){}
        void set(VH avh, int amz){
            move.set(avh, amz);
        }
        ExtMove(const ExtMove& aemv): data_(aemv.data_){}
    };*/
    
    /**************************盤面**************************/
    
    uint64_t lineKeyTable[CELLS][2];
    
    template<class board_t>
    uint64_t genLineKey(const board_t& bd){ // 1から計算
        uint64_t key = 0;
        for(int z = 0; z < CELLS; ++z){
            if(bd.line(z, DIR_2)){
                key ^= lineKeyTable[z][H];
            }
            if(bd.line(z, DIR_3)){
                key ^= lineKeyTable[z][V];
            }
        }
        return key;
    }
    
    std::string lightString(const std::string& str){
        return "\033[0m" + fat(str) + "\033[0m";
    }
    std::string normalString(const std::string& str){
        return "\033[36m" + str + "\033[0m";
    }
    std::string fullString(const std::string& str){
        return "\033[40m" + str + "\033[0m";
    }
    std::string reachString(const std::string& str, bool light){
        return "\033[46m" + (light ? lightString(str) : normalString(str)) + "\033[0m";
    }
    
    struct Board; // 変換用
    
    struct MiniBoard{
        int ply, turn;
        int occupied[CELLS];
        int area[2];
        
        bool filled()const{ return ply >= MAX_PLY; }
        bool full(int z)const{ return occupied[z] == 15; }
        bool reach(int z)const{ return countBits32(occupied[z]) == 3; }
        bool line(int z, Direction dir)const{ return (occupied[z] >> dir) & 1; }
        Color turnColor()const{ return Color(turn % 2); }
        Color winner()const{ return area[B] > area[W] ? B : W; }
        
        MiniBoard(){}
        MiniBoard(const Board&);
        void clear(){
            memset(occupied, 0, sizeof(occupied));
            ply = turn = area[0] = area[1] = 0;
        }
        void move(Move mv){
            int newArea = 0;
            VH vh = mv.vh;
            int mz = mv.mz;
            int tz;
            for(int b = 0; b < 2; ++b){
                Direction dir = besideLineDirection[mv.vh][b];
                tz = mz + move2cellDZ[dir];
                occupied[tz] |= 1 << opposite(dir);
                if(full(tz)){ newArea += 1; }
            }
            ply += 1;
            if(!newArea){
                turn += 1;
            }else{
                area[turnColor()] += newArea;
            }
        }
        std::string toString()const{
            std::ostringstream oss;
            oss << "turn " << turn << " ply " << ply << endl;
            for(int y = 0; y < LY - 1; ++y){
                oss << " " << y;
            }
            oss << endl;
            // 最初の横線
            oss << char('A' - 1) << "  ";
            for(int y = 1; y < LY - 1; ++y){
                oss << (line(xy2z(0, y), DIR_2) ? lightString("_") : normalString("_")) << " ";
            }
            oss << endl;
            for(int x = 1; x < LX - 1; ++x){
                oss << char('A' + x - 1) << " ";
                // 最初の縦線
                oss << (line(xy2z(x, 0), DIR_3) ? lightString("|") : normalString("|"));
                // 縦線と横線を交互に引く
                for(int y = 1; y < LY - 1; ++y){
                    // 陣地確定とリーチの場合は背景色変更
                    if(full(xy2z(x, y))){
                        oss << fullString("_");
                    }else if(reach(xy2z(x, y))){
                        oss << reachString("_", bool(line(xy2z(x, y), DIR_2)));
                    }else{
                        oss << (line(xy2z(x, y), DIR_2) ? lightString("_") : normalString("_"));
                    }
                    oss << (line(xy2z(x, y), DIR_3) ? lightString("|") : normalString("|"));
                }
                oss << endl;
            }
            oss << " area " << area[0] << " - " << area[1] << endl;
            return oss.str();
        }
    };
    
    struct ReachInfo{
        int z; // マス座標
        int effect; // 何マス一気に獲得できるか
        bool operator <(const ReachInfo& ari)const{
            return effect > ari.effect;
        }
    };
    
    struct Board{
        
        int ply; // 線を引いた手数
        int turn; // 連続置きを1手と考えた時の手数
        BitSet64 lineSet[2]; // 引かれた線のビット集合
        CellInfo cell[CELLS]; // マス情報
        int64_t area[2]; // すでに出来た陣地の数
        uint64_t lineKey; // 線の配置のハッシュキー
        static constexpr int MAX_REACHES = SIZE;
        std::array<ReachInfo, MAX_REACHES> reachInfo; // 3つ繋がっているところ
        int numReaches; // リーチの数
        //std::priority_queue<ReachInfo> reach;
        int immediateReachEffect; // 1 turn で即取れる得点の下界
        
        
        bool filled()const{ return ply >= MAX_PLY; }
        bool full(int z)const{ return cell[z].full(); }
        bool reach(int z)const{ return cell[z].reach(); }
        bool line(int z, Direction dir)const{ return (cell[z].occupied >> dir) & 1; }
        Color turnColor()const{ return Color(turn % 2); }
        bool mate(){ // 得点差 + リーチの数
            return area[turnColor()] + immediateReachEffect > SIZE / 2;
        }
        uint64_t key()const noexcept{
            Color c = turnColor();
            return (lineKey & 0x000FFFFFFFFFFFFF) | (area[c] << 58) | (area[~c] << 52);
        }
        int count(int z)const{ return cell[z].count(); }
        int areaDiff(Color c)const noexcept{
            int diff = area[B] - area[W];
            return (c == B) ? diff : -diff;
        }
        Color winner()const noexcept{
            return area[B] > area[W] ? B : W;
        }
        bool valid(Move mv)const noexcept{
            Direction dir = besideLineDirection[mv.vh][0];
            return !line(mv.mz + move2cellDZ[dir], opposite(dir));
        }
        
        Board(const MiniBoard& mbd){
            ply = mbd.ply; turn = mbd.turn;
            area[0] = mbd.area[0]; area[1] = mbd.area[1];
            // 線の情報
            lineSet[0] = lineSet[1] = 0;
            for(int z = 0; z < CELLS; ++z){
                if(mbd.line(z, DIR_2)){ lineSet[H].set(z); }
                if(mbd.line(z, DIR_3)){ lineSet[V].set(z); }
            }
            lineKey = genLineKey(mbd);
            // マスの情報
            for(int z = 0; z < CELLS; ++z){
                cell[z].clear();
                cell[z].setOccupied(mbd.occupied[z]);
            }
            // リーチ処理
            clearReach();
            immediateReachEffect = 0;
            for(int z = 0; z < CELLS; ++z){
                if(countBits32(mbd.occupied[z]) == 3){
                    addReach(z);
                }
            }
        }
        
        // リーチ関係
        void addReach(int z){
            ReachInfo ri;
            ri.z = z;
            ri.effect = 1;
            //reach.push(ri);
            cell[z].reachIndex = numReaches;
            reachInfo[numReaches] = ri;
            numReaches += 1;
            immediateReachEffect += ri.effect;
            DERR << "add reach " << z2string(z) << " " << cell[z].reachIndex << endl;
        }
        void removeReach(int z){
            int reachIndex = cell[z].reachIndex;
            DERR << "remove reach " << z2string(z) << " " << reachIndex << endl;
            immediateReachEffect -= reachInfo[numReaches].effect;
            --numReaches;
            reachInfo[reachIndex] = reachInfo[numReaches];
            cell[reachInfo[reachIndex].z].reachIndex = reachIndex; // リーチ番号変更
            cell[z].reachIndex = -1;
        }
        void clearReach(){
            //reach.clear();
            numReaches = 0;
            immediateReachEffect = 0;
        }

        void clear()noexcept{
            // セル情報クリア
            for(int z = 0; z < CELLS; ++z){
                cell[z].clear();
            }
            lineSet[B] = lineSet[W] = 0;
            area[B] = area[W] = 0;
            ply = turn = 0;
            lineKey = 0;
            clearReach();
        }
        int move(VH vh, int mz){
            int newArea = 0;
            int tz;
            for(int b = 0; b < 2; ++b){
                lineSet[vh].set(mz);
                Direction dir = besideLineDirection[vh][b];
                tz = mz + move2cellDZ[dir];
                cell[tz].addLine(opposite(dir));
                if(cell[tz].full()){ newArea += 1; removeReach(tz);
                }else if(cell[tz].reach()){ addReach(tz); }
            }
            lineKey ^= lineKeyTable[mz][vh];
            ply += 1;
            if(!newArea){
                turn += 1;
            }else{
                area[turnColor()] += newArea;
            }
            DERR << toString();
            ASSERT(exam(), cerr << toString() << endl;);
            return newArea;
        }
        int move(Move mv){
            ASSERT(valid(mv), cerr << mv << " is not valid." << endl;);
            DERR << "move " << turnColor() << " " << mv << endl;
            return move(mv.vh, mv.mz);
        }
        void unmove(VH vh, int mz){
            int newArea = 0;
            int tz;
            lineKey ^= lineKeyTable[mz][vh];
            for(int b = 1; b >= 0; --b){
                Direction dir = besideLineDirection[vh][b];
                tz = mz + move2cellDZ[dir];
                if(cell[tz].reach()){ removeReach(tz);
                }else if(cell[tz].full()){ addReach(tz); newArea += 1; }
                cell[tz].removeLine(opposite(dir));
                lineSet[vh].reset(mz);
            }
            ply -= 1;
            if(!newArea){
                turn -= 1;
            }else{
                area[turnColor()] -= newArea;
            }
            ASSERT(exam(), cerr << toString() << endl;);
        }
        void unmove(Move mv){
            DERR << "unmove " << mv << endl;
            return unmove(mv.vh, mv.mz);
        }
        
        bool exam()const{
            // 線の一貫性確認
            for(int z = 0; z < CELLS; ++z){
                if(!cell[z].exam()){
                    cerr << "Board::exam() : invalid cell " << z2string(z) << endl;
                    return false;
                }
            }
            // lineSet <-> occupied
            for(VH vh = VH(0); (int)vh < 2; ++vh){
                for(int mz = 0; mz < CELLS; ++mz){
                    Direction dir = besideLineDirection[vh][1];
                    if(lineSet[vh].get(mz) != line(mz, dir)){
                        cerr << "Board::exam() : inconsistent lineSet against occupied ";
                        cerr << z2string(mz) << " " << dir << " ";
                        cerr << lineSet[vh].get(mz) << " <-> " << line(mz, dir) << endl;
                        return false;
                    }
                }
            }
            // 隣接マス同士の情報確認
            for(int x = 1; x < LX - 1; ++x){
                for(int y = 1; y < LY - 1; ++y){
                    int z = xy2z(x, y);
                    for(Direction dir = DIR_0; dir <= DIR_3; ++dir){
                        int tz = z + dirDZ[dir];
                        // z と tz の間の関係確認
                        // occupied
                        if(line(z, dir) != line(tz, opposite(dir))){
                            cerr << "Board::exam() : inconsistent adjucent occupied ";
                            cerr << z2string(z) << " " << line(z, dir) << " <-> ";
                            cerr << z2string(tz) << " " << line(tz, opposite(dir)) << endl;
                            return false;
                        }
                    }
                }
            }
            // リーチ
            for(int r = 0; r < numReaches; ++r){
                int z = reachInfo[r].z;
                if(!cell[z].reach()){
                    cerr << "Board::exam() : non-reach but in reach pool " << z2string(z) << endl;
                    return false;
                }
            }
            // 陣地数
            int numFull = 0;
            for(int z = 0; z < CELLS; ++z){
                if(cell[z].full()){
                    numFull += 1;
                }
            }
            if(area[0] + area[1] != numFull){
                cerr << "Board::exam() : inconsistent num of areas ";
                cerr << area[B] << " - " << area[W] << " <-> " << numFull << endl;
                return false;
            }
            return true;
        }
        std::string toString()const{
            std::ostringstream oss;
            oss << MiniBoard(*this).toString();
            oss << " reach" << endl;
            for(int r = 0; r < numReaches; ++r){
                oss << r << " " << z2string(reachInfo[r].z) << " " << reachInfo[r].effect << endl;
            }
            for(int x = 0; x < LX; ++x){
                for(int y = 0; y < LY; ++y){
                    DERR << " " << count(xy2z(x, y));
                }
                DERR << endl;
            }
            return oss.str();
        }
    };
    
    MiniBoard::MiniBoard(const Board& bd){
        memset(occupied, 0, sizeof(occupied));
        ply = bd.ply; turn = bd.turn;
        area[0] = bd.area[0]; area[1] = bd.area[1];
        for(int z = 0; z < CELLS; ++z){
            occupied[z] = bd.cell[z].occupied;
        }
    }
    
    /**************************盤面出力**************************/
    
    std::ostream& operator <<(std::ostream& ost, const MiniBoard& bd){
        ost << bd.toString();
        return ost;
    }
    
    std::ostream& operator <<(std::ostream& ost, const Board& bd){
        ost << bd.toString();
        return ost;
    }
    
    /**************************合法手**************************/
    
    template<class move_t, class board_t>
    int genAllMoves(move_t *const pmv0, const board_t& bd){
        // とりあえず全部生成
        move_t *pmv = pmv0;
        for(int x = 1; x < LX - 1; ++x){
            for(int y = 0; y < LY - 1; ++y){
                int z = xy2z(x, y);
                if(!bd.line(z, DIR_3)){
                    pmv->set(V, z); ++pmv;
                }
            }
        }
        for(int x = 0; x < LX - 1; ++x){
            for(int y = 1; y < LY - 1; ++y){
                int z = xy2z(x, y);
                if(!bd.line(z, DIR_2)){
                    pmv->set(H, z); ++pmv;
                }
            }
        }
        return pmv - pmv0;
    }
    
    /**************************初期化**************************/
    
    void initHash(){
        for(int z = 0; z < CELLS; ++z){
            lineKeyTable[z][V] = dice();
            lineKeyTable[z][H] = dice();
        }
    }
    
    struct DABInitializer{
        DABInitializer(){
            initHash();
        }
    };
    
    DABInitializer _initializer;
}

#endif // DAB_DAB_HPP_