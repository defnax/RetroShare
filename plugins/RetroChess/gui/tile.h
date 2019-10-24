#ifndef TILE_H
#define TILE_H

#include <QLabel>
#include <QDebug>
#include <QWidget>

class Tile: public QLabel
{
public:
	//Constructors
	Tile(QWidget* pParent=0, Qt::WindowFlags f=0);
	Tile(const QString& text, QWidget* pParent = 0, Qt::WindowFlags f = 0);

	//Methods
protected:
	void mousePressEvent(QMouseEvent *event);

public:
	void display(char elem);
	void tileDisplay();
    void validate_send(int c);	// send
    void validate(int c);		// normal check(for sync)

	void setChessWindow( QWidget *board);
	QWidget* getChessWindow() const;

	//Fields
	int tileColor;	// "background" 0(black) : 1(white)
	int piece;		// 0(empty) : 1(piece occpied)
	int pieceColor;	// 0(black) : 1(white)
	int row,col;
	int tileNum;	// index in one-division array

	char pieceName;

private:
    QWidget *m_chess_window_p;	//parent board
};

void validate_tile(int row, int col, int c);

#endif // TILE_H
