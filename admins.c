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
// $Id: admins.c,v 1.11 2003/06/23 00:27:05 snazbaz Exp $
//

#include <ctype.h>
#include <signal.h>
#include <unistd.h>
#include <crypt.h>

#include "main.h"

int load_admins(char *filename)
{
	FILE *fp;
	char filebuf[512];
	char *user, *pass, *level;

	if(admins.next)
		return 0;

	fp = fopen(filename, "rt");

	if(fp == NULL)
	{
		printf("Oops!: Cannot open %s\n", filename);
		printf("You need to create an initial chans.txt before running Fireclaw for the\n");
		printf("first time. Please use ./makeuser to create the necessary files.\n\n");
		exit(0);
	}

	while(!feof(fp))
	{
		fgets(filebuf, 500, fp);
		user = filebuf;
		if(!(pass = strchr(filebuf, ' ')))
			break;
		*pass++ = 0;
		if(!(level = strchr(pass, ' ')))
			break;
		*level++ = 0;
		level[strlen(level)-1] = 0;
		adm_add(user, pass, atoi(level));
	}

	fclose(fp);

	return 1;
}

int save_admins(char *filename)
{
	FILE *fp;
	struct sadmin *i;

	fp = fopen(filename, "wt");
	if(fp == NULL)
		return 0;
	
	i = admins.next;
	while(i != NULL)
	{
		fprintf(fp, "%s %s %d\n", i->nick, i->pass, i->level);
		i = i->next;
	}

	fclose(fp);

	return 1;
}

int adm_add(char *user, char *pass, int level)
{
	struct sadmin *temp, *i;

	if(findAdmin(user))
		return 0;

	if(strlen(user) > 128)
		return 0;

	temp = (struct sadmin *)myalloc(sizeof(struct sadmin));

	temp->next = NULL;
	temp->actualNick[0] = 0;
	mystrncpy(temp->nick, user, NICKSIZE);
	mystrncpy(temp->pass, pass, PASSLEN+1);
	temp->level = level;

	i = &admins;

	while(i->next != NULL)
		i = i->next;

	i->next = temp;
	temp->prev = i;

	return 1;
}

void logout_all_admins()
{
	struct sadmin *i;

	i = admins.next;
	while(i)
	{
		i->actualNick[0] = 0;
		i = i->next;		
	}
	return;
}

void clear_admins()
{
	struct sadmin *i, *fr;

	i = &admins;

	while(i->next)
	{
		fr = i->next;
		adm_del(fr->nick);
		i = fr;		
	}
	return;
}

int count_admins()
{
	struct sadmin *i;
	int count = 0;

	i = &admins;
	while(i->next)
	{
		count++;
		i = i->next;
	}
	return count;
}

int adm_del(char *admin)
{
	struct sadmin *d = findAdmin(admin);

	if(!d)
		return 0;

	admin_deleted(d);

	if(d->next)
		d->next->prev = d->prev;
	if(d->prev)
		d->prev->next = d->next;

	free(d);

	return 1;
}

struct sadmin *findActualAdmin(char *name)
{
	struct sadmin *i;
	i = admins.next;
	while(i != NULL)
	{
		if(!strcmp(i->actualNick, name))
			return i;
		i = i->next;
	}
	return NULL;
}

int passCheck(char *supplied, char *hash)
{
	char *hash2 = crypt(supplied, hash);
	return !strncmp(hash2, hash, strlen(hash));
}

char *passCrypt(char *in)
{
	unsigned long seed[2];
	char salt[] = "$1$........";
	const char *seedchars = "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
	int i;
	/* Generate a (not very) random seed.  */
	seed[0] = time(NULL);
	seed[1] = getpid() ^ (seed[0] >> 14 & 0x30000);
	
	/* Turn it into printable characters from `seedchars'. */
	for (i = 0; i < 8; i++)
		salt[3+i] = seedchars[(seed[i/5] >> (i%5)*6) & 0x3f];
	
	return crypt(in, salt);
}

struct sadmin *findAdmin(char *nick)
{
	struct sadmin *i;
	i = admins.next;
	while(i != NULL)
	{
		if(strmi(nick, i->nick))
			return i;
		i = i->next;
	}
	return NULL;
}

void giveAdminStatus(char *nick, int *level)
{
	char modeBuf[256];
	struct chanRec *i = chans.next;

	while(i)
	{
		if(i->physical && strchr(i->flags, 'A'))
		{
			if(*level >= 100)
				sprintf(modeBuf, "MODE %s +o %s", i->name, nick);
			else
				sprintf(modeBuf, "MODE %s +v %s", i->name, nick);
			lowMsg(modeBuf);
		}		
		i = i->next;
	}
	return;
}

struct sadmin *checkLogin(char *nick, char *user, char *pass, struct socket *dcc)
{
	struct sadmin *i;

	user[NICKSIZE] = 0;
	pass[PASSLEN+1] = 0;
//	hash = passCrypt(pass);

	i = findAdmin(user);
	if(i)
	{
		if(passCheck(pass, i->pass))
		{
			if(!dcc) {
				mystrncpy(i->actualNick, nick, NICKSIZE);
				giveAdminStatus(nick, &i->level);
			}
			return i;
		}
	}
	return NULL;
}


struct sadmin *automagic_login(char *account, char *nick)
{
	char notify[513];
	struct sadmin *theAdmin;

	theAdmin = findAdmin(account);
	if(theAdmin && ((theAdmin->actualNick)[0] == 0))
	{
		mystrncpy(theAdmin->actualNick, nick, NICKSIZE);
		sprintf(notify, "NOTICE %s :You have been automagically logged in via service account '%s'. Access level: %d",
			nick, account, theAdmin->level);
		lowMsg(notify);
		sprintf(notify, "%s has been automagically logged in via server account '%s'. Level: %d.",
			nick, account, theAdmin->level);
		sRelay(notify);
		giveAdminStatus(nick, &theAdmin->level);
		return theAdmin;
	}
	return NULL;
}

void checkAllAdmins()
{
	struct sadmin *i;

	i = admins.next;
	while(i)
	{
		if(i->actualNick[0])
			checkCommonAdmin(i->actualNick);
		i = i->next;
	}

	return;
}

void checkCommonAdmin(char *nick)
{
	char sendR[400];
	struct sadmin *theAdmin;
	struct schan *c;
	struct suser *u;

	theAdmin = findActualAdmin(nick);
	if(!theAdmin)
		return;

	c = ial.channel;
	while(c != NULL)
	{
		u = c->user;
		while(u != NULL)
		{
			if(!strcmp(nick, u->nick))
				return;
			u = u->next;
		}
		c = c->next;
	}

	sprintf(sendR, "%s (admin) is no longer in any common channels with me.", nick);
	sRelay(sendR);

	theAdmin->actualNick[0] = 0;

	return;
}

int chPass(struct sadmin *theAdmin, char *newpass)
{
	char *hashed;
	hashed = passCrypt(newpass);
	mystrncpy(theAdmin->pass, hashed, PASSLEN+1);
	return save_admins(DEFAULT_ADMIN);
}
