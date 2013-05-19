/*
 * Copyright (c) 2010 Florian Jung <flo@windfisch.org>
 * for the MIDI constants and some jack-midi code, which has been
 * taken from jack-keyboard: 
 * Copyright (c) 2007, 2008 Edward Tomasz Napierała <trasz@FreeBSD.org>
 * All rights reserved.
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


/* TODO
 * dateiname finden
 * dirliste lesen
 * bytes aus datei lesen
 * patchnamen lesen
 * dateien namen geben
 * verzeichnisse anlegen (gruppen)
 * --overwrite-option?
 */

//LAST STABLE: 04


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>


#define MIDI_PORT_NAME	"midi"
#define REC_PORT_NAME	"rec"

jack_port_t	*recport, *midiport;
double		rate_limit = 0.0;
jack_client_t	*jack_client = NULL;

#define MIDI_NOTE_ON		0x90
#define MIDI_NOTE_OFF		0x80
#define MIDI_PROGRAM_CHANGE	0xC0
#define MIDI_CONTROLLER		0xB0
#define MIDI_RESET		0xFF
#define MIDI_HOLD_PEDAL		64
#define MIDI_ALL_SOUND_OFF	120
#define MIDI_ALL_MIDI_CONTROLLERS_OFF	121
#define MIDI_ALL_NOTES_OFF	123
#define MIDI_BANK_SELECT_MSB	0
#define MIDI_BANK_SELECT_LSB	32

#define BANK_MIN		0
#define BANK_MAX		127
#define PROGRAM_MIN		0
#define PROGRAM_MAX		16383
#define CHANNEL_MIN		1
#define CHANNEL_MAX		16

#define time_offsets_are_zero 0

struct MidiMessage {
	jack_nframes_t	time;
	int		len;	/* Length of MIDI message, in bytes. */
	unsigned char	data[3];
};

#define RINGBUFFER_SIZE		1024*sizeof(struct MidiMessage)

jack_ringbuffer_t *ringbuffer;

/* Number of currently selected channel (0..15). */
int		channel = 9;



//FINDMICH
#define RECBUF_LEN 44100*60
#define SILENCE_LEN 44100*2
#define SILENCE_ADD 0.005   //schon ziemlich knapp. lieber weniger
#define QUIET_LEN 441
#define WAIT_AFTER 22050
#define MAX_NOTES 10
#define DEFAULT_LEN 1000 //TODO richtigen wert finden!

jack_default_audio_sample_t buf1[RECBUF_LEN], buf2[RECBUF_LEN];
jack_default_audio_sample_t *recbuf, *workbuf;
jack_default_audio_sample_t silence;
int note=60, laut=100, notelen=22050;
int main_working, all_done;
int workstart, workend, main_rbpos;

int samp_rate;
int bytes=2;
char wavheader[44];
int flo_start;

struct entry_t
{
	char chan;
	char patch;
	
	char loud;
	char note[MAX_NOTES];
	int notelen;
	
	char *file, *dir;
	
	struct entry_t *next;
};
typedef struct entry_t entry_t;

entry_t *recentry, *workentry, *firstentry;

const int notemap[7]={33,35,24,26,28,29,31};

char *dirs[128];
char *patchname[128];


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
//						printf ("num=%i\n",num);
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
//						printf("numrange=%i-%i\n",num,end);
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



jack_default_audio_sample_t arr_dist (jack_default_audio_sample_t a[], int l)
{
	int i;
	jack_default_audio_sample_t max,min;
	min=max=a[0];
	
	for (i=0;i<l;i++)
		if (a[i]>max)
			max=a[i];
		else if (a[i]<min)
			min=a[i];
			
	return max-min;
}


int process_callback(jack_nframes_t nframes, void *notused) //WICHTIG FINDMICH
{
	static int state = 0;
	static int frame_cnt;
	static int rbpos=0, recend;
	int i;
	void           *midipb;
	jack_nframes_t	last_frame_time;
	unsigned char data[3];

	last_frame_time = jack_last_frame_time(jack_client);


	jack_default_audio_sample_t *recpb=
			(jack_default_audio_sample_t *) jack_port_get_buffer (recport, nframes);
	if (recpb==NULL) printf("rec fail\n");

	midipb = jack_port_get_buffer(midiport, nframes);
	if (midipb==NULL) printf("midi fail\n");
	jack_midi_clear_buffer(midipb);

	
	switch(state) 
	{
		case 0: // warte auf freigabe und initialisiere
			if (flo_start)
			{
				state=1;
				rbpos=0;
				recentry=firstentry;
				printf("messe stille\n");
			}
			break;
			
		case 1: // sammle daten über stille
			for (i=0;i<nframes;i++)
			{
				recbuf[rbpos]=recpb[i];
				rbpos++;
			}
			if (rbpos>=SILENCE_LEN) //genug?
			{
				silence=arr_dist(recbuf,SILENCE_LEN)  +  SILENCE_ADD;
				printf("  stille=%f\n",silence);
				state=2;
			}
			break;
		
		case 2: // spiele note, wie in recentry beschrieben
			printf("NOTE ON\n");
			//TODO ggf noch bank select?
			
			data[0]=MIDI_PROGRAM_CHANGE;
			data[1]=recentry->patch;
			if (jack_midi_event_write(midipb, 0, data, 2))
				fprintf(stderr,"Note loss when writing\n");
			
			data[0]=MIDI_NOTE_ON;
			data[2]=recentry->loud; 

			for (i=0;recentry->note[i]!=-1;i++)
			{
				data[1]=recentry->note[i]; 
				if (jack_midi_event_write(midipb, 0, data, 3))
					fprintf(stderr,"Note loss when writing\n");
			}
			frame_cnt=recentry->notelen;
			
			rbpos=0;
			recend=-1;
			
			state=3;
			break;
			
		case 3: // nehme auf und sende ggf note off
			if (frame_cnt != -1) //ggf note off
			{
				if (frame_cnt < nframes) //noteoff in diesem frame?
				{
					printf("NOTE OFF in %i\n",frame_cnt);

					data[0]=MIDI_NOTE_OFF;
					data[2]=recentry->loud; 
					for (i=0;recentry->note[i]!=-1;i++)
					{
						data[1]=recentry->note[i]; 
						if (jack_midi_event_write(midipb, frame_cnt, data, 3))
							fprintf(stderr,"Note loss when writing\n");
					}
					
					frame_cnt=-1;
				}
				else
					frame_cnt-=nframes;
			}
			
			for (i=0;i<nframes;i++) //aufnehmen
			{
				if (rbpos>=RECBUF_LEN)
					printf("WARNING: segfault\n"); //TODO: statt segfault tainted setzen und im letzten bufferteil loopen
				recbuf[rbpos]=recpb[i];
				rbpos++;
			}
			if (rbpos>QUIET_LEN) //TODO FINDMICH: QUIET_LEN mit nframes ersetzen???
			{
				if (arr_dist(&recbuf[rbpos-QUIET_LEN],QUIET_LEN)<=silence) //es wird still...
				{
					if (recend==-1) recend=rbpos-QUIET_LEN;
					
					if (frame_cnt==-1) //ggf aufs noteoff warten
					{
//						printf ("aufnahme fertig\n"); 
						
						state=4;
					}
				}
				else //nicht (mehr?) still
				{
					recend=-1;
				}
			}
			break;
		
		case 4: //auf verarbeitungs-thread warten
			if (main_working==0) //main ist fertig?
			{
				if (recbuf==buf1) //buffer wechseln
				{
					recbuf=buf2;
					workbuf=buf1;
				}
				else
				{
					recbuf=buf1;
					workbuf=buf2;
				}
				workend=recend;
				main_rbpos=rbpos;
				workentry=recentry;
				
				main_working=1; //befehl zum starten geben
				
				
				recentry=recentry->next;
				if (recentry==NULL) //fertig?
				{
					printf("all done, waiting for main to finish and clean up\n");
					all_done=1;
					state=99;
				}
				else
				{
					frame_cnt=WAIT_AFTER;
					state=5;
				}
			}
			break;
		
		case 5: //noch einige zeit warten
			frame_cnt-=nframes;
			if (frame_cnt<=0)
			{									
				frame_cnt=-1;
//				printf ("fertig mit warten\n");
				state=2;
			}
			break;
		
		case 99: //nichts tun, main wird alles fertig machen, aufräumen und beenden.
			break;
	}
	
	if (nframes <= 0) {
		fprintf(stderr,"Process callback called with nframes = 0; bug in JACK?\n");
		return 0;
	}
	

	return 0;
}

void init_jack(void) //WICHTIG !!!
{
	jack_client = jack_client_open("ripmidi", JackNullOption, NULL);
	if (jack_client == NULL)
	{
		fprintf(stderr,"Registering client failed.\n");
		exit(1);
	}

	ringbuffer = jack_ringbuffer_create(RINGBUFFER_SIZE);
	if (ringbuffer == NULL)
	{
		fprintf(stderr,"Creating ringbuffer failed.\n");
		exit(1);
	}
	jack_ringbuffer_mlock(ringbuffer);

	if (jack_set_process_callback(jack_client, process_callback, 0)) 
	{
		fprintf(stderr,"Registering callback failed.\n");
		exit(1);
	}

	recport=jack_port_register(jack_client, REC_PORT_NAME, JACK_DEFAULT_AUDIO_TYPE,
		JackPortIsInput, 0);
	midiport = jack_port_register(jack_client, MIDI_PORT_NAME, JACK_DEFAULT_MIDI_TYPE,
		JackPortIsOutput, 0);

	if ((midiport == NULL) || (recport == NULL)) {
		fprintf(stderr,"Registering ports failed.\n");
		exit(1);
	}

	samp_rate=jack_get_sample_rate(jack_client);

	if (jack_activate(jack_client)) 
	{
		fprintf(stderr,"Activating client failed.\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int i,j,k,l,m;
	FILE* f;
	char line[1000];
	char *range, *param;
	int patchlist[128];
	
	int oct,loud,len;
	int notenum, note[10];
	int state;
	int dontignore;
	int rangelen;
	
	entry_t *curr_entry;
	
	
	//TODO nettes CLI
	
	
	bytes=2;	

	//init
	recbuf=buf1; workbuf=buf2;
	main_working=0; all_done=0;
	flo_start=0;

	init_jack();


//TODO patchname und dirs initialisieren FINDMICH
	char dummy[]="unknown";
	for (i=0;i<128;i++)
		dirs[i]=patchname[i]=dummy;


	f=fopen("patches.txt","r");
	while(!feof(f))
	{
		fgets(line,sizeof(line),f);
		for (i=strlen(line)-1;i>=0;i--) //remove trailing spaces
		{
			if ((line[i]=='\n') || (line[i]==' ') || (line[i]=='\t'))
				line[i]=0;
			else
				break;
		}
		

		if (!isdigit(line[0]))
		{
			fprintf(stderr,"invalid number in patchlist, ignoring it...\n");
		}
		else
		{
			i=0;
			m=getnum(line,&i);
			while ( ((line[i]==' ') || (line[i]=='\t')) && line[i] ) i++;  //line[i] is letter or NUL
			asprintf(&patchname[m], "%s", line+i);
		}
	}
	fclose(f);


	f=fopen("groups.txt","r");
	while(!feof(f))
	{
		fgets(line,sizeof(line),f);
		for (i=strlen(line)-1;i>=0;i--) //remove trailing spaces
		{
			if ((line[i]=='\n') || (line[i]==' ') || (line[i]=='\t'))
				line[i]=0;
			else
				break;
		}
		
		char *grptmp;
		for (i=0;line[i];i++)
			if ((line[i]=='\n') || (line[i]==' ') || (line[i]=='\t'))
			{
				line[i]=0;
				grptmp=&line[i+1];
				break;
			}
			
		int grprange[128];
		m=parserange(line,grprange);
		if (m<0)
		{
			printf("error parsing range, ignoring the line...\n");
		}
		else
		{
			char *grptmp2;
			asprintf(&grptmp2, "%s", grptmp);
			for (i=0;i<m;i++)
			{
				dirs[grprange[i]]=grptmp2;
				printf("dirs[%i]='%s'\n",grprange[i],grptmp2);
			}
		}
	}

	fclose(f);

	
	f=fopen ("config.txt","r");
	l=0;
	curr_entry=firstentry=NULL;
	while(!feof(f))
	{
		fgets(line,sizeof(line),f);
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
				goto invalid;
			}

			line[i]=0;
			range=line;
			param=line+i+1;
			
			rangelen=parserange(range,patchlist);
			if (rangelen==-1)
			{
				printf ("ERROR: invalid range specified in line %i\n",l);
				goto invalid;
			}
			
			while(1)
			{
				for (i=0;(param[i]!=0) && (param[i]!=',') && (param[i]!='\n');i++); //find ','
				
				state=0;
				len=DEFAULT_LEN;
				notenum=0;
				note[notenum]=0; oct=99;
				
				for (j=0;j<i;j++)
				{
					switch (state)
					{
						case 0: //expects a note (a4) or loudness (<num>) or \0
							if ((param[j]>='a') && (param[j]<='g')) //found a note
							{
//									printf("found a note\n");
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
//									printf("found loudness\n");
								loud=getnum(param,&j);
								state=2;
							}
							else if ((param[j]!=' ') && (param[j]!='\t'))
							{
								printf("ERROR: expected note or number, found %c in line %i\n",param[j],l);
								goto invalid;
							}
							break;
						
						case 1: //expects octave (-1 or 4)
							if ((param[j]=='-') || ((param[j]>='0') && (param[j]<='9')))
							{
//									printf("found octave\n");
								oct=getnum(param,&j);
								
								note[notenum]+=oct*12;
								if ((note[notenum]<0) || (note[notenum]>127)) //illegal note
								{
									printf("ERROR: invalid note in line %i\n",l);
									goto invalid;
								}
								else
								 notenum++;
								state=0;
							}
							else
							{
								printf ("ERROR: expected - or 0-9, found %c in line %i\n",param[j],l);
								goto invalid;
							}
							break;
						
							
						case 2: //expects notelen or 0
							if ((param[j]>='0') && (param[j]<='9'))
							{
//									printf("found notelen\n");
								len=getnum(param,&j);
								if (len<0)
								{
									printf("WARNING: len was negative in line %i; using absolute value\n",l);
									len=-len;
								}
								state=4;
							}
							else if ((param[j]!=' ') && (param[j]!='\t'))
							{
								printf("ERROR: expected number, found %c in line %i\n",param[j],l);
								goto invalid;
							}
					}
				}
				
				//in die liste eintragen
				
				for (k=0;k<rangelen;k++)
				{
					entry_t *tmpptr=malloc(sizeof(entry_t));
					if (tmpptr==NULL)
					{
						printf("GAAAH... malloc error\n");
						exit(1);
					}
					if (firstentry==NULL)
						firstentry=tmpptr;
					else
						curr_entry->next=tmpptr;
						
					tmpptr->loud=loud;
					tmpptr->notelen=len*samp_rate /1000; //von msec in samples umrechnen
					tmpptr->chan=0;
					tmpptr->patch=patchlist[k];
					tmpptr->dir=dirs[patchlist[k]];

					char ntmp[100]; //trim multiple, leading and trailing spaces from param
					int supp_spc=1;
					j=0;
					for (m=0;(param[m]!=0) && (param[m]!=',') && (param[m]!='\n');m++)
					{ //if there will begin a new parameter, and there's not space or a note
						if ( !(((param[m]>='a') && (param[m]<='g')) || 
						       (param[m]==' ') || (param[m]=='\t') )     && supp_spc) break;

						if ((param[m]==' ') || (param[m]=='\t'))
						{
							if (!supp_spc)
								ntmp[j++]=' ';
							supp_spc=1;
						}
						else
						{
							supp_spc=0;
							ntmp[j++]=param[m];
						}
					}
					ntmp[j]=0;
					if (ntmp[j-1]==' ') ntmp[j-1]=0;
					
					asprintf(&tmpptr->file, "%s/%03i%s %s %03i %ims.wav",tmpptr->dir, tmpptr->patch,
					    patchname[tmpptr->patch], ntmp, tmpptr->loud, len);
					
					
					for (j=0;j<notenum;j++)
						tmpptr->note[j]=note[j];
					tmpptr->note[notenum]=-1;
					tmpptr->next=NULL;
					
					curr_entry=tmpptr;
				}
				
				if (param[i]==',') //is there a next param?
					param=&param[i+1];
				else
					break;
			}
		}
	}

	
	
	

	
	//init header
	strcpy(wavheader+ 0, "RIFF");
	strcpy(wavheader+ 8, "WAVE");
	strcpy(wavheader+12, "fmt ");
	u32cpy(wavheader+16, 16);
	u16cpy(wavheader+20, 1);
	u16cpy(wavheader+22, 1);
	u32cpy(wavheader+24, samp_rate);
	u32cpy(wavheader+28, samp_rate*bytes);
	u16cpy(wavheader+32, bytes);
	u16cpy(wavheader+34, bytes*8);
	strcpy(wavheader+36, "data");

	
	
	
	printf("connect ripmidi's midi port to some MIDI OUT and the audio\n"
	       "port to the corresponding audio input port; then press enter\n");
	char foo[10]; gets(foo);
	
	flo_start=1;
	
	main_working=0;
	printf("main: entering loop\n");
	while(1)
	{
		while ((main_working==0) && (all_done==0)) usleep(100000); // 0.1 sec

		printf("main: starting work cycle\n");
		
		workstart=-1;
		for (i=1;i<=QUIET_LEN;i++) //Find begin of non-silence
			if (arr_dist(workbuf,i) > silence)
			{
				workstart=i-1;
				break;
			}

		if (workstart==-1) // [ continued ] 
			for (i=QUIET_LEN;i<workend;i++)
				if (arr_dist(&workbuf[i-QUIET_LEN],QUIET_LEN) > silence)
				{
					workstart=i-1;
					break;
				}

		if (workstart!=-1)
		{
//			printf("main: gotcha. %i - %i [%i - %i]\n",workstart,workend,0,main_rbpos);
			jack_default_audio_sample_t max=0.0; //find max amplitude
			for (i=workstart; i<=workend; i++)
			{
				if (workbuf[i]>0)
				{
					if (workbuf[i]>max) max=workbuf[i];
				}
				else
				{
					if (-workbuf[i]>max) max=-workbuf[i];
				}
			}
		
			int datalen,filelen;
			datalen=(workend-workstart+1)*bytes;
			filelen=44+datalen;
			u32cpy(wavheader+ 4, filelen-8);
			u32cpy(wavheader+40, datalen);
			

			int ret=mkdir(workentry->dir,S_IRWXU|S_IRWXG|S_IRWXO);
			if ((ret==-1) && (errno!=EEXIST)) //a real error occurred
			{
				fprintf(stderr,"ERROR: could not create %s (%s)\n",workentry->dir,strerror(errno));
			}
			
			f=fopen(workentry->file,"w");
			if (f==NULL)
			{
				fprintf(stderr,"ERROR: could not open %s (%s)\n",workentry->file,strerror(errno));
			}
			else
			{
				printf("writing to file %s\n",workentry->file);
				unsigned char tmp[2];
				
				fwrite(wavheader, 1, 44, f);
				if (bytes==1)
					for (i=workstart;i<=workend;i++)
					{
						*tmp=(workbuf[i]/max*127.0)+127;
						fwrite(tmp,1,1,f);
					}
				else
					for (i=workstart;i<=workend;i++)
					{
						s16cpy(tmp, (long)(workbuf[i]/max*32767.0));
						fwrite(&tmp,1,2,f);
					}
							
				fclose(f);
			}
						
			
		}
		else
		{
			printf ("main: komisch. nur stille aufgenommen...\n"); //TODO handlen!
		}
		
		
		printf("main: work done\n");
		main_working=0;
		
		if (all_done) break;
	}

	printf("\n=== ALL DONE ===\n[exiting...]\n\n");
	return 0;
	
	
	
	invalid:
	printf("config file invalid, exiting...\n");
	exit(1);
}

