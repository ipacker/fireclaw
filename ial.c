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
// $Id: ial.c,v 1.17 2003/06/22 23:19:02 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <malloc.h>

#include "main.h"

int userExists(char *nick)
{
	struct schan *c;
	c = ial.channel;
	while(c != NULL)
	{
		if(findUser(c, nick))
			return 1;
		c = c->next;
	}
	return 0;
}

struct schan *findChan(char *name)
{
	struct schan *i;
	i = ial.channel;
	while(i != NULL)
	{
		if(strcmp(name, i->name) == 0)
			return i;
		i = i->next;
	}
	return NULL;
}

struct schan *findiChan(char *name)
{
	struct schan *i;
	i = ial.channel;
	while(i != NULL)
	{
		if(strmi(name, i->name))
			return i;
		i = i->next;
	}
	return NULL;
}

void clear_ial()
{
	struct schan *c, *n;
	c = ial.channel;
	while(c != NULL)
	{
		n = c;
		c = c->next;
		cl_del(n->name);
	}
	ial.channel = NULL;
}

int count_criminals(struct schan *c)
{
	struct criminalRecord *i;
	int count = 0;

	i = c->nortyusers;
	while(i != NULL)
	{
		count++;
		i = i->next;
	}
	return count;
}

void clear_nortyusers(struct schan *c)
{
	struct criminalRecord *fr, *i;

	i = c->nortyusers;
	while(i != NULL)
	{
		fr = i;
		i = i->next;
		nu_del(c, fr);
	}
}

void clear_users(struct schan *c)
{
	struct suser *fr, *i;

	i = c->user;
	while(i != NULL)
	{
		fr = i;
		i = i->next;
		u_del(c, fr);
	}
}

void clear_bans(struct schan *c)
{
	struct sban *fr, *i;

	i = c->bans;
	while(i != NULL)
	{
		fr = i;
		i = i->next;
		bl_del(c, fr);
	}
}

void cl_add(char *name)
{
	struct schan *temp, *i;
	int size;

	if(findChan(name))
	{
		sRelay("Debug: SYNC Error trying to add channel twice");
		return;
	}

	temp = (struct schan *)myalloc(sizeof(struct schan));
	size = strlen(name) + 1;

	temp->userSize = 0;
	temp->flood_last = 0;
	temp->flood_lines = 0;
	temp->banSize = 0;
	temp->limit = 0;
	temp->tsLim = 0;
	temp->next = NULL;
	temp->user = NULL;
	temp->nortyusers = NULL;
	temp->bans = NULL;
	temp->record = fetchChanRec(name);
	temp->name = (char *)myalloc(size);
	memcpy(temp->name, name, size);
	memset(temp->topic, 0, TOPICLEN);
	memset(temp->modes, 0, MODELEN);
	memset(temp->key, 0, KEYLEN);

	i = ial.channel;
	
	if(i == NULL)
	{
		ial.channel = temp;
		temp->prev = NULL;
	}
	else
	{
		while(i->next != NULL)
			i = i->next;
		i->next = temp;
		temp->prev = i;
	}

	if(temp->record)
		temp->record->physical = temp;

	return;
}

void cl_del(char *name)
{
	struct schan *theChan = findChan(name);

	if(!theChan)
	{
		sRelay("Debug: SYNC Error trying to delete channel that doesn't exist.");
		return;
	}

	if(theChan->record)
		theChan->record->physical = NULL;

	checkLastPointer(theChan);

	free(theChan->name);
	
	/* kill users + bans of this channel */
	clear_users(theChan);
	clear_bans(theChan);
	clear_nortyusers(theChan);

	/* finally, unlink from list */
	if(theChan->prev == NULL)
		ial.channel = theChan->next;
	else
		theChan->prev->next = theChan->next;

	if(theChan->next != NULL)
		theChan->next->prev = theChan->prev;


	/* and boom! */
	free(theChan);

	return;
}

struct suser *findiUser(struct schan *channel, char *nick)
{
	struct suser *i;
	i = channel->user;
	while(i != NULL)
	{
		if(strmi(nick, i->nick))
			return i;
		i = i->next;
	}
	return NULL;
}

struct suser *findUser(struct schan *channel, char *nick)
{
	struct suser *i;
	i = channel->user;
	while(i != NULL)
	{
		if(!strcmp(nick, i->nick))
			return i;
		i = i->next;
	}
	return NULL;
}

struct criminalRecord *findCriminal(struct schan *channel, char *nick)
{
	struct criminalRecord *i;
	i = channel->nortyusers;
	while(i != NULL)
	{
		if(!strcmp(nick, i->nick))
			return i;
		i = i->next;
	}
	return NULL;
}

struct suser *u_add(struct schan *channel, char *nick, char *ident, char *host)
{
	struct suser *temp, *i;
	int si, sh, sm;

	if(findUser(channel, nick))
		return NULL;

	si = strlen(ident) + 1;
	sh = strlen(host) + 1;
	sm = strlen(nick) + strlen(ident) + strlen(host) + 3;

	temp = (struct suser *)myalloc(sizeof(struct suser));

	temp->isVoice = 0;
	temp->isOp = 0;
	temp->flines = 0;
	temp->nickchanges = 0;
	temp->joinTime = (int)time(0);
	temp->idleMsg = 0;
	temp->lastMsg = 0;
	temp->lastNick = 0;
	temp->next = NULL;
	temp->realname = NULL;
	temp->ident = (char *)myalloc(si);
	temp->host = (char *)myalloc(sh);
	temp->mask = (char*)myalloc(sm);
	memcpy(temp->ident, ident, si);
	memcpy(temp->host, host, sh);
	mystrncpy(temp->nick, nick, NICKSIZE);
	sprintf(temp->mask, "%s!%s@%s", nick, ident, host);

	i = channel->user;

	if(i == NULL)
	{
		channel->user = temp;
		temp->prev = NULL;
	} else
	{
		while(i->next != NULL)
			i = i->next;
		i->next = temp;
		temp->prev = i;
	}

	channel->userSize++;

	return temp;
}

void u_del(struct schan *theChan, struct suser *theUser)
{

	if(theUser->realname)
		free(theUser->realname);
	free(theUser->ident);
	free(theUser->host);
	free(theUser->mask);

	if(!theUser->prev)
		theChan->user = theUser->next;
	else
		theUser->prev->next = theUser->next;

	if(theUser->next)
		theUser->next->prev = theUser->prev;

	free(theUser);

	theChan->userSize--;

	return;
}

void addMode(struct schan *theChan, char mode)
{
	char *pch = theChan->modes;
	if(!strchr(pch, mode))
	{
		for(; (*pch); pch++);
		*(pch++) = mode;
		*pch = '\0';
	}	
	return;
}

void removeMode(struct schan *theChan, char mode)
{
	char *pch = strchr(theChan->modes, mode);
	if(pch)
		for(; (*pch); pch++)
			*pch = *(pch + 1);
	return;
}

struct sban *findBan(struct schan *channel, char *mask)
{
	struct sban *i;
	i = channel->bans;
	while(i != NULL)
	{
		if(!strcmp(mask, i->mask))
			return i;
		i = i->next;
	}
	return NULL;
}

struct criminalRecord *nu_add(struct schan *channel, char *nick, char *ident, char *host)
{
	struct criminalRecord *temp, *i;
	int sih;

	if(findCriminal(channel, nick))
		return NULL;

	sih = strlen(ident) + strlen(host) + 2;

	temp = (struct criminalRecord *)myalloc(sizeof(struct criminalRecord));

	temp->shortTerm = temp->longTerm = 0;
	temp->lastJoin = 0;
	temp->njoins = 0;
	temp->laststamp = time(NULL);
	temp->identhost = (char *)myalloc(sih);
	sprintf(temp->identhost, "%s@%s", ident, host);
	mystrncpy(temp->nick, nick, NICKSIZE);
	temp->next = NULL;
	for(sih = 0; sih < REPEAT_HISTORY_SIZE; sih++)
		temp->trackrepeat[sih].id = 0;
	
	i = channel->nortyusers;

	if(i == NULL)
	{
		channel->nortyusers = temp;
		temp->prev = NULL;
	} else
	{
		while(i->next != NULL)
			i = i->next;
		i->next = temp;
		temp->prev = i;
	}

	return temp;
}

void nu_del(struct schan *theChan, struct criminalRecord *criminal)
{
	free(criminal->identhost);

	if(!criminal->prev)
		theChan->nortyusers = criminal->next;
	else
		criminal->prev->next = criminal->next;

	if(criminal->next)
		criminal->next->prev = criminal->prev;

	free(criminal);

	return;
}

struct sban *bl_add(struct schan *channel, char *mask, char *setby, int ts)
{
	struct sban *temp, *i;
	int sm, ss;

	if(findBan(channel, mask))
		return NULL;

	sm = strlen(mask) + 1;
	ss = strlen(setby) + 1;

	temp = (struct sban *)myalloc(sizeof(struct sban));

	temp->ts = ts;
	temp->mask = (char *)myalloc(sm);
	temp->setby = (char *)myalloc(ss);
	temp->next = NULL;
	memcpy(temp->mask, mask, sm);
	memcpy(temp->setby, setby, ss);

	i = channel->bans;

	if(i == NULL)
	{
		channel->bans = temp;
		temp->prev = NULL;
	} else
	{
		while(i->next != NULL)
			i = i->next;
		i->next = temp;
		temp->prev = i;
	}

	channel->banSize++;

	return temp;
}

void bl_del(struct schan *theChan, struct sban *theBan)
{
	free(theBan->mask);
	free(theBan->setby);

	if(!theBan->prev)
		theChan->bans = theBan->next;
	else
		theBan->prev->next = theBan->next;

	if(theBan->next)
		theBan->next->prev = theBan->prev;

	free(theBan);

	theChan->banSize--;

	return;
}
