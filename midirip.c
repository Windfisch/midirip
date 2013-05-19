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


//LAST STABLE: 02


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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

jack_default_audio_sample_t buf1[RECBUF_LEN], buf2[RECBUF_LEN];
jack_default_audio_sample_t *recbuf, *workbuf;
jack_default_audio_sample_t silence;
int note=60, laut=100, notelen=22050;
int main_working, all_done;
int workstart, workend, main_rbpos;

int samp_rate;
int bytes=2;
char wavheader[44];
int flo_start=0;

//typedef struct {} entry_t;
struct entry_t
{
	
	struct entry_t *next;
};

typedef struct entry_t entry_t;

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

	
	switch(state) //TODO: statt segfault tainted setzen und im letzten bufferteil loopen
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
			data[0]=MIDI_NOTE_ON; //TODO
			data[1]=note; 
			data[2]=laut; 
			if (jack_midi_event_write(midipb, 0, data, 3))
				fprintf(stderr,"Note loss when writing\n");
			frame_cnt=notelen; //TODO
			
			rbpos=0;
			recend=-1;
			
			state=3;
			break;
			
		case 3: // nehme auf und sende ggf note off
			if (frame_cnt != -1) //ggf note off
			{
				if (frame_cnt < nframes) //noteoff in diesem frame?
				{
					printf("NOTE OFF in %i\n",frame_cnt); //TODO
					data[0]=MIDI_NOTE_OFF;
					data[1]=note; 
					data[2]=laut; 

					if (jack_midi_event_write(midipb, frame_cnt, data, 3))
						fprintf(stderr,"Note loss when writing\n");
					
					frame_cnt=-1;
				}
				else
					frame_cnt-=nframes;
			}
			
			for (i=0;i<nframes;i++) //aufnehmen
			{
				if (rbpos>=RECBUF_LEN)
					printf("WARNING: segfault\n"); //TODO
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
						printf ("aufnahme fertig\n"); 
						
						state=4;
					}
				}
				else //nicht (mehr?) still
				{
					if (recend!=-1)
						printf("komisch... erst still, dann laut? %i\n",rbpos);
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
				workend=recend; //TODO evtl noch mehr?
				main_rbpos=rbpos;
				workentry=recentry;
				
				main_working=1; //befehl zum starten geben
				
				
				recentry=recentry->next;
				if (recentry==NULL) //fertig?
				{
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
				printf ("fertig mit warten\n");
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

	if (jack_activate(jack_client)) 
	{
		fprintf(stderr,"Activating client failed.\n");
		exit(1);
	}
}

int main(int argc, char *argv[])
{
	int i;
	
	//init
	recbuf=buf1; workbuf=buf2;
	main_working=0; all_done=0;

	
	samp_rate=44100; bytes=2;
	
	//init header
	strcpy(wavheader+ 0, "RIFF");
	//u32cpy(wavheader+ 4, filelen-8);
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
	//u32cpy(wavheader+40, datalen);

	
	init_jack();
	
	char foo[10]; gets(foo);
	
	flo_start=1;
	
	main_working=0;
	printf("main: entering loop\n");
	while(1)
	{
		while ((main_working==0) && (all_done==0)) usleep(100000); // 0.1 sec
		//TODO: all_done auswerten
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
			printf("main: gotcha. %i - %i [%i - %i]\n",workstart,workend,0,main_rbpos);
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
			
			FILE* f;
			f=fopen("flo1.wav","w");
			
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
		else
		{
			printf ("main: komisch. nur stille aufgenommen...\n"); //TODO handlen!
		}
		
		
		printf("main: work done\n");
		main_working=0;
	}

	return 0;
}

