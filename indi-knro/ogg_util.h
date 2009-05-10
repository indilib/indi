#ifndef OGG_UTIL_H
#define OGG_UTIL_H

#include <al.h>
#include <alut.h>
#include <vorbis/vorbisfile.h>
#include <cstdio>
#include <iostream>
#include <vector>

#define BUFFER_SIZE     32768       // 32 KB buffers

using namespace std;

class OggFile
{
  public:
	OggFile();
	~OggFile();

	void load_file(const string & fileName);
	void play();
	void stop();

	bool is_playing();
	inline void set_looping(bool toLoop) { ogg_looping = toLoop; }
	inline bool is_looping() { return ogg_looping; }

  private:

	// This function loads a .ogg file into a memory buffer and returns
	// the format and frequency.
    void LoadOGG(const char *fileName, vector<char> &buffer, ALenum &format, ALsizei &freq);

    ALuint bufferID;                        // The OpenAL sound buffer ID
    ALuint sourceID;                        // The OpenAL sound source
    ALenum format;                          // The sound data format
    ALsizei freq;                           // The frequency of the sound data
    vector<char> bufferData;                // The sound buffer data from file

    bool ogg_looping;
    bool init_success;

};

#endif
