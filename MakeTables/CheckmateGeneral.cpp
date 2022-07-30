/*
This program evaluates chess board positions for up to 4 pieces, which must include the two kings.
By Barton Stander
September, 2009
More in June, 2014
More in August, 2020
*/
#include <iostream>
#include <iomanip>
#include <ctime>
#include <assert.h>
#include <fstream>
#include <algorithm>
using namespace std;
#include "CheckmateGeneral.h"

int min(int x, int y)
{
	return x <= y ? x : y;
}

int max(int x, int y)
{
	return x >= y ? x : y;
}

#define FULL_SYMMETRY_CONVERSION 0
#define PARTIAL_SYMMETRY_CONVERSION 0

void SymmetryConversion(int &blackKing, int &whiteKing, int &other1, int &other2)
{
#if FULL_SYMMETRY_CONVERSION || PARTIAL_SYMMETRY_CONVERSION
	int bk_row = blackKing / 8;
	int bk_column = blackKing % 8;
	int wk_row = whiteKing / 8;
	int wk_column = whiteKing % 8;
	int o1_row = other1 / 8;
	int o1_column = other1 % 8;
	int o2_row = other2 / 8;
	int o2_column = other2 % 8;
	if (bk_column >= 4)
	{
		// flip over vertical middle axis
		bk_column = 7 - bk_column;
		wk_column = 7 - wk_column;
		o1_column = 7 - o1_column;
		o2_column = 7 - o2_column;
	}

#if FULL_SYMMETRY_CONVERSION
	if (bk_row >= 4)
	{
		// flip over horizontal middle axis
		bk_row = 7 - bk_row;
		wk_row = 7 - wk_row;
		o1_row = 7 - o1_row;
		o2_row = 7 - o2_row;
	}
	blackKing = bk_row * 8 + bk_column;
	if (blackKing == 8 || blackKing == 16 || blackKing == 17 || blackKing == 24 || blackKing == 25 || blackKing == 26)
	{
		// flip over diagonal axis
		int temp = bk_row;
		bk_row = bk_column;
		bk_column = temp;

		temp = wk_row;
		wk_row = wk_column;
		wk_column = temp;

		temp = o1_row;
		o1_row = o1_column;
		o1_column = temp;

		temp = o2_row;
		o2_row = o2_column;
		o2_column = temp;
	}
#endif

	blackKing = bk_row * 8 + bk_column;
	whiteKing = wk_row * 8 + wk_column;
	other1 = o1_row * 8 + o1_column;
	other2 = o2_row * 8 + o2_column;
#endif

}

/////////////////////////////////////////////// Public Methods ///////////////////////////////////////////////

Checkmate::Checkmate()
//	: B(NULL), S(NULL)
{
	mLegalMovesRawMemoryRequested = 0;
	mLegalMovesRawMemory = NULL;
	mLegalMovesRawMemoryIndex = 0;
	mLegalMoves2 = NULL;
	B = NULL;
	S = NULL;
	mTotalPositions = 0;
}

void Checkmate::Initialize(const std::vector< PIECE_TYPES> & pieces, bool loadData, bool printEvaluation)
{
	time_t t1 = time(0);

	Assert(pieces[0] == PIECE_TYPES::BLACK_KING && pieces[1] == PIECE_TYPES::WHITE_KING, "p0==BLACK_KING && p1==WHITE_KING");
	Assert(NUM_PIECES == pieces.size(), "NUM_PIECES == pieces.size(). Update NUM_PIECES!");
	mPieces = pieces;

	AllocateMemory(loadData, printEvaluation);

	if (loadData)
	{
		if (LoadTable1(printEvaluation, mPieces, B, S)) // Table was pre-made, and is now ready to go!
		{
//			CacheAllLegalMovesForAllPositions();
			if(printEvaluation)
				PrintEvaluation();
			return;
		}
		else
		{
			cout << "Error loading data files!" << endl;
			return;
		}
	}

	int moves = 0;

	cout << "Making the table data." << endl;
    cout << "Starting..." << endl;

	// Initialize graph vertices:
	InitBoardB();
	InitAllStatusBitsS();

	cout << "Checking From and To conversions:" << endl;
	CheckFromAndTo();

	cout << "Find the three kinds of illegal board configurations:" << endl;
    InitAdjacentKings();
	InitOnTop();
	InitBadPawns();
	InitCheckAndBadCheck();

	// Initialize graph edges:
	CacheAllLegalMovesForAllPositions(); // for speed. But costs a lot of memory.


	InitInsufficientMaterial();
	InitIsStalemate();
	InitIsCheckmate();

	AssignPawnPromotions(PIECE_TYPES::WHITE_PAWN, PIECE_TYPES::WHITE_QUEEN, 7);
	AssignPawnPromotions(PIECE_TYPES::BLACK_PAWN, PIECE_TYPES::BLACK_QUEEN, 0);

	// Find "Mate In X" positions:
	cout << endl;
	moves=1;
	while(true)
	{
		int count = IsMateInX(moves);
		if(count==0)
			break;
		count = IsResponseMateInX(moves);
		if(count==0)
			break;
		moves++;
	}

	// Find "Insufficient Material In X" positions:
	cout << endl;
	moves=1;
	while(true)
	{
		int count = CanInsufficientMaterialInX(moves);
		if(count==0)
			break;
		count = CanResponseInsufficientMaterialInX(moves);
		if(count==0)
			break;
		moves++;
	}

	SwitchMovecountValues();

	if(printEvaluation)
		PrintEvaluation();

	SaveTable1(mPieces);

	time_t t2 = time(0);
	cout << "Total Initialize time in seconds is: " << (double)(t2 - t1) << endl;
}

void Checkmate::AllocateMemory(bool loadData, bool printEvaluation)
{
	mLegalMovesRawMemory = NULL;
	mLegalMovesRawMemoryIndex = 0;
	mLegalMoves2 = NULL;
	B = NULL;
	S = NULL;

	// The table isn't pre-made, so we have to make it.
	//const int TOTAL_POSITIONS = 2 * KING_SQUARES * KING_SQUARES * OTHER_SQUARES * OTHER_SQUARES;
	mTotalPositions = 2 * KING_SQUARES * KING_SQUARES;
	for (unsigned int i = 2; i < mPieces.size(); i++)
		mTotalPositions *= OTHER_SQUARES;
	mLegalMovesRawMemoryRequested = mTotalPositions * (long long)10;

	try
	{
		// If we are loading the data, we only need the B array.
		if (!loadData)
		{
			std::cout << "Trying to get " << mLegalMovesRawMemoryRequested << " unsigned ints of RAW_MEMORY for mLegalMovesRawMemory..." << endl;
			mLegalMovesRawMemory = new unsigned int[mLegalMovesRawMemoryRequested]; // *29 = 4,014,899,200 bytes
			std::cout << "Got the memory!" << endl;

			std::cout << "Trying to get " << mTotalPositions + 1 << " long longs of RAW_MEMORY for mLegalMoves2..." << endl;
			mLegalMoves2 = new long long[mTotalPositions + 1];
			std::cout << "Got the memory!" << endl;
		}
		if (!loadData || printEvaluation)
		{
			std::cout << "Trying to get " << mTotalPositions << " bytes of RAW_MEMORY for S..." << endl;
			S = new unsigned char[mTotalPositions];
			std::cout << "Got the memory!" << endl;
		}

		std::cout << "Trying to get " << mTotalPositions << " bytes of RAW_MEMORY for B..." << endl;
		B = new char[mTotalPositions];
		std::cout << "Got the memory!" << endl;
	}
	catch (int e)
	{
		std::cout << "An exception occurred. Exception getting initial memory. " << e << '\n';
		if (mLegalMovesRawMemory)
			delete[] mLegalMovesRawMemory;
		if (B)
			delete[] B;
		if (S)
			delete[] S;
		if (mLegalMoves2)
			delete[] mLegalMoves2;
		system("pause");
		system("exit");
	}

}

Checkmate::~Checkmate()
{
	if (mLegalMovesRawMemory)
		delete[] mLegalMovesRawMemory;
	if (B)
		delete[] B;
	if (S)
		delete[] S;
	if (mLegalMoves2)
		delete[] mLegalMoves2;
}

void Checkmate::FromIndex(int index, vector<int>& positions)
{
//	vector<int> temp(mPieces.size() + 1);
	for (size_t i = mPieces.size(); i > 0; i--)
	{
		if (i > 2)
		{
			positions[i] = index % OTHER_SQUARES;
			index = index / OTHER_SQUARES;
		}
		else
		{
			positions[i] = index % KING_SQUARES;
			index = index / KING_SQUARES;
		}
	}
	positions[0] = index;
//	positions = temp;
}

void Checkmate::FromIndex(int index, int positions[])
{
	//	vector<int> temp(mPieces.size() + 1);
	for (size_t i = POSITION_ARRAY_SIZE-1; i > 0; i--)
	{
		if (i > 2)
		{
			positions[i] = index % OTHER_SQUARES;
			index = index / OTHER_SQUARES;
		}
		else
		{
			positions[i] = index % KING_SQUARES;
			index = index / KING_SQUARES;
		}
	}
	positions[0] = index;
	//	positions = temp;
}

int Checkmate::ToIndex(const std::vector<int>& positions)
{
	//int index =	t*KING_SQUARES*KING_SQUARES*OTHER_SQUARES*OTHER_SQUARES + 
	//			i*KING_SQUARES*OTHER_SQUARES*OTHER_SQUARES + 
	//			j*OTHER_SQUARES*OTHER_SQUARES + 
	//			k*OTHER_SQUARES + 
	//			l;

	//int index = OTHER_SQUARES * (OTHER_SQUARES * (KING_SQUARES * (t * KING_SQUARES + i) + j) + k) + l;
	int index = positions[0]; // the turn t
	for (unsigned int i = 1; i < positions.size(); i++)
	{
		if (i <= 2)
			index *= KING_SQUARES;
		else
			index *= OTHER_SQUARES;
		index += positions[i];
	}
	return index;
}

int Checkmate::ToIndex(const int positions[])
{
	int index = positions[0]; // the turn t
	for (unsigned int i = 1; i < POSITION_ARRAY_SIZE; i++)
	{
		if (i <= 2)
			index *= KING_SQUARES;
		else
			index *= OTHER_SQUARES;
		index += positions[i];
	}
	return index;
}

void Checkmate::InitBoardB()
{
	cout << "The total board positions are ";
	CoutLongLongAsCommaInteger(mTotalPositions);
	cout << "Initializing all board values to \"UNKNOWN\"..." << endl;
	for (int p = 0; p < mTotalPositions; p++)
		B[p] = UNKNOWN;
}

void Checkmate::InitAllStatusBitsS()
{
	cout << "Initializing all board status bits to 0...\n" << endl;
	for (int p = 0; p < mTotalPositions; p++)
		S[p] = 0;
}

void Checkmate::CheckFromAndTo()
{
	for (int p = 0; p < mTotalPositions; p++)
	{
		//vector<int> positions(POSITION_ARRAY_SIZE);
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		int p2 = ToIndex(positions);
		if (p != p2)
		{
			cout << "Error. " << p << " " << p2 << endl;
			system("pause");
		}
	}

}

void Checkmate::InitAdjacentKings()
{
	// 484 of the 4096 king combinations have kings adjacent, or on top.
	// or when *2 for whose move it is, 968 of 8192
	int count = 0;
	cout << "Initializing some board status bits to KINGS_ADJACENT... ";

	for (int p = 0; p < mTotalPositions; p++)
	{
		//vector<int> positions(POSITION_ARRAY_SIZE);
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		int i = positions[1]; // first king
		int j = positions[2]; // second king
		int bk_row = i / 8;
		int bk_column = i % 8;
		int wk_row = j / 8;
		int wk_column = j % 8;
		if (abs(bk_row - wk_row) <= 1 && abs(bk_column - wk_column) <= 1)
		{
			S[p] |= KINGS_ADJACENT;
			count++;
		}
	}

	CoutLongAsCommaInteger(count);
}


void Checkmate::InitOnTop()
{
	int count = 0;
	cout << "Initializing some board status bits to ON_TOP...         ";
	for (int p = 0; p < mTotalPositions; p++)
	{
		if (IsLegalPosition(p)) // don't make something illegal for multiple reasons
		{
			int positions[POSITION_ARRAY_SIZE];
			FromIndex(p, positions);
			std::sort(positions+1, positions+ POSITION_ARRAY_SIZE);
			for ( int i = 1; i < POSITION_ARRAY_SIZE - 1; i++)
			{
				if (positions[i] == positions[i + 1] && positions[i] != DEAD_POSITION)
				{
					S[p] |= ON_TOP;
					count += 1;
					break;
				}
			}
		}
	}
	CoutLongAsCommaInteger(count);
}

void Checkmate::InitBadPawns()
{
	int count = 0;
	cout << "Initializing some board status bits to BAD_PAWNS...         ";
	for (int p = 0; p < mTotalPositions; p++)
	{
		if (IsLegalPosition(p)) // don't make something illegal for multiple reasons
		{
			int positions[POSITION_ARRAY_SIZE];
			FromIndex(p, positions);
			for (int i = 3; i < POSITION_ARRAY_SIZE; i++)
			{
				int row = positions[i] / 8;
				int pieceIndex = i - 1;
				if (mPieces[pieceIndex] == PIECE_TYPES::WHITE_PAWN && row == 0)
				{
					S[p] |= BAD_PAWN;
					count += 1;
					break;
				}
				if (mPieces[pieceIndex] == PIECE_TYPES::BLACK_PAWN && row == 7)
				{
					S[p] |= BAD_PAWN;
					count += 1;
					break;
				}
			}
		}
	}
	CoutLongAsCommaInteger(count);
}

bool Checkmate::IsLegalPosition(int position)
{
	if (S != NULL)
	{
		char s = S[position];
		if (s & ON_TOP)
			return false;
		if (s & KINGS_ADJACENT)
			return false;
		if (s & BAD_PAWN)
			return false;
		if (s & BAD_CHECK)
			return false;
	}
	else
	{
		if (B[position] == ILLEGAL)
			return false;
	}
	return true;
}

// Mark the status bits for all positions representing a Check or a Bad Check.
void Checkmate::InitCheckAndBadCheck()
{
	int countBad = 0;
	int countLegal = 0;

	for (int p = 0; p < mTotalPositions; p++)
	{
		if (IsLegalPosition(p))
		{
			int positions[POSITION_ARRAY_SIZE];
			FromIndex(p, positions);
			PIECE_COLOR  turn = (PIECE_COLOR)positions[0];
//			int i = positions[1]; // first king
//			int j = positions[2]; // second king
			for (int r = 3; r < POSITION_ARRAY_SIZE; r++) // loop through the rest of the piece indices
			{
				int piecePosition = positions[r];
				if (piecePosition == DEAD_POSITION)
					continue; // this piece is dead
				int pieceIndex = r - 1; // positions are 1 based, but mPieces are 0 based.
				PIECE_TYPES currentPiece = mPieces[pieceIndex];
				switch (currentPiece)
				{
				case PIECE_TYPES::WHITE_QUEEN:
					if (IsQueenAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::WHITE)) // Is White Queen attacking Black King, regardless of turn?
						if (turn == PIECE_COLOR::BLACK) // black's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // white's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::BLACK_QUEEN:
					if (IsQueenAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::BLACK)) // Is Black Queen attacking White King?
						if (turn == PIECE_COLOR::WHITE) // white's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // black's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::WHITE_ROOK:
					if (IsRookAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::WHITE)) // Is White attacking Black King?
						if (turn == PIECE_COLOR::BLACK) // black's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // white's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::BLACK_ROOK:
					if (IsRookAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::BLACK)) // Is Black attacking White King?
						if (turn == PIECE_COLOR::WHITE) // white's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // black's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
					
				case PIECE_TYPES::WHITE_BISHOP:
					if (IsBishopAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::WHITE)) // Is White attacking Black King?
						if (turn == PIECE_COLOR::BLACK) // black's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // white's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
					
				case PIECE_TYPES::BLACK_BISHOP:
					if (IsBishopAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::BLACK)) // Is Black attacking White King?
						if (turn == PIECE_COLOR::WHITE) // white's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // black's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::WHITE_KNIGHT:
					if (IsKnightAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::WHITE)) // Is White attacking Black King?
						if (turn == PIECE_COLOR::BLACK) // black's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // white's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::BLACK_KNIGHT:
					if (IsKnightAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::BLACK)) // Is Black attacking White King?
						if (turn == PIECE_COLOR::WHITE) // white's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // black's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;

				case PIECE_TYPES::WHITE_PAWN:
					if (IsPawnAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::WHITE)) // Is White attacking Black King?
						if (turn == PIECE_COLOR::BLACK) // black's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // white's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;
				case PIECE_TYPES::BLACK_PAWN:
					if (IsPawnAttackingEnemyKing(p, pieceIndex, PIECE_COLOR::BLACK)) // Is Black attacking White King?
						if (turn == PIECE_COLOR::WHITE) // white's turn. Normal check:
						{
							S[p] |= IN_CHECK;
							countLegal += 1;
						}
						else // black's turn. Bad check!:
						{
							S[p] |= BAD_CHECK;
							countBad += 1;
						}
					break;

				} // switch
			} // for r
		} // IsLegalPosition
	} // for p

	cout << "Initializing some board status bits to BAD_CHECK...      ";
	CoutLongAsCommaInteger(countBad);
	cout << "\nInitializing some board status bits to IN_CHECK... ";
	CoutLongAsCommaInteger(countLegal);
}

// p is the complete board position, containing all piece positions and the turn.
// pieceIndex references into mPieces, and here it must be 2 or greater.
// player refers to who owns the attacking Bishop, WHITE or BLACK
// return true if said piece is attacking enemy, regardless of whose turn it is.
bool Checkmate::IsBishopAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player)
{
	Assert(pieceIndex >=2, "pieceIndex>=2");
	Assert(player == PIECE_COLOR::WHITE || player == PIECE_COLOR::BLACK, "player==WHITE || player==BLACK");

	int positions[POSITION_ARRAY_SIZE];
	FromIndex(p, positions);
	int i = positions[1]; // first king
	int j = positions[2]; // second king

	int defending_king_row = (player == PIECE_COLOR::WHITE) ? (i / 8) : (j / 8);
	int defending_king_column  = (player == PIECE_COLOR::WHITE) ? (i % 8) : (j % 8);
	int bish_row = positions[pieceIndex+1] / 8;
	int bish_column = positions[pieceIndex+1] % 8;

	// check for up-left diagonal attack:
	if (defending_king_column + defending_king_row == bish_column + bish_row) // bishop is in same diagonal as defending king
	{
		// check if any other piece is blocking the attack.
		bool blocking = false;
		for (int r = 1; r < POSITION_ARRAY_SIZE; r++) // loop through the rest of the piece indices
		{
			int k = positions[r];
			if (k == DEAD_POSITION)
				continue; // dead pieces can't block
			int other_row = k / 8;
			int other_column = k % 8;
			if (defending_king_column + defending_king_row == other_column + other_row) // blocking piece in same diagonal
				if ((other_column > min(defending_king_column, bish_column) && other_column < max(defending_king_column, bish_column))) // blocking piece in between defending king and attacking bishop
					blocking = true;
		}
		if (!blocking)
			return true; // in check, not blocked
	}

	// check for up-right diagonal attack:
	if (7 - defending_king_column + defending_king_row == 7 - bish_column + bish_row)
	{
		// check if any other piece is blocking the attack.
		bool blocking = false;
		for (int r = 1; r < POSITION_ARRAY_SIZE; r++) // loop through the rest of the piece indices
		{
			int k = positions[r];
			if (k == DEAD_POSITION)
				continue; // dead pieces can't block
			int other_row = k / 8;
			int other_column = k % 8;
			if (7 - defending_king_column + defending_king_row == 7 - other_column + other_row)
				if ((other_column > min(defending_king_column, bish_column) && other_column < max(defending_king_column, bish_column)))
					blocking = true;
		}
		if (!blocking)
			return true; // in check, not blocked
	}

	return false;
}

// p is the complete board position, containing all piece positions and the turn.
// pieceIndex references into mPieces, and here it must be 2 or greater.
// player refers to who owns the attacking rook, WHITE or BLACK
// return true if said piece is attacking enemy, regardless of whose turn it is.
bool Checkmate::IsRookAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player)
{
	int positions[POSITION_ARRAY_SIZE];
	FromIndex(p, positions);
	int i = positions[1]; // first king
	int j = positions[2]; // second king

	int defending_king_row = (player == PIECE_COLOR::WHITE) ? (i / 8) : (j / 8);
	int defending_king_column = (player == PIECE_COLOR::WHITE) ? (i % 8) : (j % 8);
	int rook_row = positions[pieceIndex+1] / 8;
	int rook_column = positions[pieceIndex+1] % 8;

	// horizontal attack:
	if (defending_king_row == rook_row) // rook is in same row as defending king
	{
		// check if any other piece is blocking the attack.
		bool blocking = false;
		for (int r = 1; r < POSITION_ARRAY_SIZE; r++) // loop through the rest of the piece indices
		{
			int k = positions[r];
			if (k == DEAD_POSITION)
				continue; // dead pieces can't block
			int other_row = k / 8;
			int other_column = k % 8;
			if (other_row == defending_king_row) // blocking piece in same row
				if (other_column > min(defending_king_column, rook_column) && other_column < max(defending_king_column, rook_column)) // blocking piece in between defending king and attacking rook
					blocking = true;
		}
		if (!blocking)
			return true; // in check, not blocked
	}

	// vertical attack:
	if (defending_king_column == rook_column)
	{
		// check if any other piece is blocking the attack.
		bool blocking = false;
		for (int r = 1; r < POSITION_ARRAY_SIZE; r++) // loop through the rest of the piece indices
		{
			int k = positions[r];
			if (k == DEAD_POSITION)
				continue; // dead pieces can't block
			int other_row = k / 8;
			int other_column = k % 8;
			if (other_column == defending_king_column)
				if (other_row > min(defending_king_row, rook_row) && other_row < max(defending_king_row, rook_row)) // blocking piece in between defending king and attacking rook
					blocking = true;
		}
		if (!blocking)
			return true; // in check, not blocked
	}

	return false;
}

// p is the complete board position, containing all piece positions and the turn.
// pieceIndex references into mPieces, and here it must be 2 or greater.
// player refers to who owns the attacking queen, WHITE or BLACK
// return true if said Queen is attacking enemy, regardless of whose turn it is.
bool Checkmate::IsQueenAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player)
{
	return IsRookAttackingEnemyKing(p, pieceIndex, player) || 
		IsBishopAttackingEnemyKing(p, pieceIndex, player);
}


// p is the complete board position, containing all piece positions and the turn.
// pieceIndex references into mPieces, and here it must be 2 or greater.
// player refers to who owns the attacking piece, WHITE or BLACK
// return true if said Knight is attacking enemy, regardless of whose turn it is.
bool Checkmate::IsKnightAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player)
{
	int positions[POSITION_ARRAY_SIZE];
	FromIndex(p, positions);
	int i = positions[1]; // first king
	int j = positions[2]; // second king

	int defending_king_row = (player == PIECE_COLOR::WHITE) ? (i / 8) : (j / 8);
	int defending_king_column = (player == PIECE_COLOR::WHITE) ? (i % 8) : (j % 8);
	int knight_row = positions[pieceIndex+1] / 8;
	int knight_column = positions[pieceIndex+1] % 8;

	if ((knight_row + 1 == defending_king_row && knight_column - 2 == defending_king_column) ||
		(knight_row + 1 == defending_king_row && knight_column + 2 == defending_king_column) ||
		(knight_row - 1 == defending_king_row && knight_column - 2 == defending_king_column) ||
		(knight_row - 1 == defending_king_row && knight_column + 2 == defending_king_column) ||
		(knight_row + 2 == defending_king_row && knight_column - 1 == defending_king_column) ||
		(knight_row + 2 == defending_king_row && knight_column + 1 == defending_king_column) ||
		(knight_row - 2 == defending_king_row && knight_column - 1 == defending_king_column) ||
		(knight_row - 2 == defending_king_row && knight_column + 1 == defending_king_column))
		return true;

	return false;
}

// p is the complete board position, containing all piece positions and the turn.
// pieceIndex references into mPieces, and here it must be 2 or greater.
// player refers to who owns the attacking piece, WHITE or BLACK
// return true if said Pawn is attacking enemy, regardless of whose turn it is.
bool Checkmate::IsPawnAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player)
{
	int positions[POSITION_ARRAY_SIZE];
	FromIndex(p, positions);
	int i = positions[1]; // first king
	int j = positions[2]; // second king

	int defending_king_row = (player == PIECE_COLOR::WHITE) ? (i / 8) : (j / 8);
	int defending_king_column = (player == PIECE_COLOR::WHITE) ? (i % 8) : (j % 8);
	int pawn_row = positions[pieceIndex + 1] / 8;
	int pawn_column = positions[pieceIndex + 1] % 8;

	if (player == PIECE_COLOR::WHITE)
	{
		if (defending_king_row == pawn_row + 1)
			if (defending_king_column == pawn_column + 1 || defending_king_column == pawn_column - 1)
				return true;
	}
	else
	{
		if (defending_king_row == pawn_row - 1)
			if (defending_king_column == pawn_column + 1 || defending_king_column == pawn_column - 1)
				return true;
	}

	return false;
}

void Checkmate::CacheAllLegalMovesForAllPositions()
{
	cout << "\nCaching all legal moves for all board positions..." << endl << endl;
	// Find and store the legal moves!
	for (int p = 0; p < mTotalPositions; p++)
	{
		CacheAllLegalMovesForThisPosition(p);
	}
	mLegalMoves2[mTotalPositions] = mLegalMovesRawMemoryIndex++;
	Assert(mLegalMovesRawMemoryIndex <= mLegalMovesRawMemoryRequested, "Error. Increase mLegalMovesRawMemoryRequested");
}

void Checkmate::CacheAllLegalMovesForThisPosition(int p)
{
	if (p== 17293269)
	{
		int zz = 5;
		zz++;
	}
	mLegalMoves2[p] = mLegalMovesRawMemoryIndex;

	if (!IsLegalPosition(p))
		return; // There are no legal moves if we start from an illegal position.

	int positions[POSITION_ARRAY_SIZE];
	FromIndex(p, positions);
	PIECE_COLOR turn = (PIECE_COLOR)positions[0];

	for (int pieceIndex = 0; pieceIndex < NUM_PIECES; pieceIndex++)
	{
		PIECE_TYPES currentPiece = mPieces[pieceIndex];

		if (GetColor(currentPiece) != turn)
			continue; // not this piece color's turn

		int piecePosition = positions[pieceIndex + 1];
		if (piecePosition == 64)
			continue; // dead pieces can't move

		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES];
		int legalMoveCount = 0;
		GatherLegalMovesForPiece(pieceIndex, positions,
			allLegalMoves, legalMoveCount);

		for (int i = 0; i < legalMoveCount; i++)
		{
			int newIndex = ToReplaceIndex(positions,
				pieceIndex, allLegalMoves[i].newPosition);

			if (mLegalMovesRawMemoryIndex >= mLegalMovesRawMemoryRequested)
				Assert(false, "mLegalMovesRawMemoryIndex >= mLegalMovesRawMemoryRequested");
			else
				mLegalMovesRawMemory[mLegalMovesRawMemoryIndex++] = newIndex;
		}
	}
}

PIECE_COLOR Checkmate::GetColor(PIECE_TYPES pt)
{
	if (pt < PIECE_TYPES::BLACK_KING)
		return PIECE_COLOR::WHITE;
	else if (pt < PIECE_TYPES::NONE)
		return PIECE_COLOR::BLACK;
	else
		return PIECE_COLOR::NO_COLOR; // this piece is not being used.
}

// Given that player <turn> moved <pieceIndex> to <newPiecePosition>, return the index of the new position.
int Checkmate::ToReplaceIndex(const int oldPositions[],
	int pieceIndex, int newPiecePosition)
{
	int positions[POSITION_ARRAY_SIZE];
	for (int i = 0; i < POSITION_ARRAY_SIZE; i++)
		positions[i] = oldPositions[i];

	positions[0] = 1-positions[0]; // the other player's turn
	positions[pieceIndex + 1] = newPiecePosition;

	// Check if piece at position i was captured
	for (int i = 3; i < POSITION_ARRAY_SIZE; i++)
		if (i != (pieceIndex+1) && positions[i] == newPiecePosition) 
			positions[i] = DEAD_POSITION;
	int newPositionIndex = ToIndex(positions);
	return newPositionIndex;
}


void Checkmate::GatherLegalMovesForPiece(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	PIECE_TYPES pieceType = mPieces[pieceIndex];
	switch (pieceType)
	{
	case PIECE_TYPES::WHITE_KING:
	case PIECE_TYPES::BLACK_KING:
		GatherLegalMovesForKing(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	case PIECE_TYPES::WHITE_BISHOP:
	case PIECE_TYPES::BLACK_BISHOP:
		GatherLegalMovesForBishop(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	case PIECE_TYPES::WHITE_ROOK:
	case PIECE_TYPES::BLACK_ROOK:
		GatherLegalMovesForRook(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	case PIECE_TYPES::WHITE_QUEEN:
	case PIECE_TYPES::BLACK_QUEEN:
		GatherLegalMovesForQueen(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	case PIECE_TYPES::WHITE_KNIGHT:
	case PIECE_TYPES::BLACK_KNIGHT:
		GatherLegalMovesForKnight(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	case PIECE_TYPES::WHITE_PAWN:
	case PIECE_TYPES::BLACK_PAWN:
		GatherLegalMovesForPawn(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
		break;
	}

}

void Checkmate::GatherLegalMovesForKing(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	int turn = positions[0];
	int kingPosition = positions[pieceIndex + 1];
	int row = kingPosition / 8;
	int column = kingPosition % 8;

	// all the places the king can move:
	for (int r = row - 1; r <= row + 1; r++)
		for (int c = column - 1; c <= column + 1; c++)
			// can't move off the board, and must move to a different square:
			if (r >= 0 && r <= 7 && c >= 0 && c <= 7 && (r != row || c != column))
			{
				int newPosition = r * 8 + c;
				int newPositions[POSITION_ARRAY_SIZE];
				for (int i = 0; i < POSITION_ARRAY_SIZE; i++)
					newPositions[i] = positions[i];

				newPositions[pieceIndex+1] = newPosition;
				newPositions[0] = 1 - newPositions[0]; // flip the turn.

				int deadPieceIndex = 0;
				bool capture = SameSquareOppositeColor(newPositions, pieceIndex, deadPieceIndex);
				if (capture)
					newPositions[deadPieceIndex+1] = DEAD_POSITION;

				// Can't move onto a covered square, or on top of my own piece
				int newIndex = ToIndex(newPositions);
				if (IsLegalPosition(newIndex))
				{
					int oldPosition = kingPosition;
					InsertAnotherLegalMove(pieceIndex, oldPosition, newPosition, allLegalMoves, legalMoveCount, capture, deadPieceIndex);
				}
			}
}

// measure the given potential position to see if a King's position is on top of a piece of its same color.
bool Checkmate::SameSquareSameColorKing(const int positions[])
{
	for (int kingNumber = 0; kingNumber < 2; kingNumber++)
		for (int i = 2; i < NUM_PIECES; i++)
		{
			if (GetColor(mPieces[kingNumber]) == GetColor(mPieces[i]) &&
				positions[kingNumber+1] == positions[i+1])
			{
				return true;
			}
		}
	return false;
}


bool Checkmate::SameSquareOppositeColor(const int positions[], int capturingPieceIndex, int& deadPieceIndex)
{
	PIECE_COLOR playerColor = GetColor(mPieces[capturingPieceIndex]); // 0 for white, 1 for black
	PIECE_COLOR otherPlayerColor = playerColor==PIECE_COLOR::WHITE ? PIECE_COLOR::BLACK : PIECE_COLOR::WHITE;
//	int piecePosition[4] = {i,j,k,l}; // same as position[1] through position[4]
	for (int pi = 0; pi < NUM_PIECES; pi++)
	{
		if (pi != capturingPieceIndex && 
			GetColor(mPieces[pi]) == otherPlayerColor &&
			positions[pi + 1] == positions[capturingPieceIndex + 1] && 
			positions[pi + 1] != DEAD_POSITION)
		{
			deadPieceIndex = pi;
			return true;
		}
	}
	return false;
}

bool Checkmate::SameSquareSameColor(const int positions[], int pieceIndex, PIECE_COLOR player)
{
	PIECE_COLOR playerColor = GetColor(mPieces[pieceIndex]); // 0 for white, 1 for black
	for (int pi = 0; pi < NUM_PIECES; pi++)
	{
		if (pi != pieceIndex && GetColor(mPieces[pi]) == playerColor &&
			positions[pi + 1] == positions[pieceIndex + 1] && positions[pi + 1] != DEAD_POSITION)
		{
			return true;
		}
	}
	return false;
}

void Checkmate::GatherLegalMovesForBishop(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	Assert(pieceIndex >= 2, "pieceIndex>=2");
	int bishopPosition = positions[pieceIndex + 1];
	Assert(bishopPosition != DEAD_POSITION, "bishopPosition != DEAD_POSITION");
	int bishop_row = bishopPosition / 8;
	int bishop_column = bishopPosition % 8;

	//	if (r == bishop_row) // already checked for this.
	//		return; // dead pieces can't move

	int c = 0;
	int r = 0;
	bool stop = false;
	PIECE_COLOR turn = (PIECE_COLOR)positions[0];

	// movement up-right:
	c = bishop_column + 1;
	r = bishop_row + 1;
	stop = false;
	while (c <= 7 && r <= 7 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c += 1;
		r += 1;
	}

	// movement up-left:
	c = bishop_column - 1;
	r = bishop_row + 1;
	stop = false;
	while (c >= 0 && r <= 7 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c -= 1;
		r += 1;
	}

	// movement down-right:
	c = bishop_column + 1;
	r = bishop_row - 1;
	stop = false;
	while (c <= 7 && r >= 0 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c += 1;
		r -= 1;
	}

	// movement down-left:
	c = bishop_column - 1;
	r = bishop_row - 1;
	stop = false;
	while (c >= 0 && r >= 0 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c -= 1;
		r -= 1;
	}
}

void Checkmate::GatherLegalMovesForRook(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	Assert(pieceIndex >= 2, "pieceIndex>=2");
	int rookPosition = positions[pieceIndex + 1];
	Assert(rookPosition != DEAD_POSITION, "rookPosition != DEAD_POSITION");
	int rook_row = rookPosition / 8;
	int rook_column = rookPosition % 8;

	//	if (r == rook_row) // already checked for this.
	//		return; // dead pieces can't move

	int c = 0;
	int r = 0;
	bool stop = false;
	PIECE_COLOR turn = (PIECE_COLOR)positions[0];

	// horizontal movement right:
	c = rook_column + 1;
	r = rook_row;
	stop = false;
	while (c <= 7 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c += 1;
	}

	// horizontal movement left:
	c = rook_column - 1;
	r = rook_row;
	stop = false;
	while (c >= 0 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		c -= 1;
	}

	// verticle movement up:
	c = rook_column;
	r = rook_row + 1;
	stop = false;
	while (r <= 7 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		r += 1;
	}

	// verticle movement down:
	c = rook_column;
	r = rook_row - 1;
	stop = false;
	while (r >= 0 && !stop)
	{
		IsPieceLegalToMoveHere(pieceIndex, turn, r, c, positions, stop, allLegalMoves, legalMoveCount);
		r -= 1;
	}

}

void Checkmate::GatherLegalMovesForQueen(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	GatherLegalMovesForBishop(pieceIndex, positions,
		allLegalMoves, legalMoveCount);
	GatherLegalMovesForRook(pieceIndex, positions,
		allLegalMoves, legalMoveCount);
}

void Checkmate::GatherLegalMovesForKnight(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	Assert(pieceIndex >= 2 , "pieceIndex>=2");
	int knightPosition = positions[pieceIndex + 1];
	Assert(knightPosition != DEAD_POSITION, "knightPosition != DEAD_POSITION");
	int r = knightPosition / 8;
	int c = knightPosition % 8;
//	if (r == 8) // already checked for this.
//		return; // dead pieces can't move

	bool stop = false;
	PIECE_COLOR turn = (PIECE_COLOR)positions[0];

	IsPieceLegalToMoveHere(pieceIndex, turn, r + 1, c - 2, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r + 1, c + 2, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r - 1, c - 2, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r - 1, c + 2, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r + 2, c - 1, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r + 2, c + 1, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r - 2, c - 1, positions, stop, allLegalMoves, legalMoveCount);
	IsPieceLegalToMoveHere(pieceIndex, turn, r - 2, c + 1, positions, stop, allLegalMoves, legalMoveCount);
}

void Checkmate::GatherLegalMovesForPawn(int pieceIndex, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	Assert(pieceIndex >= 2, "pieceIndex>=2");
	int pawnPosition = positions[pieceIndex + 1];
	Assert(pawnPosition != DEAD_POSITION, "pawnPosition != DEAD_POSITION");
	int pawn_row = pawnPosition / 8;
	int pawn_column = pawnPosition % 8;

	int c = 0;
	int r = 0;
	PIECE_COLOR turn = (PIECE_COLOR)positions[0];
	PIECE_COLOR playerColor = GetColor(mPieces[pieceIndex]);
	Assert(turn == playerColor, "turn == playerColor");

	int direction = (turn == PIECE_COLOR::WHITE) ? +1 : -1;
	int start_row = (turn == PIECE_COLOR::WHITE) ? 1 : 6;

	// verticle movement up (or down) 1:
	c = pawn_column;
	r = pawn_row + direction;
	if (r>=0 && r <= 7 && NoPieceHere(r, c, positions))
	{
		AddLegalMoveForPawn(pieceIndex, turn, r, c, positions, allLegalMoves, legalMoveCount);
		
		// Now try for verticle movement up 2:
		c = pawn_column;
		r = pawn_row + 2*direction;
		if (pawn_row == start_row && r>=0 && r <= 7 && NoPieceHere(r, c, positions))
			AddLegalMoveForPawn(pieceIndex, turn, r, c, positions, allLegalMoves, legalMoveCount);
	}

	// up (or down) and right:
	c = pawn_column + 1;
	r = pawn_row + direction;
	if (r>=0 && r <= 7 && c <= 7 && EnemyPieceHere(pieceIndex, r, c, positions))
	{
		AddLegalMoveForPawn(pieceIndex, turn, r, c, positions, allLegalMoves, legalMoveCount);
	}

	// up (or down) and left:
	c = pawn_column - 1;
	r = pawn_row + direction;
	if (r>=0 && r <= 7 && c>=0 && EnemyPieceHere(pieceIndex, r, c, positions))
	{
		AddLegalMoveForPawn(pieceIndex, turn, r, c, positions, allLegalMoves, legalMoveCount);
	}
}

// returns true if no piece is at location r,c.
bool Checkmate::NoPieceHere(int r, int c, const int positions[])
{
	int newPosition = r * 8 + c;
	for (int pi = 0; pi < NUM_PIECES; pi++)
	{
		if (positions[pi + 1] == newPosition)
		{
			return false; // there IS a piece here
		}
	}
	return true;
}

// returns true if there is a piece owned by the opposite color of pieceIndex at location r,c.
bool Checkmate::EnemyPieceHere(int pieceIndex, int r, int c, const int positions[])
{
	int newPosition = r * 8 + c;
	PIECE_COLOR playerColor = GetColor(mPieces[pieceIndex]); 
	PIECE_COLOR otherPlayerColor = playerColor == PIECE_COLOR::WHITE ? PIECE_COLOR::BLACK : PIECE_COLOR::WHITE;
	for (int pi = 0; pi < NUM_PIECES; pi++)
	{
		if (GetColor(mPieces[pi]) == otherPlayerColor &&
			positions[pi + 1] == newPosition)
		{
			return true;
		}
	}
	return false;
}


void Checkmate::AddLegalMoveForPawn(int pieceIndex, PIECE_COLOR turn, int r, int c, const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	int newPositions[POSITION_ARRAY_SIZE + 1]; // +1 just to make a bad warning go away.
	for (int i = 0; i < POSITION_ARRAY_SIZE; i++)
		newPositions[i] = positions[i];
	int newPosition = r * 8 + c;
	int positionIndex = pieceIndex + 1;
	newPositions[positionIndex] = newPosition;
	newPositions[0] = 1 - newPositions[0]; // flip turn.

	int deadPieceIndex = 0;
	bool capture = SameSquareOppositeColor(newPositions, pieceIndex, deadPieceIndex);
	if (capture)
	{
		newPositions[deadPieceIndex + 1] = DEAD_POSITION;
	}

	int newIndex = ToIndex(newPositions);
	if (IsLegalPosition(newIndex))
	{
		int oldPosition = positions[pieceIndex + 1];
		InsertAnotherLegalMove(pieceIndex, oldPosition, newPosition,
			allLegalMoves, legalMoveCount, capture, deadPieceIndex);
	}
}

// If r,c is legal for the given pieceIndex, add it to allLegalMoves.
// r,c is not legal if outside board boundaries.
// r,c is not legal if on a piece of the same color.
// Sets stop to true if r,c is on top of any other piece of either color.
void Checkmate::IsPieceLegalToMoveHere(int pieceIndex, PIECE_COLOR turn, int r, int c, const int positions[],
	bool& stop, LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	Assert(pieceIndex >= 2 && pieceIndex < NUM_PIECES, "pieceIndex>=2 && pieceIndex < NUM_PIECES");
	Assert(turn == PIECE_COLOR::WHITE || turn == PIECE_COLOR::BLACK, "turn==WHITE || turn==BLACK");

	stop = false;

	if (r < 0 || r>7 || c < 0 || c>7)
		return;

	int newPositions[POSITION_ARRAY_SIZE+1]; // +1 just to make a bad warning go away.
	for (int i = 0; i < POSITION_ARRAY_SIZE; i++)
		newPositions[i] = positions[i];
	int newPosition = r * 8 + c;
	int positionIndex = pieceIndex + 1;
	newPositions[positionIndex] = newPosition;
	newPositions[0] = 1 - newPositions[0]; // flip turn.

	if (SameSquareSameColor(newPositions, pieceIndex, turn))
	{
		// blocked by your own piece
		stop = true;
		return;
	}

	int deadPieceIndex = 0;
	bool capture = SameSquareOppositeColor(newPositions, pieceIndex, deadPieceIndex);
	if (capture)
	{
		newPositions[deadPieceIndex+1] = DEAD_POSITION;
		stop = true;
	}

	int newIndex = ToIndex(newPositions);
	if (IsLegalPosition(newIndex))
	{
		int oldPosition = positions[pieceIndex+1];
		InsertAnotherLegalMove(pieceIndex, oldPosition, newPosition, 
			allLegalMoves, legalMoveCount, capture, deadPieceIndex);
	}

}

void Checkmate::InsertAnotherLegalMove(int pieceIndex, int oldPosition, int newPosition,
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount,
	bool capture, int deadPieceIndex)
{
	allLegalMoves[legalMoveCount].pieceIndex = pieceIndex;
	allLegalMoves[legalMoveCount].oldPosition = oldPosition;
	allLegalMoves[legalMoveCount].newPosition = newPosition;
	allLegalMoves[legalMoveCount].capture = capture;
	if (capture)
	{
		allLegalMoves[legalMoveCount].pieceIndex2 = deadPieceIndex;
		allLegalMoves[legalMoveCount].oldPosition2 = newPosition;
		allLegalMoves[legalMoveCount].newPosition2 = DEAD_POSITION;
	}
	legalMoveCount++;
}

// BSFIX can eliminate this and other draw cases. Whatever isn't winnable is a draw.
void Checkmate::InitInsufficientMaterial()
{
	int count = 0;
	cout << "\nFinding \"Draw\" positions due to INSUFFICIENT_MATERIAL... ";

	for (int p = 0; p < mTotalPositions; p++)
	{
		if (IsLegalPosition(p))
		{
			int positions[POSITION_ARRAY_SIZE];
			FromIndex(p, positions);
			/*
			for (int t = 0; t < 2; t++)
				for (int i = 0; i < KING_SQUARES; i++)
					for (int j = 0; j < KING_SQUARES; j++)
						for (int k = 0; k < OTHER_SQUARES; k++)
							for (int l = 0; l < OTHER_SQUARES; l++)
							{
								// Legal
								if (IsLegalPosition(t, i, j, k, l))
								{
								*/

			// If there are two or more pieces on the board, there is sufficient material to mate
			// If there is only 1 piece, there is sufficient material unless its a knight or bishop
			// If there are no pieces, there is not sufficient material.
			int totalPieces = 0;
			int valuePieces = 0; // not bishops or knights
			for (int pi = 2; pi < NUM_PIECES; pi++)
			{
				if (positions[pi + 1] != DEAD_POSITION)
				{
					totalPieces++;
					if (!(mPieces[pi] == PIECE_TYPES::WHITE_BISHOP || mPieces[pi] == PIECE_TYPES::BLACK_BISHOP ||
						mPieces[pi] == PIECE_TYPES::WHITE_KNIGHT || mPieces[pi] == PIECE_TYPES::BLACK_KNIGHT))
					{
						valuePieces++;
					}
				}
			}

			if (totalPieces <= 1 && valuePieces == 0)
			{
				S[p] |= INSUFFICIENT_MATERIAL;
				B[p] = 0;
				count++;
			}
		} // if IsLegalPosition
	} // for p
	CoutLongAsCommaInteger(count);
}

// Check for IN_STALE_MATE, which means a legal position, NOT in Check, and no legal moves.
void Checkmate::InitIsStalemate()
{
	InitIsCheckmateOrStalemate(0);
}


// Check for IN_CHECK_MATE, which means a legal position, In Check, and no legal moves.
void Checkmate::InitIsCheckmate()
{
	InitIsCheckmateOrStalemate(IN_CHECK);
}


void Checkmate::InitIsCheckmateOrStalemate(char checkForCheckmate)
{
	int blackCount = 0;
	int whiteCount = 0;
	cout << "Initializing some board status bits to " << (checkForCheckmate ? "IN_CHECK_MATE" : "IN_STALE_MATE") << "... ";

	for (int p = 0; p < mTotalPositions; p++)
	{
		// Legal and check status is according to checkForCheckmate parameter
		if (IsLegalPosition(p) && (S[p] & IN_CHECK) == checkForCheckmate)
		{
			// ... and no legal moves
			if (GetLegalMovesCount(p) == 0)
			{
				int positions[POSITION_ARRAY_SIZE];
				FromIndex(p, positions);

				PIECE_COLOR t = (PIECE_COLOR)positions[0];
				if (t == PIECE_COLOR::WHITE) // white's turn
					whiteCount += 1;
				else
					blackCount += 1;
				S[p] |= (checkForCheckmate ? IN_CHECK_MATE : IN_STALE_MATE);
				B[p] = 0;
				/*
				if (mInUse[2] == mInUse[3])
				{
					if (t == WHITE)
						whiteCount += 1;
					else
						blackCount += 1;
					S[currentIndex] |= (checkForCheckmate ? IN_CHECK_MATE : IN_STALE_MATE);
					B[currentIndex] = 0;
				}
				*/

			} // if (GetLegalMovesCount(p) == 0)
		} // if IsLegalPosition
	} // for p
	cout << " (" << whiteCount << ") and (" << blackCount << ")" << endl;
}

void Checkmate::AssignPawnPromotions(PIECE_TYPES fromPawn, PIECE_TYPES toQueen, 
	int promotionRow) // promotionRow is 7 if fromPawn is PIECE_TYPES::WHITE_PAWN, 0 if BLACK_PAWN
{
	cout << "\nAssigning B and S for Pawn Promotions ";

	bool atLeastOnePawn = false;
	std::vector< PIECE_TYPES> mPiecesPromotedPawn = mPieces;
	for (int pi = 2; pi < NUM_PIECES; pi++)
	{
		if (mPiecesPromotedPawn[pi] == fromPawn)
		{
			atLeastOnePawn = true;
			mPiecesPromotedPawn[pi] = toQueen;
			break; // for multiple pawns, just switch the first to a queen.
		}
	}
	if (!atLeastOnePawn)
		return;

	bool needTableS = true;
	char* BPromotedPawns = NULL;
	unsigned char* SPromotedPawns = NULL;

	try
	{
		std::cout << "Trying to get " << mTotalPositions << " bytes for SPromotedPawns..." << endl;
		SPromotedPawns = new unsigned char[mTotalPositions];
		std::cout << "Got the memory!" << endl;

		std::cout << "Trying to get " << mTotalPositions << " bytes for BPromotedPawns..." << endl;
		BPromotedPawns = new char[mTotalPositions];
		std::cout << "Got the memory!" << endl;
	}
	catch (int e)
	{
		std::cout << "An exception occurred getting SPromotedPawns or BPromotedPawns. " << e << '\n';
		system("pause");
		exit(1);
	}
	if (!LoadTable1(needTableS, mPiecesPromotedPawn, BPromotedPawns, SPromotedPawns))
	{
		cout << "Error loading pawn promoted data files!" << endl;
		system("pause");
		exit(1);
	}

	for (int p = 0; p < mTotalPositions; p++)
	{
		if (IsLegalPosition(p))
		{
			int positions[POSITION_ARRAY_SIZE];
			FromIndex(p, positions);
			if (p == 19705214)
			{
				int zz = 5;
				zz++;
			}
			for (int pi = 2; pi < NUM_PIECES; pi++)
			{
				if (mPieces[pi] == fromPawn)
				{
					int pawnRow = positions[pi + 1] / 8;
					if (pawnRow == promotionRow) // promotion row for white or black
					{
						B[p] = BPromotedPawns[p];
						S[p] = SPromotedPawns[p];
					}
				}
			}
		}
	}
	delete[]BPromotedPawns;
	delete[]SPromotedPawns;
}

int Checkmate::GetLegalMovesCount(int currentPosition)
{
	int count = (int)(mLegalMoves2[currentPosition + 1] - mLegalMoves2[currentPosition]);
	return count;
}


// Check for white or black to mate in x
int Checkmate::IsMateInX(int x)
{
	int count = 0;
	//	cout << "Finding all board positions that are Mate In " << x << "... ";
	cout << x << ": ";
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		if (p == 8519)
		{
			int zz = 5;
			zz++;
		}
		// Check positions that are not yet know, yet legal:
		if (B[p] == UNKNOWN && IsLegalPosition(p))
		{
			//int positions[POSITION_ARRAY_SIZE];
			//FromIndex(p, positions);

			bool mateInX = false; // unless shown otherwise
			PIECE_COLOR t = (PIECE_COLOR)positions[0];
			int legalMoveCount = (int) (mLegalMoves2[p + 1] - mLegalMoves2[p]);
			long long rawIndex = mLegalMoves2[p];
			for (int m = 0; m < legalMoveCount; m++)
			{
				int newIndex = mLegalMovesRawMemory[rawIndex + m];
				int pos2[POSITION_ARRAY_SIZE];
				FromIndex(newIndex, pos2);
				char x2 = B[newIndex];
				char s2 = S[newIndex];

				if (x == 1)
				{
					if ((s2 & IN_CHECK_MATE))
					{
						mateInX = true;
						break;
					}
				}
				else /* x>1 */
				{
					if ((x2 == UNKNOWN) || (s2 & IN_STALE_MATE) || (s2 & INSUFFICIENT_MATERIAL))
						continue;
					if ((t == PIECE_COLOR::WHITE && x2 == x - 1) || (t == PIECE_COLOR::BLACK && x2 == -x + 1))
					{
						mateInX = true;
						break;
					}
				}
			}
			if (mateInX)
			{
				count++;
//				cout << p << " ";
				B[p] = (t == PIECE_COLOR::WHITE) ? x : -x;
			}
		}
	}
//		cout << count << endl;
	cout << count << " ";
	return count;
}


// Check for white or black to have a guaranteed mate in X after the other player moves
int Checkmate::IsResponseMateInX(int x)
{
	int blackCount = 0;
	int whiteCount = 0;
	//	cout << "Finding loser positions that can be mated in " << x << "... ";
	//int signedX = -x;
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);

		PIECE_COLOR t = (PIECE_COLOR)positions[0]; // turn
		int signedX = (t == PIECE_COLOR::WHITE) ? -x : x;
					// Legal but unknown mate count
		if (IsLegalPosition(p) && GetMovesToCheckmateCount(p) == UNKNOWN)
		{
			int currentIndex = p;
			bool responseMateInX = true; // unless shown otherwise
			int legalMoveCount = 0;
			char s2A[500];
			char x2A[500];
			bool breakOnUnknownExists = true;
			bool unknownExists = GetLegalMovesMetrics(currentIndex, s2A, x2A, legalMoveCount, breakOnUnknownExists);
			if (unknownExists)
				continue; // not a ResponseInX

			for (int m = 0; m < legalMoveCount; m++)
			{
				char s2 = s2A[m];
				char x2 = x2A[m];

				if ((s2 & IN_STALE_MATE) || (s2 & INSUFFICIENT_MATERIAL) || x2 == UNKNOWN || abs(x2) > x /* cannot response mate in x moves or less */ || signedX * x2 < 0 /* a switch of who can win */)
				{
					responseMateInX = false;
					break;
				}
			}

			if (responseMateInX && legalMoveCount >= 1)
			{
				if (t == PIECE_COLOR::WHITE)
				{
					blackCount += 1;
					B[currentIndex] = signedX;
				}
				else
				{
					whiteCount += 1;
					B[currentIndex] = signedX;
				}
			}
		} // if IsLegalPosition(p)
	} // for p
	//	cout << " (" << whiteCount << ") and (" << blackCount << ")" << endl;
	cout << " (" << whiteCount << ") and (" << blackCount << ") ";
	return whiteCount + blackCount;
}


char Checkmate::GetMovesToCheckmateCount(const int positions[])
{
	int p = ToIndex(positions);
	//	SymmetryConversion(blackKing, whiteKing, other1, other2);
	return GetMovesToCheckmateCount(p);
}

char Checkmate::GetMovesToCheckmateCount(int p)
{
	//	SymmetryConversion(blackKing, whiteKing, other1, other2);
	return B[p];
}


unsigned char Checkmate::GetStatus(const int positions[])
{
	int p = ToIndex(positions);
	//	SymmetryConversion(blackKing, whiteKing, other1, other2);
	return GetStatus(p);
}

unsigned char Checkmate::GetStatus(int p)
{
	//	SymmetryConversion(blackKing, whiteKing, other1, other2);
	return S[p];
}


// 
bool Checkmate::GetLegalMovesMetrics(int currentPosition,
	char s2[], char x2[], int& moveCount, bool breakOnUnknownExists)
{
	moveCount = 0;
	int totalLegalMoves = GetLegalMovesCount(currentPosition);
	long long rawIndex = mLegalMoves2[currentPosition];
	for (int m = 0; m < totalLegalMoves; m++)
	{
		int newIndex = mLegalMovesRawMemory[rawIndex + m];
		int toMateCount = B[newIndex];
		if (toMateCount == UNKNOWN || toMateCount == UNFORCEABLE)
		{
			if (breakOnUnknownExists)
				return true;
		}
		else // only store moves where the toMateCount is known.
		{
			x2[moveCount] = toMateCount;
			s2[moveCount] = S[newIndex];
			moveCount++;
		}
	}

	return false;
}

// Check for white or black to draw in x
int Checkmate::CanInsufficientMaterialInX(int x)
{
	int blackCount = 0;
	int whiteCount = 0;
	cout << "Finding INSUFFICIENT_MATERIAL In " << x << "...";
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		PIECE_COLOR t = (PIECE_COLOR)positions[0];
					// Legal
		if (IsLegalPosition(p) && GetMovesToCheckmateCount(p) == UNKNOWN)
		{
			int currentIndex = p;
			bool drawInX = false; // unless shown otherwise

			int legalMoveCount2 = (int)(mLegalMoves2[currentIndex + 1] - mLegalMoves2[currentIndex]);
			long long rawIndex = mLegalMoves2[currentIndex];
			for (int m = 0; m < legalMoveCount2; m++)
			{
				int newIndex = mLegalMovesRawMemory[rawIndex + m];
				char x2 = B[newIndex];
				char s2 = S[newIndex];

				if (x == 1)
				{
					if ((s2 & INSUFFICIENT_MATERIAL))
					{
						drawInX = true;
						break;
					}
				}
				else /* x>1 */
				{
					if ((x2 == UNKNOWN) || !(s2 & INSUFFICIENT_MATERIAL)) // maybe a stalemate check too.
						continue;
					if ((t == PIECE_COLOR::WHITE && x == x2 + 1) || (t == PIECE_COLOR::BLACK && x == -x2 + 1))
					{
						drawInX = true;
						break;
					}
				}
			}

			if (drawInX)
			{
				S[currentIndex] |= INSUFFICIENT_MATERIAL;
				if (t == PIECE_COLOR::WHITE)
				{
					whiteCount += 1;
					B[currentIndex] = x;
				}
				else
				{
					blackCount += 1;
					B[currentIndex] = -x;
				}
			}
		} // if IsLegalPosition(p)
	} // for p
	cout << " (" << whiteCount << ") and (" << blackCount << ")" << endl;
	return whiteCount + blackCount;
}


// Check for white or black to have a guaranteed Insufficient in X after the other player moves
int Checkmate::CanResponseInsufficientMaterialInX(int x)
{
	int blackCount = 0;
	int whiteCount = 0;
	cout << "Unlucky INSUFFICIENT_MATERIAL response in " << x << "... ";
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		PIECE_COLOR t = (PIECE_COLOR)positions[0];
		int signedX = (t == PIECE_COLOR::BLACK) ? x : -x;
						// Legal but unknown mate count
		if (IsLegalPosition(p) && GetMovesToCheckmateCount(p) == UNKNOWN)
		{
			int currentIndex = p;
			bool responseInsufficientInX = true; // unless shown otherwise
			int legalMoveCount = 0;
			char s2A[500];
			char x2A[500];
			bool breakOnUnknownExists = true;
			bool unknownExists = GetLegalMovesMetrics(currentIndex, s2A, x2A, legalMoveCount, breakOnUnknownExists);
			if (unknownExists)
				continue; // not a ResponseInX

			for (int m = 0; m < legalMoveCount; m++)
			{
				char s2 = s2A[m];
				char x2 = x2A[m];

				if (x2 == UNKNOWN || abs(x2) > x || !(s2 & IN_STALE_MATE || s2 & INSUFFICIENT_MATERIAL)  /* cannot response insufficient in x moves or less */ || signedX * x2 < 0 /* a switch of who can draw */)
				{
					responseInsufficientInX = false;
					break;
				}
			}

			if (responseInsufficientInX && legalMoveCount >= 1)
			{
				S[currentIndex] |= INSUFFICIENT_MATERIAL;
				if (t == PIECE_COLOR::WHITE)
				{
					blackCount += 1;
					B[currentIndex] = signedX;
				}
				else
				{
					whiteCount += 1;
					B[currentIndex] = signedX;
				}
			}
		}
	}
	cout << " (" << whiteCount << ") and (" << blackCount << ")" << endl;
	return whiteCount + blackCount;
}


void Checkmate::PrintEvaluation()
{
	int totalCount = 0;
	int illegalCount = 0;

	int whiteCheckmateCount = 0;
	int blackCheckmateCount = 0;
	int whiteKnownMateCount = 0;
	int blackKnownMateCount = 0;

	int insufficientMaterialCount = 0;
	int insufficientIn1Count = 0;
	int insufficientIn2Count = 0;
	int insufficientIn3Count = 0;

	int stalemateCount = 0;
	int stalemateIn1Count = 0;
	int stalemateIn2Count = 0;
	int stalemateIn3Count = 0;

	int unknownMate = 0;
	int unForceable = 0;

	int mateInX[100] = { 0 };
	int responseMateInX[100] = { 0 };
	int highestX = 1;


	cout << "\nGathering statistics on all data positions..." << endl;
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		PIECE_COLOR t = (PIECE_COLOR)positions[0];
		totalCount += 1;

		int mateCount = GetMovesToCheckmateCount(p);
		char S = GetStatus(p);

		if (!IsLegalPosition(p))
			illegalCount += 1;
		else if (S & INSUFFICIENT_MATERIAL && mateCount == 0)
			insufficientMaterialCount++;
		else if (S & INSUFFICIENT_MATERIAL && abs(mateCount) == 1)
			insufficientIn1Count++;
		else if (S & INSUFFICIENT_MATERIAL && abs(mateCount) == 2)
			insufficientIn2Count++;
		else if (S & INSUFFICIENT_MATERIAL && abs(mateCount) >= 3)
			insufficientIn3Count++;
		else if (S & IN_STALE_MATE && mateCount == 0)
			stalemateCount++;
		else if (S & IN_STALE_MATE && abs(mateCount) == 1)
			stalemateIn1Count++;
		else if (S & IN_STALE_MATE && abs(mateCount) == 2)
			stalemateIn2Count++;
		else if (S & IN_STALE_MATE && abs(mateCount) >= 3)
			stalemateIn3Count++;

		else if (mateCount == UNKNOWN) /* mateCount is not known */
		{
			unknownMate += 1;
		} /* mateCount is not known */
		else if (mateCount == UNFORCEABLE) /* mateCount is not forceable */
		{
			unForceable += 1;
		}

		else /* mateCount is known */
		{
			if (abs(mateCount) > highestX)
				highestX = abs(mateCount);

			if (mateCount > 0)
			{
				whiteKnownMateCount += 1;
				if (t == PIECE_COLOR::WHITE)
					mateInX[mateCount]++;
				else
					responseMateInX[mateCount]++;
			}
			else if (mateCount < 0)
			{
				blackKnownMateCount += 1;
				if (t == PIECE_COLOR::WHITE)
					mateInX[-mateCount]++;
				else
					responseMateInX[-mateCount]++;
			}
			else /* mateCount is zero */
			{
				if (t == PIECE_COLOR::WHITE)
					whiteCheckmateCount++;
				else
					blackCheckmateCount++;
			}
		} /* mateCount is NOT known - not forceable */
	} // for all board positions

	int stringWidth = 30;
	int countWidth = 10;
	cout << endl;
	cout << setw(stringWidth) << "totalCount = " << setw(countWidth) << totalCount << endl;
	cout << setw(stringWidth) << "illegalCount = " << setw(countWidth) << illegalCount << endl;

	cout << setw(stringWidth) << "whiteCheckmateCount = " << setw(countWidth) << whiteCheckmateCount << endl;
	cout << setw(stringWidth) << "blackCheckmateCount = " << setw(countWidth) << blackCheckmateCount << endl;
	cout << setw(stringWidth) << "whiteKnownMateCount = " << setw(countWidth) << whiteKnownMateCount << endl;
	cout << setw(stringWidth) << "blackKnownMateCount = " << setw(countWidth) << blackKnownMateCount << endl;
	
		for(int c=1; c<=highestX; c++)
		{
			char intAsString[100];
			sprintf_s(intAsString,100,"%i",c);
			cout << setw(stringWidth-strlen(intAsString)-3)<<"Mate in " << intAsString << " = " << setw(countWidth) << mateInX[c] << endl;
		}
		for(int c=1; c<=highestX; c++)
		{
			char intAsString[100];
			sprintf_s(intAsString,100,"%i",c);
			cout << setw(stringWidth-strlen(intAsString)-3)<<"Response Mate in " << intAsString << " = " << setw(countWidth) << responseMateInX[c] << endl;
		}
	
	cout << setw(stringWidth) << "insufficientMaterialCount = " << setw(countWidth) << insufficientMaterialCount << endl;
	cout << setw(stringWidth) << "insufficientIn1Count = " << setw(countWidth) << insufficientIn1Count << endl;
	cout << setw(stringWidth) << "insufficientIn2Count = " << setw(countWidth) << insufficientIn2Count << endl;
	cout << setw(stringWidth) << "insufficientIn3Count = " << setw(countWidth) << insufficientIn3Count << endl;
	cout << endl;
	cout << setw(stringWidth) << "stalemateCount = " << setw(countWidth) << stalemateCount << endl;
	cout << setw(stringWidth) << "stalemateIn1Count = " << setw(countWidth) << stalemateIn1Count << endl;
	cout << setw(stringWidth) << "stalemateIn2Count = " << setw(countWidth) << stalemateIn2Count << endl;
	cout << setw(stringWidth) << "stalemateIn3Count = " << setw(countWidth) << stalemateIn3Count << endl;
	cout << endl;
	cout << setw(stringWidth) << "unknownMate = " << setw(countWidth) << unknownMate << endl;
	cout << setw(stringWidth) << "Unforceable = " << setw(countWidth) << unForceable << endl;
	cout << endl << "totalCount - illegalCount - whiteCheckmateCount - blackCheckmateCount" << endl;
	cout << "- whiteKnownMateCount - blackKnownMateCount" << endl;
	cout << "- variousStalemateCounts -insufficientMaterialCount: " <<
		totalCount - illegalCount - whiteCheckmateCount - blackCheckmateCount - whiteKnownMateCount - blackKnownMateCount
		- stalemateCount - stalemateIn1Count - stalemateIn2Count - stalemateIn3Count
		- insufficientMaterialCount - insufficientIn1Count - insufficientIn2Count - insufficientIn3Count << endl;
	cout << endl;

}

// For all positions where S is some kind of illegal, set B to ILLEGAL
// For all positions where S is INSUFFICIENT_MATERIAL or UNKNOWN, set B to UNFORCEABLE.
// Then we don't need to save and load S, just B.
void Checkmate::SwitchMovecountValues()
{
	for (int p = 0; p < mTotalPositions; p++)
	{
		if (!IsLegalPosition(p))
			B[p] = ILLEGAL;
		if (S[p] & INSUFFICIENT_MATERIAL || S[p] & IN_STALE_MATE || B[p]==UNKNOWN)
			B[p] = UNFORCEABLE;
	}
}

string Checkmate::MakeFilenameFromPieces(const std::vector< PIECE_TYPES> & mPieces)
{
	string filename = "..\\MakeTables\\";
	for (unsigned int i = 2; i < NUM_PIECES; i++)
	{
		switch (mPieces[i])
		{
		case PIECE_TYPES::WHITE_BISHOP:
			filename += "WB";
			break;
		case PIECE_TYPES::WHITE_KNIGHT:
			filename += "WN";
			break;
		case PIECE_TYPES::WHITE_QUEEN:
			filename += "WQ";
			break;
		case PIECE_TYPES::WHITE_ROOK:
			filename += "WR";
			break;
		case PIECE_TYPES::WHITE_PAWN:
			filename += "WP";
			break;

		case PIECE_TYPES::BLACK_BISHOP:
			filename += "BB";
			break;
		case PIECE_TYPES::BLACK_KNIGHT:
			filename += "BN";
			break;
		case PIECE_TYPES::BLACK_QUEEN:
			filename += "BQ";
			break;
		case PIECE_TYPES::BLACK_ROOK:
			filename += "BR";
			break;
		case PIECE_TYPES::BLACK_PAWN:
			filename += "BP";
			break;

		default:
			cout << "Error in MakeFilenameFromPieces!" << endl;
		};
	}
	return filename;
}

// This includes saving DEAD_POSITIONS
void Checkmate::SaveTable1(const std::vector< PIECE_TYPES>& mPieces)
{
	string filename = MakeFilenameFromPieces(mPieces);
	string filename1 = filename + ".table.bin";
	string filename2 = filename + ".status.bin";
	cout << "Writing the data to " << filename << "..." << endl;
	ofstream fout(filename1, ios::binary);
	ofstream fout2(filename2, ios::binary);

	fout.write(&B[0], mTotalPositions); 
	fout2.write((char*)(&S[0]), mTotalPositions);
	fout.close();
	fout2.close();
	cout << "Saved the table data" << endl;
}

bool Checkmate::LoadTable1(bool printEvaluation, const std::vector< PIECE_TYPES>& mPieces, 
		char* B, unsigned char* S)
{
	Assert(B != NULL, "B != NULL");
	Assert(S != NULL || !printEvaluation, "S != NULL || !printEvaluation");

	string filename = MakeFilenameFromPieces(mPieces);
	string filename1 = filename + ".table.bin";
	cout << "Trying to load the B table data from " << filename1 << "..." << endl;
	ifstream fin(filename1, ios::binary);
	if (!fin)
	{
		cout << "Unable to load the B data." << endl;
		return false;
	}
	fin.read(&B[0], mTotalPositions);
	fin.close();
	cout << "Successfully loaded the B data" << endl;

	// If we are going to print an evaluation, then we need to load S. Otherwise skip it.
	if (printEvaluation)
	{
		string filename2 = filename + ".status.bin";
		cout << "Trying to load the S table data from " << filename2 << "..." << endl;
		ifstream fin2(filename2, ios::binary);
		if (!fin2)
		{
			cout << "Unable to load the S data." << endl;
			return false;
		}
		fin2.read((char*)(&S[0]), mTotalPositions);
		fin2.close();
		cout << "Successfully loaded the S data" << endl;
	}

	return true;
}

#if 0
// This includes saving DEAD_POSITIONS
void Checkmate::SaveTable2()
{
	string filename = MakeFilenameFromPieces();
	string filename1 = filename + ".table.compressed.bin";
	string filename2 = filename + ".status.compressed.bin";
	cout << "Writing the data to " << filename << "..." << endl;
	ofstream fout(filename1, ios::binary);
	ofstream fout2(filename2, ios::binary);

	int totalCount = 0;
	int compressedCount = 0;
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
		int i = positions[1]; // position of first king.
		totalCount += 1;
		if (!(i == 0 || i == 1 || i == 2 || i == 3 || i == 9 || i == 10 || i == 11 || i == 18 || i == 19 || i == 27))
		{
			continue;
		}
		// Check for OnTop:
		/*
		if (i == j || i == k || i == l || j == k || j == l)
		{
			//continue;
		}
		// Check for AdjacentKings:
		int bk_row = i / 8;
		int bk_column = i % 8;
		int wk_row = j / 8;
		int wk_column = j % 8;
		if (abs(bk_row - wk_row) <= 1 && abs(bk_column - wk_column) <= 1)
		{
			//continue;
		}
		int index = p;
		char b = B[index];
		if (b == UNKNOWN)
		{
			//continue;
		}
		if (b < 0)
		{
			//continue;
		}
		*/

		char b = B[p];
		fout.put(b);
		char s = S[p];
		fout2.put(s);

		compressedCount += 1;
	} // for p

	fout.close();
	fout2.close();
	cout << "Saved the table data" << endl;
}


bool Checkmate::LoadTable2()
{
	string filename = MakeFilenameFromPieces();
	string filename1 = filename + ".table.compressed.bin";
	string filename2 = filename + ".status.compressed.bin";
	cout << "Trying to load the table data from " << filename << "..." << endl;
	ifstream fin(filename1, ios::binary);
	ifstream fin2(filename2, ios::binary);

	if (!fin || !fin2)
	{
		cout << "Unable to load the data." << endl;
		return false;
	}
	int totalCount = 0;
	int compressedCount = 0;
	for (int p = 0; p < mTotalPositions; p++)
	{
		int positions[POSITION_ARRAY_SIZE];
		FromIndex(p, positions);
						int index = p;
						/*
						if (k == 64 || l == 64) // we shouldn't need dead square data.
						{
							B[index] = -1;
							S[index] = -1;
						}
						*/
						int i = positions[1]; // position of king.
						if ((i == 0 || i == 1 || i == 2 || i == 3 || i == 9 || i == 10 || i == 11 || i == 18 || i == 19 || i == 27))
						{
							char b;
							fin.get(b);
							B[index] = b;
							char s;
							fin2.get(s);
							S[index] = s;
							if (compressedCount == 327872)
							{
								int zz = 5;
								zz++;
							}
							compressedCount += 1;
						}
						else
						{
							// This data can be found by symmetry. Eliminate it!
							B[index] = -1;
							S[index] = -1;
						}
						totalCount += 1;
					}

	cout << "TotalCount: " << totalCount << ". CompressedCount: " << compressedCount << endl;
	fin.close();
	fin2.close();
	cout << "Successfully loaded the data" << endl;
	return true;
}
#endif

PIECE_COLOR Checkmate::GetTurnFromPosition(int p)
{
	// shortcut for:
	//int positions[POSITION_ARRAY_SIZE];
	//FromIndex(p, positions);
	//int t = positions[0];
	//return t;

	int t = (int) (p / (mTotalPositions / 2));
	return (PIECE_COLOR)t;
}

// Called only externally.
PIECE_COLOR Checkmate::GetExpectedWinner(const int positions[]) // returns WHITE, BLACK, or NO_COLOR
{
	//SymmetryConversion(i, j, k, l); done by called methods.
	int p = ToIndex(positions);
//	char S = GetStatus(p);
	char B = GetMovesToCheckmateCount(p);
	PIECE_COLOR t = GetTurnFromPosition(p);

	if (!IsLegalPosition(p))
		return PIECE_COLOR::NO_COLOR;

	if (S == NULL)
	{
		// If S is NULL then we loaded B, so B's UNFORCEABLE will be set.
		if (B == UNFORCEABLE)
			return PIECE_COLOR::NO_COLOR;
	}
	else
	{
		//  If S is not NULL then we can use it.
		char s = GetStatus(p);
		if (s & INSUFFICIENT_MATERIAL)
			return PIECE_COLOR::NO_COLOR;
		if (s & IN_STALE_MATE)
			return PIECE_COLOR::NO_COLOR;
	}

	if (B == UNKNOWN)
		return PIECE_COLOR::NO_COLOR; // shouldn't happen because gets reset to UNFORCEABLE
	if (B > 0)
		return PIECE_COLOR::WHITE;
	if (B < 0)
		return PIECE_COLOR::BLACK;
	// B==0. Someone is in checkmate
	if (t == PIECE_COLOR::WHITE)
		return PIECE_COLOR::BLACK;
	return PIECE_COLOR::WHITE;
}


// Called externally.
// Does not use the legal moves cache, because it won't have been calculated!
void Checkmate::CalculateLegalMovesPositions(const int positions[],
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount)
{
	legalMoveCount = 0;
	int p = ToIndex(positions);
	PIECE_COLOR turn = GetTurnFromPosition(p);
	if (!IsLegalPosition(p))
		return; // There are no legal moves if we start from an illegal position.

	for (unsigned int pieceIndex = 0; pieceIndex < mPieces.size(); pieceIndex++)
	{
		PIECE_TYPES pt = mPieces[pieceIndex];
		if (GetColor(pt) != turn)
			continue; // not this piece color's turn

		GatherLegalMovesForPiece(pieceIndex, positions,
			allLegalMoves, legalMoveCount);
	}
}

// called externally
void Checkmate::GenerateNewPositionFromLegalMove(const int positions1[], const LEGAL_MOVE& lm,
	int positions2[])
{
	int p = ToIndex(positions1);
	PIECE_COLOR t = GetTurnFromPosition(p);
	Assert(t == PIECE_COLOR::WHITE || t == PIECE_COLOR::BLACK, "t==WHITE || t==BLACK");
	Assert(IsLegalPosition(p), "Pre:IsLegalPosition(p)");

	PIECE_COLOR t2 = (PIECE_COLOR) (1 - (int)t);
	int pieceIndex = lm.pieceIndex;
	int newPosition = lm.newPosition;
	positions2[0] = (int)t2;
	for (int i = 0; i < NUM_PIECES; i++)
		positions2[i+1] = positions1[i+1];
	positions2[pieceIndex+1] = newPosition;

	if (lm.capture)
	{
		int pieceIndex2 = lm.pieceIndex2;
		int newPosition2 = lm.newPosition2;
		positions2[pieceIndex2+1] = newPosition2;
	}

	int p2 = ToIndex(positions2);
	if (!IsLegalPosition(p2))
	{
		cout << "Error. Post:IsLegalPosition(p2)" << endl;
		PrintPosition(positions1);
		PrintPosition(positions2);
		cout << "Why are things not printing?" << endl;
		//cout << t << " " << i << " " << j << " " << k << " " << l << endl;
		//cout << t2 << " " << i2 << " " << j2 << " " << k2 << " " << l2 << endl;
		system("pause");
	}

	Assert(IsLegalPosition(p2), "IsLegalPosition(p2)");
}

void Checkmate::PrintPosition(const int position[])
{
	if (position[0] == (int)PIECE_COLOR::WHITE)
		cout << "WHITE ";
	else
		cout << "BLACK ";
	for (int i = 1; i < POSITION_ARRAY_SIZE; i++)
		cout << position[i] << " ";
	cout << endl;
}

void Assert(int value, const char message[])
{
	if(!value)
	{
		cout << "Assert error: " << message << endl;
		assert(false);
		system("pause");
		exit(1);
	}
}

void InsertCommaEveryThreeDigits(char result[])
{
	char buffer[30];
	strcpy_s(buffer, 30,result);

	// for every 3 digits, insert a comma.
	size_t length = strlen(buffer);
	int result_pos = 0;
	int buffer_pos = 0;
	while(length>0)
	{
		length--;
		result[result_pos++] = buffer[buffer_pos++];
		if(length%3==0 && length!=0)
		{
			result[result_pos++] = ',';
		}
	}
	result[result_pos] = '\0';
}

void CoutLongAsCommaInteger(unsigned long x)
{
	char result[100];
	_itoa_s(x, result, 100, 10);
	InsertCommaEveryThreeDigits(result);
	cout << result << endl;
}

void CoutLongLongAsCommaInteger(long long x)
{
	char result[100];
	_i64toa_s(x, result, 100, 10);
	InsertCommaEveryThreeDigits(result);
	cout << result << endl;
}

