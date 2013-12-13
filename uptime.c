/* 
 * This file taken from eggdrop 1.6.15 and modified for Fireclaw.
 */

/*
 * $Id: uptime.c,v 1.7 2003/06/22 23:19:02 snazbaz Exp $
 *
 * This module reports uptime information about your bot to http://uptime.eggheads.org. The
 * purpose for this is to see how your bot rates against many others (including EnergyMechs
 * and Eggdrops) -- It is a fun little project, jointly run by Eggheads.org and EnergyMech.net.
 *
 * If you don't like being a part of it please just unload this module.
 *
 * Also for bot developers feel free to modify this code to make it a part of your bot and
 * e-mail webmaster@eggheads.org for more information on registering your bot type. See how
 * your bot's stability rates against ours and ours against yours <g>.
 */
/*
 * Copyright (C) 2001 proton
 * Copyright (C) 2001, 2002, 2003 Eggheads Development Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#define MAKING_UPTIME
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "main.h"
#ifdef REPORT_UPTIME
#include "uptime.h"

/*
 * both regnr and cookie are unused; however, they both must be here inorder for
 * us to create a proper struct for the uptime server. 
 */

typedef struct PackUp {
	int regnr;
	int pid;
	int type;
	unsigned long cookie;
	unsigned long uptime;
	unsigned long ontime;
	unsigned long now2;
	unsigned long sysup;
	char string[3];
} PackUp;

PackUp upPack;

static int uptimesock;
static int uptimecount;
static unsigned long uptimeip;
static char uptime_version[50] = "";
static int lastMsg;


void set_uptime_status(char *str)
{
	char temp[300];
	sprintf(temp, "%d uptime packet%s sent.", uptimecount,
			(uptimecount != 1) ? "s" : "");
	strcat(str, temp);
	return;
}

unsigned long get_ip()
{
	struct hostent *hp;
	IP ip;
	struct in_addr *in;

	/* could be pre-defined */
	if (uptime_host[0]) {
		if ((uptime_host[strlen(uptime_host) - 1] >= '0') &&
			(uptime_host[strlen(uptime_host) - 1] <= '9'))
		return (IP) inet_addr(uptime_host);
	}
	hp = gethostbyname(uptime_host);
	if (hp == NULL)
		return -1;
	in = (struct in_addr *) (hp->h_addr_list[0]);
	ip = (IP) (in->s_addr);
//	freehostent(hp);
	return ip;
}

int init_uptime(void)
{
	struct sockaddr_in sai;

	upPack.regnr = 0;  /* unused */
	upPack.pid = 0;    /* must set this later */
	upPack.type = htonl(uptime_type);
	upPack.cookie = 0; /* unused */
	upPack.uptime = 0; /* must set this later */
	uptimecount = 0;
	uptimeip = -1;

	lastMsg = time(NULL);

	sprintf(uptime_version, "%s", SHORT_VER);

	if ((uptimesock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("init_uptime socket returned < 0 %d\n", uptimesock);
		return ((uptimesock = -1));
	}
	memset(&sai, 0, sizeof(sai));
	sai.sin_addr.s_addr = INADDR_ANY;
	sai.sin_family = AF_INET;
	if (bind(uptimesock, (struct sockaddr *) &sai, sizeof(sai)) < 0) {
		printf("init_uptime bind returned < 0 %d\n", uptimesock);
		close(uptimesock);
		return ((uptimesock = -1));
	}
	fcntl(uptimesock, F_SETFL, O_NONBLOCK | fcntl(uptimesock, F_GETFL));
	printf("Reporting uptime to %s\n", uptime_host);
	return 0;
}


int send_uptime(void)
{
	struct sockaddr_in sai;
	struct stat st;
	PackUp *mem;
	int len;
	char uptime_version[64], relaybuf[513];
  
	sprintf(uptime_version, "%s", SHORT_VER);

	if (uptimeip == -1) {
		uptimeip = get_ip();
		if (uptimeip == -1)
			return -2;
	}
	uptimecount++;
	upPack.now2 = htonl(time(NULL));
	upPack.ontime = 0;

	if (main_irc) {
		upPack.ontime = htonl(main_irc->con_ts);
	}


	if (!upPack.pid)
		upPack.pid = htonl(getpid());

	if (!upPack.uptime)
		upPack.uptime = htonl(tStart);

	if (stat("/proc", &st) < 0)
		upPack.sysup = 0;
	else
		upPack.sysup = htonl(st.st_ctime);

	len = sizeof(upPack) + strlen(cNick) + strlen(serv->hostname) + strlen(uptime_version);
	mem = (PackUp *) myalloc(len);
	memcpy(mem, &upPack, sizeof(upPack));
	sprintf(mem->string, "%s %s %s", config.nick, serv->hostname, uptime_version);
	memset(&sai, 0, sizeof(sai));
	sai.sin_family = AF_INET;
	sai.sin_addr.s_addr = uptimeip;
	sai.sin_port = htons(uptime_port);
	sprintf(relaybuf, "%s: Sending uptime packet: \"%s\" (%d bytes, regnr: %d)", uptime_host, mem->string, len, mem->regnr);
	fprintf(out, "%s\n", relaybuf);
//	sRelay(relaybuf);
	len = sendto(uptimesock, (void *) mem, len, 0, (struct sockaddr *) &sai,
		sizeof(sai));
	free((void*)mem);
	return len;
}

void check_hourly()
{
	if(time(NULL) - lastMsg > 21600)	/* 6 hours */
	{
		lastMsg = time(NULL);
		send_uptime();
	}
	return;
}

#endif
