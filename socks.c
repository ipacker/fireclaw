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
// $Id: socks.c,v 1.14 2003/07/16 15:35:02 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <fcntl.h>

#include "main.h"

static fd_set readset;
static fd_set writeset;
static int highsock;
static int running = 1;

struct socket *sockets;

void stop_running() { running = 0; }

void setup_socks()
{
	sockets = NULL;
	highsock = 0;
	main_irc = 0;
	FD_ZERO(&readset);
	FD_ZERO(&writeset);
}

void set_highsock()
{
	struct socket *i;
	i = sockets;
	highsock = 0;
	while(i) {
		if(i->fd > highsock)
			highsock = i->fd;
		i = i->next;
	}
}

struct socket *getsockbyid(int id)
{
	struct socket *i = sockets;
	while(i)
	{
		if(i->id == id)
			return i;
		i = i->next;
	}
	return NULL;
}

int _setnonblock(int fd)
{
/*    int flags;
    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
    	return 0;

    flags |= ~O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags ) < 0)
		return 0;

	return 1;
	*/
	
    if(fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
        return 0;
    }
	return 1;
	
}

int _setblock(int fd)
{
    int flags;
    if((flags = fcntl(fd, F_GETFL, 0)) < 0)
    	return 0;

    flags &= ~O_NONBLOCK;
    if(fcntl(fd, F_SETFL, flags ) < 0)
        return 0;

    return 1;
}

int irc_bind(int fd, char *vhost, int port)
{
	struct sockaddr_in myaddr;

	memset(&myaddr, 0, sizeof(myaddr) );
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons(port);

	if(vhost)
		myaddr.sin_addr.s_addr = inet_addr(vhost);
	else
		myaddr.sin_addr.s_addr = 0;

	if(0xffffffff == myaddr.sin_addr.s_addr)
		fprintf(out, "no vhost?\n");

	if(bind(fd, (struct sockaddr *)&myaddr, sizeof(myaddr) ) == -1) {
		fprintf(out, "bind error [%s]\n", vhost);
		return 0;
	}

	return 1;
}

unsigned long get_listen_ip()
{
	if(strlen(config.dcc_listenip) > 6)
		return ntohl(inet_addr(config.dcc_listenip));
	return ntohl(inet_addr(myip));
}

void accept_new(struct socket *a)
{
	struct sockaddr faddr;
	socklen_t flength_ptr;
	struct sockaddr_in *data;
	struct socket *temp, *i;

	temp = (struct socket *)myalloc(sizeof(struct socket));

	temp->status = SOCK_CONNECTING;
	temp->last_ts = time(0);
	temp->con_ts = time(0);
	temp->checkLink = 0;
	temp->next = 0;
	temp->buffer[0] = 0;

	temp->fd = accept(a->fd, &faddr, &flength_ptr);

	data = (struct sockaddr_in *)&faddr;

	switch(a->type) {
		case 'D':
			temp->type = 'd';
			temp->id = get_next_dcc_id() + DCC_ID_OFSET;
			add_dcc_chat(0, inet_ntoa(data->sin_addr), 0, temp);
			break;
		case 'F':
			temp->type = 'f';
			temp->id = get_next_fire_id();
			firelink_new(temp, 1);
			break;
		default:
			close(temp->fd);
			free(temp);
			return;
	}

	if(!sockets) {
		sockets = temp;
	} else {
		i = sockets;
		while(i->next)
			i = i->next;
		i->next = temp;
	}

	FD_SET(temp->fd, &readset);
	FD_SET(temp->fd, &writeset);

	set_highsock();
	return;
}

struct socket *add_listen(char type, int id, int port, char *vhost)
{
	int err;
	struct socket *temp, *i;

	if(getsockbyid(id))
		return 0;

	/* we assume all parameters are checked and valid here. */

	temp = (struct socket *)myalloc(sizeof(struct socket));

	temp->type = type;
	temp->status = SOCK_LISTEN;
	temp->id = id;
	temp->last_ts = time(0);
	temp->con_ts = time(0);
	temp->checkLink = 0;
	temp->next = 0;
	temp->buffer[0] = 0;

	temp->fd = socket(AF_INET, SOCK_STREAM, 0);

	if(strlen(vhost) > 3)
		err = irc_bind(temp->fd, vhost, port);
	else
		err = irc_bind(temp->fd, 0, port);

	if(!err) {
		free(temp);
		return 0;
	}

	if(!_setnonblock(temp->fd)) {
		fprintf(out, "Error, unable to set socket into nonblocking mode!\n");
		free(temp);
		return 0;
	}

	err = listen(temp->fd, 5);

	if(err) {
		fprintf(out, "Error opening socket for listening.\n");
		free(temp);
		return 0;
	}

	if(!sockets) {
		sockets = temp;
	} else {
		i = sockets;
		while(i->next)
			i = i->next;
		i->next = temp;
	}

	FD_SET(temp->fd, &readset);
//	FD_SET(temp->fd, &writeset);

	set_highsock();

	return temp;
}

struct socket *add_socket(char type, int id, char *addr, int port, char *vhost, unsigned long ulip)
{
	int err;
	struct socket *temp, *i;
	struct sockaddr_in target;
	struct hostent *hp;
	struct in_addr *t;

	if(getsockbyid(id))
		return 0;

	/* we assume all parameters are checked and valid here. */

	temp = (struct socket *)myalloc(sizeof(struct socket));

	temp->type = type;
	temp->status = SOCK_CONNECTING;
	temp->id = id;
	temp->last_ts = time(0);
	temp->con_ts = time(0);
	temp->next = 0;
	temp->buffer[0] = 0;
	temp->fd = socket(AF_INET, SOCK_STREAM, 0);

	if(type == 'm')
		main_irc = 0;

	memset(&target, 0, sizeof(target));
	target.sin_family = AF_INET;
	target.sin_port = htons(port);

	if(addr) {
		if ((addr[strlen(addr)-1] >= '0') &&
				(addr[strlen(addr)-1] <= '9')) {
			target.sin_addr.s_addr = inet_addr(addr);
		} else {
			hp = gethostbyname(addr);
			if (hp == NULL)
				return 0;
			t = (struct in_addr *) hp->h_addr_list[0];
			target.sin_addr.s_addr = t->s_addr;
		}
	} else {
		target.sin_addr.s_addr = htonl(ulip);
	}


	if(strlen(vhost) > 3)
		irc_bind(temp->fd, vhost, 0);

	if(!_setnonblock(temp->fd)) {
		fprintf(out, "Error, unable to set socket into nonblocking mode!\n");
		free(temp);
		return 0;
	}

	err = connect(temp->fd, (struct sockaddr *)&target, sizeof(target));

//	if(err) {
//		free(temp);
//		return 0;
//	}

	if(!sockets) {
		sockets = temp;
	} else {
		i = sockets;
		while(i->next)
			i = i->next;
		i->next = temp;
	}

	if(type == 'm')
		main_irc = temp;

	FD_SET(temp->fd, &readset);
	FD_SET(temp->fd, &writeset);

	set_highsock();

	return temp;
}

void on_connect_success(struct socket *i)
{
	switch(i->type) {
		case 'f':
			firelink_add(i);
			break;
		case 'm':
			register_self();
			break;
		case 'd':
			dcc_connected(i->id);
			break;
		case 'J':
			oj_connect();
			break;
	}
}

void parseData(struct socket *i, char *newdata, int bytes)
{
	int iter;
	char *pch, *pnew;

	pch = i->buffer;

	while(*pch)
		pch++;

	pnew = newdata;
	for(iter = 0; iter < bytes; iter++)
	{
		if(*pnew == '\n') {
			*pch = 0;
			fprintf(out, "(%d) <-- [%s]\n", i->id, i->buffer);
			switch(i->type) {
				case 'm': 
					{
						count_main_in += (pch - i->buffer);
						parseLine(i->buffer);
						break;
					}
				case 'd':
					{
						count_dcc_in  += (pch - i->buffer);
						parseDCC(i->id, i->buffer);
						break;
					}
				case 'f':
					{
						count_link_in += (pch - i->buffer);
						firelink_parse(i, i->buffer);
						break;
					}
				case 'J':
					{
						count_oj_in += (pch - i->buffer);
						oj_line(i->buffer);
						break;
					}
			}
			pch = i->buffer;
		} else if(*pnew != '\r' && *pnew)
			*pch++ = *pnew;
		pnew++;
	}

	*pch = 0;
}

void fixBrokenSocks()
{
	struct socket *i, *last, *d;
	char ssb[513];

	i = sockets;
	last = NULL;
	while(i) {
		if(i->status == SOCK_CONNECTING && time(0) - i->con_ts > 300) {
			fprintf(out, "timed out.\n");
		} else if(i->status == SOCK_BROKEN) {
			fprintf(out, "Lost connection on socket %d, type: '%c'\n", i->id, i->type);
			if(last)
				last->next = i->next;
			else
				sockets = i->next;
			FD_CLR(i->fd, &readset);
			FD_CLR(i->fd, &writeset);
			close(i->fd);
			
			if(i == main_irc) {
				main_irc = 0;
				if(time(0) - i->con_ts > tConMax)
					tConMax = time(0) - i->con_ts;
				lastConnect = time(0);
				strcpy(myNetwork, "none");
				sprintf(ssb, "SET %s NETWORK none", config.linkname);
				send_to_all_links(ssb);
				sprintf(ssb, "SET %s NICK %s", config.linkname, config.nick);
				send_to_all_links(ssb);
			} else if(i->type == 'd') {
				chat_terminated(i->id);
			} else if(i->type == 'J') {
				oj_dead();
			} else if(i->type == 'f') {
				firelink_die(i);
			}
			d = i;
			i = i->next;
			free(d);
			set_highsock();
		} else if(i->type == 'd' && (time(0) - i->last_ts > config.dcc_timeout)) {
			sockwrite(i, "Session timed out, no activity.");
			i->status = SOCK_BROKEN;
		} else if(i->type == 'm' && (time(0) - i->last_ts > 60) && i->checkLink == 0) {
			irc_write("PING :wibble");
			i->checkLink = 1;
		} else if(i->type == 'm' && (time(0) - i->last_ts > 600)) {
			sendQUIT("broken server?");
			i->status = SOCK_BROKEN;
		} else if(i->type == 'f' && (time(0) - i->last_ts > 60) && i->checkLink == 0) {
			sockwrite(i, "PING");
			i->checkLink = 1;
		} else if(i->type == 'f' && (time(0) - i->last_ts > 600)) {
			sockwrite(i, "ERROR Timeout");
			i->status = SOCK_BROKEN;
		} else {
			last = i;
			i = i->next;
		}
	}
	return;
}

int io_loop()
{
	char incoming[1024];
	struct timeval tv;
	struct socket *i;
	fd_set rs;
	fd_set ws;
	int sret, check, bytes, count_responding;

	tv.tv_usec = 0;
	check = 0;
	bytes = 0;
	
	while(running) {
		tv.tv_sec = 1;
		count_responding = 0;
		rs = readset;
		ws = writeset;

		sret = select(highsock + 1, &rs, &ws, NULL, &tv);

//		fprintf(out, "%d\n", sret);

		if(sret < 0) {
			fprintf(out, "select() error\n");
			sleep(1);
		} else if(sret > 0) {
			i = sockets;
			while(i) {
				if(i->status == SOCK_CONNECTING) {
					if(FD_ISSET(i->fd, &ws)) {
						i->status = SOCK_READY;
						on_connect_success(i);
						FD_CLR(i->fd, &writeset);
					}
				} else if(i->status == SOCK_LISTEN) {
					if(FD_ISSET(i->fd, &rs)) {
						fprintf(out, "new connection\n");
						accept_new(i);
					}
				} else if(FD_ISSET(i->fd, &rs)) {
					bytes = read(i->fd, incoming, 1000);
					if(bytes > 0) {
						i->last_ts = time(0);
						i->checkLink = 0;
						parseData(i, incoming, bytes);
					} else if(bytes <= 0) {
						/* something is broken on this socket */
						count_responding = 1;
						i->status = SOCK_BROKEN;
					}
				} 
				
				i = i->next;
			}
		} else {
			if(main_irc) {
				popMsg();
				check_timers();
			} else {
				if( (time(0) - lastConnect > config.condelay) || doHop) {
					doHop = 0;
					initiate_main_connect();
				}
			}
		}
		fixBrokenSocks();
	} /* while */

	return -1;
}

int sockwrite(struct socket *outsock, char *output)
{
	char temp[513];
	int len;

	if(strlen(output) >= 510)
	{
		fprintf(out, "[warning] tried to send a long (512+) msg\n");
		return 0;
	}

	strcpy(temp, output);
	strcat(temp, "\r\n");

	len = strlen(temp);

	fprintf(out, "(%d) --> [%s]\n", outsock->id, output);

	switch(outsock->type) {
		case 'm':
			count_main_out += len;
			break;
		case 'd':
			count_dcc_out += len;
			break;
		case 'J':
			count_oj_out += len;
			break;
		case 'f':
		case 'e':
			count_link_out += len;
			break;
	}

	return send(outsock->fd, temp, len, 0);
}

int irc_write(char *output)
{
	if(!main_irc)
		return 0;
	lastMsg = time(0);
	bper += strlen(output) + 2;
	return sockwrite(main_irc, output);
}

void irc_disconnect()
{
	if(main_irc) {
		main_irc->status = SOCK_BROKEN;
		fixBrokenSocks();
	}
	return;
}

void clean_all_socks()
{
	struct socket *i, *f;
	i = sockets;
	while(i) {
		f = i;
		i = i->next;
		close(f->fd);
		free(f);
	}
	setup_socks();
}


void kill_dcc_listen()
{
	struct socket *i;
	i = sockets;
	while(i)
	{
		if(i->type == 'D' || i->type == 'F') {
			i->status = SOCK_BROKEN;
		}
		i = i->next;
	}
	fixBrokenSocks();
}
