// Fireclaw Copyright (C) 2003 Ian Packer <snazbaz at fireclaw.org>
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
// $Id: servers.c,v 1.5 2003/06/21 12:43:11 snazbaz Exp $
//

#include <ctype.h>

#include "main.h"

static int i;

void register_self()
{
	char sendbuf[513];
	if(serv->password)
	{
		sprintf(sendbuf, "PASS %s", serv->password);
		irc_write(sendbuf);
	}
	strcpy(cNick, config.nick);
	snprintf(sendbuf, 510, "NICK %s", config.nick);
	irc_write(sendbuf);
	snprintf(sendbuf, 510, "USER %s %d * :%s", config.ident, INIT_USERMASK, config.realname);
	irc_write(sendbuf);
}

void initiate_main_connect()
{
	init_oj();
	lastConnect = time(0);
	serv = get_next_server();
	serv->numtries++;
	if(!serv) {
		fprintf(out, "Error, no servers in my list!\n");
		exit(0);
	}
	fprintf(out, "Connection attempt %d [%s:%d]\n", serv->numtries, serv->hostname, serv->port);
	if(add_socket('m', 1, serv->hostname, serv->port, config.vhost, 0))
	{
		clear_ial();
		reset_queue();
		reset_timers();
		logout_all_admins();
	} else
		fprintf(out, "Could not connect.\n");
}

void init_servers()
{
	numserves = 0;
	i = -1;
}

struct Server *get_next_server()
{
	if(numserves == 0)
		return NULL;
	if(++i >= numserves)
		i = 0;
	return &servers[i];
}

int add_server(char *confstring)
{
	int portn;
	char *host, *port, *pass;
	host = confstring;
	port = strchr(host, ':');
	if(!port)
	{
		portn = DEFAULT_PORT;
		pass = NULL;
	} else {
		*port++ = '\0';
		pass = strchr(port, ' ');
		if(!pass)
		{
			portn = atoi(port);
			pass = NULL;
		} else {
			*pass++ = '\0';
			portn = atoi(port);
		}
	}

	servers[numserves].hostname = (char*)myalloc(strlen(host)+1);
	servers[numserves].port = portn;
	servers[numserves].password = pass ? (char*)myalloc(strlen(pass)+1) : NULL;
	if(pass)
		strcpy(servers[numserves].password, pass);
	strcpy(servers[numserves].hostname, host);

	servers[numserves].numtries = 0;
	servers[numserves].lastconnect = 0;
	servers[numserves].lastfail = 0;

	numserves++;

	return 1;
}

void kill_servers()
{
	int i;
	for(i = 0; i < numserves; i++)
	{
		free(servers[i].hostname);
		if(servers[i].password)
			free(servers[i].password);
	}
	init_servers();
}
