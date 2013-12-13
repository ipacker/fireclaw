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
// $Id: tasks.c,v 1.13 2003/07/16 15:35:02 snazbaz Exp $
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include "main.h"

static char taskbuf[BUFSIZE];

/* From chan_msg.c */
void set_lower_message(char *output, char *input);
extern regex_t swear_norm, swear_mild;
/*                 */

void massMode(struct schan *chan, char sign, char mode, int count, char *arguments)
{
	char *pch;
	int i;

	if(!chan || (count < 1))
		return;

	sprintf(taskbuf, "MODE %s %c", chan->name, sign);
	pch = taskbuf + strlen(taskbuf);
	
	for(i = 0; i < count; i++)
		*pch++ = mode;

	strcpy(pch, arguments);

	quickMsg(taskbuf);
	return;
}

void check_idle_ops(struct chanRec *theChan)
{
	struct suser *i;
	
	if(!theChan->physical)
		return;

	i = theChan->physical->user;
	while(i)
	{
		if( (i->isOp || (i->isVoice && config.idlevoice == 1)) && 
			(time(0) - i->idleMsg > config.idle_limit) && 
			strcmp(cNick, i->nick) ) 
		{
			sprintf(taskbuf, "WHOIS %s %s", i->nick, i->nick);
			lowMsg(taskbuf);
			return;
		}
		i = i->next;
	}
}

int count_clones(struct schan *theChan, char *host)
{
	struct suser *i;
	int count = 0;

	i = theChan->user;
	while(i)
	{
		if(!strcmp(host, i->host))
			count++;
		i = i->next;
	}
	return count;
}

int enforce_ban(struct schan *theChan, char *mask, char *reason, char *setby, int expire)
{
	struct suser *i;
	struct suser **kicklist;
	char *wild, lmask[513], *pch;
	int iter, count = 0;

	if(check_dependants(theChan))
		return 0;

	kicklist = (struct suser **)myalloc(config.maxkicks * sizeof(struct suser*));
	
	/* create lowercase version of the mask */
	wild = (char*)myalloc(strlen(mask)+1);
	for(iter = 0; iter <= strlen(mask); iter++)
		wild[iter] = tolower(mask[iter]);

	i = theChan->user;
	while(i)
	{
		if(!strcmp(i->nick, cNick))
		{
			if(!i->isOp)		/* checking if bot is opped. if not, might as well abort. */
			{
				free(wild);
				free(kicklist);
				return -1;
			}
		}
		/* generate lowercase mask to match the wildmask against */
		pch = lmask;
		for(iter = 0; iter < strlen(i->nick); iter++)
			*pch++ = tolower((i->nick)[iter]);
		*pch++ = '!';
		for(iter = 0; iter < strlen(i->ident); iter++)
			*pch++ = tolower((i->ident)[iter]);
		*pch++ = '@';
		for(iter = 0; iter < strlen(i->host); iter++)
			*pch++ = tolower((i->host)[iter]);
		*pch++ = '\0';
		if(wildcmp(wild, lmask))
		{
			if(!strcmp(i->nick, cNick))	/* if ban matches me */
			{
				free(wild);				/* lets unban the mask */
				sprintf(lmask, "MODE %s -b %s", theChan->name, mask);
				quickMsg(lmask);
				free(kicklist);
				return -2;
			}
			kicklist[count] = i;		/* add this matching user to list */
			if(++count >= config.maxkicks)
				break;
		}
		i = i->next;
	}
	free(wild);

	/* run through the list we created and kick the users */
	for(iter = 0; iter < count; iter++)
	{
		sprintf(lmask, "KICK %s %s :[%d%s %s (%s)", 
			theChan->name, (kicklist[iter])->nick, count, (count == config.maxkicks) ? "+]" : "]", reason, setby);
		if(expire > time(NULL)) {
			strcat(lmask, " Duration: ");
			strcat(lmask, small_duration(expire - time(NULL)) );
		}
		if(expire == -1)
			highMsg(lmask);
		else
			quickMsg(lmask);
	}

	free(kicklist);
	return count;
}

int isOp(struct schan *chan, char *nick)
{
	struct suser *i = chan->user;
	while(i)
	{
		if(!strcmp(i->nick, nick))
		{
			if(i->isOp)
				return 1;
			else
				return 0;
		}
		i = i->next;
	}
	return 0;
}

void massDeop(struct schan *chan, int count, char *args)
{
	int i;

	if(!chan || (count < 1))
		return;

	sprintf(taskbuf, "MODE %s -", chan->name);
	
	for(i = 0; i < count; i++)
		strcat(taskbuf, "o");

	strcat(taskbuf, args);

	quickMsg(taskbuf);

	return;
}

void unsetBans(struct schan *chan, int count, char *args)
{
	int i;

	if(!chan || (count < 1))
		return;

	if(!isOp(chan, cNick))
		return;

	sprintf(taskbuf, "MODE %s -", chan->name);
	
	for(i = 0; i < count; i++)
		strcat(taskbuf, "b");

	strcat(taskbuf, args);

	quickMsg(taskbuf);

	return;
}

void sendQUIT(char *extra)
{
	sprintf(taskbuf, "QUIT :\037/\002Fireclaw\002.%s/\037Running %s ~ %s", SHORT_VER, fixed_duration(time(0) - tStart), extra);
	irc_write(taskbuf);
	return;
}

void checkBadHost(struct schan *theChan, sUser *theUser)
{
	set_lower_message(theUser->mask, theUser->mask);
	if(regmatch(&swear_norm, theUser->mask) || regmatch(&swear_mild, theUser->mask))
		punish_user(theChan->record, findUser(theChan, theUser->nick), 'm', 0, 0);
	return;
}

void checkLamehost(struct schan *theChan, sUser *theUser)
{
	char field[32];
	int points, len, fields, badfields;
	char *pch, *anchor;

	len = strlen(theUser->host);
	points = fields = badfields = 0;

	anchor = theUser->host;

	/* ip address ? */
	if(anchor[len-1] >= '0' && anchor[len-1] <= '9')
		return;

	if(regmatch(&safehosts, anchor))
		return;

	if(regmatch(&swear_norm, theUser->mask)) {
		points += 30;
		badfields++;
	}

	pch = strchr(anchor, '.');
	while(pch)
	{
		fields++;
		if(pch-anchor >= 31) {
			points += (int)(pch-anchor);
			badfields++;
			field[0] = '\0';
		} else {
			memcpy(field, anchor, pch-anchor);
			field[pch-anchor] = '\0';
		}
		anchor = ++pch;
		pch = strchr(anchor, '.');
		if(pch && field[0]) {		/* nb, this comes after setting pch again, so that we dont check the last *2* fields */
			if(regmatch(&lamehost_5, field)) {
				points += 5;
				badfields++;
			} 
			if(regmatch(&lamehost_10, field)) {
				points += 10;
				badfields++;
			} 
			if(regmatch(&lamehost_15, field)) {
				points += 15;
				badfields++;
			}
		}
	}

	if(fields > 4)
		points += 10;
	points *= badfields;

	sprintf(field, "%d spampoints", points);
	if(points > 50)
		punish_user(theChan->record, findUser(theChan, theUser->nick), 'h', 0, field);

	return;
}
