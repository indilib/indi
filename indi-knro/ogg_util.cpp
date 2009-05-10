#include "ogg_util.h"
#include <indidevapi.h>
#include <stdlib.h>

OggFile::OggFile()
{
    init_success = false;

    // Initialize the OpenAL library
    /*if (alutInit(NULL, NULL) == AL_FALSE)
    {
	IDLog("alutInit Failed!\n");
	alutExit();
	return;
    }*/

    alutInit(NULL, NULL);

    // Create sound buffer and source
    alGenBuffers(1, &bufferID);
    alGenSources(1, &sourceID);

    // Set the source and listener to the same location
    alListener3f(AL_POSITION, 0.0f, 0.0f, 0.0f);
    alSource3f(sourceID, AL_POSITION, 0.0f, 0.0f, 0.0f);

    ogg_looping = false;
    init_success = true;
}

OggFile::~OggFile()
{
    if (!init_success)
	return;

    // Clean up sound buffer and source
    alDeleteBuffers(1, &bufferID);
    alDeleteSources(1, &sourceID);

    // Clean up the OpenAL library
    alutExit();
}

void OggFile::load_file(const string & fileName)
{
    if (!init_success)
	return;

    // Load the OGG file into memory
    LoadOGG(fileName.c_str(), bufferData, format, freq);

    // Upload sound data to buffer
    alBufferData(bufferID, format, &bufferData[0], static_cast<ALsizei>(bufferData.size()), freq);

    // Attach sound buffer to source
    alSourcei(sourceID, AL_BUFFER, bufferID);
}

void OggFile::play()
{
    if (!init_success)
	return;

  // Make it loop
  alSourcei (sourceID, AL_LOOPING,  ogg_looping ? AL_TRUE : AL_FALSE );

  alSourcePlay(sourceID);
}

void OggFile::stop()
{
   if (!init_success)
	return;

  alSourceStop(sourceID);
}

bool OggFile::is_playing()
{
     if (!init_success)
	return false;

    ALint state;                            // The state of the sound source

    alGetSourcei(sourceID, AL_SOURCE_STATE, &state);

    return (state == AL_PLAYING);
}

// This function loads a .ogg file into a memory buffer and returns
// the format and frequency.
void OggFile::LoadOGG(const char *fileName, vector<char> &buffer, ALenum &format, ALsizei &freq)
{
     if (!init_success)
	return;

    int endian = 0;                         // 0 for Little-Endian, 1 for Big-Endian
    int bitStream;
    long bytes;
    char array[BUFFER_SIZE];                // Local fixed size array
    FILE *f;

    // Open for binary reading
    f = fopen(fileName, "rb");

    if (f == NULL)
        {
        cerr << "Cannot open " << fileName << " for reading..." << endl;
        exit(-1);
        }
    // end if

    vorbis_info *pInfo;
    OggVorbis_File oggFile;

    // Try opening the given file
    if (ov_open(f, &oggFile, NULL, 0) != 0)
        {
        cerr << "Error opening " << fileName << " for decoding..." << endl;
        exit(-1);
        }
    // end if

    // Get some information about the OGG file
    pInfo = ov_info(&oggFile, -1);

    // Check the number of channels... always use 16-bit samples
    if (pInfo->channels == 1)
        format = AL_FORMAT_MONO16;
    else
        format = AL_FORMAT_STEREO16;
    // end if

    // The frequency of the sampling rate
    freq = pInfo->rate;

    // Keep reading until all is read
    do
        {
        // Read up to a buffer's worth of decoded sound data
        bytes = ov_read(&oggFile, array, BUFFER_SIZE, endian, 2, 1, &bitStream);

        if (bytes < 0)
            {
            ov_clear(&oggFile);
            cerr << "Error decoding " << fileName << "..." << endl;
            exit(-1);
            }
        // end if

        // Append to end of buffer
        buffer.insert(buffer.end(), array, array + bytes);
        }
    while (bytes > 0);

    // Clean up!
    ov_clear(&oggFile);
}
// end of LoadOGG
