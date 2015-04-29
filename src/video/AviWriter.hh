// Code based on DOSBox-0.65

#ifndef AVIWRITER_HH
#define AVIWRITER_HH

#include "ZMBVEncoder.hh"
#include "File.hh"
#include "endian.hh"
#include <vector>
#include <memory>

namespace openmsx {

class Filename;
class FrameSource;

class AviWriter
{
public:
	AviWriter(const Filename& filename, unsigned width, unsigned height,
	          unsigned bpp, unsigned channels, unsigned freq);
	~AviWriter();
	void addFrame(FrameSource* frame, unsigned samples, short* sampleData);
	void setFps(double fps_) { fps = fps_; }

private:
	void addAviChunk(const char* tag, unsigned size, void* data, unsigned flags);

	File file;
	ZMBVEncoder codec;
	std::vector<Endian::L32> index;

	double fps;
	const unsigned width;
	const unsigned height;
	const unsigned channels;
	const unsigned audiorate;

	unsigned frames;
	unsigned audiowritten;
	unsigned written;
};

} // namespace openmsx

#endif
