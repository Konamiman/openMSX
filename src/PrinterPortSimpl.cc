// $Id$

#include "PrinterPortSimpl.hh"
#include "DACSound8U.hh"
#include "DeviceConfig.hh"
#include "XMLElement.hh"
#include "serialize.hh"

using std::unique_ptr;

namespace openmsx {

PrinterPortSimpl::PrinterPortSimpl(const HardwareConfig& hwConf_)
	: hwConf(hwConf_)
{
}

bool PrinterPortSimpl::getStatus(EmuTime::param /*time*/)
{
	return true; // TODO check
}

void PrinterPortSimpl::setStrobe(bool /*strobe*/, EmuTime::param /*time*/)
{
	// ignore strobe // TODO check
}

void PrinterPortSimpl::writeData(byte data, EmuTime::param time)
{
	dac->writeDAC(data, time);
}

static XMLElement createXML()
{
	XMLElement xml("simpl");
	unique_ptr<XMLElement> sound(new XMLElement("sound"));
	sound->addChild(unique_ptr<XMLElement>(new XMLElement("volume", "12000")));
	xml.addChild(std::move(sound));
	return xml;
}

void PrinterPortSimpl::createDAC()
{
	static XMLElement xml = createXML();
	dac.reset(new DACSound8U("simpl", getDescription(),
	                         DeviceConfig(hwConf, xml)));
}

void PrinterPortSimpl::plugHelper(Connector& /*connector*/, EmuTime::param /*time*/)
{
	createDAC();
}

void PrinterPortSimpl::unplugHelper(EmuTime::param /*time*/)
{
	dac.reset();
}

const std::string& PrinterPortSimpl::getName() const
{
	static const std::string name("simpl");
	return name;
}

string_ref PrinterPortSimpl::getDescription() const
{
	return "Play samples via your printer port.";
}

template<typename Archive>
void PrinterPortSimpl::serialize(Archive& ar, unsigned /*version*/)
{
	if (isPluggedIn()) {
		if (ar.isLoader()) {
			createDAC();
		}
		ar.serialize("dac", *dac);
	}
}
INSTANTIATE_SERIALIZE_METHODS(PrinterPortSimpl);
REGISTER_POLYMORPHIC_INITIALIZER(Pluggable, PrinterPortSimpl, "PrinterPortSimpl");

} // namespace openmsx
