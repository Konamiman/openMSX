// $Id$

#ifndef __MIXER_HH__
#define __MIXER_HH__

#ifdef DEBUG
//#define DEBUG_MIXER
#endif

#include <SDL.h>
#include <vector>
#include <map>
#include "EmuTime.hh"
#include "IntegerSetting.hh"
#include "BooleanSetting.hh"
#include "SettingListener.hh"
#include "InfoTopic.hh"

namespace openmsx {

class SoundDevice;
class MSXCPU;
class SettingsConfig;
class RealTime;
class CliCommOutput;
class InfoCommand;
class VolumeSetting;

class Mixer : private SettingListener
{
public:
	static const int MAX_VOLUME = 32767;
	enum ChannelMode {
		MONO, MONO_LEFT, MONO_RIGHT, STEREO, NB_MODES
	};

	static Mixer& instance();

	/**
	 * Use this method to register a given sounddevice.
	 *
	 * While registering, the device its setSampleRate() method is
	 * called (see SoundDevice for more info).
	 * After registration the device its updateBuffer() method is
	 * 'regularly' called. It asks the device to fill a buffer with
	 * a certain number of samples. (see SoundDevice for more info)
	 * The maximum number of samples asked for is returned by this
	 * method.
	 */
	int registerSound(SoundDevice* device, short volume, ChannelMode mode);

	/**
	 * Every sounddevice must unregister before it is destructed
	 */
	void unregisterSound(SoundDevice* device);

	/**
	 * Use this method to force an 'early' call to all
	 * updateBuffer() methods.
	 */
	void updateStream(const EmuTime& time);

	/**
	 * This methods (un)locks the audio thread.
	 * You can use this method to delay the call to the SoundDevices
	 * updateBuffer() method. For example, this is usefull if
	 * you are updating a lot of registers and you don't want the
	 * half updated set being used to produce sound
	 */
	void lock();
	void unlock();

	/**
	 * This methods (un)mute the sound.
	 * These methods may be called multiple times, as long as
	 * you never call unmute() more than mute()
	 */
	void mute();
	void unmute();

private:
	Mixer();
	virtual ~Mixer();

	void reInit();
	void updtStrm(int samples);
	static void audioCallbackHelper(void* userdata, Uint8* stream, int len);
	void audioCallback(short* stream);
	void muteHelper(int muteCount);
	
	void updateMasterVolume(int masterVolume);

	// SettingListener
	virtual void update(const SettingLeafNode* setting) throw();

	SoundDevice* getSoundDevice(const string& name);

	bool init;
	int muteCount;

	struct SoundDeviceInfo {
		ChannelMode mode;
		int normalVolume;
		IntegerSetting* volumeSetting;
		EnumSetting<ChannelMode> *modeSetting;
	};
	map<SoundDevice*, SoundDeviceInfo> infos;

	SDL_AudioSpec audioSpec;
	vector<SoundDevice*> devices[NB_MODES];
	vector<int*> buffers;

	short* mixBuffer;
	int samplesLeft;
	int offset;
	EmuTime prevTime;

	MSXCPU& cpu;
	RealTime& realTime;
	SettingsConfig& settingsConfig;
	CliCommOutput& output;
	InfoCommand& infoCommand;

	BooleanSetting muteSetting;
	IntegerSetting masterVolume;
	BooleanSetting& pauseSetting;

	int prevLeft, outLeft;
	int prevRight, outRight;
#ifdef DEBUG_MIXER
	int nbClipped;
#endif

	class SoundDeviceInfoTopic : public InfoTopic {
	public:
		SoundDeviceInfoTopic(Mixer& parent);
		virtual void execute(const vector<string>& tokens,
		                     CommandResult& result) const;
		virtual string help   (const vector<string>& tokens) const;
		virtual void tabCompletion(vector<string>& tokens) const;
	private:
		Mixer& parent;
	} soundDeviceInfo;
};

} // namespace openmsx

#endif //__MIXER_HH__

