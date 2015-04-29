// Code based on DOSBox-0.65

#include "AviWriter.hh"
#include "FileOperations.hh"
#include "MSXException.hh"
#include "memory.hh"
#include "build-info.hh"
#include "Version.hh"
#include "cstdiop.hh" // for snprintf
#include <cstring>
#include <ctime>
#include <cassert>

namespace openmsx {

static const unsigned AVI_HEADER_SIZE = 500;

AviWriter::AviWriter(const Filename& filename, unsigned width_,
                     unsigned height_, unsigned bpp, unsigned channels_,
		     unsigned freq_)
	: file(filename, "wb")
	, codec(width_, height_, bpp)
	, fps(0.0) // will be filled in later
	, width(width_)
	, height(height_)
	, channels(channels_)
	, audiorate(freq_)
{
	char dummy[AVI_HEADER_SIZE];
	memset(dummy, 0, sizeof(dummy));
	file.write(dummy, sizeof(dummy));

	index.resize(2);

	frames = 0;
	written = 0;
	audiowritten = 0;
}

AviWriter::~AviWriter()
{
	if (written == 0) {
		// no data written yet (a recording less than one video frame)
		std::string filename = file.getURL();
		file.close(); // close file (needed for windows?)
		FileOperations::unlink(filename);
		return;
	}
	assert(fps != 0.0); // a decent fps should have been set

	// Possible cleanup: use structs for the different headers, that
	// also allows to use the aligned versions of the Endian routines.
	unsigned char avi_header[AVI_HEADER_SIZE];
	memset(&avi_header, 0, sizeof(avi_header));
	unsigned header_pos = 0;

#define AVIOUT4(_S_) memcpy(&avi_header[header_pos],_S_,4);header_pos+=4;
#define AVIOUTw(_S_) Endian::write_UA_L16(&avi_header[header_pos], _S_);header_pos+=2;
#define AVIOUTd(_S_) Endian::write_UA_L32(&avi_header[header_pos], _S_);header_pos+=4;
#define AVIOUTs(_S_) memcpy(&avi_header[header_pos],_S_,strlen(_S_)+1);header_pos+=(strlen(_S_)+1 + 1) & ~1;

	bool hasAudio = audiorate != 0;

	// write avi header
	AVIOUT4("RIFF");                    // Riff header
	AVIOUTd(AVI_HEADER_SIZE + written - 8 + unsigned(index.size() * sizeof(Endian::L32)));
	AVIOUT4("AVI ");
	AVIOUT4("LIST");                    // List header
	unsigned main_list = header_pos;
	AVIOUTd(0);                         // size of list, filled in later
	AVIOUT4("hdrl");

	AVIOUT4("avih");
	AVIOUTd(56);                        // # of bytes to follow
	AVIOUTd(unsigned(1000000 / fps));   // Microseconds per frame
	AVIOUTd(0);
	AVIOUTd(0);                         // PaddingGranularity (whatever that might be)
	AVIOUTd(0x110);                     // Flags,0x10 has index, 0x100 interleaved
	AVIOUTd(frames);                    // TotalFrames
	AVIOUTd(0);                         // InitialFrames
	AVIOUTd(hasAudio? 2 : 1);           // Stream count
	AVIOUTd(0);                         // SuggestedBufferSize
	AVIOUTd(width);                     // Width
	AVIOUTd(height);                    // Height
	AVIOUTd(0);                         // TimeScale:  Unit used to measure time
	AVIOUTd(0);                         // DataRate:   Data rate of playback
	AVIOUTd(0);                         // StartTime:  Starting time of AVI data
	AVIOUTd(0);                         // DataLength: Size of AVI data chunk

	// Video stream list
	AVIOUT4("LIST");
	AVIOUTd(4 + 8 + 56 + 8 + 40);       // Size of the list
	AVIOUT4("strl");
	// video stream header
	AVIOUT4("strh");
	AVIOUTd(56);                        // # of bytes to follow
	AVIOUT4("vids");                    // Type
	AVIOUT4(ZMBVEncoder::CODEC_4CC);    // Handler
	AVIOUTd(0);                         // Flags
	AVIOUTd(0);                         // Reserved, MS says: wPriority, wLanguage
	AVIOUTd(0);                         // InitialFrames
	AVIOUTd(1000000);                   // Scale
	AVIOUTd(unsigned(1000000 * fps));   // Rate: Rate/Scale == samples/second
	AVIOUTd(0);                         // Start
	AVIOUTd(frames);                    // Length
	AVIOUTd(0);                         // SuggestedBufferSize
	AVIOUTd(unsigned(~0));              // Quality
	AVIOUTd(0);                         // SampleSize
	AVIOUTd(0);                         // Frame
	AVIOUTd(0);                         // Frame
	// The video stream format
	AVIOUT4("strf");
	AVIOUTd(40);                        // # of bytes to follow
	AVIOUTd(40);                        // Size
	AVIOUTd(width);                     // Width
	AVIOUTd(height);                    // Height
	AVIOUTd(0);
	AVIOUT4(ZMBVEncoder::CODEC_4CC);    // Compression
	AVIOUTd(width * height * 4);        // SizeImage (in bytes?)
	AVIOUTd(0);                         // XPelsPerMeter
	AVIOUTd(0);                         // YPelsPerMeter
	AVIOUTd(0);                         // ClrUsed: Number of colors used
	AVIOUTd(0);                         // ClrImportant: Number of colors important

	if (hasAudio) {
		// 1 fragment is 1 (for mono) or 2 (for stereo) samples
		unsigned bitsPerSample = 16;
		unsigned bytesPerSample = bitsPerSample / 8;
		unsigned bytesPerFragment = bytesPerSample * channels;
		unsigned bytesPerSecond = audiorate * bytesPerFragment;
		unsigned fragments = audiowritten / channels;

		// Audio stream list
		AVIOUT4("LIST");
		AVIOUTd(4 + 8 + 56 + 8 + 16);// Length of list in bytes
		AVIOUT4("strl");
		// The audio stream header
		AVIOUT4("strh");
		AVIOUTd(56);                // # of bytes to follow
		AVIOUT4("auds");
		AVIOUTd(0);                 // Format (Optionally)
		AVIOUTd(0);                 // Flags
		AVIOUTd(0);                 // Reserved, MS says: wPriority, wLanguage
		AVIOUTd(0);                 // InitialFrames
		// Rate/Scale should be number of samples per second!
		AVIOUTd(bytesPerFragment);  // Scale
		AVIOUTd(bytesPerSecond);    // Rate, actual rate is scale/rate
		AVIOUTd(0);                 // Start
		AVIOUTd(fragments);         // Length
		AVIOUTd(0);                 // SuggestedBufferSize
		AVIOUTd(unsigned(~0));      // Quality
		AVIOUTd(bytesPerFragment);  // SampleSize (should be the same as BlockAlign)
		AVIOUTd(0);                 // Frame
		AVIOUTd(0);                 // Frame
		// The audio stream format
		AVIOUT4("strf");
		AVIOUTd(16);                // # of bytes to follow
		AVIOUTw(1);                 // Format, WAVE_ZMBV_FORMAT_PCM
		AVIOUTw(channels);          // Number of channels
		AVIOUTd(audiorate);         // SamplesPerSec
		AVIOUTd(bytesPerSecond);    // AvgBytesPerSec
		AVIOUTw(bytesPerFragment);  // BlockAlign: for PCM: nChannels * BitsPerSaple / 8
		AVIOUTw(bitsPerSample);     // BitsPerSample
	}

	std::string versionStr = Version::full();

	// The standard snprintf() function does always zero-terminate the
	// output it writes. Though windows doesn't have a snprintf() function,
	// instead we #define snprintf to _snprintf and the latter does NOT
	// properly zero-terminate. See also
	//   http://stackoverflow.com/questions/7706936/is-snprintf-always-null-terminating
	char dateStr[11];
	time_t t = time(nullptr);
	struct tm *tm = localtime(&t);
	snprintf(dateStr, 11, "%04d-%02d-%02d", 1900 + tm->tm_year,
		 tm->tm_mon + 1, tm->tm_mday);
	dateStr[10] = 0; // only required on windows

	AVIOUT4("LIST");
	AVIOUTd(4 // list type
		+ (4 + 4 + ((versionStr.size() + 1 + 1) & ~1)) // 1st chunk
		+ (4 + 4 + ((strlen(dateStr  ) + 1 + 1) & ~1)) // 2nd chunk
		); // size of the list
	AVIOUT4("INFO");
	AVIOUT4("ISFT");
	AVIOUTd(unsigned(versionStr.size()) + 1); // # of bytes to follow
	AVIOUTs(versionStr.c_str());
	AVIOUT4("ICRD");
	AVIOUTd(unsigned(strlen(dateStr)) + 1); // # of bytes to follow
	AVIOUTs(dateStr);
	// TODO: add artist (IART), comments (ICMT), name (INAM), etc.
	// use a loop over chunks (type + string) to create the above bytes in
	// a much nicer way

	// Finish stream list, i.e. put number of bytes in the list to proper pos
	int nmain = header_pos - main_list - 4;
	int njunk = AVI_HEADER_SIZE - 8 - 12 - header_pos;
	assert(njunk > 0); // increase AVI_HEADER_SIZE if this occurs
	AVIOUT4("JUNK");
	AVIOUTd(njunk);
	// Fix the size of the main list
	header_pos = main_list;
	AVIOUTd(nmain);
	header_pos = AVI_HEADER_SIZE - 12;

	AVIOUT4("LIST");
	AVIOUTd(written + 4);               // Length of list in bytes
	AVIOUT4("movi");

	try {
		// First add the index table to the end
		unsigned idxSize = unsigned(index.size()) * sizeof(Endian::L32);
		memcpy(&index[0], "idx1", 4);
		index[1] = idxSize - 8;
		file.write(&index[0], idxSize);
		file.seek(0);
		file.write(&avi_header, AVI_HEADER_SIZE);
	} catch (MSXException&) {
		// can't throw from destructor
	}
}

void AviWriter::addAviChunk(const char* tag, unsigned size, void* data, unsigned flags)
{
	struct {
		char t[4];
		Endian::L32 s;
	} chunk;
	memcpy(chunk.t, tag, sizeof(chunk.t));
	chunk.s = size;
	file.write(&chunk, sizeof(chunk));

	unsigned writesize = (size + 1) & ~1;
	file.write(data, writesize);
	unsigned pos = written + 4;
	written += writesize + 8;

	size_t idxSize = index.size();
	index.resize(idxSize + 4);
	memcpy(&index[idxSize], tag, sizeof(Endian::L32));
	index[idxSize + 1] = flags;
	index[idxSize + 2] = pos;
	index[idxSize + 3] = size;
}

void AviWriter::addFrame(FrameSource* frame, unsigned samples, short* sampleData)
{
	bool keyFrame = (frames++ % 300 == 0);
	void* buffer;
	unsigned size;
	codec.compressFrame(keyFrame, frame, buffer, size);
	addAviChunk("00dc", size, buffer, keyFrame ? 0x10 : 0x0);

	if (samples) {
		assert((samples % channels) == 0);
		assert(audiorate != 0);
		if (OPENMSX_BIGENDIAN) {
			// See comment in WavWriter::write()
			//VLA(Endian::L16, buf, samples); // doesn't work in clang
			//std::vector<Endian::L16> buf(sampleData, sampleData + samples); // needs c++11
			std::vector<Endian::L16> buf(samples);
			for (unsigned i = 0; i < samples; ++i) {
				buf[i] = sampleData[i];
			}
			addAviChunk("01wb", samples * sizeof(short), buf.data(), 0);
		} else {
			addAviChunk("01wb", samples * sizeof(short), sampleData, 0);
		}
		audiowritten += samples;
	}
}

} // namespace openmsx
