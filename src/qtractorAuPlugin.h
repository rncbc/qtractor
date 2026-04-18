// qtractorAuPlugin.h
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.
   Copyright (C) 2024, Nebula Audio. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#ifndef __qtractorAuPlugin_h
#define __qtractorAuPlugin_h

#include "qtractorPlugin.h"

#ifdef __APPLE__
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#endif

#include <QString>
#include <QList>


// Forward declarations.
class qtractorPluginFactory;
class qtractorPluginFile;


//----------------------------------------------------------------------
// class qtractorAuPlugin -- AudioUnit plugin instance.
//

class qtractorAuPlugin : public qtractorPlugin
{
public:

	// Constructor.
	qtractorAuPlugin(qtractorPluginFactory *pFactory,
		const QString& sName = QString());

	// Destructor.
	virtual ~qtractorAuPlugin();

	// Plugin type accessor.
	Type type() const override { return AudioUnit; }

	// Plugin initialization method.
	bool init() override;

	// Plugin activation methods.
	bool activate() override;
	void deactivate() override;

	// Parameter value accessors.
	float parameter(unsigned int iParam) const override;
	void setParameter(unsigned int iParam, float fValue) override;

	// Preset list accessors.
	int presetCurrent() const override;
	void setPresetCurrent(int iPreset) override;

	const QStringList& presets() const override { return m_presets; }

	// Program list accessors.
	int programCurrent() const override;
	void setProgramCurrent(int iProgram) override;

	const QStringList& programs() const override { return m_programs; }

	// MIDI controller/observer mapper predicate.
	bool isMidiController(unsigned int iParam) const override;

	// Plugin buffer size change notification.
	void bufferSizeChanged(unsigned int nframes) override;

	// Audio processor executive.
	void process(unsigned int nframes,
		float **ppInputs, unsigned short iInputs,
		float **ppOutputs, unsigned short iOutputs) override;

#ifdef __APPLE__
	// AudioUnit accessor.
	AudioUnit audioUnit() const { return m_audioUnit; }

	// AudioComponent description accessor.
	const AudioComponentDescription& componentDescription() const
		{ return m_componentDesc; }
#endif

	// Static factory methods.
	static bool scan(const QString& sPath,
		QList<qtractorPluginFile *>& files);

protected:

	// Parameter name accessor.
	QString parameterName(unsigned int iParam) const override;

	// Parameter unit accessor.
	QString parameterUnit(unsigned int iParam) const override;

	// Parameter range accessors.
	float parameterMin(unsigned int iParam) const override;
	float parameterMax(unsigned int iParam) const override;
	float parameterDefault(unsigned int iParam) const override;

private:

#ifdef __APPLE__
	// AudioUnit render callback.
	static OSStatus renderCallback(
		void *inRefCon,
		AudioUnitRenderActionFlags *ioActionFlags,
		const AudioTimeStamp *inTimeStamp,
		UInt32 inBusNumber,
		UInt32 inNumberFrames,
		AudioBufferList *ioData);
#endif

	// Instance variables.
#ifdef __APPLE__
	AudioUnit              m_audioUnit;
	AudioComponentDescription m_componentDesc;
	AUParameterListenerRef m_paramListener;
#endif

	QStringList m_presets;
	QStringList m_programs;

	// Parameter cache.
	QList<float> m_paramValues;
};


#endif  // __qtractorAuPlugin_h


// end of qtractorAuPlugin.h

