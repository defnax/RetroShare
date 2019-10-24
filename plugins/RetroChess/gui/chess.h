#ifndef CHESS_H
#define CHESS_H

#include "tile.h"
#include "validation.h"

#include <QWidget>

#include "retroshare/rspeers.h"

namespace Ui
{
class RetroChessWindow;
};

class RetroChessWindow : public QWidget
{
	Q_OBJECT

private:
	Ui::RetroChessWindow *m_ui;	//ui

	void initAccessories();
	void initChessBoard();
	RsPeerId p1id;
	RsPeerId p2id;
	std::string p1name;
	std::string p2name;

public:
	std::string mPeerId;
	explicit RetroChessWindow(std::string peerid, int player = 0, QWidget *parent = 0);
	~RetroChessWindow();
	int currentplayer;
	int myid;

	//from global

	int wR,wC,bR,bC;

	Tile *click1;
	Tile *tile[8][8];

	int count;	// click counter
    int turn;	// player turn (0: black turn / 1: white turn)
    int m_localplayer_turn;	// record local player's character (0: black / 1: white)
	int max;
	int *texp;
    int m_flag_finished;	// finish flag: (0: not finish / non-zero: finished)

	void disOrange();
	void validate_tile(int row, int col, int c);

	int flag,retVal;
	int chooser(Tile *temp);
	int validateBishop(Tile *temp);
	int validateQueen(Tile *temp);
	int validateKing(Tile *temp);
	int validateHorse(Tile *temp);
	int validateRook(Tile *temp);
	int validatePawn(Tile *temp);
	void orange();	// draw orange background represent avaiable movement of tiles
	int check(Tile *temp);

    int resultJudge();	// judge result (slow method)
};


extern QWidget* make_board();

#endif // CHESS_H
