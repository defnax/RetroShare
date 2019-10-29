!include("../Common/retroshare_plugin.pri"): error("Could not include file ../Common/retroshare_plugin.pri")

greaterThan(QT_MAJOR_VERSION, 4) {
	# Qt 5
	QT += widgets svg
}

exists($$[QMAKE_MKSPECS]/features/mobility.prf) {
  CONFIG += mobility
} else {
  QT += multimedia
}
CONFIG += qt uic qrc resources 
MOBILITY = multimedia
TARGET = RetroChess

DEPENDPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/temp/ui ../../libretroshare/src
INCLUDEPATH += ../../retroshare-gui/src/retroshare-gui

INCLUDEPATH += ../../rapidjson-1.1.0

#################################### Windows #####################################

linux-* {
	#INCLUDEPATH += /usr/include
	#LIBS += $$system(pkg-config --libs opencv)
}

win32 {
	LIBS_DIR = $$PWD/../../../libs
}

QMAKE_CXXFLAGS *= -Wall

SOURCES += RetroChessPlugin.cpp               \
          services/p3RetroChess.cc           \
          services/rsRetroChessItems.cc \
    gui/NEMainpage.cpp \
    gui/RetroChessNotify.cpp \
    gui/chess.cpp \
    gui/tile.cpp \
    gui/validation.cpp \
    gui/RetroChessChatWidgetHolder.cpp

HEADERS += RetroChessPlugin.h                 \
          services/p3RetroChess.h            \
          services/rsRetroChessItems.h       \
          interface/rsRetroChess.h \
    gui/NEMainpage.h \
    gui/RetroChessNotify.h \
    gui/tile.h \
    gui/validation.h \
    gui/chess.h \
    gui/RetroChessChatWidgetHolder.h



RESOURCES += gui/RetroChess_images.qrc

FORMS += \
    gui/NEMainpage.ui \
    gui/chess.ui

	
#################################### QCheckers #####################################

HEADERS	+= qcheckers/src/pdn.h \
	    qcheckers/src/checkers.h \
		qcheckers/src/echeckers.h \
		qcheckers/src/rcheckers.h \
	    qcheckers/src/capture.h \
		qcheckers/src/theme.h \
		qcheckers/src/field.h \
		qcheckers/src/toplevel.h \
		qcheckers/src/view.h \
		qcheckers/src/history.h \
		qcheckers/src/board.h \
	    qcheckers/src/newgamedlg.h \
	    qcheckers/src/common.h \
	    qcheckers/src/player.h \
		qcheckers/src/humanplayer.h \
		qcheckers/src/computerplayer.h
	   

SOURCES	+= qcheckers/src/pdn.cc \
	    qcheckers/src/checkers.cc \
		qcheckers/src/echeckers.cc \
		qcheckers/src/rcheckers.cc \
	    qcheckers/src/capture.cc \
		qcheckers/src/theme.cc \
		qcheckers/src/field.cc \
		qcheckers/src/toplevel.cc \
		qcheckers/src/view.cc \
		qcheckers/src/history.cc \
		qcheckers/src/board.cc \
	    qcheckers/src/newgamedlg.cc \
	    qcheckers/src/humanplayer.cc \
		qcheckers/src/computerplayer.cc

RESOURCES += qcheckers/qcheckers.qrc qcheckers/icons.qrc
