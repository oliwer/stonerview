CFLAGS = -O2 -g -pedantic -Wall
LDLIBS = -lm -lGL -lGLU -lXmu -lXext -lXi -lSM -lICE -lXt -lX11

stonerview: osc.o move.o view.o

clean:
	$(RM) *~ *.o stonerview
