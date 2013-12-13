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
// $Id: chans.c,v 1.9 2003/06/23 00:27:05 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "main.h"

/***
 * Chanflags:
 * j = join
 * e = enforcebans
 * l = autolimit
 * o = kickops
 * a = autologin
 */

/*struct chanRecord {
	char *name;
	char flags[FLAGLEN+1];
	char key[KEYLEN+1];
	char goodModes[MODELEN+1];
	char badModes[MODELEN+1];
	struct *schan physical;
};*/

char validFlags(char *str, char *valid)
{
	for(; *str; str++)
		if(!strchr(valid, *str))
			return *str;
	return 0;
}

void load_chans(char *filename)
{
	FILE *fp;
	char filebuf[1024];
	char *name, *flags, *key, *gModes, *bModes;
	char *banmask, *reason, *setby, *expires, *type;

	if(chans.next)
		return;

	fp = fopen(filename, "r");

	if(fp == NULL)
	{
		printf("Oops!: Cannot open %s\n", filename);
		printf("You need to create an initial chans.txt before running Fireclaw for the\n");
		printf("first time. Please use ./makeuser to create the necessary files.\n\n");
		exit(0);
	}

	while(!feof(fp))
	{
		fgets(filebuf, 1000, fp);
		name = filebuf;
		if(!(flags = strchr(filebuf, ' ')))
			break;
		*flags++ = 0;
		if(!(key = strchr(flags, ' ')))
			break;
		*key++ = 0;
		if(!(gModes = strchr(key, ' ')))
			break;
		*gModes++ = 0;
		if(!(bModes = strchr(gModes, ' ')))
			break;
		*bModes++ = 0;
		bModes[strlen(bModes)-1] = 0;
//		printf("Adding: /%s/%s/%s/%s/%s/\n", name, flags, key, gModes, bModes);
		cr_add(name, flags, key, gModes, bModes);
	}

	fclose(fp);

	fp = fopen(DEFAULT_BANS, "r");

	if(fp == NULL)
	{
		printf("Cannot open %s\n", DEFAULT_BANS);
		return;
	}

	while(!feof(fp))
	{
		fgets(filebuf, 1000, fp);
		name = filebuf;
		if(!(banmask = strchr(name, ' ')))
			break;
		*banmask++ = 0;
		if(!(setby = strchr(banmask, ' ')))
			break;
		*setby++ = 0;
		if(!(type = strchr(setby, ' ')))
			break;
		*type++ = 0;
		if(!(expires = strchr(type, ' ')))
			break;
		*expires++ = 0;
		if(!(reason = strchr(expires, ' ')))
			break;
		*reason++ = 0;
		reason[strlen(reason)-1] = 0;
//		printf("Adding: /%s/%s/%s/%s/%s/%s/\n", name, banmask, setby, type, expires, reason);
		br_add(name, banmask, reason, type[0], setby, atoi(expires));
	}

	fclose(fp);

	return;
}

int save_chans(char *filename)
{
	FILE *fp, *bp;
	struct chanRec *i;
	struct softBan *b;

	fp = fopen(filename, "w");
	if(fp == NULL)
		return 0;

	bp = fopen(DEFAULT_BANS, "w");
	if(bp == NULL)
		{ fclose(fp);	return 0; }
	
	i = chans.next;
	while(i != NULL)
	{
		fprintf(fp, "%s %s %s %s %s\n", i->name, i->flags, i->key, i->goodModes, i->badModes);
		b = i->softbans;
		while(b)
		{
			fprintf(bp, "%s %s %s %c %d %s\n", i->name, b->banmask, b->setby, b->type, b->expires, b->reason);
			b = b->next;
		}
		i = i->next;
	}

	fclose(fp);
	fclose(bp);

	return 1;
}

int isChanWithJ()
{
	struct chanRec *i;
	i = chans.next;
	while(i != NULL)
	{
		if(i->physical && strchr(i->flags, 'd'))
			return 1;
		i = i->next;
	}
	return 0;
}

struct chanRec *fetchChanRec(char *name)
{
	struct chanRec *i;
	i = chans.next;
	while(i != NULL)
	{
		if(strmi(name, i->name))
			return i;
		i = i->next;
	}
	return NULL;
}

int cr_add(char *name, char *flags, char *key, char *gModes, char *bModes)
{
	struct chanRec *temp, *i;

	if(fetchChanRec(name))
		return 0;

	if(strlen(name) > 64)
		return 0;

	temp = (struct chanRec *)myalloc(sizeof(struct chanRec));

	if(!temp)
		return 0;

	temp->next = NULL;
	temp->physical = NULL;
	temp->softbans = NULL;
	temp->name = (char *)myalloc(strlen(name)+1);
	strcpy(temp->name, name);
	mystrncpy(temp->flags, flags, FLAGLEN+1);
	mystrncpy(temp->key, key, KEYLEN+1);
	mystrncpy(temp->goodModes, gModes, MODELEN+1);
	mystrncpy(temp->badModes, bModes, MODELEN+1);

	i = &chans;

	while(i->next != NULL)
		i = i->next;

	i->next = temp;
	temp->prev = i;

	return 1;
}

int br_add(char *channel, char *banmask, char *reason, char type, char *setby, int expires)
{
	struct chanRec *theChan;
	struct softBan *temp, *i;

	if(!(theChan = fetchChanRec(channel)))
		return 0;

	if(fetchSoftBan(theChan, banmask))
		return 0;

	if(strlen(banmask) > 256)
		return 0;

	temp = (struct softBan *)myalloc(sizeof(struct softBan));

	if(!temp)
		return 0;

	temp->next = NULL;
	temp->type = type;
	temp->expires = expires;
	temp->banmask = (char*)myalloc(strlen(banmask)+1);
	temp->reason = (char*)myalloc(strlen(reason)+1);
	temp->setby = (char*)myalloc(strlen(setby)+1);
	strcpy(temp->banmask, banmask);
	strcpy(temp->reason, reason);
	strcpy(temp->setby, setby);

	i = theChan->softbans;

	if(!i)	{
		theChan->softbans = temp;
		temp->prev = NULL;
	} else {
		while(i->next != NULL)
			i = i->next;
		i->next = temp;
		temp->prev = i;
	}

	return 1;
}

struct softBan *fetchSoftBan(struct chanRec *theChan, char *banmask)
{
	struct softBan *i;

	i = theChan->softbans;
	while(i)
	{
		if(strmi(banmask, i->banmask))
			return i;
		i = i->next;
	}
	return NULL;
}

int br_del(struct chanRec *theChan, char *banmask)
{
	struct softBan *i = fetchSoftBan(theChan, banmask);

	if(!i)
		return 0;

	free(i->banmask); free(i->reason); free(i->setby);

	if(i->prev)
		i->prev->next = i->next;
	else
		theChan->softbans = i->next;

	if(i->next)
		i->next->prev = i->prev;

	free(i);

	return 1;
}

int cr_del(char *name)
{
	struct chanRec *d = fetchChanRec(name);

	if(!d)
		return 0;

	clear_softbans(d);

	if(d->next)
		d->next->prev = d->prev;
	if(d->prev)
		d->prev->next = d->next;


	free(d->name);
	free(d);

	return 1;
}

void clear_softbans(struct chanRec *cr)
{
	struct softBan *i, *fr;
	i = cr->softbans;
	while(i)
	{
		fr = i;
		free(i->banmask);
		free(i->reason);
		free(i->setby);
		i = i->next;
		free(fr);
	}
	cr->softbans = NULL;
	return;
}

void clear_chans()
{
	struct chanRec *i, *fr;

	i = chans.next;

	while(i)
	{
		fr = i->next;
		cr_del(i->name);
		i = fr;
	}
	return;
}

int count_chans()
{
	struct chanRec *i;
	int count = 0;

	i = chans.next;
	while(i)
	{
		count++;
		i = i->next;
	}
	return count;
}

int count_softBansOn(struct chanRec *c)
{
	struct softBan *i;
	int count = 0;
	i = c->softbans;
	while(i)
	{
		count++;
		i = i->next;
	}
	return count;

}
