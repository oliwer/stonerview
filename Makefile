DEBUG = -g -Wall -Werror -Wmissing-prototypes -Wno-unused
CFLAGS = $(DEBUG) -I/usr/X11R6/include

OBJS = main.o osc.o move.o view.o

XLIBS = -L/usr/X11R6/lib -lGL -lGLU -lXmu -lXext -lXi -lSM -lICE -lXt -lX11 

stonerview: $(OBJS)
	$(CC) $(CFLAGS) -o stonerview $(OBJS) $(XLIBS) -lm

clean:
	rm -f *~ *.o stonerview
