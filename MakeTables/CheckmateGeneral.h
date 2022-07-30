// This program computes Endgame Tablebases when there are 4 pieces.
// This was first done by someone in the late 1980s.
// In the early 1990s, the same was done for 5 pieces.
// In 2005, 6-piece endings were solved.
// Now (2014 or before) it has been done for 7 pieces. The total volume of all tablebases is 140,000 gigabytes
// There are 525 tablebases of the 4 vs. 3 type and 350 tablebases of the 5 vs. 2 type. http://chessok.com/?page_id=27966
//

// BSFIX:
// Try different orderings for the saved files to see which compresses the best
//		Take advantage of symmetry on the king (especially with no pawns) when saving.
//		Move the "unknown" option to be zero?
//		Negate every other value (so black mates are positive also)
//		Try to eliminate some or all of the illegal moves from needing to be saved.
//		Combine S information into the B file
// Enhance speed through process spawning
// Allow capture through table switching lookup
// Support pawns, and pawn promotion through table switching lookup
// make consistent BLACK_KING is at pieces[0], but BLACK turn is t==1.
//	make all of these methods work when k or l == DEAD_POSITION, or when p2 or p3 is NONE.
	// Maybe get rid of the option for p2 or p3 to be NONE.
	// Maybe make all dimentions 64 instead of 65, eliminating DEAD_POSITION. No?


// Data:
// New machine, Bishop and Knight: 445 seconds without cache, 86 with cache. Now 22 seconds in x64, 24 in x86.


//
// The 64 board positions:
// 56 57 58 59 60 61 62 63
// 48 49 50 51 52 53 54 55
// 40 41 42 43 44 45 46 47
// 32 33 34 35 36 37 38 39
// 24 25 26 27 28 29 30 31
// 16 17 18 19 20 21 22 23
//  8  9 10 11 12 13 14 15
//  0  1  2  3  4  5  6  7
//
// row = position/8; // y direction
// column = position%8; // x direction
// position = row*8+column;

#include <string>
#include <vector>
const int DEAD_POSITION = 64;

// Used for piece color and also for player turn:
enum class PIECE_COLOR {
		WHITE, BLACK, NO_COLOR}; // NO_COLOR means this piece is not being used.
const char gPieceColorNames[3][9] = {
		"White", "Black", "No Color"};

enum class PIECE_TYPES {
		WHITE_KING, WHITE_QUEEN, WHITE_BISHOP, WHITE_KNIGHT, WHITE_ROOK, WHITE_PAWN,
		BLACK_KING, BLACK_QUEEN, BLACK_BISHOP, BLACK_KNIGHT, BLACK_ROOK, BLACK_PAWN,
		NONE};
const char gPieceTypeNames[(int)(PIECE_TYPES::NONE)+1][7] = {
		"King", "Queen","Bishop","Night","Rook","Pawn",
		"King", "Queen","Bishop","Night","Rook","Pawn",
		"None"};


const int NUM_PIECES = 4; // Change this according to how many pieces are passed to Initialize.
const int POSITION_ARRAY_SIZE = NUM_PIECES + 1; // one more for the turn at index zero.

struct LEGAL_MOVE
{
	int pieceIndex;
	int oldPosition;
	int newPosition;

	bool capture;
	int pieceIndex2;
	int oldPosition2; // must be newPosition
	int newPosition2; // must be DEAD_POSITION
	bool operator==(LEGAL_MOVE & rhs)
	{
		if( this->pieceIndex!=rhs.pieceIndex || this->oldPosition!=rhs.oldPosition || this->newPosition!=rhs.newPosition || this->capture!=rhs.capture)
			return false;
		if (capture)
		{
			if( this->pieceIndex2!=rhs.pieceIndex2 || this->oldPosition2!=rhs.oldPosition2 || this->newPosition2!=rhs.newPosition2)
				return false;
		}
		return true;
	}
};

// With 4 pieces, Maximum moves for one player is 1 king and 2 queens (versus 1 king), 8+27+25=60. 
// 1 king, 1 bishop, and 1 knight = 8+13+8=29.
// 4 for pawn, 8 for king, 8 for knight, 13 for bishop, 14 for rook, 27 for queen
const int MAX_LEGAL_MOVES = 8+27+25; 

// Status Bit field is defined as follows:
const  unsigned char START_STATUS = 0;
// illegal statuses:
const  unsigned char KINGS_ADJACENT = 1; // illegal because kings adjacent 
const  unsigned char ON_TOP = 2; // illegal because pieces on top of each other (except where already KINGS_ADJACENT)
const  unsigned char BAD_CHECK = 4; // illegal because one player in check while other player's turn (except for previous illegals)
const  unsigned char BAD_PAWN = 8; // illegal because a pawn is on it's pre-first rank (except for previous illegals)
	// not illegal:
const  unsigned char IN_CHECK = 16; // current player in check, current player's turn
const  unsigned char IN_CHECK_MATE = 32; // current player in check mate, current player's turn
const  unsigned char IN_STALE_MATE = 64; // current player in stale mate, current player's turn
const  unsigned char INSUFFICIENT_MATERIAL = 128; // Insufficient material for either player to mate.

// MoveCount (variable B) is defined as follows:
//
// If the status is Not IsLegalPosition (illegal) then B should initially stay at UNKNOWN.
//		IMPORTANT: Before saving the file, all these values of B switch to ILLEGAL.
//
// Else if the status IsLegalPosition, then...
//
// if status is START_STATUS or IN_CHECK or IN_CHECK_MATE, then B means the following:
// Positive values of B mean White can mate, no matter whose turn.
// Negative values of B mean Black can mate, no matter whose turn.
//
//		B[T][i][j][k][l], value -128, means UNKNOWN. Later becomes UNFORCEABLE
//
//		B[0][i][j][k][l], value 0, means white's turn, white is in checkmate
//		B[1][i][j][k][l], value 0, means black's turn, black is in checkmate
//
//		B[0][i][j][k][l], value 1, means white's turn, white can mate this move
//		B[1][i][j][k][l], value 1, means black's turn, no matter what black does, white can mate him in 1
//		B[0][i][j][k][l], value 2, means white's turn, white can mate in 2
//		B[1][i][j][k][l], value 2, means black's turn, no matter what black does, white can mate him in 2

//		B[1][i][j][k][l], value -1, means black's turn, black can mate this move
//		B[0][i][j][k][l], value -1, means white's turn, no matter what white does, black can mate him in 1
//		B[1][i][j][k][l], value -2, means black's turn, black can mate in 2
//		B[0][i][j][k][l], value -2, means white's turn, no matter what white does, black can mate him in 2
//		etc.
//		+120 or -120 for now means overflow. The answer is stored in an overflow area.
//
// If IN_STALE_MATE is set, B[...] must be +127. No! Not currently used!
//
//		B[0][i][j][k][l], value 127, means white's turn, white is in stalemate
//		B[1][i][j][k][l], value 127, means black's turn, black is in stalemate
//
// If INSUFFICIENT_MATERIAL is set, B[...] initially means:
//
//		B[0][i][j][k][l], value 0, means white's turn, INSUFFICIENT_MATERIAL
//		B[1][i][j][k][l], value 0, means black's turn, INSUFFICIENT_MATERIAL
//
//		B[0][i][j][k][l], value 1, means white's turn, white can INSUFFICIENT_MATERIAL this move
//		B[1][i][j][k][l], value 1, means black's turn, no matter what black does, white can INSUFFICIENT_MATERIAL him in 1
//		B[0][i][j][k][l], value 2, means white's turn, white can force INSUFFICIENT_MATERIAL in 2
//		B[1][i][j][k][l], value 2, means black's turn, no matter what black does, white can force INSUFFICIENT_MATERIAL in 2

//		B[1][i][j][k][l], value -1, means black's turn, black can INSUFFICIENT_MATERIAL this move
//		B[0][i][j][k][l], value -1, means white's turn, no matter what white does, black can force INSUFFICIENT_MATERIAL him in 1
//		B[1][i][j][k][l], value -2, means black's turn, black can force INSUFFICIENT_MATERIAL in 2
//		B[0][i][j][k][l], value -2, means white's turn, no matter what white does, black can force INSUFFICIENT_MATERIAL in 2
//		etc.
//
//		IMPORTANT: Before saving the file, all these values of B switch to UNFORCEABLE.



const char UNKNOWN = -128; // later becomes UNFORCEABLE
const char ILLEGAL = -127;
const char UNFORCEABLE = -126;
//const char STALEMATE = 127;
const char POSITIVE_OVERFLOW = 120; // Not used yet.
const char NEGATIVE_OVERFLOW = -120;

const int KING_SQUARES = 64;
const int OTHER_SQUARES = 65;
//const int TOTAL_POSITIONS = 2 * KING_SQUARES * KING_SQUARES * OTHER_SQUARES * OTHER_SQUARES;
const int AVERAGE_MOVES_PER_POSITION = 14;

class Checkmate
{
public:
	// Call Initialize first!
	// BLACK_KING and WHITE_KING must always be p0 and p1, repectively.  Last 2 slots can vary.
	Checkmate();
	void Initialize(const std::vector< PIECE_TYPES> & pieces, bool loadData, bool printEvaluation = false);
	void AllocateMemory(bool loadData, bool printEvaluation);
	~Checkmate();

	long long mTotalPositions; // long long is 8 bytes. Really only need a 4 bytes unsigned int for 5 pieces or less.
	// B represents all the board positions. Use FromIndex and ToIndex for Turn and Individual pieces.
	//		Note that the last pieces (non kings) can be at position 64, which means DEAD
	char* B; // mTotalPositions, dynamic
	// S represents the status bits for all the board positions. (see the above header file)
	unsigned char* S; // mTotalPositions, dynamic

	// for indexing into B and S arrays:
	void FromIndex(int index, std::vector<int>& positions);
	void FromIndex(int index, int positons[]);
	int ToIndex(const std::vector<int>& positions);
	int ToIndex(const int positons[]);

	long long mLegalMovesRawMemoryRequested;
	unsigned int* mLegalMovesRawMemory; // mLegalMovesRawMemoryRequested, dynamic
		// The total number of these we need, mLegalMovesRawMemoryRequested, is more than 4G.
		// But the type, unsigned int, is sufficient.
		// An unsigned int counts up to 4B, and is enough to store a single legal move (new position)
		// if there are 5 or less pieces. 2*64*64*64*64*64 = 2B. 
		// 2*64*64*65*65*65 also fits in a 4B unsigned int.
	long long mLegalMovesRawMemoryIndex; // keeps track of how much mLegalMovesRawMemory has been used.
									// Starts at zero. Should not exceed mLegalMovesRawMemoryRequested.
									// This will get bigger then 4G, so must be long long.
	long long* mLegalMoves2;	// indexes into mLegalMovesRawMemory
								// The number of them won't be more than 4G for 5 pieces.
								// We need mTotalPositions+1 of them.
								// But the value of each WILL exceed 4G, so must be long long.
									// 24 or 25 seconds this way. 22 after GetMovesToCheckmateCount simplify

	std::vector< PIECE_TYPES> mPieces;

	void InitBoardB();
	void InitAllStatusBitsS();
	void InitAdjacentKings();
	void InitOnTop();
	void InitBadPawns();
	bool IsLegalPosition(int position);
	void CheckFromAndTo();

	void InitCheckAndBadCheck();
	bool IsBishopAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player);
	bool IsRookAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player);
	bool IsQueenAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player);
	bool IsKnightAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player);
	bool IsPawnAttackingEnemyKing(int p, int pieceIndex, PIECE_COLOR player);


	// For caching all legal moves for all positions:
	void AssignPawnPromotions(PIECE_TYPES fromPawn, PIECE_TYPES toQueen,
		int promotionRow);
	void CacheAllLegalMovesForAllPositions();  // Call this to make the cache
	void CacheAllLegalMovesForThisPosition(int p);
	PIECE_COLOR GetColor(PIECE_TYPES pt); // returns WHITE, BLACK, or NO_COLOR for NONE slots.
	int ToReplaceIndex(const int oldPositions[],
		int pieceIndex, int newPiecePosition);
	void GatherLegalMovesForPiece(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void GatherLegalMovesForKing(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	bool SameSquareSameColorKing(const int positions[]);
	bool SameSquareSameColor(const int positions[], int pieceIndex, PIECE_COLOR player);
	bool SameSquareOppositeColor(const int positions[], int capturingPieceIndex, int& deadPieceIndex);

	void GatherLegalMovesForBishop(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void GatherLegalMovesForKnight(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void GatherLegalMovesForRook(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void GatherLegalMovesForQueen(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void GatherLegalMovesForPawn(int pieceIndex, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	bool EnemyPieceHere(int pieceIndex, int r, int c, const int positions[]);
	bool NoPieceHere(int r, int c, const int positions[]);
	void AddLegalMoveForPawn(int pieceIndex, PIECE_COLOR turn, int r, int c, const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void IsPieceLegalToMoveHere(int pieceIndex, PIECE_COLOR turn, int r, int c, const int positions[],
		bool& stop, LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount);
	void InsertAnotherLegalMove(int pieceIndex, int oldPosition, int newPosition,
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount, bool capture, int deadPieceIndex);

	void InitInsufficientMaterial();
	void InitIsStalemate();
	void InitIsCheckmate();
	void InitIsCheckmateOrStalemate(char checkForCheckmate);
	int GetLegalMovesCount(int currentPosition);

	int IsMateInX(int x);
	int IsResponseMateInX(int x);
	char GetMovesToCheckmateCount(const int positions[]); // See above chart. BSFIX check for return values of UNKNOWN and UNFORCEABLE
	char GetMovesToCheckmateCount(int p);
	unsigned char GetStatus(const int positions[]);
	unsigned char GetStatus(int p);
	bool GetLegalMovesMetrics(int position, // call this to retrieve part of the legal moves cache
		char s2[], char x2[], int& moveCount, bool breakOnUnknownExists = false);

	int CanInsufficientMaterialInX(int x);
	int CanResponseInsufficientMaterialInX(int x);

	void PrintEvaluation(); // Prints everything about B and S
	void PrintPosition(const int position[]); // prints one position

	// Saving and Loading B and S:
	void SwitchMovecountValues();
	std::string MakeFilenameFromPieces(const std::vector< PIECE_TYPES>& mPieces);
	void SaveTable1(const std::vector< PIECE_TYPES>& mPieces);
	bool LoadTable1(bool printEvaluation, const std::vector< PIECE_TYPES>& mPieces, 
			char* B, unsigned char* S);
//	void SaveTable2();
//	bool LoadTable2();

	PIECE_COLOR GetTurnFromPosition(int position);
	PIECE_COLOR OtherColor(PIECE_COLOR turn1) {
		return (turn1 == PIECE_COLOR::WHITE) ? PIECE_COLOR::BLACK : PIECE_COLOR::WHITE;
	}

	// Called only externally.
	PIECE_COLOR GetExpectedWinner(const int positions[]); // returns WHITE, BLACK, or NO_COLOR for drawish. 
	void CalculateLegalMovesPositions(const int positions[],
		LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES], int& legalMoveCount); // Does not use legal move cache.
	void GenerateNewPositionFromLegalMove(const int positions1[], const LEGAL_MOVE& lm,
		int positions2[]);

};

void Assert(int value, const char message[]);
void CoutLongAsCommaInteger(unsigned long x);
void CoutLongLongAsCommaInteger(long long x);
