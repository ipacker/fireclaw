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
// $Id: dcc.c,v 1.7 2003/06/25 23:37:41 snazbaz Exp $
//

#include <ctype.h>

#include "main.h"

static char dccbuf[4096];
static int dccpos = 0;
static int idnum = 0;
struct dcc dcclist[MAX_DCC];

int get_next_dcc_id() { return dccpos; }

/*
 * Used for .boot command and called from admin_msg.c
 */
void dcc_close_user(int did, char *msg)
{
	sockwrite(dcclist[did].con, msg);
	dcclist[did].con->status = SOCK_BROKEN;
	fixBrokenSocks();
}

/*
 * A guest has logged in and needs their access elevated,
 * this does the dirty work.
 */
void set_dcc_adm(int id, struct sadmin *adm)
{
	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return;
	dcclist[id].adm = adm;
	if(!dcclist[id].user) {
		dcclist[id].user = (char*)myalloc(strlen(adm->nick)+1);
		mystrncpy(dcclist[id].user, adm->nick, 32);
	} else {
		sprintf(dccbuf, "%s(guest)", dcclist[id].user);
		send_diepartyliner(id, dccbuf, "Authenticated.");
	}
	send_newpartyliner(id, dcclist[id].adm->nick);
}

/*
 * If an admin account gets deleted and they are in a dcc session,
 * the .adm pointer is broken. It's easier to just dump them out
 * of the session completly if this happens rather than fiddle around
 * changing them back to guest if config.allowguests is on etc.
 */
void admin_deleted(struct sadmin *admin)
{
	int i;
	for(i = 0; i < dccpos; i++) {
		if(dcclist[i].adm == admin) {
			sockwrite(dcclist[i].con, "Your admin account has been removed, dcc chat terminated.");
			dcclist[i].con->status = SOCK_BROKEN;
		}
	}
	fixBrokenSocks();
}

char *get_dcc_user(int id)
{
	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return "";
	if(dcclist[id].adm)
		return dcclist[id].adm->nick;
	return dcclist[id].user;
}

char *get_dcc_host(int id)
{
	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return "";
	return dcclist[id].host;
}

void dcc_closeall(char *msg)
{
	int i;
	for(i = 0; i < dccpos; i++) {
		sockwrite(dcclist[i].con, msg);
		dcclist[i].con->status = SOCK_BROKEN;
	}
	fixBrokenSocks();
}

void dcc_connected(int id)
{
	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return;
	if(!dcclist[id].user) {
		sprintf(dccbuf, "New user connected via telnet/reverse dcc. (%s)", dcclist[id].host);
		sRelay(dccbuf);
		sprintf(dccbuf, "You need to auth (.login <user> <pass>) to continue...");
		sockwrite(dcclist[id].con, dccbuf);
	} else if(dcclist[id].adm) {
		sprintf(dccbuf, "%s is making a dcc connection with us.", dcclist[id].adm->nick);
		sRelay_butone(dccbuf, dcclist[id].con);
		sprintf(dccbuf, "Welcome sweetypie, you are admin \"%s\" with level %d access.", dcclist[id].adm->nick, dcclist[id].adm->level);
		sockwrite(dcclist[id].con, dccbuf);
		send_newpartyliner(id, dcclist[id].adm->nick);
	} else {
		sprintf(dccbuf, "%s(guest) is making a dcc connection with us.", dcclist[id].user);
		sRelay_butone(dccbuf, dcclist[id].con);
		sprintf(dccbuf, "Welcome. You are a guest here, therefore you cannot execute any commands.");
		sockwrite(dcclist[id].con, dccbuf);
		sprintf(dccbuf, "%s(guest)", dcclist[id].user);
		send_newpartyliner(id, dccbuf);
	}

}

int send_to_dcc_butone(int did, char *line)
{
	int i, ret;
	for(i = ret = 0; i < dccpos; i++) {
		if(dcclist[i].user && (i != did) && dcclist[i].con && (dcclist[i].con->status == SOCK_READY)) {
			sockwrite(dcclist[i].con, line);
			ret++;
		}
	}
	return ret;
}

int send_to_dcc_butonesock(char *line, struct socket *sock, int for_guest)
{
	int i, ret;
	for(i = ret = 0; i < dccpos; i++) {
		if(dcclist[i].user && dcclist[i].con != sock && (dcclist[i].con->status == SOCK_READY) && (for_guest || dcclist[i].adm)) {
			sockwrite(dcclist[i].con, line);
			ret++;
		}
	}
	return ret;
}

int send_to_all_dcc(char *line, int for_guest)
{
	int i, ret;
	for(i = ret = 0; i < dccpos; i++) {
		if(dcclist[i].user && dcclist[i].con && (dcclist[i].con->status == SOCK_READY) && (for_guest || dcclist[i].adm)) {
			sockwrite(dcclist[i].con, line);
			ret++;
		}
	}
	return ret;
}

struct dcc *add_dcc_chat(char *nick, char *host, struct sadmin *adm, struct socket *con)
{
	if(!con)
		return 0;

	if(dccpos >= MAX_DCC)
		return 0;

	if(!nick) {	 /* telnet or reverse dcc */
		dcclist[dccpos].user = 0;
		dcclist[dccpos].adm = 0;
	} else {
		dcclist[dccpos].user = (char*)myalloc(strlen(nick)+1);
		mystrncpy(dcclist[dccpos].user, nick, 32);
		dcclist[dccpos].adm = adm;
	}

	dcclist[dccpos].con = con;
	dcclist[dccpos].id = idnum++;
	dcclist[dccpos].host = (char*)myalloc(strlen(host)+1);
	strcpy(dcclist[dccpos].host, host);

	fprintf(out, "New DCC: %s (%d)(%d)\n", nick ? nick : "Unknown", dccpos, dcclist[dccpos].con->id);

	return &dcclist[dccpos++];
}

void chat_terminated(int id)
{
	int i;

	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return;

	fprintf(out, "Removing DCC: %s %s (%d of %d)\n", dcclist[id].user, dcclist[id].host, id, dccpos);

	sprintf(dccbuf, "Connection lost to %s (%s)", dcclist[id].user ? dcclist[id].user : "Unauthed user", dcclist[id].host);
	sRelay(dccbuf);

	if(dcclist[id].adm)
		send_diepartyliner(id, dcclist[id].adm->nick, "Gone !");
	else if(dcclist[id].user)
		send_diepartyliner(id, dcclist[id].user, "Gone !");

	if(dcclist[id].user)
		free(dcclist[id].user);

	free(dcclist[id].host);
	
	for(i = id+1; i < dccpos; i++)
	{
		if(dcclist[i].con) {
			dcclist[i].con->id--;
		}
		dcclist[i-1] = dcclist[i];
	}

	dccpos--;
}

void parseDCC(int id, char *line)
{
	char tokenline[513];
	char *tokens[MAXTOK];
	int numtok;
	sUser theUser;

	id -= DCC_ID_OFSET;
	if(id >= dccpos)
		return;

	if(!*line)
		return;

	if(dcclist[id].user)
		mystrncpy(theUser.nick, dcclist[id].user, NICKSIZE);
	else
		strcpy(theUser.nick, "");
	
	mystrncpy(theUser.host, dcclist[id].host, HOSTSIZE);

	theUser.ident[0] = 0;
	theUser.mask[0] = 0;
	mystrncpy(tokenline, line, 513);
	memset(tokens, 0, sizeof(tokens) );
	numtok = tokenize(tokenline, " ", &tokens[3], MAXTOK-3) + 3;

	if(*line == '.') { /* command */
		if(dcclist[id].adm || !strcmp(tokens[3], ".login") || !strcmp(tokens[3], ".who"))
			admin_msg(&theUser, line + 1, tokens, &numtok, dcclist[id].con, dcclist[id].adm);
		else
			sockwrite(dcclist[id].con, "You are a guest, you may not execute commands here.");
	} else if(dcclist[id].user) { /* partyline baby! */
	
		if(!strcmp(tokens[3], "\001ACTION")) {
			line[strlen(line)-1] = 0;
			if(dcclist[id].adm) {
				send_partylineaction(dcclist[id].adm->nick, line + 8);
				sprintf(dccbuf, "* %s %s", dcclist[id].adm->nick, line + 8);
			} else {
				sprintf(dccbuf, "%s(guest)", dcclist[id].user);
				send_partylineaction(dccbuf, line + 8);
				sprintf(dccbuf, "* %s(guest) %s", dcclist[id].user, line + 8);
			}
		} else {
			if(dcclist[id].adm) {
				send_partylinemsg(dcclist[id].adm->nick, line);
				sprintf(dccbuf, "<%s> %s", dcclist[id].adm->nick, line);
			} else {
				sprintf(dccbuf, "%s(guest)", dcclist[id].user);
				send_partylinemsg(dccbuf, line);
				sprintf(dccbuf, "<%s(guest)> %s", dcclist[id].user, line);
			}
		}
		send_to_dcc_butone(id, dccbuf);
	}



}
