#include "..\\MakeTables\\CheckmateGeneral.h"

struct GraphicalPiece
{
public:
	// i and j are the board column and row, between 0 and 7, of this piece
	// x and y are the graphical position of where to draw this piece.
	void Init(PIECE_TYPES pt, int i, int j);

	void Init(PIECE_TYPES pt, int index) 
	{
		int i = index%8;
		int j = index/8;
		Init(pt,i,j);
	}
	void SetPosition(double i, double j)
	{
		x = i+.5;
		y = j+.5;
	};

	void Draw();

	int GetI() { return (int)x; }
	int GetJ() { return (int)y; }
	int GetIndex() { return GetI() + GetJ() * 8;} // 0 through 63

	PIECE_TYPES p;
	double x;
	double y;
};

