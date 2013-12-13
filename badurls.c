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
// $Id: badurls.c,v 1.5 2003/06/23 23:08:20 snazbaz Exp $
//

#include "main.h"

static struct badUrl *badlist = NULL;

int match_badurl(char *text)
{
	struct badUrl *i;
	i = badlist;
	while(i)
	{
		if(strstr(text, i->match)) {
			i->count++;
			return 1;
		}
		i = i->next;
	}
	return 0;
}

void init_badurls()
{
	badlist = NULL;
}

void reset_badurls()
{
	struct badUrl *i, *last;
	if(badlist) {
		i = badlist;
		while(i)
		{
			last = i;
			i = i->next;
			free(last->match);
			free(last);
		}
		badlist = NULL;
	}
	return;
}

void add_badurl(char *match)
{
	struct badUrl *temp, *i;

	temp = (struct badUrl *)myalloc(sizeof(struct badUrl));

	temp->match = (char*)myalloc(strlen(match)+1);
	memcpy(temp->match, match, strlen(match)+1);
	temp->count = 0;
	temp->next = NULL;
	
	if(badlist == NULL)
		badlist = temp;
	else {
		i = badlist;
		while(i->next)
			i = i->next;
		i->next = temp;
	}
	
	return;
}
