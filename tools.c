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
// $Id: tools.c,v 1.11 2003/07/16 15:35:02 snazbaz Exp $
//

#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "main.h"

/* safer strncpy that terminates the string. */
char* mystrncpy(char* s1, const char* s2, size_t n)
{
  char* endp = s1 + n - 1;
  char* s = s1;
  while(s < endp && (*s++ = *s2++));
  *s = 0;
  return s1;
}

char *cropnick(const char *nick, int nicklen)
{
	static char nickbuf[128];
	mystrncpy(nickbuf, nick, nicklen+1);
	return nickbuf;
}

void modifyFlags(char *flags, char *changes)
{
	char ptt[2];
	char *pch, *set;
	char sign = '+';
	
	for(ptt[1] = 0; *changes; changes++)
	{
		if(*changes == '+' || *changes == '-') {
			sign = *changes;
		} else if(strchr(VALIDFLAGS, *changes)) {
			if(sign == '+' && !strchr(flags, *changes)) {
				ptt[0] = *changes;
				strcat(flags, ptt);
			} else if(sign == '-') {
				pch = strchr(flags, *changes);
				if(pch) {
					set = pch + 1;
					while(*pch)
						*pch++ = *set++;
				}
			}
		}
	}
	return;
}

void str_remove(char *str, char *remove)
{
	int len, rlen;
	char *pch;
	len = strlen(str);
	rlen = strlen(remove);

	pch = strstr(str, remove);
	while(pch)
	{
		memcpy(pch, pch + rlen, rlen + 1);
		pch = strstr(pch, remove);
	}

	return;
}

int strmi(char *str1, char *str2)
{
	int i = 0;
	for(;;i++)	{
		if(tolower(str1[i]) != tolower(str2[i]))
			return 0;
		if(str1[i] == '\0')
			return 1;
	}
	return 1;
}

int tokenize( char *line, char *separators, char *tokens[], int max )
{
   int i = 0;
   char *pch;
   pch = (char*)strtok( line, separators ); 
   while(pch != NULL)
   {
      if ( i < max )
         tokens[i] =pch;
      pch = (char*)strtok( NULL, separators );
      i++;
   }
   return( i );
}

int write_memory(char *file, char *begin, unsigned size)
{
	int handle, bytes;
	handle = open(file, O_CREAT | O_WRONLY, 0);
	if(handle == -1)
		return 0;
	bytes = write(handle, begin, size);
	close(handle);
	if(bytes != size)
		return 0;
	return 1;
}

int read_memory(char *file, char *begin, unsigned size)
{
	int handle, bytes;
	handle = open(file, O_RDONLY, 0);
	if(handle == -1)
		return 0;
	bytes = read(handle, begin, size);
	close(handle);
	if(bytes != size)
		return 0;
	return 1;
}

char *fixed_duration(int duration)
{
		static char dbuffer[20];
		int days = 0, hours = 0, mins = 0;
        days = (int)(duration/(24*60*60));
        duration %= (24*60*60);
        hours = (int)(duration/(60*60));
        duration %= (60*60);
        mins = (int)(duration/60);
        duration %= 60;
        sprintf(dbuffer, "%01d days, %02d:%02d:%02d", days, hours, mins, duration);
        return dbuffer;
}

char *small_duration(int duration)
{
	static char dbuffer[20];
	int days = 0, hours = 0, mins = 0;
	dbuffer[0] = 0;
	days = (int)(duration/(24*60*60));
	duration %= (24*60*60);
	hours = (int)(duration/(60*60));
	duration %= (60*60);
	mins = (int)(duration/60);
	if(days)
		sprintf(dbuffer, "%dd %dh %dm", days, hours, mins);
	else if(hours)
		sprintf(dbuffer, "%dh %dm", hours, mins);
	else
		sprintf(dbuffer, "%dm", mins);
	return dbuffer;
}

int world_uptime()
{
	FILE *fp;
	char *pch;
	char buffer[64];

	if (!(fp=fopen("/proc/uptime","r"))) { 
		fprintf(out, "Cannot open proc, not installed!\n"); 
        return 0; 
    }
	fgets(buffer, 64, fp);
	fclose(fp);
	pch = strchr(buffer, '.');
	*pch = '\0';
	return atoi(buffer);	
}

double my_cputime()
{
	char buffer[1024];
	FILE *fp;
	char *pch, *p2, *p3;
	double ret;
	int i, a, b;

	if(!(fp = fopen("/proc/self/stat", "r"))) {
		fprintf(out, "Cannot open proc.\n");
		return 0;
	}
	fgets(buffer, 1000, fp);
	fclose(fp);
	
	pch = buffer;

	for(i = 0; i < 13; i++)
		pch = strchr(pch, ' ')+1;

	p2 = strchr(pch, ' ');
	*p2++ = '\0';
	p3 = strchr(p2, ' ');
	*p3 = '\0';

	a = atoi(pch);
	b = atoi(p2);

	ret = ((double)(a+b)/100.0);
	return ret;
}

// Wildcard algorithm.
// Credits: www.codeproject.com
int wildcmp(char *wild, char *string) 
{
	char *cp, *mp;
	mp = cp = 0;
	
	while ((*string) && (*wild != '*')) {
		if ((*wild != *string) && (*wild != '?')) {
			return 0;
		}
		wild++;
		string++;
	}
		
	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return 1;
			}
			mp = wild;
			cp = string+1;
		} else if ((*wild == *string) || (*wild == '?')) {
			wild++;
			string++;
		} else {
			wild = mp;
			string = cp++;
		}
	}
		
	while (*wild == '*') {
		wild++;
	}
	return !*wild;
}

int wildicmp(char *wild, char *string) 
{
	char *cp, *mp;
	mp = cp = 0;
	
	while ((*string) && (*wild != '*')) {
		if ((tolower(*wild) != tolower(*string)) && (*wild != '?')) {
			return 0;
		}
		wild++;
		string++;
	}
		
	while (*string) {
		if (*wild == '*') {
			if (!*++wild) {
				return 1;
			}
			mp = wild;
			cp = string+1;
		} else if ((tolower(*wild) == tolower(*string)) || (*wild == '?')) {
			wild++;
			string++;
		} else {
			wild = mp;
			string = cp++;
		}
	}
		
	while (*wild == '*') {
		wild++;
	}
	return !*wild;
}

char checkSimChars(char *str1, char *str2)
{
	for(; *str1; str1++)
		if(strchr(str2, *str1))
			return *str1;
	return 0;
}
