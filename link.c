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
// $Id: link.c,v 1.9 2003/07/16 15:35:02 snazbaz Exp $
//

#include <ctype.h>

#include "main.h"

struct authWait {
	int status;
	struct socket *psock;
};

static char lbuf[1024];
static struct authWait authlist[MAX_AUTHWAIT];
static struct linkline *links = NULL;
static int linkcount = 5000;
struct rMember *rusers = NULL;

extern struct dcc dcclist[MAX_DCC];
extern struct socket *sockets;

void firelink_init()
{
	memset(authlist, 0, sizeof(authlist));
}

int get_next_fire_id()
{
	int i;
	for(i = 0; i < MAX_AUTHWAIT; i++) {
		if(authlist[i].status == 0)
			return i + 15000;
	}
	return 15000;
}

void firelink_new(struct socket *sock, int accepting)
{
	int authid = sock->id - 15000;
	authlist[authid].status = 1;
	authlist[authid].psock = sock;
}

void firelink_die(struct socket *sock)
{
	struct linkline *ldat;
	int i;
	if(sock->id >= 15000) {
		for(i = 0; i < MAX_AUTHWAIT; i++) {
			if(sock == authlist[i].psock) {
				authlist[i].status = 0;
				authlist[i].psock = NULL;
			}
		}
		return;
	}

	ldat = fetch_link_id(sock->id);

	if(ldat) {
		sprintf(lbuf, "UNLINK %s", ldat->name);
		send_to_links_butone(sock->id, lbuf);
		unlink_bot(ldat->bots);
		ldat->bots = NULL;
		sprintf(lbuf, "[\002link\002] Lost connection to %s. (burst: %d)", ldat->name, ldat->burst);
		sRelay(lbuf);
		ldat->burst = -1;
	}
	
}

void firelink_add(struct socket *sock)
{
	struct linkline *ldat = fetch_link_id(sock->id);

	if(sock->id >= 15000)
		return;

	if(!ldat) {
		sock->status = SOCK_BROKEN;
		return;
	}

	ldat->burst = 0;
	
	sprintf(lbuf, "LOGIN %s %s", config.linkname, ldat->password);
	sockwrite(sock, lbuf);
}

void firelink_parse(struct socket *sock, char *buffer)
{
	char tokenline[513];
	struct linkline *linkdat;
	char *tokens[MAXTOK];
	int numtok;

	linkdat = fetch_link_id(sock->id);

	if(!*buffer)
		return;
	
	mystrncpy(tokenline, buffer, 513);
	memset(tokens, 0, sizeof(tokens) );
	numtok = tokenize(tokenline, " ", tokens, MAXTOK);

	if(sock->id >= 15000) {
		int aid = sock->id - 15000;
		/* socket still authenticating */
		if(!strcmp(tokens[0], "LOGIN") && numtok == 3) {
			linkdat = fetch_link(tokens[1]);
			if(!linkdat || strcmp(linkdat->password, tokens[2]) || linkdat->burst != -1 || linkdat->type != 'f') {
				sprintf(lbuf, "[\002link\002] Denied connection from %s.", tokens[1]);
				sRelay(lbuf);
				sprintf(lbuf, "LOGIN %s DENIED", config.linkname);
				sockwrite(sock, lbuf);
				sock->status = SOCK_BROKEN;
				authlist[aid].status = 0;
				authlist[aid].psock = NULL;
			} else {
				sprintf(lbuf, "[\002link\002] Accepting connection from %s.", tokens[1]);
				sRelay(lbuf);
 				sprintf(lbuf, "LOGIN %s ACCEPTED", config.linkname);
				sockwrite(sock, lbuf);
				sock->id = linkdat->id;
				firelink_burst(sock, linkdat);
				authlist[aid].status = 0;
				authlist[aid].psock = NULL;
			}
		} else {
			sockwrite(sock, "ERROR Expecting LOGIN");
		}
		return;
	}

	if(!linkdat)
		return;

	if(numtok > 1 && !strcmp(tokens[0], "ERROR")) {
		sprintf(lbuf, "[\002link\002] Error from %s: %s", linkdat->name, buffer + (tokens[1] - tokens[0]));
		sRelay(lbuf);
		return;
	}

	if(numtok == 3 && !strcmp(tokens[0], "LOGIN")) {
		if(strcmp(linkdat->name, tokens[1])) {
			sockwrite(sock, "ERROR Different linkname expected.");
			sock->status = SOCK_BROKEN;
			sprintf(lbuf, "[\002link\002] Error, link to '%s' cancelled, (%s != %s)", linkdat->name, linkdat->name, tokens[1]);
		} else {
			if(!strcmp(tokens[2], "ACCEPTED")) {
				sprintf(lbuf, "[\002link\002] Link to '%s' accepted.", linkdat->name);
				sRelay(lbuf);
				firelink_burst(sock, linkdat);
			} else {
				sprintf(lbuf, "[\002link\002] Link to '%s' denied.", linkdat->name);
				sRelay(lbuf);
				sock->status = SOCK_BROKEN;
			}
		}
		return;
	}

	if(!strcmp(tokens[0], "ENDBURST")) {
		linkdat->burst = 1;
		return;
	}

	if(linkdat->burst < 1)
		return;

	if(!strcmp(tokens[0], "PING")) {
		sockwrite(sock, "PONG");
		return;
	}

	if(!strcmp(tokens[0], "PONG")) {
		sock->checkLink = 0;
		return;
	}

	if(numtok < 2)
		return;

	if(!strcmp(tokens[0], "UNLINK")) {
		find_unlink_bot(&linkdat->bots, tokens[1]);
		send_to_links_butone(sock->id, buffer);
	}

	if(numtok < 3)
		return;

	if(!strcmp(tokens[0], "MSG") && config.dcc_isolate == 0) {
		sprintf(lbuf, "<%s@%s> %s", tokens[2], tokens[1], buffer + (tokens[3] - tokens[0]));
		send_to_all_dcc(lbuf, 1);
		send_to_links_butone(sock->id, buffer);
		return;
	}

	if(!strcmp(tokens[0], "ACT") && config.dcc_isolate == 0) {
		sprintf(lbuf, "* %s@%s %s", tokens[2], tokens[1], buffer + (tokens[3] - tokens[0]));
		send_to_all_dcc(lbuf, 1);
		send_to_links_butone(sock->id, buffer);
		return;
	}

	if(numtok < 3)
		return;

	/* GOTOP bot #chan */
	if(!strcmp(tokens[0], "GOTOP")) {
		struct rBot *b = fetch_bot_from_line(tokens[1], linkdat->bots);
		if(!b) {
			sockwrite(sock, "ERROR (GOTOP) Don't have that bot in my list.");
			return;
		}
		if(!strcmp(b->set_network, myNetwork)) {
			struct schan *c = findiChan(tokens[2]);
			if(c && !isOp(c, cNick)) {
				sprintf(lbuf, "OP %s %s %s", config.linkname, cNick, tokens[2]);
				sockwrite(sock, lbuf);
			}
		}
		send_to_links_butone(sock->id, buffer);
		return;
	}

	if(numtok < 4)
		return;

	/* OP bot nick #chan */
	if(!strcmp(tokens[0], "OP")) {
		struct rBot *b = fetch_bot_from_line(tokens[1], linkdat->bots);
		if(!b) {
			sockwrite(sock, "ERROR (OP) Don't have that bot in my list.");
			return;
		}
		if(strchr(linkdat->flags, 't') && strchr(b->flags, 't') && !strcmp(b->set_network, myNetwork)) {
			struct schan *c = findiChan(tokens[3]);
			if(c && isOp(c, cNick)) {
				sprintf(lbuf, "MODE %s +o %s", c->name, tokens[2]);
				highMsg(lbuf);
			}
		}
		send_to_links_butone(sock->id, buffer);
		return;
	}

	/* INV bot nick #chan */
	if(!strcmp(tokens[0], "INV")) {
		struct rBot *b = fetch_bot_from_line(tokens[1], linkdat->bots);
		if(!b) {
			sockwrite(sock, "ERROR (INV) Don't have that bot in my list.");
			return;
		}
		if(strchr(linkdat->flags, 't') && strchr(b->flags, 't') && !strcmp(b->set_network, myNetwork)) {
			struct schan *c = findiChan(tokens[3]);
			if(c && isOp(c, cNick)) {
				sprintf(lbuf, "INVITE %s :%s", tokens[2], c->name);
				highMsg(lbuf);
			}
		}
		send_to_links_butone(sock->id, buffer);
		return;
	}

	/* SET bot var value */
	if(!strcmp(tokens[0], "SET")) {
		struct rBot *b = fetch_bot_from_line(tokens[1], linkdat->bots);
		if(!b) {
			sockwrite(sock, "ERROR (SET) Don't have that bot in my list.");
			return;
		}
		if(!strcmp(tokens[2], "NETWORK")) {
			mystrncpy(b->set_network, tokens[3], 32);
		} else if(!strcmp(tokens[2], "NICK")) {
			mystrncpy(b->set_nick, tokens[3], 32);
		}
		send_to_links_butone(sock->id, buffer);
		return;
	}

	/* QUIT id bot reason */
	if(!strcmp(tokens[0], "QUIT")) {
		int tid;
		struct rMember *i = rusers;
		tid = atoi(tokens[1]);
		while(i) {
			if(i->id == tid && strmi(tokens[2], i->bot)) {
				sprintf(lbuf, "[\002quit\002] (%s) %s has left (Reason: %s)", i->bot, i->nick, buffer + (tokens[3] - tokens[0]));
				sRelay(lbuf);
				free(i->host);
				if(i->prev) {
					i->prev->next = i->next;
				} else {
					rusers = i->next;
				}
				if(i->next) {
					i->next->prev = i->prev;
				}
				free(i);
				break;
			}
			i = i->next;
		}
		send_to_links_butone(sock->id, buffer);
		return;
	}

	/* USER id nick host bot */
	if(!strcmp(tokens[0], "USER")) {
		struct rMember *temp, *i;
		temp = (struct rMember *)myalloc(sizeof(struct rMember));
		temp->id = atoi(tokens[1]);
		mystrncpy(temp->nick, tokens[2], 32);
		mystrncpy(temp->bot, tokens[4], 32);
		temp->host = (char*)myalloc(strlen(tokens[3])+1);
		strcpy(temp->host, tokens[3]);
		temp->next = NULL;

		if(!rusers) { 
			rusers = temp;
			temp->prev = NULL;
		} else {
			i = rusers;
			while(i->next)
				i = i->next;
			i->next = temp;
			temp->prev = i;
		}
		sprintf(lbuf, "[\002join\002] (%s) %s is now with us.", tokens[4], tokens[2]);
		sRelay(lbuf);
		send_to_links_butone(sock->id, buffer);
		return;
	}

	/* LINK host newlink flags */
	if(!strcmp(tokens[0], "LINK")) {
		struct rBot *temp, *i;
		/* if newlink exists on my side, reject linkline. else: */
		if(fetch_bot_from_all(tokens[2])) {
			sockwrite(sock, "ERROR Bot exists already.");
			sock->status = SOCK_BROKEN;
			return;
		} else if(strchr(linkdat->flags, 'l') && tokens[1][0] != '\001') { /* not allowed links */
			sockwrite(sock, "ERROR You are not allowed to hub other links.");
			sock->status = SOCK_BROKEN;
			return;
		}
		temp = (struct rBot *)myalloc(sizeof(struct rBot));
		mystrncpy(temp->name, tokens[2], 32);
		mystrncpy(temp->flags, tokens[3], FLAGLEN);
		strcpy(temp->set_network, "none");
		strcpy(temp->set_nick, "none");
		temp->bots = NULL;
		temp->next = NULL;
		if(strcmp(tokens[1], "\001")) {
			i = fetch_bot_from_line(tokens[1], linkdat->bots);
			if(!i) {
				free(temp);
				sockwrite(sock, "ERROR Missing link.");
				return;
			}
			if(!i->bots) {
				i->bots = temp;
			} else {
				i = i->bots;
				while(i->next)
					i = i->next;
				i->next = temp;
			}
			sprintf(lbuf, "[\002link\002] New link %s connected to %s.", tokens[2], tokens[1]);
			sRelay(lbuf);
			send_to_links_butone(sock->id, buffer);
		} else if(!linkdat->bots) {
			strcpy(temp->flags, linkdat->flags);
			linkdat->bots = temp;
			sprintf(lbuf, "LINK %s %s %s", config.linkname, tokens[2], linkdat->flags);
			send_to_links_butone(sock->id, lbuf);
		} else {
			free(temp);
			sockwrite(sock, "ERROR Sent your info twice?");
		}
	}


}

void quit_users(char *botname)
{
	struct rMember *i;
	struct rMember *o = rusers;
	while(o) {
		i = o;
		o = o->next;
		if(strmi(botname, i->bot)) {
			sprintf(lbuf, "[\002quit\002] (%s) %s has left (Reason: Bot unlinked)", i->bot, i->nick);
			sRelay(lbuf);
			free(i->host);
			if(i->prev) {
				i->prev->next = i->next;
			} else {
				rusers = i->next;
			}
			if(i->next) {
				i->next->prev = i->prev;
			}
			free(i);
		}
	}

}

void unlink_bot(struct rBot *bot)
{
	struct rBot *del;
	while(bot) {
		sprintf(lbuf, "[\002link\002] Unlinking bot: %s", bot->name);
		sRelay(lbuf);
		quit_users(bot->name);
		unlink_bot(bot->bots);
		del = bot;
		bot = bot->next;
		free(del);
	}
	bot = NULL;
}

struct rBot *fetch_bot_from_all(char *name)
{
	struct linkline *i;
	struct rBot *ret;
	i = links;
	while(i) {
		ret = fetch_bot_from_line(name, i->bots);
		if(ret)
			return ret;
		i = i->next;
	}
	return NULL;
}

int find_unlink_bot(struct rBot **start, char *name)
{
	struct rBot *r;
	struct rBot *i = r = *start;
	while(i) {
		if(strmi(i->name, name)) {
			if(r == *start) {
				*start = i->next;
			} else {
				r->next = i->next;
			}
			unlink_bot(i);
			return 1;
		}
		if(find_unlink_bot(&i->bots, name)) {
			return 1;
		}
		r = i;
		i = i->next;
	}
	return 0;
}

struct rBot *fetch_bot_from_line(char *name, struct rBot *linkline)
{
	struct rBot *r;
	struct rBot *i = linkline;
	while(i) {
		if(strmi(i->name, name))
			return i;
		r = fetch_bot_from_line(name, i->bots);
		if(r)
			return r;
		i = i->next;
	}
	return NULL;
}

void burst_bots_on_link(struct socket *sock, char *host, struct rBot *bots)
{
	struct rBot *i = bots;
	while(i) {
		if(host) {
			sprintf(lbuf, "LINK %s %s %s", host, i->name, i->flags);
			sockwrite(sock, lbuf);
			sprintf(lbuf, "SET %s NETWORK %s", i->name, i->set_network);
			sockwrite(sock, lbuf);
			sprintf(lbuf, "SET %s NICK %s", i->name, i->set_nick);
			sockwrite(sock, lbuf);
		}
		burst_bots_on_link(sock, i->name, i->bots);
		i = i->next;
	}
	return;
}

void firelink_burst(struct socket *sock, struct linkline *linkdat)
{
	struct linkline *i;
	struct rMember *ri;
	int iter;

	sockwrite(sock, "ENDBURST");

	sprintf(lbuf, "LINK \001 %s N/A", config.linkname);
	sockwrite(sock, lbuf);

	sprintf(lbuf, "SET %s NETWORK %s", config.linkname, myNetwork);
	sockwrite(sock, lbuf);

	sprintf(lbuf, "SET %s NICK %s", config.linkname, cNick);
	sockwrite(sock, lbuf);

	i = links;
	while(i) {
		if(i != linkdat) {
			burst_bots_on_link(sock, config.linkname, i->bots);
		}
		i = i->next;
	}

	for(iter = 0; iter < get_next_dcc_id(); iter++) {
		if(dcclist[iter].adm) 
			sprintf(lbuf, "USER %d %s %s %s", dcclist[iter].id, dcclist[iter].adm->nick, dcclist[iter].host, config.linkname);
		else if(dcclist[iter].user)
			sprintf(lbuf, "USER %d %s %s %s", dcclist[iter].id, dcclist[iter].user, dcclist[iter].host, config.linkname);	
		else
			continue;
		sockwrite(sock, lbuf);
	}

	ri = rusers;
	while(ri) {
		sprintf(lbuf, "USER %d %s %s %s", ri->id, ri->nick, ri->host, ri->bot);
		sockwrite(sock, lbuf);
		ri = ri->next;
	}	

}

void send_to_all_links(char *buffer)
{
	struct socket *i = sockets;
	struct linkline *l;
	while(i) {
		l = fetch_link_id(i->id);
		if(i->type == 'f' && l && l->burst > 0) {
			sockwrite(i, buffer);
		}
		i = i->next;
	}
}

void send_to_links_butone(int id, char *buffer)
{
	struct socket *i = sockets;
	struct linkline *l;
	while(i) {
		l = fetch_link_id(i->id);
		if(i->type == 'f' && l && l->burst > 0 && l->id != id) {
			sockwrite(i, buffer);
		}
		i = i->next;
	}
}

void send_newpartyliner(int dccid, char *nick)
{
	if(config.dcc_isolate == 1)
		return;

	sprintf(lbuf, "USER %d %s %s %s", dcclist[dccid].id, nick, dcclist[dccid].host, config.linkname);
	send_to_all_links(lbuf);
}

void send_diepartyliner(int dccid, char *nick, char *reason)
{
	if(config.dcc_isolate == 1)
		return;

	sprintf(lbuf, "QUIT %d %s %s", dcclist[dccid].id, config.linkname, reason);
	send_to_all_links(lbuf);
}

void send_partylinemsg(char *nick, char *message)
{
	if(config.dcc_isolate == 1)
		return;

	sprintf(lbuf, "MSG %s %s %s", config.linkname, nick, message);
	send_to_all_links(lbuf);
}

void send_partylineaction(char *nick, char *message)
{
	if(config.dcc_isolate == 1)
		return;

	sprintf(lbuf, "ACT %s %s %s", config.linkname, nick, message);
	send_to_all_links(lbuf);
}

void start_links()
{
	struct linkline *i;
	lbuf[512] = 0;
	i = links;
	while(i) {
		if(i->port > 0 && i->burst == -1) {
			if(add_socket(i->type, i->id, i->ip, i->port, config.vhost, 0)) {
				snprintf(lbuf, 512, "[\002link\002] Starting link with %s@%s:%d", i->name, i->ip, i->port);
				sRelay(lbuf);
			}
		}
		i = i->next;
	}
	return;
}

/*
 * fetch link pointer via id (matches socket ids)
 */
struct linkline *fetch_link_id(int id)
{
	struct linkline *i;
	i = links;
	while(i) {
		if(id == i->id)
			return i;
		i = i->next;
	}
	return NULL;	
}

/*
 * fetch link pointer via name 
 */
struct linkline *fetch_link(char *name)
{
	struct linkline *i;
	i = links;
	while(i) {
		if(strmi(i->name, name))
			return i;
		i = i->next;
	}
	return NULL;
}

/*
 * "name password ip[:port] flags"
 * type = 'f' or 'e' (fireclaw/eggdrop)
 */
int add_link(char *confstring, char type)
{
	int portn;
	char *name, *port, *pass, *ip, *flags;
	struct linkline *temp, *i;

	name = confstring;
	pass = strchr(name, ' ');
	if(!pass)
		return 0;
	*pass++ = '\0';

	if(fetch_link(name))
		return 0;

	ip = strchr(pass, ' ');
	if(!ip)
		return 0;
	*ip++ = '\0';

	port = strchr(ip, ':');
	
	if(port) {
		*port++ = '\0';
	} else {
		port = ip;
	}

	flags = strchr(port, ' ');
	if(!flags)
		return 0;
	*flags++ = '\0';

	if(port > ip)
		portn = atoi(port);
	else
		portn = 0;

	/* phew! */

	fprintf(out, "DEBUG: /%s/%s/%s/%s/%d/\n", name, pass, ip, flags, portn);

	temp = (struct linkline *)myalloc(sizeof(struct linkline));

	temp->id = linkcount++;
	temp->type = type;
	temp->port = portn;
	temp->burst = -1;
	temp->bots = NULL;
	temp->next = NULL;

	temp->name = (char*)myalloc(strlen(name)+1);
	temp->password = (char*)myalloc(strlen(pass)+1);
	temp->ip = (char*)myalloc(strlen(ip)+1);
	temp->flags = (char*)myalloc(strlen(flags)+1);
	strcpy(temp->name, name);
	strcpy(temp->password, pass);
	strcpy(temp->ip, ip);
	strcpy(temp->flags, flags);

	if(links == NULL)
		links = temp;
	else {
		i = links;
		while(i->next)
			i = i->next;
		i->next = temp;
	}

	return 1;
}

void kill_linksocks()
{
	struct socket *i = sockets;
	while(i) {
		if(i->type == 'f')
			i->status = SOCK_BROKEN;
		i = i->next;
	}
	fixBrokenSocks();
}

/*
 * zap all linklines (for config rehash)
 */
void reset_linklines()
{
	struct linkline *i, *last;
	kill_linksocks();
	i = links;
	while(i)
	{
		last = i;
		i = i->next;
		free(last->name);
		free(last->password);
		free(last->ip);
		free(last->flags);
		free(last);
	}
	links = NULL;
	linkcount = 5000;
	return;
}

void tree_recur(int islast, int level, int indent, struct rBot *bot, struct socket *sock)
{
	char minibuf[128];
	char *pch;
	int i;
	while(bot) {
		pch = lbuf;
		for(i = 0; i < level; i++) {
			if(i < (islast ? indent-1 : indent))
				*pch++ = ' ';
			else
				*pch++ = '|';
			*pch++ = ' ';
		}
		if(islast) {
			*pch++ = '|';
		} else {
			*pch++ = '`';
		}
		*pch++ = '-';
		strcpy(pch, bot->name);
		sprintf(minibuf, " (%d users, network: %s, flags: %s, realnick: %s)", count_users(bot->name), bot->set_network, bot->flags, bot->set_nick);
		strcat(pch, minibuf);		
		sockwrite(sock, lbuf);
		tree_recur(bot->next ? 1 : 0, bot->next ? indent : indent + 1, level + 1, bot->bots, sock);
		bot = bot->next;
	}
}

void send_tree(struct socket *sock)
{
	struct linkline *l;

	sprintf(lbuf, config.linkname);
	sockwrite(sock, lbuf);

	l = links;
	while(l) {
		tree_recur((l->next && l->next->bots) ? 1 : 0, 0, 0, l->bots, sock);
		l = l->next;
	}
}

int check_dependants(struct schan *theChan)
{
	struct linkline *l;

	l = links;
	while(l) {
		if(strchr(l->flags, 'd') && !strcmp(l->bots->set_network, myNetwork) && isOp(theChan, l->bots->set_nick))
			return 1;
		l = l->next;
	}
	return 0;
}

void send_listlinks(struct socket *sock)
{
	struct linkline *l;

	if(!links) {
		sockwrite(sock, "I have no links configured.");
		return;
	}

	l = links;
	while(l) {
		sprintf(lbuf, "Link: %s, target: %s:%d. Flags: %s - Connected? %s",
			l->name, l->ip, l->port, l->flags ,l->bots ? "Yes" : "No");
		sockwrite(sock, lbuf);
		l = l->next;
	}
}

void disconnect_all_links(char *reason)
{
	struct socket *i = sockets;
	while(i) {
		if(i->type == 'f') {
			sockwrite(i, reason);
			i->status = SOCK_BROKEN;
		}
		i = i->next;
	}
	fixBrokenSocks();
}

int firelink_wildconnect(char *mask)
{
	struct linkline *i;
	int ret = 0;

	i = links;
	while(i) {
		if(i->burst == -1 && wildicmp(mask, i->name) && i->port > 0) {
			if(add_socket(i->type, i->id, i->ip, i->port, config.vhost, 0)) {
				snprintf(lbuf, 512, "[\002link\002] Starting link with %s@%s:%d", i->name, i->ip, i->port);
				sRelay(lbuf);
				ret++;
			}
		}
		i = i->next;
	}

	return ret;
}

int count_users(char *botname)
{
	struct rMember *ri;
	int ret = 0;
	ri = rusers;
	while(ri) {
		if(strmi(ri->bot, botname))
			ret++;
		ri = ri->next;
	}	
	return ret;
}
