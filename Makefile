SOURCES.c = admins.c admin_msg.c allcomp.c badurls.c bans.c chans.c \
            chan_msg.c conf.c dcc.c events.c handle.c \
            link.c help.c ial.c main.c onjoinspam.c punish.c queue.c \
            servers.c signals.c socks.c tasks.c timers.c tools.c uptime.c  	    

INCLUDES=
CFLAGS= -Wall -O3 -g 
SLIBS= -lcrypt
CC = gcc
PROGRAM= claw

OBJECTS= $(SOURCES.c:.c=.o)

.KEEP_STATE:

debug := CFLAGS= -g

all debug: $(PROGRAM)

$(PROGRAM): $(INCLUDES) $(OBJECTS)
	$(LINK.c) -o $@ $(OBJECTS) $(SLIBS)

clean:
	rm -f $(PROGRAM) $(OBJECTS) adduser sysinfo.h core

*.o: cfg.h main.h

punish.o: punish.h

uptime.o: uptime.h
