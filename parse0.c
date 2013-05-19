/*
 * Copyright (c) 2010 Florian Jung <flo@windfisch.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */




#include <stdio.h>

//                    a0 b0 c0 d0 e0 f0 g0 
const int notemap[7]={33,35,24,26,28,29,31};

int getnum(char *s, int *i)
{
	int minus=0;
	int num=0;
	
	if (s[*i]=='-')
	{
		minus=1;
		(*i)++;
	}
	while((s[*i]>='0') && (s[*i]<='9'))
	{
		num=num*10+s[*i]-'0';
		(*i)++;
	}
	
	if (minus) return -num; else return num;
}

int parserange(char *s, int l[])
{
	int i, state=0;
	int j=0;
	int num, end;
	
	for (i=0;;i++)
		if ((s[i]!=' ') && (s[i]!='\t'))
		{
			switch(state)
			{
				case 0: //expects the (first) number
					if ((s[i]>='0') && (s[i]<='9')) //a number?
					{
						num=getnum(s,&i);
						i--;
						state=1;
					}
					break;
				case 1: //expects a , or a -
					if (s[i]=='-')
					{
						state=2;
					}
					else if ((s[i]==',') || (s[i]==0))
					{
						l[j]=num;
						j++;
						printf ("num=%i\n",num);
						state=0;
					}
					else
					{
						printf("ERROR: expected ',' or '-', found '%c'\n",s[i]);
						return -1;
					}
					break;
					
				case 2: //expects the second number that ends a range
					if ((s[i]>='0') && (s[i]<='9')) //a number?
					{
						end=getnum(s,&i);
						printf("numrange=%i-%i\n",num,end);
						for (;num<=end;num++)
						{
							l[j]=num;
							j++;
						}
						
						i--;
						state=3;
					}
					else
					{
						printf("ERROR: expected number, found '%c'\n",s[i]);
						return -1;
					}
					break;
				
				case 3: //expects ','
					if ((s[i]==',') || (s[i]==0))
						state=0;
					else
					{
						printf("ERROR: expected ',', found '%c'\n",s[i]);
						return -1;
					}
			}
			if (s[i]==0) break;
		}
		
		return j;
		
}

int main(int argc, char** argv)
{
	int i,l;
	FILE* f;
	char line[1000];
	char *range, *param;
	int patchlist[128];
	
	int oct,loud,len;
	int notenum, note[10];
	int j;
	int state;
	int dontignore;
	int rangelen;
	
	f=fopen ("config.txt","r");
	l=0;
	while(!feof(f))
	{
		fgets(line,sizeof(line),f);
		printf ("read %s",line);
		l++;

		dontignore=0;		
		for (i=0;line[i];i++)
			if ((line[i]!=' ') && (line[i]!='\t') && (line[i]!='\n'))
			{
				dontignore=1;
			}
		
		if (dontignore)
		{
			for (i=0;(line[i]!=0) && (line[i]!=')');i++); //find ')'
			if (line[i]==0)
			{
				printf("ERROR: expected ), found EOL in line %i\n",l);
			}
			else
			{
				line[i]=0;
				range=line;
				param=line+i+1;
				
				rangelen=parserange(range,patchlist);
				if (rangelen==-1)
				{
					printf ("ERROR: invalid range specified in line %i\n",l);
				}
				
				while(1)
				{
					for (i=0;(param[i]!=0) && (param[i]!=',') && (param[i]!='\n');i++); //find ','
					
					printf("'%s', '%s'\n",range,param);
					
						state=0;
					len=1337;//TODO sane default
					notenum=0;
					note[notenum]=0; oct=99;
					
					for (j=0;j<i;j++)
					{
						printf ("%i: ",j);
						switch (state)
						{
							case 0: //expects a note (a4) or loudness (<num>) or \0
								if ((param[j]>='a') && (param[j]<='g')) //found a note
								{
									printf("found a note\n");
									note[notenum]=notemap[param[j]-'a'];
									if ((param[j+1]=='b') || param[j+1]=='#') //vorzeichen?
									{
										if (param[j+1]=='b')
											note[notenum]--;
										else
											note[notenum]++;
										j++;
									}
									state=1;
								}
								else if ((param[j]>='0') && (param[j]<='9')) //found a number
								{
									printf("found loudness\n");
									loud=getnum(param,&j);
									printf("j=%i\n",j);
									state=2;
								}
								else if ((param[j]!=' ') && (param[j]!='\t'))
								{
									printf("ERROR: expected note or number, found %c in line %i\n",param[j],l);
								}
								break;
							
							case 1: //expects octave (-1 or 4)
								if ((param[j]=='-') || ((param[j]>='0') && (param[j]<='9')))
								{
									printf("found octave\n");
									oct=getnum(param,&j);
									note[notenum]+=oct*12;
									printf (" note=%i\n",note[notenum]);
									
									notenum++;
									state=0;
								}
								else
								{
									printf ("ERROR: expected - or 0-9, found %c in line %i\n",param[j],l);
								}
								break;
							
								
							case 2: //expects notelen or 0
								if ((param[j]>='0') && (param[j]<='9'))
								{
									printf("found notelen\n");
									len=getnum(param,&j);
									state=4;
								}
								else if ((param[j]!=' ') && (param[j]!='\t'))
								{
									printf("ERROR: expected number, found %c in line %i\n",param[j],l);
								}
						}
					}
					
					//TODO: neuen eintrag erstellen
					printf ("==== RESULT: loud=%i, len=%i, notes=",loud,len);
					for (j=0;j<notenum;j++)
						printf("%i  ",note[j]);
						
					printf("\n");
					
					if (param[i]==',') //is there a next param?
					{
						param=&param[i+1];
					}
					else
					{
						break;
					}
				}
			}
		}
		
		printf("%i\n",i);
	}

	return 0;
}
