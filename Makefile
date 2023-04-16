CC = g++ -g -Wno-deprecated

SESSION_LAYER_HOME = ${SPS_HOME}/SharedObjects/SessionLayer

INCLUDE = -I${SESSION_LAYER_HOME}/Include \
			-I/${XERCESCROOT}/include \
			-I${SPS_HOME}/Include

LIBS = -L${SPS_HOME}/Lib
ABL_FLAGS = -labld -ldl -lpthread

OBJECTS = OSSUserInfo.o SessionLayer.o XMLIAClient.o
MAINOBJ = Main.o

EXE = ${SESSION_LAYER_HOME}/Bin/Session.exe

vpath %.cpp ${SESSION_LAYER_HOME}/Source
vpath %.h ${SESSION_LAYER_HOME}/Include
vpath %.o ${SESSION_LAYER_HOME}/Lib

.SUFFIXES: .cpp .o .h

debug : CC = g++ -g

build : ${EXE}
debug : ${EXE}

.cpp.o: %.h
	$(CC) -c -fPIC -o ${SESSION_LAYER_HOME}/Lib/$*.o ${SESSION_LAYER_HOME}/Source/$*.cpp $(INCLUDE) ${LIBS} ${ABL_FLAGS}

${EXE} : ${OBJECTS} ${MAINOBJ}
	${CC} -o $@ ${SESSION_LAYER_HOME}/Lib/*.o ${ABL_FLAGS} -I${INCLUDE} ${LIBS} ${ABL_FLAGS}

clean:
	rm -f ${SESSION_LAYER_HOME}/Bin/SessionLayer.exe
	rm -f ${SESSION_LAYER_HOME}/Lib/*.o
