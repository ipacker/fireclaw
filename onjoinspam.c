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
// $Id: onjoinspam.c,v 1.3 2003/06/22 23:19:02 snazbaz Exp $
//

#include "main.h"

struct wlist {
	char *word;
	struct wlist *next;
};

struct vlist {
	char *host;
	struct vlist *next;
};

static char ojbuf[1024];
static int ojConnect = 0;
static int ojiscon = 0;
static struct socket *ojsock = 0;
static struct wlist *wordlist = 0;
static struct vlist *vhostlist = 0;
static int vcount = 0;
static int wcount = 0;
static char ojNick[NICKSIZE];

/*
 * doesn't do much, called in main.c - might be used for more later.
 */
void init_oj()
{
	ojConnect = time(0);
}

/*
 * completely removes all words and vhosts from their lists.
 */
void wipe_ojdata()
{
	struct wlist *iw, *delw;
	struct vlist *iv, *delv;

	iw = wordlist;
	while(iw) {
		delw = iw;
		iw = iw->next;
		free(delw->word);
		free(delw);
	}
	wordlist = 0;

	iv = vhostlist;
	while(iv) {
		delv = iv;
		iv = iv->next;
		free(delv->host);
		free(delv);
	}
	vhostlist = 0;
	vcount = wcount = 0;
}

/*
 * called from conf.c when reading config entries.
 */
int add_word(char *confstring)
{
	struct wlist *temp, *i;

	if(strlen(confstring) > 9)
		confstring[9] = 0;

	temp = (struct wlist*)myalloc(sizeof(struct wlist));

	temp->word = (char*)myalloc(strlen(confstring)+1);
	strcpy(temp->word, confstring);
	temp->next = 0;

	if(wordlist) {
		i = wordlist;
		while(i->next)
			i = i->next;
		i->next = temp;
	} else
		wordlist = temp;

	wcount++;

	return 1;
}

/*
 * called from conf.c when reading config entries.
 */
int add_vhost(char *confstring)
{
	struct vlist *temp, *i;
	if(strlen(confstring) > 15)
		confstring[15] = 0;

	temp = (struct vlist*)myalloc(sizeof(struct vlist));

	temp->host = (char*)myalloc(strlen(confstring)+1);
	strcpy(temp->host, confstring);
	temp->next = 0;

	if(vhostlist) {
		i = vhostlist;
		while(i->next)
			i = i->next;
		i->next = temp;
	} else
		vhostlist = temp;

	vcount++;

	return 1;
}

/*
 * gets a 'random' vhost from list specified in config. 
 * this isnt very random.
 */
char *get_random_vhost()
{
	static char vhost[16];
	struct vlist *i;
	int wantpos;
	vhost[0] = 0;
	if(vcount == 0)
		return vhost;

	wantpos = (time(0)+bper+count_main_in)% vcount;

	i = vhostlist;
	while(i->next && (wantpos-- > 0))
	{
		i = i->next;
	}

	mystrncpy(vhost, i->host, 16);

	return vhost;
}

/*
 * gets a 'random' word from list specified in config. 
 * this isnt very random. the argument x is because we need to get 3
 * random words at once, the vars the 'randomness' is based on wont
 * change during each get of the word..therefore we need to ofset them.
 */
char *get_random_word(int x)
{
	static char word[10];
	struct wlist *i;
	int wantpos;
	
	strcpy(word, "none");

	if(wcount == 0)
		return word;

	wantpos = (time(0)+x+bper+count_main_in) % wcount;

	i = wordlist;
	while(i->next && (wantpos-- > 0))
	{
		i = i->next;
	}

	if(i)
		mystrncpy(word, i->word, 10);

	return word;
}

/*
 * called after connect(), when conncetion is confirmed ready for writing.
 */
void oj_connect()
{
	ojiscon = 0;
	if(serv->password)
	{
		sprintf(ojbuf, "PASS %s", serv->password);
		sockwrite(ojsock, ojbuf);
	}
	strcpy(cNick, config.nick);
	snprintf(ojbuf, 510, "NICK %s", get_random_word(0));
	sockwrite(ojsock, ojbuf);
	snprintf(ojbuf, 450, "USER %s %d * :", get_random_word(1), INIT_USERMASK);
	strcat(ojbuf, get_random_word(2));
	sockwrite(ojsock, ojbuf);
}

/*
 * called when the socket is closed.
 */
void oj_dead()
{
	sRelay("On-join detector client disconnected.");
	ojsock = 0;
}

/*
 * called when we get 005 numeric and need to perform startup actions.
 */
void oj_burst()
{
	struct chanRec *i;
	i = chans.next;
	while(i != NULL)
	{
		if(i->physical && strchr(i->flags, 'd')) {
			sprintf(ojbuf, "JOIN %s %s", i->name, i->key);
			sockwrite(ojsock, ojbuf);
		}			
		i = i->next;
	}
}

/*
 * called from io_loop when we get a line on the oj socket
 */
void oj_line(char *message)
{
	char tokenline[513];
	char *tokens[MAXTOK];
	sUser theUser;
	int numtok;

	mystrncpy(tokenline, message, 513);
	memset(tokens, 0, sizeof(tokens) );
	numtok = tokenize(tokenline, " ", tokens, MAXTOK);

	if(numtok < 2)
		return;

	if(strcmp(tokens[0], "PING") == 0) {
		sprintf(ojbuf, "PONG %s", tokens[1]);
		sockwrite(ojsock, ojbuf);
		return;
	} else if(!strcmp(tokens[1], "005") && ojiscon == 0) {
		mystrncpy(ojNick, tokens[2], NICKSIZE);
		sprintf(ojbuf, "On-join detector connected with nick: %s", ojNick);
		sRelay(ojbuf);
		oj_burst();
		ojiscon = 1;
	} else if(
				(!strcmp(tokens[1], "PRIVMSG") || !strcmp(tokens[1], "NOTICE")) && 
				!strcmp(tokens[2], ojNick) &&
				(time(0) - ojConnect < config.oj_listenperiod) &&
				strchr(tokens[0], '!')
		) {
		struct chanRec *i;
		struct suser *fuser;
		make_user_struct(tokens[0], &theUser);
		sprintf(ojbuf, "[onjoin] <%s> %s", theUser.nick, message + (tokens[3]-tokens[0]) + 1);
		sRelay(ojbuf);
		i = chans.next;
		while(i) {
			if(i->physical && strchr(i->flags, 'd')) {
				fuser = findUser(i->physical, theUser.nick);
				if(fuser)
					punish_user(i, fuser, 'd', 0, 0);
			}
			i = i->next;
		}		
	}
	

}

/*
 * called once a minute, checks if we need to take any periodic action.
 * we will either quit the client or start to connect the client here.
 */
void checkNeedCon()
{
	if(ojsock && ojsock->status == SOCK_READY) {
		if(time(0) - ojConnect > 180) {
			sockwrite(ojsock, "QUIT");
			ojsock->status = SOCK_BROKEN;
			fixBrokenSocks();
		}
	} else {
		if(isChanWithJ() && (time(0) - ojConnect > config.oj_period)) {
			ojsock = add_socket('J', 2, serv->hostname, serv->port, get_random_vhost(), 0);
			ojConnect = time(0);
		}
	}
}
