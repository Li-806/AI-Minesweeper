// ======================================================================
// FILE:        MyAI.cpp
//
// AUTHOR:      Jian Li
//
// DESCRIPTION: This file contains your agent class, which you will
//              implement. You are responsible for implementing the
//              'getAction' function and any helper methods you feel you
//              need.
//
// NOTES:       - If you are having trouble understanding how the shell
//                works, look at the other parts of the code, as well as
//                the documentation.
//
//              - You are only allowed to make changes to this portion of
//                the code. Any changes to other portions of the code will
//                be lost when the tournament runs your code.
// ======================================================================


#include "MyAI.hpp"


MyAI::MyAI ( int _rowDimension, int _colDimension, int _totalMines, int _agentX, int _agentY ) : Agent()
{
   // ======================================================================
   // YOUR CODE BEGINS
   // ======================================================================


   int num_of_tiles = _rowDimension * _colDimension - _totalMines;
   if (num_of_tiles < 0) {
       cout << "Invalid input: number of mines is greater than or equal to the number of tiles." << endl;
       exit(1);
   }
   else if (num_of_tiles == 0) {
       cout << "GG, you completed this level/world!" << endl;
       exit(0);
   }


   // ======================================================================
   // YOUR CODE ENDS
   // ======================================================================
};


Agent::Action MyAI::getAction( int number )
{
   // ======================================================================
   // YOUR CODE BEGINS
   // ======================================================================


   auto start_time = std::chrono::steady_clock::now();
   bool moved = false;
   while (!moved) {
       auto current_time = std::chrono::steady_clock::now();
       auto elapsed_time = current_time - start_time;
       if (elapsed_time > 10s) {
           //if almost out of time make random move
           //return {UNCOVER, agentX, agentY};
           moved = true;
       }
       //normal AI logic here, if it can make a move, return the move
       // and set moved to true, otherwise keep looping until time runs out


       //for now, brekaing the loop to avoid infinte loop (REMOVE AFTER IMPLETEING AI LOGIC)
       break;
   }
      




   return {LEAVE,-1,-1};
   //return {UNCOVER,agentX,agentY};




   // ======================================================================
   // YOUR CODE ENDS
   // ======================================================================


}


// ======================================================================
// YOUR CODE BEGINS
// ======================================================================






// ======================================================================
// YOUR CODE ENDS
// ======================================================================
