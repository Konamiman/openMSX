// $Id$

#if defined(_WIN32)
#include "MidiInWindows.hh"
#include "MidiInConnector.hh"
#include "PluggingController.hh"
#include "PlugException.hh"
#include "EventDistributor.hh"
#include "Scheduler.hh"
#include "serialize.hh"
#include <cstring>
#include <cerrno>
#include "Midi_w32.hh"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <sys/types.h>
#include <sys/stat.h>

using std::string;

namespace openmsx {

void MidiInWindows::registerAll(EventDistributor& eventDistributor,
                               Scheduler& scheduler,
                               PluggingController& controller)
{
	w32_midiInInit();
	unsigned devnum = w32_midiInGetVFNsNum();
	for (unsigned i = 0 ; i <devnum; ++i) {
		controller.registerPluggable(std::unique_ptr<Pluggable>(
			new MidiInWindows(eventDistributor, scheduler, i)));
	}
}


MidiInWindows::MidiInWindows(EventDistributor& eventDistributor_,
                           Scheduler& scheduler_, unsigned num)
	: eventDistributor(eventDistributor_), scheduler(scheduler_)
	, thread(this), devidx(unsigned(-1)), lock(1)
{
	name = w32_midiInGetVFN(num);
	desc = w32_midiInGetRDN(num);

	eventDistributor.registerEventListener(OPENMSX_MIDI_IN_WINDOWS_EVENT, *this);
}

MidiInWindows::~MidiInWindows()
{
	eventDistributor.unregisterEventListener(OPENMSX_MIDI_IN_WINDOWS_EVENT, *this);

	//w32_midiInClean(); // TODO
}

// Pluggable
void MidiInWindows::plugHelper(Connector& connector_, EmuTime::param /*time*/)
{
	devidx = w32_midiInOpen(name.c_str(), thrdid);
	if (devidx == unsigned(-1)) {
		throw PlugException("Failed to open " + name);
	}

	MidiInConnector& midiConnector = static_cast<MidiInConnector&>(connector_);
	midiConnector.setDataBits(SerialDataInterface::DATA_8); // 8 data bits
	midiConnector.setStopBits(SerialDataInterface::STOP_1); // 1 stop bit
	midiConnector.setParityBit(false, SerialDataInterface::EVEN); // no parity

	setConnector(&connector_); // base class will do this in a moment,
	                           // but thread already needs it
	thread.start();
}

void MidiInWindows::unplugHelper(EmuTime::param /*time*/)
{
	ScopedLock l(lock);
	thread.stop();
	if (devidx != unsigned(-1)) {
		w32_midiInClose(devidx);
		devidx = unsigned(-1);
	}
}

const string& MidiInWindows::getName() const
{
	return name;
}

string_ref MidiInWindows::getDescription() const
{
	return desc;
}

void MidiInWindows::procLongMsg(LPMIDIHDR p)
{
	if (p->dwBytesRecorded) {
		ScopedLock l(lock);
		for (unsigned i = 0; i < p->dwBytesRecorded; ++i) {
			queue.push_back(p->lpData[i]);
		}
		eventDistributor.distributeEvent(
			new SimpleEvent(OPENMSX_MIDI_IN_WINDOWS_EVENT));
	}
}

void MidiInWindows::procShortMsg(DWORD param)
{
	int num;
	switch (param & 0xF0) {
		case 0x80: case 0x90: case 0xA0: case 0xB0: case 0xE0:
			num = 3; break;
		case 0xC0: case 0xD0:
			num = 2; break;
		default:
			num = 1; break;
	}
	ScopedLock l(lock);
	while (num--) {
		queue.push_back(param & 0xFF);
		param >>= 8;
	}
	eventDistributor.distributeEvent(
		new SimpleEvent(OPENMSX_MIDI_IN_WINDOWS_EVENT));
}

// Runnable
void MidiInWindows::run()
{
	assert(isPluggedIn());
	thrdid = SDL_ThreadID();

	MSG msg;
	bool fexit = false;
	int gmer;
	while (!fexit) {
		gmer = GetMessage(&msg, NULL, 0, 0);
		if (gmer == 0 || gmer == -1) {
			break;
		}
		switch (msg.message) {
			case MM_MIM_OPEN:
				break;
			case MM_MIM_DATA:
			case MM_MIM_MOREDATA:
				procShortMsg(DWORD(msg.lParam));
				break;
			case MM_MIM_LONGDATA:
				procLongMsg(reinterpret_cast<LPMIDIHDR>(msg.lParam));
				break;
			case MM_MIM_ERROR:
			case MM_MIM_LONGERROR:
			case MM_MIM_CLOSE:
				fexit = true;
				break;
			default:
				break;
		}
	}
	thrdid = 0;
}

// MidiInDevice
void MidiInWindows::signal(EmuTime::param time)
{
	MidiInConnector* connector = static_cast<MidiInConnector*>(getConnector());
	if (!connector->acceptsData()) {
		queue.clear();
		return;
	}
	if (!connector->ready()) return;

	ScopedLock l(lock);
	if (queue.empty()) return;
	byte data = queue.front();
	queue.pop_front();
	connector->recvByte(data, time);
}

// EventListener
int MidiInWindows::signalEvent(const shared_ptr<const Event>& /*event*/)
{
	if (isPluggedIn()) {
		signal(scheduler.getCurrentTime());
	} else {
		ScopedLock l(lock);
		queue.empty();
	}
	return 0;
}

template<typename Archive>
void MidiInWindows::serialize(Archive& /*ar*/, unsigned /*version*/)
{
	// don't restore this after loadstate
}
INSTANTIATE_SERIALIZE_METHODS(MidiInWindows);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, MidiInWindows, "MidiInWindows");

} // namespace openmsx

#endif // defined(_WIN32)
