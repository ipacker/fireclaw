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
// $Id: punish.c,v 1.13 2003/07/16 15:35:02 snazbaz Exp $
//

#include "main.h"
#include "punish.h"

static char punbuf[BUFSIZE];

void write_cstat_msg(char *buffer)
{
	char smallbuf[32];
	int i;

	for(i = 0; i < 26; i++)
	{
		if(pmap[i].weight > 0)
		{
			sprintf(smallbuf, "%s(\002%d\002) ", pmap[i].shortName, pmap[i].counter);
			strcat(buffer, smallbuf);
		}
	}
	return;
}

void punish_user(struct chanRec *theChan, struct suser *theUser, char type, char subtype, char *extra)
{
	struct punishment *p;
	struct criminalRecord *c;
	char action[32];
	char reason[250];
	int index = ((int)((unsigned char)type - 97));

	if(!theUser || !theChan || !theChan->physical)
		return;

	if(check_dependants(theChan->physical))
		return;

	/* check punishment flag is between a - z */
	if((index < 0) || (index > 25))
		return;

	/* see punish.h for map declaration */
	p = &pmap[index];

	/* we don't hurt ops unless +o (kickops) is set, but we might as well 
		tell them off on the relay */
	if(theUser->isOp && !strchr(theChan->flags, 'o')) {
		snprintf(punbuf, 500, "[\002%s\002] %s triggered '%s' punishment, but is an op.", 
			theChan->name, theUser->mask, p->shortName);
		sRelay(punbuf);
		return;
	}

	c = findCriminal(theChan->physical, theUser->nick);

	if(!c) {
		c = nu_add(theChan->physical, theUser->nick, theUser->ident, theUser->host);
		if(!c)
			return;
	} else {
		c->laststamp = time(NULL);
	}

	c->longTerm += p->weight + subtype;
	c->shortTerm += p->weight + subtype;

	/* If user joined in the last 60 seconds, treat more harshly */
	if(time(NULL) - theUser->joinTime < 60) {
		c->shortTerm++;
		c->longTerm += 2;
	}

	if(extra)
		snprintf(reason, 250, "%s (%s)", p->reason, extra);
	else
		mystrncpy(reason, p->reason, 250);

	if(c->shortTerm <= 1) {		/* warn user */
		snprintf(punbuf, 510, config.warntemplate, theUser->nick, theChan->name, reason);
		quickMsg(punbuf);
		strcpy(action, "warn");
	} else if(c->shortTerm == 2) { /* kick user */
		snprintf(punbuf, 510, "KICK %s %s :[Warning] %s", theChan->name, theUser->nick, reason);
		quickMsg(punbuf);
		strcpy(action, "kick");
	} else {					/* ban user */
		c->shortTerm = 0;
		sprintf(punbuf, "*!*@%s", theUser->host);
		normal_ban(theChan, punbuf, "Auto", (c->longTerm * (*p->banlen)), reason);
		strcpy(action, "ban");
	}

	sprintf(punbuf, "[\002%s\002] Criminal: %s, Karma: %d, Offence: %s, Action Taken: %s", theChan->name, theUser->nick, c->longTerm, p->shortName, action);
	sRelay(punbuf);
	
	p->counter++;

	return;
}

