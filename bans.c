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
// $Id: bans.c,v 1.15 2003/07/16 15:35:02 snazbaz Exp $
//

#include "main.h"

static char banbuf[BUFSIZE];
static char newmask[256];

int normal_ban(struct chanRec *theChan, char *mask, char *setby, int secs, char *reason)
{
	secs += time(NULL);

	if(!theChan->physical)
		return -1;

	if(!br_add(theChan->name, mask, reason, 'n', setby, secs))
		return -1;

	makeBanSpace(theChan->physical, 1);
	sprintf(banbuf, "MODE %s +b %s", theChan->name, mask);
	quickMsg(banbuf);
	
	save_chans(DEFAULT_CHANS);

	return enforce_ban(theChan->physical, mask, reason, setby, secs);
}

int rn_ban(struct chanRec *theChan, char *mask, char *setby, int secs, char *reason)
{
	int count;
	struct suser *i;

	secs += time(NULL);

	if(!theChan->physical)
		return -1;

	if(!br_add(theChan->name, mask, reason, 'r', setby, secs))
		return -1;

	i = theChan->physical->user;

	count = 0;

	while(i)
	{
		if(i->realname && (!i->isOp || strchr(theChan->flags, 'o')) )
			if(wildicmp(mask, i->realname))
			{
				makeBanSpace(theChan->physical, count + 1);
				sprintf(newmask, "*!*@%s", i->host);
				br_add(theChan->name, newmask, reason, 'd', setby, secs);
				sprintf(banbuf, "MODE %s +b %s", theChan->name, newmask);
				quickMsg(banbuf);
				count += enforce_ban(theChan->physical, newmask, reason, setby, secs);
			}
		i = i->next;
		if(count >= config.maxkicks)
			break;
	}

	save_chans(DEFAULT_CHANS);

	return count;
}

void chkfor_rn_ban(struct schan *theChan, struct suser *theUser)
{
	struct softBan *i;
	int count = 0;

	i = theChan->record->softbans;

	while(i)
	{
		if(i->type == 'r' && wildicmp(i->banmask, theUser->realname) && (!theUser->isOp || strchr(theChan->record->flags, 'o')))
		{
			makeBanSpace(theChan, count + 1);
			sprintf(newmask, "*!*@%s", theUser->host);
			br_add(theChan->name, newmask, i->reason, 'd', i->setby, i->expires);
			sprintf(banbuf, "MODE %s +b %s", theChan->name, newmask);
			quickMsg(banbuf);		
			enforce_ban(theChan, newmask, i->reason, i->setby, i->expires);
			count++;
		}
		i = i->next;
	}

	if(count > 0)
		save_chans(DEFAULT_CHANS);

	return;
}

void chkfor_ban(struct schan *theChan, struct suser *theUser)
{
	struct softBan *i;

	i = theChan->record->softbans;

	while(i)
	{
		if( (i->type == 'n' || i->type == 'd') && (!theUser->isOp || strchr(theChan->record->flags, 'o')) ) {
			if(wildicmp(i->banmask, theUser->mask))
			{
				makeBanSpace(theChan, 1);
				sprintf(banbuf, "MODE %s +b %s", theChan->name, i->banmask);
				quickMsg(banbuf);
				enforce_ban(theChan, i->banmask, i->reason, i->setby, i->expires);
			}
		}
		i = i->next;
	}
	return;
}

int chan_has_rn_bans(struct chanRec *theChan)
{
	struct softBan *i;

	i = theChan->softbans;
	while(i)
	{
		if(i->type == 'r')
			return 1;
		i = i->next;
	}
	return 0;
}

void searchForMissedBans(struct schan *theChan)
{
	struct suser *i;

	i = theChan->user;
	while(i)
	{
		chkfor_ban(theChan, i);
		if(i->realname)
			chkfor_rn_ban(theChan, i);
		i = i->next;
	}
	return;
}

void expireBans()
{
	char targs[513];
	struct chanRec *i;
	struct softBan *b, *d;
	struct sban *h;
	int mc, count, meOp;

	mc = count = 0;

	i = chans.next;

	while(i)
	{
		if(i->physical) {
			meOp = isOp(i->physical, cNick);
			if(!meOp) {
				sprintf(targs, "OP %s %s %s", config.linkname, cNick, i->name);
				send_to_all_links(targs);
			}
		} else {
			meOp = 0;
		}

		mc = 0;
		targs[0] = '\0';

		/* Remove any bans on the hardlist that are set by me but dont have an 
		internal ban matching. These are likely to happen if I expire a ban 
		while not opped/in the chan. This routine will make sure they dont get
		stuck there forever. */
		if(meOp && !check_dependants(i->physical))
		{
			h = i->physical->bans;
			while(h)
			{
				/* this is so unclean :( */
				if(!isSoftBan(i, h->mask)) {
					if( !strcmp(h->setby, cNick) || ( 
						(config.cleanbanlist > 0) && 
						(h->ts < time(0) - (config.cleanbanlist*3600)) &&
						(h->ts > 0) )
					  )
					/* This second condition for unsetting a ban is if we have
					cleanbanlist set in the config. This will expire all bans
					that are <x> hours long, whoever they belong to. It doesnt
					remove softbans, and the users will be rebanned again if
					they join. */
					{
						strcat(targs, " "); strcat(targs, h->mask);
						if(++mc >= config.maxmodes)
						{
							unsetBans(i->physical, mc, targs);
							mc = 0;
							targs[0] = '\0';	
						}
					}
				}
				h = h->next;
			}
			/* lets also use this branch to check for idle ops once a min */
			if(strchr(i->flags, 'I'))
				check_idle_ops(i);
		}

		/* cycle softban list, and expire these now */
		b = i->softbans;
		while(b)
		{
			d = b->next;
			if( (b->expires < time(NULL)) ) 
			{
				count++;
				if( (b->type != 'r') && main_irc && isHardBan(i->physical, b->banmask) )
				{
					strcat(targs, " "); strcat(targs, b->banmask);
					if(++mc >= config.maxmodes)
					{
						unsetBans(i->physical, mc, targs);
						mc = 0;
						targs[0] = '\0';	
					}
				}
				br_del(i, b->banmask);
			}
			b = d;
		}

		if(mc > 0)
		{
			unsetBans(i->physical, mc, targs);
		}
		i = i->next;
	}

	if(count > 0)
		save_chans(DEFAULT_CHANS);

	return;
}

int *wildunban(struct chanRec *theChan, char *mask)
{
	static int count[2];
	char targs[513];
	struct softBan *i, *fr;
	struct sban *i2;
	int mc;

	mc = count[0] = count[1] = 0;
	targs[0] = '\0';

	i = theChan->softbans;

	while(i)
	{
		fr = i;
		i = i->next;
		if(wildicmp(mask, fr->banmask))
		{
			count[1]++;
			if(fr->type != 'r' && isHardBan(theChan->physical, fr->banmask))
			{
				strcat(targs, " "); 
				strcat(targs, fr->banmask);
				if(++mc >= config.maxmodes)
				{
					unsetBans(theChan->physical, mc, targs);
					mc = 0;
					targs[0] = '\0';
				}
			}
			br_del(theChan, fr->banmask);
		}
	}

	/* now lets remove hardbans that arnt ours but still match the mask */
	if(theChan->physical)
	{
		i2 = theChan->physical->bans;
		while(i2)
		{
			fprintf(out, "Ban: %s\n", i2->mask);
			if(wildicmp(mask, i2->mask) && !isSoftBan(theChan, i2->mask))
			{
				count[0]++;
				strcat(targs, " "); 
				strcat(targs, i2->mask);
				if(++mc >= config.maxmodes)
				{
					unsetBans(theChan->physical, mc, targs);
					mc = 0;
					targs[0] = '\0';
				}				
			}
			i2 = i2->next;
		}
	}

	if(mc > 0)
	{
		unsetBans(theChan->physical, mc, targs);
	}

	if(count > 0)
		save_chans(DEFAULT_CHANS);

	return count;
}

void makeBanSpace(struct schan *theChan, int space)
{
	int i;
	struct sban *b;
	char targs[513];

	targs[0] = 0;

	if(!theChan)
		return;

	if(theChan->banSize + space <= config.banlim)
		return;

	b = theChan->bans;
	i = space;

	while(b && i--)
	{
		strcat(targs, " ");
		strcat(targs, b->mask);
		b = b->next;
	}

	unsetBans(theChan, space, targs);

	return;
}

int isSoftBan(struct chanRec *c, char *mask)
{
	struct softBan *i;
	if(!c)
		return 0;
	i = c->softbans;
	while(i)
	{
		if(!strcmp(i->banmask, mask))
			return 1;
		i = i->next;
	}
	return 0;
}

int isHardBan(struct schan *c, char *mask)
{
	struct sban *i;
	if(!c)
		return 0;
	i = c->bans;
	while(i)
	{
		if(!strcmp(i->mask, mask))
			return 1;
		i = i->next;
	}
	return 0;
}
