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

int main(int argc, char** argv)
{
	char param[]="    a4 c#5   e5  200  ";
	
						char ntmp[100];
					int supp_spc=1;
					int i,j;
					j=0;
					for (i=0;param[i];i++)
					{ //if there will begin a new parameter, and there's not space or a note
						if ( !((param[i]>='a') && (param[i]<='g') || 
						       (param[i]==' ') || (param[i]=='\t') )     && supp_spc) break;

						if ((param[i]==' ') || (param[i]=='\t'))
						{
							if (!supp_spc)
								ntmp[j++]=' ';
							supp_spc=1;
						}
						else
						{
							supp_spc=0;
							ntmp[j++]=param[i];
						}
					}
					ntmp[j]=0;
					if (ntmp[j-1]==' ') ntmp[j-1]=0;

printf("'%s'\n",ntmp);
	return 0;
}
