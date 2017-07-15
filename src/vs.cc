/*
 vs.cc
 Katsuki Ohto
 */

#include "dab.hpp"
#include "agent.hpp"

using namespace DotsAndBoxes;

int battle(Color myColor, std::vector<Move> orecord){
    SearchAgent *const pa = new SearchAgent(15000);
    MiniBoard mbd;
    mbd.clear();
    std::vector<Move> record = orecord;
    for(auto move : record){
        cerr << mbd;
        mbd.move(move);
    }
    while(!mbd.filled()){
        mbd.clear();
        for(auto move : record){
            mbd.move(move);
        }
        
        cerr << mbd << endl;
        cerr << "record ";
        for(auto mv : record){ cerr << mv << " "; }
        cerr << endl;
        
        Move move;
        if(mbd.turnColor() == myColor){
            auto moveValue = pa->searchMove(mbd, 100);
            move = std::get<0>(moveValue);
            mbd.move(move);
        }else{
            std::string str;
            std::cin >> str;
            if(str == "back"){
                int lastOpp = 0;
                mbd.clear();
                for(auto move : record){
                    if(mbd.turnColor() == ~myColor){ lastOpp = max(lastOpp, mbd.ply); }
                    mbd.move(move);
                }
                for(int m = record.size() - 1; m >= lastOpp; --m){
                    record.pop_back();
                }
                continue;
            }
            move = string2move(str);
            mbd.move(move);
        }
        record.push_back(move);
    }
    return 0;
}

int main(int argc, char *argv[]){
    std::vector<Move> record;
    for(int c = 1; c < argc; ++c){
        if(!strcmp(argv[c], "-R")){
            for(int cc = c + 1; cc < argc; ++cc){
                if(!strcmp(argv[c], "-F")){
                    break;
                }
                std::string str = std::string(argv[cc]);
                Move move = string2move(str);
                record.push_back(move);
            }
        }
    }
    battle(W, record);
    return 0;
}