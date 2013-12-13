// Fireclaw Copyright (C) 2003 Ian Packer <snazbaz at onetel.net.uk>
// 
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
// 
// $Id: timers.c,v 1.24 2003/07/16 15:35:02 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "main.h"

#ifdef REPORT_UPTIME
void check_hourly();
#endif

struct Timer {
	time_t ts;
	void *function;
};

static struct Timer *timers;

/* the vars above are the beginnings of a better timer system, not used yet. */




static int timer60, timer5, timer1;
static char whobuf[BUFSIZE];

void init_timers()
{
	timers = NULL;
}

void timer_skew()
{
	struct chanRec *c;
	struct suser *u;
	struct criminalRecord *i;
	int iter;
	sRelay("(!) clock skew detected. resetting timers and protection mechanisms.");
	timeSkewWait = time(0) + 10;
	reset_timers();
	/* lets cycle chans and ial and reset all flood protection and repeat timestamps */
	c = chans.next;
	while(c) {
		if(c->physical) {
			c->physical->flood_last = time(0);
			c->physical->flood_lines = 0;
			u = c->physical->user;
			while(u) {
				u->flines = 0;
				u->lastMsg = time(0);
				u->idleMsg = time(0);
				u->lastNick = time(0);
				u = u->next;
			}
			i = c->physical->nortyusers;
			while(i) {
				i->lastJoin = time(0);
				i->njoins = 1;
				for(iter = 0; iter < REPEAT_HISTORY_SIZE; iter++)
					i->trackrepeat[iter].id = 0;
				i = i->next;
			}
		}
		c = c->next;
	}
	/* phew! */
}

void reset_timers()
{
	timer5 = time(NULL);
	timer1 = time(NULL);
	timer60 = time(NULL);
	return;
}

void check_timers()
{
	/* someone changed the system clock backward (urgh) */
	if(timer1 > time(NULL))
		timer_skew();

	if(timer1 < time(NULL) - 60)
	{
		timer1 = time(NULL);
		expireBans();
		/* someone skipped the system clock forward excessive amounts? */
		if(time(NULL) - timer5 > 1000)
			timer_skew();

		checkNeedCon();

		/*******  5 min timer ********/
		if(timer5 < time(NULL) - 300)
		{
			timer5 = time(NULL);
			runTimer5();
		}
		/******* 1 hour timer ********/
		if(timer60 < time(NULL) - 3600)
		{
			timer60 = time(NULL);
			expireCriminals();
		}
	}
	return;
}

void runTimer5()
{
	struct chanRec *i = chans.next;

	service_auth();
	start_links();

	fprintf(out, "DEBUG: Timer5\n");

	/* add_listen wont add multiple entries with the same ID, so we don't need to
		check if there is already a listening dcc socket. */
	if(config.dcc_listenport > 0)
		if(add_listen('D', 3, config.dcc_listenport, config.dcc_listenip))
			fprintf(out, "Listening for connections on %s:%d\n", config.dcc_listenip, config.dcc_listenport);

	if(config.linkport > 0)
		if(add_listen('F', 4, config.linkport, config.vhost))
			fprintf(out, "Listening for connections on %s:%d\n", config.vhost, config.linkport);

	while(i != NULL)
	{
		if(i->physical)
		{
			if(chan_has_rn_bans(i) || strchr(i->flags, 'a'))
			{
				sprintf(whobuf, "WHO %s %%cnar", i->name);
				lowMsg(whobuf);
			} 
			if(strchr(i->flags, 'l'))
				checkLimit(i->physical);
			searchForMissedBans(i->physical);
			checkNiceModes(i->physical);
		} else
			if(strchr(i->flags, 'j'))
		{
			sprintf(whobuf, "JOIN %s %s", i->name, i->key);
			lowMsg(whobuf);
		}
		i = i->next;
	}

	if(strcmp(cNick, config.nick))
	{
		sprintf(whobuf, "NICK %s", config.nick);
		highMsg(whobuf);
	}

	#ifdef REPORT_UPTIME
	check_hourly();
	#endif

	return;
}

void expireCriminals()
{
	struct criminalRecord *i, *fr;
	struct schan *c;

	c = ial.channel;
	while(c != NULL)
	{
		i = c->nortyusers;
		while(i)
		{
			if(--i->longTerm <= 0) {
				fr = i; i = i->next;
				nu_del(c, fr);
			} else
				i = i->next;
		}
		c = c->next;
	}

	return;
}

void checkNiceModes(struct schan *theChan)
{
	char *p1;
	char yn = 0;

	sprintf(whobuf, "MODE %s ", theChan->name);

	for(p1 = theChan->modes; *p1; p1++)
	{
		if(strchr(theChan->record->badModes, *p1))
		{
			strcat(whobuf, theChan->record->badModes);
			yn = 1;
			break;
		}
	}

	for(p1 = theChan->record->goodModes + 1; *p1; p1++)
	{
		if(!strchr(theChan->modes, *p1))
		{
			strcat(whobuf, theChan->record->goodModes);
			yn = 1;
			break;
		}
	}

	if(yn)
		lowMsg(whobuf);

	return;
}

void checkLimit(struct schan *theChan)
{
	if(check_dependants(theChan))
		return;
	if(theChan->userSize >= theChan->limit) {
		theChan->tsLim = time(0);
		sprintf(whobuf, "MODE %s +l %d", theChan->name, theChan->userSize + config.float_thresh);
		lowMsg(whobuf);
	} else
	if((time(0) - theChan->tsLim) > config.float_period) {
		if( (theChan->userSize > theChan->limit - config.float_step) || (theChan->userSize < theChan->limit - config.float_step4) )
		{
			theChan->tsLim = time(0);
			sprintf(whobuf, "MODE %s +l %d", theChan->name, theChan->userSize + config.float_thresh);
			highMsg(whobuf);
		}
	}
	return;
}
