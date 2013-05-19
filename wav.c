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



void u16cpy(char *dest, int src)
{
	dest[0]=src%256;
	dest[1]=src/256;
}

void s16cpy(char *dest, int src)
{
	if (src<0)
		src=(~(-src))+1;

	u16cpy(dest,src);
}

void u32cpy(char *dest, long int src)
{
	dest[0]=src%256;
	dest[1]=(src/256)%256;
	dest[2]=(src/256/256)%256;
	dest[3]=(src/256/256/256)%256;
}

int main(int argc, char** argv)
{
	FILE *f;
	f=fopen("test.wav","w");
	
	//char buf[44];
	char buf[244];
	
	int datalen,filelen,n_chans,samp_rate,bytes;
	
	datalen=100;
	n_chans=1;
	samp_rate=22050;
	bytes=2;
	
	
	filelen=12 + 24 + 8+datalen; //RIFF<len>WAVE|fmt <len><fmt>|DATA<len><data>
	
	strcpy(buf+ 0, "RIFF");
	u32cpy(buf+ 4, filelen-8);
	strcpy(buf+ 8, "WAVE");
	
	strcpy(buf+12, "fmt ");
	u32cpy(buf+16, 16);
	u16cpy(buf+20, 1); // format=1 (PCM)
	u16cpy(buf+22, n_chans); // mono
	u32cpy(buf+24, samp_rate);
	u32cpy(buf+28, samp_rate*n_chans*bytes);
	u16cpy(buf+32, n_chans*bytes);
	u16cpy(buf+34, bytes*8);
	
	strcpy(buf+36, "data");
	u32cpy(buf+40, datalen);
	//data follows	
	
	int i;
	for (i=0;i<100;i++)
	{
		s16cpy(buf+i*2+44, sin((double)i/50.0*3.141592654*2)*30000);
		//buf[i*2+44]= (sin((double)i/100.0*3.141592654*2)*30000);
	}
	
	
	fwrite(buf,1,244,f);
	fclose(f);
	
	return 0;
}
