#include <stdio.h>
#include <jack/jack.h>
#include <jack/midiport.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define MIDI_EVENT_TYPE_NOTEON 0x90
#define MIDI_EVENT_TYPE_NOTEOFF 0x80
#define MIDI_EVENT_TYPE_POLY_AFTERTOUCH 0xA0
#define MIDI_EVENT_TYPE_CC 0xB0
#define MIDI_EVENT_TYPE_PROGRAM 0xC0
#define MIDI_EVENT_TYPE_AFTERTOUCH 0xD0
#define MIDI_EVENT_TYPE_PITCHBEND 0xE0
#define MIDI_EVENT_TYPE_SYSTEM 0xF0

#define MIDI_MSG_ALL_NOTES_OFF 0x7B

#define MIDI_CC_ALL_NOTES_OFF 123

#define MIDI_TYPE_FROM_BUFFER(x) x[0]&0xF0
#define MIDI_CHANNEL_FROM_BUFFER(x) x[0]&0x0F
#define MIDI_DATA1_FROM_BUFFER(x) x[1]
#define MIDI_DATA2_FROM_BUFFER(x) x[2]

jack_port_t *input_port;
jack_port_t *output_port;

unsigned char*    out_buffer;
void*             input_port_buf;
void*             output_port_buf;
jack_nframes_t    input_event_count;
jack_midi_event_t in_event;

int lastvel = 80;
int* notes;
int notesCount = 0;

int
process (jack_nframes_t nframes, void *arg)

{
/*
	jack_default_audio_sample_t *out = 
                (jack_default_audio_sample_t *) 
                jack_port_get_buffer (output_port, nframes);
	jack_default_audio_sample_t *in = 
                (jack_default_audio_sample_t *) 
                jack_port_get_buffer (input_port, nframes);

	memcpy (out, in, sizeof (jack_default_audio_sample_t) * nframes);
*/

    // Get output port buffer
    output_port_buf = jack_port_get_buffer( output_port, nframes);
    jack_midi_clear_buffer( output_port_buf );


    // Get our midi input
    input_port_buf = jack_port_get_buffer(input_port, nframes);
    input_event_count = jack_midi_get_event_count(input_port_buf);
    
    
    // For each midi input event...
    for (int i=0; i<input_event_count; i++) {

        // Get input event
        jack_midi_event_get(&in_event, input_port_buf, i);

        // Filter
        int type = MIDI_TYPE_FROM_BUFFER( in_event.buffer );
        if (type == MIDI_EVENT_TYPE_NOTEON) {
            int data1 = MIDI_DATA1_FROM_BUFFER( in_event.buffer );
            char found = 0;
            for (int n=0; n<notesCount; n++) {
                if (notes[n] == data1) {
                    found = 1;
                    // Incoming note is one of the notes we want to modify.
                    // Set the incoming note velocity to lastvel.
                    in_event.buffer[2] = lastvel;
                    break;
                }
            }

            if (found == 0) {
                // Incoming note was not one of the notes we want to modify.
                // Use this velocity as lastvel.
                lastvel = MIDI_DATA2_FROM_BUFFER( in_event.buffer );
            }
        }

        // Get output buffer, based on size and time of input event
        out_buffer = jack_midi_event_reserve( output_port_buf, in_event.time, in_event.size);

        memcpy( out_buffer, in_event.buffer, in_event.size );

    }

	
	return 0;      
}

void
error (const char *desc)
{
	fprintf (stderr, "JACK error: %s\n", desc);
}

void
jack_shutdown (void *arg)
{
	exit (1);
}

int
srate (jack_nframes_t nframes, void *arg)

{
	printf ("the sample rate is now %lu/sec\n", nframes);
	return 0;
}

int
main (int argc, char *argv[])
{
        /*KIRBY: Create a JACK client.  
         This is our connection to the JACK daemon.*/

	jack_client_t *client;

	/*KIRBY: A pointer for an array of ports.  Remember, we 
          already saw this being 
	  used to hold the array of available ports
	  See the previous chapter*/

	const char **ports;


	/*KIRBY: This doesn't really need an explanation*/

        if (argc < 3) {
                fprintf (stderr, "usage: oldnotes jack_client_name note1 note2 note3 ... \n");
		return 1;
	}

        notesCount = argc-2;
        notes = malloc( sizeof(int)*notesCount );
        for (int i=0; i<notesCount; i++) {
            notes[i] = strtol(argv[i+2], NULL, 10);
        }




	/* tell the JACK server to call error() whenever it
	   experiences an error.  Notice that this callback is
	   global to this process, not specific to each client.
	
	   This is set here so that it can catch errors in the
	   connection process
	*/
	/*KIRBY: Nuff said.*/
	jack_set_error_function (error);

	/* try to become a client of the JACK server */
	/*KIRBY:  This is where our pointer "client" 
          gets something to point to.  
	  You will notice that the functions called later take a client as 
	  a parameter - this is what we pass.*/
	if ((client = jack_client_new (argv[1])) == 0) {
		fprintf (stderr, "jack server not running?\n");
		return 1;
	}

	/* tell the JACK server to call `process()' whenever
	   there is work to be done.
	*/

	jack_set_process_callback (client, process, 0);

	/* tell the JACK server to call `srate()' whenever
	   the sample rate of the system changes.
	*/

	jack_set_sample_rate_callback (client, srate, 0);

	/* tell the JACK server to call `jack_shutdown()' if
	   it ever shuts down, either entirely, or if it
	   just decides to stop calling us.
	*/

	jack_on_shutdown (client, jack_shutdown, 0);

	/* display the current sample rate. once the client is activated 
	   (see below), you should rely on your own sample rate
	   callback (see above) for this value.
	*/
	printf ("engine sample rate: %lu\n", jack_get_sample_rate (client));

	/* create two ports */

	input_port = jack_port_register (client, "input", 
                     JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	output_port = jack_port_register (client, "output", 
                     JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

	/* tell the JACK server that we are ready to roll */
	/*KIRBY: So, once we are in a position to start 
          doing whatever it is we do, this is how we announce that.*/
	if (jack_activate (client)) {
		fprintf (stderr, "cannot activate client");
		return 1;
	}

	/* connect the ports. Note: you can't do this before
	   the client is activated, because we can't allow
	   connections to be made to clients that aren't
	   running.
	*/
	/*KIRBY: We already discussed this.  Go back a chapter 
                 if you missed it.*/

//	if ((ports = jack_get_ports (client, NULL, NULL,
//                               JackPortIsPhysical|JackPortIsOutput)) == NULL) {
//		fprintf(stderr, "Cannot find any physical capture ports\n");
//		exit(1);
//	}

//	if (jack_connect (client, ports[0], jack_port_name (input_port))) {
//		fprintf (stderr, "cannot connect input ports\n");
//	}

//	free (ports);
	
//	if ((ports = jack_get_ports (client, NULL, NULL,
//                               JackPortIsPhysical|JackPortIsInput)) == NULL) {
//		fprintf(stderr, "Cannot find any physical playback ports\n");
//		exit(1);
//	}
	
	/*KIRBY: This is our modified bit.  Groovy, eh?*/
	
//	int i=0;
//	while(ports[i]!=NULL){
//	  if (jack_connect (client, jack_port_name (output_port), ports[i])) {
//	    fprintf (stderr, "cannot connect output ports\n");
//	  }
//	  i++;
//	}

//	free (ports);

	/* Since this is just a toy, run for a few seconds, then finish */
	/*KIRBY: We changed that, too.  Now we run until we get killed.*/
	for(;;)
	  sleep (1);

	/*KIRBY: Close the client*/
	jack_client_close (client);
	exit (0);
}
