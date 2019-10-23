# RS .6 Chess Plugin

this is a combination of an existing chess game, and the exampleplugin (rename script makes it easy to use as base for new plugin)
based on https://github.com/RetroShare/ExampleRSPlugin


~~Both players must currently select each other and press "play game" - ensure your friend has their chess window open before making a move!~~

# build & install:

put/clone `RetroChess` to `RetroShare/plugins/` recommend

	cd ${YOUR_DIR}/RetroShare/plugins/RetroChess/
	mkdir build
	cd build
	qmake ..
	make -j$(nproc)

Then restart your RetroShare. You'll see a chess logo in your chat dialog's tool-bar or home's tool-bar.

# Usage:


send a invite in chat dialog with your friend (peer mode only now TuT)



# disclaimer
- may not support playing more than one game of chess

- may crash RS

~~may kill your cat, dog, or other loved one.~~


#  extra info
based on: https://github.com/chozabu/CHESS-in-Qt
