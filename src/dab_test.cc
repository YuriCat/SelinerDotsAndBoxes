/*
 dab_test.cc
 Katsuki Ohto
 */

#include "dab.hpp"
#include "agent.hpp"

int main(int argc, char *argv[]){
    
    using namespace DotsAndBoxes;
    
    MiniBoard mbd;
    mbd.clear();
    
    cerr << mbd << endl;
    const int N = 100;
    
    SearchAgent *const pa0 = new SearchAgent(980);
    SearchAgent *const pa1 = new SearchAgent(980);
    
    int w[2] = {0};
    for(int n = 0; n < N; ++n){
        mbd.clear();
        // ランダムに線を引く
        int numRandomLine = dice() % 8;
        for(int i = 0; i < numRandomLine; ++i){
            auto moveValue = randomMove(mbd);
            Move move = std::get<0>(moveValue);
            mbd.move(move);
        }
        pa0->initialize(); pa1->initialize();
        
        while(!mbd.filled()){
            std::tuple<Move, Value> moveValue;
            if(mbd.turnColor() == B){
                moveValue = pa0->searchMove(mbd, 2);
                //moveValue = randomMove(mbd);
            }else{
                moveValue = pa1->searchMove(mbd, 10);
                //moveValue = randomMove(mbd);
            }
            Move move = std::get<0>(moveValue);
            mbd.move(move);
            cerr << move << endl;
            cerr << mbd << endl;
        }
        w[mbd.winner()] += 1;
        cerr << w[0] << " - " << w[1] << endl;
    }
    return 0;
}