// Mate In X
// By Barton Stander
// April, 2011
// More in June, 2014

// There should be two main modes - Analyze Mode and Play Mode.
// Analyze: Allow free drag of pieces, including the DEAD position.
//			Allow turn switch.
//			Allow piece set changes
// Play:	Human/Computer all options
//			Computer speed, or wait for a click
//			User clock
//
//	Viewing Menu Options:
//			Legal moves (Best Moves, Okay Moves, Losing Moves, MateCount per move (or stats per move), Board repeat count per move),
//			clock, Who's winning status, status bits, moves till mate, move count without a piece capture or pawn movement.
//			Show State summary, where State summary might be "White can mate in 3, Black is in checkmate, Stalemate, Illegal", etc.
//			show "covered squares."
//			Always show who's turn it is.
//
//  End of Game possibilities:
//			Win by Checkmate
//			Win by oppenent's clock expiration
//			Draw by mutual agreement. (Does not apply here)
//			Draw by Stalemate
//			Draw by Threefold Repetition
//			Draw by Fifty Move Rule. Note that in some situations of rook and bishop versus rook, a forcable win in 59 moves exists! 
//									For some cases of knight and knight versus pawn, a 115 force exists!
//									There are more exceptions. 
//									Some positions of a queen and knight versus a rook, bishop, and knight, can be forced (for white) in 545 moves! http://chessok.com/?page_id=27966
//			Draw by impossible material. king versus king. King and bishop, king and knight. any number of bishops on the same color squares.
//			Draw by "no sequence of legal moves can lead to a checkmate". For examples, there is a wall of pawns between the two sides.
//			Note that Draw by Perpetual Check was removed in 1965, as it is covered by the above rules.
//
//			Draw by improbable material? For example, king and a bishop/knight versus king and a bishop/knight. http://en.wikipedia.org/wiki/Draw_(chess)
//										There must be < 2 minutes left without a win my "Normal means."
//										Even then, the Arbitrator may rule either way.
//			A "Book draw" or "Theoretical draw" is a position that is known to result in a draw if both sides play optimally. http://en.wikipedia.org/wiki/Draw_(chess)


#include <iostream>
#include <cmath>
#include <assert.h>
using namespace std;
#include "GraphicalCheckmate.h"
#include "graphics.h"

//
// Global Variables (Only what you need!):
//
double gScreen_x = 900;
double gScreen_y = 900;

Checkmate *gCheckmate=NULL; // a "smart" checkmate object

GraphicalPiece gPieces[NUM_PIECES]; // 4 pieces and their locations
int gGrabbedPiece = -1; // for grabbing and dragging a piece.
int gStartI = -1;
int gStartJ = -1;
double gDx=0; // if a piece is grabbed near its edge, this prevents visual popping.
double gDy=0;
PIECE_COLOR gTurn=PIECE_COLOR::WHITE; 

int gLastI=-1;
int gLastJ=-1;
int gLastK=-1;
int gLastL=-1;
PIECE_COLOR gLastTurn = PIECE_COLOR::NO_COLOR;
//
// Global Constants:
//

// World Coordinates
const double WXL=-1.0;
const double WXH = 9.0;
const double WYL=-1.0;
const double WYH= 9.5;
const double WX=WXH-WXL;
const double WY=WYH-WYL;
// Desire Size of stroke characters, as a proportion of the whole screen.
const double WCX=.05;
const double WCY=.05;
const double PIECE_RADIUS = .4;
// The maximum top character in the font is 119.05 units; the  bottom
//                descends 33.33 units. Each character is 104.76 units wide
const double FONT_WIDTH = 104.76;
const double FONT_HEIGHT = 119.05; // for capital letters only!

void GraphicalPiece::Init(PIECE_TYPES pt, int i, int j)
{
	p = pt;
	x = i + .5;
	y = j + .5;
}

void PrintMateCount()
{
	int positions[POSITION_ARRAY_SIZE];
	positions[0] = (int)gTurn;
	for (int i = 1; i < POSITION_ARRAY_SIZE; i++)
		positions[i] = gPieces[i - 1].GetIndex();

	int mateCount = (int)gCheckmate->GetMovesToCheckmateCount(positions);

	if(mateCount==0)
		cout << gPieceColorNames[(int)gTurn] << " is in Checkmate!" << endl;
	else if(mateCount==1)
	{
		if (gTurn==PIECE_COLOR::WHITE)
			cout << "White can mate this move." << endl;
		else
			cout << "No matter what Black does, White can mate in " <<mateCount << endl;
	}
	else if(mateCount>1)
	{
		if (gTurn== PIECE_COLOR::WHITE)
			cout << "White can mate in "<< mateCount << endl;
		else
			cout << "No matter what Black does, White can mate in " <<mateCount << endl;
	}
	else if(mateCount==-1)
	{
		if (gTurn== PIECE_COLOR::BLACK)
			cout << "Black can mate this move." << endl;
		else
			cout << "No matter what White does, Black can mate in " <<-mateCount << endl;
	}
	else if(mateCount<-1)
	{
		if (gTurn== PIECE_COLOR::BLACK)
			cout << "Black can mate in "<< -mateCount << endl;
		else
			cout << "No matter what White does, Black can mate in " <<-mateCount << endl;
	}
}


void PrintStatus()
{
	// Don't print a new status if we are still dragging over the same square.
	if(gLastI==gPieces[0].GetIndex() && gLastJ==gPieces[1].GetIndex() && gLastK==gPieces[2].GetIndex() && gLastL==gPieces[3].GetIndex() && (int)gLastTurn==(int)gTurn)
		return;

	int positions[POSITION_ARRAY_SIZE];
	positions[0] = (int)gTurn;
	for (int i = 1; i < POSITION_ARRAY_SIZE; i++)
		positions[i] = gPieces[i - 1].GetIndex();

	cout << "Status: ";
	int moveCount = (int)gCheckmate->GetMovesToCheckmateCount(positions);
	cout << gPieceColorNames[(int)gTurn] << "'s turn. ";

	if (moveCount == ILLEGAL)
		cout << "Illegal Position" << endl;
	else if (moveCount == UNFORCEABLE)
		cout << "Not Forceable" << endl;
	else if (moveCount == UNKNOWN)
		cout << "UNKNOWN" << endl;
	else
		PrintMateCount();


	gLastI = gPieces[0].GetIndex();
	gLastJ = gPieces[1].GetIndex();
	gLastK = gPieces[2].GetIndex();
	gLastL = gPieces[3].GetIndex();
	gLastTurn = gTurn;
}

// Outputs a string of text at the specified location.
void DrawStrokeText(double x, double y, const char *text)
{
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_BLEND);
	glEnable(GL_LINE_SMOOTH);
	glLineWidth(3.0);

	glPushMatrix();
	glTranslated(x, y, 0);
	glScaled(WX/FONT_HEIGHT*WCX,WY/FONT_WIDTH*WCY,1);
	for (const char *p = text; *p; p++)
	{
		glutStrokeCharacter(GLUT_STROKE_MONO_ROMAN, *p);
	}
	glPopMatrix();
}

void GraphicalPiece::Draw()
{
	PIECE_COLOR color = gCheckmate->GetColor(this->p);

	// Draw the bounding circle
	if(color== PIECE_COLOR::WHITE)
		glColor3d(.7,.7,.7);
	else
		glColor3d(.4,.4,.4);
	DrawCircle(this->x,this->y,PIECE_RADIUS);

	// Draw the letter representing this piece
	if(color== PIECE_COLOR::WHITE)
		glColor3d(1,1,1);
	else
		glColor3d(0,0,0);

	char firstLetter[2];
	strncpy_s(firstLetter, 2, gPieceTypeNames[(int)p], 1);
	firstLetter[1]='\0';
	DrawStrokeText(x-.2, y-.2,firstLetter);
}


//
// GLUT callback functions
//

void DrawBoard()
{
	for(int i=0; i<8; i++)
		for(int j=0; j<8; j++)
		{
			if((i+j)%2==0)
				glColor3d(.1,.1,.1);
			else
				glColor3d(.9, .9, .9);
			DrawRectangle((double)i,(double)j,(double)(i)+1.0,(double)(j)+1.0);
		}
}

void DrawPieces()
{
	for(int i=0; i<NUM_PIECES; i++)
		gPieces[i].Draw();
}

void DrawTurnBar()
{
	glColor3d(.1,.1,.9);
	DrawRectangle(0,8.4, 2.4,9.1);
	glColor3d(1,1,1);
	if(gTurn== PIECE_COLOR::WHITE)
		DrawStrokeText(.1,8.5,"White");
	else
		DrawStrokeText(.1,8.5,"Black");
}

bool ShouldDrawBestMove()
{
	int positions[POSITION_ARRAY_SIZE];
	positions[0] = (int)gTurn;
	for (int i = 1; i < POSITION_ARRAY_SIZE; i++)
		positions[i] = gPieces[i - 1].GetIndex();

	bool drawBestMoves = false;
	int moveCount =  (int)gCheckmate->GetMovesToCheckmateCount(positions);

	if(moveCount!=UNKNOWN && moveCount!=UNFORCEABLE && moveCount!=0 )
		drawBestMoves = true;

	return drawBestMoves;
}


void DrawLegalMoves(bool drawBestMoves)
{
	int positions[POSITION_ARRAY_SIZE];
	positions[0] = (int)gTurn;
	for (int i = 1; i < POSITION_ARRAY_SIZE; i++)
		positions[i] = gPieces[i - 1].GetIndex();

//	unsigned char s = gCheckmate->GetStatus(positions);
	int moveCount = (int)gCheckmate->GetMovesToCheckmateCount(positions);
	PIECE_COLOR expectedWinner = gCheckmate->GetExpectedWinner(positions);
	LEGAL_MOVE allLegalMoves[MAX_LEGAL_MOVES];
	int legalMoveCount = 0;

	gCheckmate->CalculateLegalMovesPositions(positions, allLegalMoves, legalMoveCount);
	for (int m = 0; m < legalMoveCount; m++)
	{
		int pieceIndex = allLegalMoves[m].pieceIndex;
		int oldPosition = allLegalMoves[m].oldPosition;
		int newPosition = allLegalMoves[m].newPosition;
		int x1 = oldPosition%8;
		int y1 = oldPosition/8;
		int x2 = newPosition%8;
		int y2 = newPosition/8;
		double dx = (double)x2-(double)x1;
		double dy = (double)y2-(double)y1;
		double length = sqrt(dx*dx+dy*dy);
		glColor3d(.1,.5,.9);

//		int t2, i2, j2, k2, l2;
//		t2=i2=j2=k2=l2=0;
		int positions2[POSITION_ARRAY_SIZE];
		gCheckmate->GenerateNewPositionFromLegalMove(positions,
						 allLegalMoves[m], positions2);

		if (drawBestMoves)
		{
			bool isBestMove = false;
			int x2 =  (int)gCheckmate->GetMovesToCheckmateCount(positions2);
//			unsigned char s2 = gCheckmate->GetStatus(positions2);
			PIECE_COLOR expectedWinner2 = gCheckmate->GetExpectedWinner(positions2);

			if( expectedWinner == PIECE_COLOR::WHITE && expectedWinner2 == PIECE_COLOR::WHITE)
			{
				if(gTurn== PIECE_COLOR::WHITE && x2==moveCount-1)						// tested and works
					isBestMove = true;
				if(gTurn== PIECE_COLOR::BLACK && x2==moveCount)						// 
					isBestMove = true;
			}
			if( expectedWinner == PIECE_COLOR::BLACK && expectedWinner2 == PIECE_COLOR::BLACK)
			{
				if(gTurn== PIECE_COLOR::WHITE && x2==moveCount)						// 
					isBestMove = true;
				if(gTurn== PIECE_COLOR::BLACK && x2==moveCount+1)						// 
					isBestMove = true;
			}

			/*
			if( ((s&IN_STALE_MATE) || (s&INSUFFICIENT_MATERIAL)) && ((s2&IN_STALE_MATE) || (s2&INSUFFICIENT_MATERIAL)) )
			{
				if(moveCount < 0) // black is successfully going for stalemate
				{
					if(gTurn== PIECE_COLOR::WHITE && x2==moveCount)						// 
						isBestMove = true;
					if(gTurn== PIECE_COLOR::BLACK && x2==moveCount+1)						// 
						isBestMove = true;
				}
				if(moveCount > 0) // white is successfully going for stalemate
				{
					if(gTurn== PIECE_COLOR::WHITE && x2==moveCount-1)						// 
						isBestMove = true;
					if(gTurn== PIECE_COLOR::BLACK && x2==moveCount)						// 
						isBestMove = true;
				}
			}
			*/

			if (isBestMove)
				glColor3d(.1, .9, .2); // this is a best move.
		} // if drawBestMoves
		
		double radians = atan2(dy, dx);
		double degrees = radians*180/3.141592654;
		glPushMatrix();
		glTranslated(x1+.5, y1+.5, 0);
		glRotated(degrees, 0,0,1);
		DrawRectangle(0+.35,-.07, length-.4,+.07);
		DrawTriangle(length-.4,+.3, length-.4,-.3, length-.15,0);
		glPopMatrix();
	} // for all legal moves
}


// This callback function gets called by the Glut
// system whenever it decides things need to be redrawn.
void display(void)
{
	glClear(GL_COLOR_BUFFER_BIT);

	DrawBoard();
	DrawPieces();

	// Decide whether to draw best moves:
	bool drawBestMoves = ShouldDrawBestMove();

	DrawLegalMoves(drawBestMoves);
	DrawTurnBar();

	glutSwapBuffers();
}


// This callback function gets called by the Glut
// system whenever a key is pressed.
void keyboard(unsigned char c, int x, int y)
{
	switch (c) 
	{
		case 27: // escape character means to quit the program
			exit(0);
			break;
		case 'b':
			// do something when 'b' character is hit.
			break;
		default:
			return; // if we don't care, return without glutPostRedisplay()
	}

	glutPostRedisplay();
}


// This callback function gets called by the Glut
// system whenever the window is resized by the user.
void reshape(int w, int h)
{
	// Reset our global variables to the new width and height.
	gScreen_x = w;
	gScreen_y = h;

	// Set the pixel resolution of the final picture (Screen coordinates).
	glViewport(0, 0, w, h);

	// Set the projection mode to 2D orthographic, and set the world coordinates:
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluOrtho2D(WXL, WXH, WYL, WYH);
	glMatrixMode(GL_MODELVIEW);

}

void motion(int x, int y)
{
	if(gGrabbedPiece!=-1)
	{
		y = (int)gScreen_y - y;
		double wx = (double)x/gScreen_x * WX+WXL;
		double wy = (double)y/gScreen_y * WY+WYL;

		gPieces[gGrabbedPiece].x = wx-gDx;
		gPieces[gGrabbedPiece].y = wy-gDy;
//		PrintStatus();
		glutPostRedisplay();
	}
}

// This callback function gets called by the Glut
// system whenever any mouse button goes up or down.
void mouse(int mouse_button, int state, int x, int y)
{
	// pixel to world coordinates:
	y = (int)gScreen_y - y;
	double wx = (double)x/gScreen_x * WX+WXL;
	double wy = (double)y/gScreen_y * WY+WYL;

	if (mouse_button == GLUT_LEFT_BUTTON && state == GLUT_DOWN) 
	{
		gStartI = -1;
		gStartJ = -1;
		gGrabbedPiece = -1;
		for(int i=0; i<NUM_PIECES; i++)
		{
			gDx = wx-gPieces[i].x;
			gDy = wy-gPieces[i].y;
			if (gDx*gDx + gDy*gDy < PIECE_RADIUS*PIECE_RADIUS)
			{
				gGrabbedPiece = i;
				gStartI = gPieces[i].GetI();
				gStartJ = gPieces[i].GetJ();
				break;
			}
		}

		// Switch Turn button?
		if (gGrabbedPiece==-1 && wx>0 && wx<2.4 && wy>8.4 && wy<9.1)
		{
			gTurn = gCheckmate->OtherColor(gTurn);
			PrintStatus();
		}
	}

	if (mouse_button == GLUT_LEFT_BUTTON && state == GLUT_UP) 
	{
		if (gGrabbedPiece!=-1)
		{
			int column = (int)(wx-gDx);
			int row = (int)(wy-gDy);
			if(column<0)
				column=0;
			if(column>7)
				column=7;
			if(row<0)
				row=0;
			if(row>7)
			{
				row=8;
				column=0;
			}
			if (gStartI != gPieces[gGrabbedPiece].GetI() || gStartJ != gPieces[gGrabbedPiece].GetJ())
				gTurn = gCheckmate->OtherColor(gTurn);;
			gPieces[gGrabbedPiece].SetPosition(column, row);
			gGrabbedPiece = -1;
			PrintStatus();
		}
	}

	glutPostRedisplay();
}

// Your initialization code goes here.
void InitializeMyStuff()
{
	gTurn = PIECE_COLOR::BLACK;
//	gPieces[0].Init(PIECE_TYPES::BLACK_KING, 4, 4); // column,row  or x,y  or horizontal,verticle
//	gPieces[1].Init(PIECE_TYPES::WHITE_KING, 5, 0); // 0 through 7 in both directions.
//	gPieces[2].Init(PIECE_TYPES::WHITE_QUEEN, 0, 1);
//	gPieces[3].Init(PIECE_TYPES::BLACK_ROOK, 1, 0);
//	gPieces[0].Init(PIECE_TYPES::BLACK_KING, 0, 1); // column,row  or x,y  or horizontal,verticle
//	gPieces[1].Init(PIECE_TYPES::WHITE_KING, 2, 0); // 0 through 7 in both directions.
//	gPieces[2].Init(PIECE_TYPES::WHITE_BISHOP, 0, 3);
//	gPieces[3].Init(PIECE_TYPES::WHITE_KNIGHT, 0, 0);
	gPieces[0].Init(PIECE_TYPES::BLACK_KING, 0, 0); // column,row  or x,y  or horizontal,verticle
	gPieces[1].Init(PIECE_TYPES::WHITE_KING, 7, 6); // 0 through 7 in both directions.
	gPieces[2].Init(PIECE_TYPES::WHITE_PAWN, 1, 2);
	gPieces[3].Init(PIECE_TYPES::BLACK_PAWN, 0, 1);

	std::vector< PIECE_TYPES> pieces;
	for (int i = 0; i < NUM_PIECES; i++)
		pieces.push_back(gPieces[i].p);

	bool loadData = false;
	bool printEvaluation = true;
	gCheckmate = new Checkmate(); // a "smart" checkmate object
	gCheckmate->Initialize(pieces, loadData, printEvaluation);

	cout << "Done Initializing Data." << endl;
	PrintStatus();
}


int main(int argc, char **argv)
{
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize((int)gScreen_x, (int)gScreen_y);
	glutInitWindowPosition(800, 50);

	int fullscreen = 0;
	if (fullscreen) 
	{
		glutGameModeString("800x600:32");
		glutEnterGameMode();
	} 
	else 
	{
		glutCreateWindow("Program 1 - Shapes");
	}

	glutDisplayFunc(display);
	glutKeyboardFunc(keyboard);
	glutReshapeFunc(reshape);
	glutMouseFunc(mouse);
	glutMotionFunc(motion);

	glColor3d(0,0,0); // forground color
	glClearColor(1, 1, 1, 0); // background color
	InitializeMyStuff();

	glutMainLoop();

	return 0;
}
