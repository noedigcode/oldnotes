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

int process (jack_nframes_t nframes, void *arg)

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

void error (const char *desc)
{
    fprintf (stderr, "JACK error: %s\n", desc);
}

void jack_shutdown (void *arg)
{
    exit (1);
}

int srate (jack_nframes_t nframes, void *arg)

{
    //printf ("The sample rate is now %d/sec\n", nframes);
    return 0;
}

int main (int argc, char *argv[])
{
    jack_client_t *client;
    
    const char **ports;
    
    if (argc < 3) {
        fprintf (stderr, "Usage: oldnotes jack_client_name note1 note2 note3 ... \n");
        return 1;
    }

    notesCount = argc-2;
    notes = malloc( sizeof(int)*notesCount );
    for (int i=0; i<notesCount; i++) {
        notes[i] = strtol(argv[i+2], NULL, 10);
    }
    
    jack_set_error_function (error);

    if ((client = jack_client_open(argv[1], JackUseExactName, NULL)) == 0) {
        fprintf (stderr, "Cannot open new JACK client.\n");
        return 1;
    }

    jack_set_process_callback (client, process, 0);

    jack_set_sample_rate_callback (client, srate, 0);

    jack_on_shutdown (client, jack_shutdown, 0);

    //printf ("Engine sample rate: %lu\n", jack_get_sample_rate (client));

    /* create two ports */
    input_port = jack_port_register (client, "input", 
                     JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
    output_port = jack_port_register (client, "output", 
                     JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);

    if (jack_activate (client)) {
        fprintf (stderr, "Cannot activate client");
        return 1;
    }

    /* Run until killed. */
    for(;;)
      sleep (1);

    jack_client_close (client);
    exit (0);
}
