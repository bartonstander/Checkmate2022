#include <iostream>
#include "..\\MakeTables\\CheckmateGeneral.h"
Checkmate gCheckmate; // a "smart" checkmate object

int main()
{
	bool loadData = false;
	std::vector< PIECE_TYPES> pieces;
	pieces.push_back(PIECE_TYPES::BLACK_KING);
	pieces.push_back(PIECE_TYPES::WHITE_KING);
	pieces.push_back(PIECE_TYPES::WHITE_BISHOP);
	pieces.push_back(PIECE_TYPES::WHITE_KNIGHT);
//	pieces.push_back(WHITE_ROOK);
	gCheckmate.Initialize(pieces, loadData );

	std::cout << "Done Initializing Data." << std::endl;
	//system("pause");

	return 0;
}
