all:
	(cd src/vme; make all)
	(cd src/vmeTest; make all)

clean:
	(cd src/vme; make clean)
	(cd src/vmeTest; make clean)
	(cd src/vipo; make clean)

