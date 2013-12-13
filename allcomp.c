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
// $Id: allcomp.c,v 1.7 2003/06/18 20:23:15 snazbaz Exp $
//

#include "main.h"

#ifdef INCLUDE_ALLCOMP

static int listPos = 0;

void sort_list(struct tagList *list)
{
	struct tagList buf;
	int changes, iter;
	for(;;)
	{
		changes = 0;
		for(iter = 0; iter < listPos - 1; iter++)
		{
			if(list[iter].count < list[iter+1].count) {
				buf = list[iter];
				list[iter] = list[iter+1];
				list[iter+1] = buf;
				changes++;
			}
		}
		if(changes == 0)
			break;
	}
	return;
}

int print_list(struct tagList *list, char *filename, char *chan, struct tagRet *ret)
{
	int iter;
	FILE *tagfile;
	tagfile = fopen(filename, "w");
	if(!tagfile) {
		return 0;
	}

	fprintf(tagfile, "<html>\n<body>\n<center>Fireclaw tag count in %s, scanned %d nicks and found %d tags.<br><br>\n", chan, ret->counted_users, ret->num_tags);
	fprintf(tagfile, "<table width=300 align=center border=1 cellspacing=0>\n<tr><td><b>Position</b></td><td><b>Tag</b></td><td><b>Count</b></td></tr>\n");

	for(iter = 0; iter < listPos; iter++)
	{
		if(list[iter].count > 0)
			fprintf(tagfile, "<tr><td>#%d.</td><td>%s</td><td>%d</td></tr>\n", iter+1, list[iter].tag, list[iter].count);
	}

	fprintf(tagfile, "</table></center>\n</body>\n</html>\n");

	fclose(tagfile);
	return 1;
}

struct tagRet *count_tags(struct schan *theChan, int no_spam)
{
	static struct tagRet ret;
	struct tagList *list;
	struct suser *i;
	char *pch, *write;
	char temptag[NICKSIZE];
	char outline[513];
	int iter, excount;
	FILE *excludes;
	size_t size;

	ret.counted_users = ret.num_tags = 0;
	ret.topTags[0] = 0;
	listPos = iter = 0;
	size = sizeof(struct tagList) * theChan->userSize;

	/* allocate plenty of memory (enough for 1 tag per user if necessary 
		it doesnt matter really, it's only 20 bytes per user and we're going
		to free it again in a second. */
	list = (struct tagList *)myalloc(size);
	memset(list, 0, size);

	i = theChan->user;
	while(i)
	{
		ret.counted_users++;
		/* extract the first tag from this nick */
		write = temptag;
		pch = i->nick;
		while(*pch && *pch++ != '[');
		if(!*pch)
			goto do_next;
		while(*pch && *pch != ']')
			*write++ = *pch++;
		if(!*pch)
			goto do_next;
		*write = '\0';
		
		/* find it in the list and increment, *or* add it to the list. */
		for(iter = 0; iter < listPos; iter++) {
			if(!strcasecmp(list[iter].tag, temptag)) {
				list[iter].count++;
				goto do_next;
			}
		}

		/* add to list */
		strcpy(list[listPos].tag, temptag);
		list[listPos++].count = 1;

		if(listPos >= theChan->userSize)
			break;

		do_next:
		i = i->next;
	}
	
	/* got our list, time to sort it! */
	sort_list(list);
	
	ret.num_tags = listPos;

	if(listPos >= 3)
		sprintf(ret.topTags, "1. %s (%d) 2. %s (%d) 3. %s (%d)", list[0].tag, list[0].count, list[1].tag, list[1].count, list[2].tag, list[2].count);
	else
		strcpy(ret.topTags, "None set, too few tags found.");
	
	/* write the whole list to file */
	if(!print_list(list, ALLCOMP_FILE, theChan->name, &ret)) {
		free(list);
		return NULL;
	}

	excount = 0;
#ifdef ALLCOMP_EXCLUDE
	/* Now to remove exclusions from the list */
	excludes = fopen(ALLCOMP_EXCLUDE, "r");

	if(excludes) {

		while(!feof(excludes))
		{
			fgets(outline, NICKSIZE, excludes);
			if(strlen(outline) > 0)
				outline[strlen(outline)-1] = 0;
			for(iter =0; iter < listPos; iter++) {
				if(strmi(outline, list[iter].tag)) { 
					list[iter].count = 0;
					excount++;
				}
			}
		}

		fclose(excludes);

		/* resort the list.. */
		sort_list(list);

		/* reprint it */
		if(!print_list(list, ALLCOMP_FINAL, theChan->name, &ret)) {
			free(list);
			return NULL;
		}

	}
	ret.num_tags -= excount;
#endif

	if(!no_spam) {
		/* now lets output the top 10 in reverse order to the channel */
		if(listPos <= 10)
			iter = listPos - 1;
		else 
			iter = 9;

		sprintf(outline, "PRIVMSG %s :Tags counted, scanned %d nicks and found %d different tags. %d tags were excluded/banned. Top \002%d\002 tags are:", 
			theChan->name, ret.counted_users, ret.num_tags, excount, iter + 1);
		highMsg(outline);

		for(; iter >= 0; iter--)
		{
			sprintf(outline, "PRIVMSG %s :\002#%d\002. Tag: %s, count: %d", theChan->name, iter + 1, list[iter].tag, list[iter].count);
			lowMsg(outline);
		}
	}

	free(list);
	return &ret;
}

#endif
