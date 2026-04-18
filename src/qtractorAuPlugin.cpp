// qtractorAuPlugin.cpp
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

#include "qtractorAbout.h"
#include "qtractorAuPlugin.h"
#include "qtractorPluginFactory.h"

#ifdef __APPLE__
#include <AudioUnit/AudioUnit.h>
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include <QDebug>


//----------------------------------------------------------------------
// class qtractorAuPlugin -- AudioUnit plugin instance.
//

// Constructor.
qtractorAuPlugin::qtractorAuPlugin(qtractorPluginFactory *pFactory,
	const QString& sName)
	: qtractorPlugin(pFactory, sName),
#ifdef __APPLE__
	m_audioUnit(nullptr),
	m_paramListener(nullptr),
#endif
	m_paramValues()
{
	memset(&m_componentDesc, 0, sizeof(m_componentDesc));
}


// Destructor.
qtractorAuPlugin::~qtractorAuPlugin()
{
	deactivate();

#ifdef __APPLE__
	if (m_paramListener) {
		AUParameterListenerRemove(m_paramListener);
		m_paramListener = nullptr;
	}

	if (m_audioUnit) {
		AudioComponentInstanceDispose(m_audioUnit);
		m_audioUnit = nullptr;
	}
#endif
}


// Plugin initialization method.
bool qtractorAuPlugin::init()
{
#ifdef __APPLE__
	if (!m_audioUnit) return false;

	OSStatus status;

	// Get parameter list
	UInt32 size = 0;
	status = AudioUnitGetPropertyInfo(m_audioUnit,
		kAudioUnitProperty_ParameterList,
		kAudioUnitScope_Global, 0, &size, nullptr);
	
	if (status == noErr && size > 0) {
		int numParams = size / sizeof(AudioUnitParameterID);
		
		for (int i = 0; i < numParams; ++i) {
			AudioUnitParameterID paramID = i;
			
			// Get parameter info
			AUParameterInfo paramInfo;
			size = sizeof(paramInfo);
			status = AudioUnitGetProperty(m_audioUnit,
				kAudioUnitProperty_ParameterInfo,
				kAudioUnitScope_Global, paramID,
				&paramInfo, &size);
			
			if (status == noErr) {
				// Cache parameter value
				Float32 value = 0.0f;
				status = AudioUnitGetParameter(m_audioUnit,
					paramID, kAudioUnitScope_Global, 0, &value);
				
				if (status == noErr) {
					m_paramValues.append(value);
				} else {
					m_paramValues.append(0.0f);
				}
			}
		}
	}

	// Setup parameter listener
	status = AUParameterListenerCreate(nullptr, nullptr, &m_paramListener);
	if (status != noErr) {
		qWarning("qtractorAuPlugin::init(): Cannot create parameter listener");
	}

#endif // __APPLE__

	return true;
}


// Plugin activation methods.
bool qtractorAuPlugin::activate()
{
#ifdef __APPLE__
	if (!m_audioUnit) return false;

	OSStatus status = AudioUnitInitialize(m_audioUnit);
	if (status != noErr) {
		qWarning("qtractorAuPlugin::activate(): Cannot initialize AudioUnit");
		return false;
	}

#endif // __APPLE__

	return true;
}

void qtractorAuPlugin::deactivate()
{
#ifdef __APPLE__
	if (m_audioUnit) {
		AudioUnitUninitialize(m_audioUnit);
	}
#endif
}


// Parameter value accessors.
float qtractorAuPlugin::parameter(unsigned int iParam) const
{
	if (iParam < static_cast<unsigned int>(m_paramValues.size())) {
		return m_paramValues[iParam];
	}
	return 0.0f;
}

void qtractorAuPlugin::setParameter(unsigned int iParam, float fValue)
{
#ifdef __APPLE__
	if (!m_audioUnit || iParam >= static_cast<unsigned int>(m_paramValues.size()))
		return;

	OSStatus status = AudioUnitSetParameter(m_audioUnit,
		iParam, kAudioUnitScope_Global, 0, fValue, 0);
	
	if (status == noErr) {
		m_paramValues[iParam] = fValue;
	}
#else
	Q_UNUSED(iParam);
	Q_UNUSED(fValue);
#endif
}


// Preset list accessors.
int qtractorAuPlugin::presetCurrent() const
{
	return -1; // Not implemented
}

void qtractorAuPlugin::setPresetCurrent(int iPreset)
{
	Q_UNUSED(iPreset);
}


// Program list accessors.
int qtractorAuPlugin::programCurrent() const
{
	return -1; // Not implemented
}

void qtractorAuPlugin::setProgramCurrent(int iProgram)
{
	Q_UNUSED(iProgram);
}


// MIDI controller/observer mapper predicate.
bool qtractorAuPlugin::isMidiController(unsigned int iParam) const
{
	Q_UNUSED(iParam);
	return false;
}


// Plugin buffer size change notification.
void qtractorAuPlugin::bufferSizeChanged(unsigned int nframes)
{
#ifdef __APPLE__
	if (!m_audioUnit) return;

	// Set maximum frames per slice
	OSStatus status = AudioUnitSetProperty(m_audioUnit,
		kAudioUnitProperty_MaximumFramesPerSlice,
		kAudioUnitScope_Global, 0,
		&nframes, sizeof(nframes));
	
	if (status != noErr) {
		qWarning("qtractorAuPlugin::bufferSizeChanged(): Cannot set max frames");
	}
#else
	Q_UNUSED(nframes);
#endif
}


// Audio processor executive.
void qtractorAuPlugin::process(unsigned int nframes,
	float **ppInputs, unsigned short iInputs,
	float **ppOutputs, unsigned short iOutputs)
{
#ifdef __APPLE__
	if (!m_audioUnit) return;

	// Setup audio buffer list
	AudioBufferList bufferList;
	bufferList.mNumberBuffers = iOutputs;
	
	for (unsigned short i = 0; i < iOutputs; ++i) {
		bufferList.mBuffers[i].mNumberChannels = 1;
		bufferList.mBuffers[i].mDataByteSize = nframes * sizeof(float);
		bufferList.mBuffers[i].mData = ppOutputs[i];
	}

	// Setup audio unit render context
	AudioUnitRenderActionFlags flags = 0;
	AudioTimeStamp timeStamp;
	memset(&timeStamp, 0, sizeof(timeStamp));
	timeStamp.mSampleTime = 0;
	timeStamp.mFlags = kAudioTimeStampSampleTimeValid;

	// Render audio
	OSStatus status = AudioUnitRender(m_audioUnit,
		&flags, &timeStamp, 0, nframes, &bufferList);
	
	if (status != noErr) {
		qWarning("qtractorAuPlugin::process(): AudioUnit render failed");
	}
#else
	Q_UNUSED(nframes);
	Q_UNUSED(ppInputs);
	Q_UNUSED(iInputs);
	Q_UNUSED(ppOutputs);
	Q_UNUSED(iOutputs);
#endif
}


#ifdef __APPLE__
// AudioUnit render callback.
OSStatus qtractorAuPlugin::renderCallback(
	void *inRefCon,
	AudioUnitRenderActionFlags *ioActionFlags,
	const AudioTimeStamp *inTimeStamp,
	UInt32 inBusNumber,
	UInt32 inNumberFrames,
	AudioBufferList *ioData)
{
	qtractorAuPlugin *pPlugin = static_cast<qtractorAuPlugin *>(inRefCon);
	if (!pPlugin) return noErr;

	// Process plugin audio
	// This would typically call the plugin's process method
	
	return noErr;
}
#endif


// Parameter name accessor.
QString qtractorAuPlugin::parameterName(unsigned int iParam) const
{
#ifdef __APPLE__
	if (!m_audioUnit) return QString();

	AUParameterInfo paramInfo;
	UInt32 size = sizeof(paramInfo);
	
	OSStatus status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_ParameterInfo,
		kAudioUnitScope_Global, iParam,
		&paramInfo, &size);
	
	if (status == noErr && paramInfo.cfNameString) {
		CFStringRef cfName = reinterpret_cast<CFStringRef>(paramInfo.cfNameString);
		char buffer[256];
		if (CFStringGetCString(cfName, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
			CFRelease(cfName);
			return QString::fromUtf8(buffer);
		}
		CFRelease(cfName);
	}
#else
	Q_UNUSED(iParam);
#endif

	return QString("Parameter %1").arg(iParam);
}


// Parameter unit accessor.
QString qtractorAuPlugin::parameterUnit(unsigned int iParam) const
{
#ifdef __APPLE__
	if (!m_audioUnit) return QString();

	AUParameterInfo paramInfo;
	UInt32 size = sizeof(paramInfo);
	
	OSStatus status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_ParameterInfo,
		kAudioUnitScope_Global, iParam,
		&paramInfo, &size);
	
	if (status == noErr) {
		switch (paramInfo.unitID) {
		case kAudioUnitParameterUnit_Generic:
			return QString();
		case kAudioUnitParameterUnit_Indexed:
			return QString("index");
		case kAudioUnitParameterUnit_Boolean:
			return QString();
		case kAudioUnitParameterUnit_Percent:
			return QString("%");
		case kAudioUnitParameterUnit_Seconds:
			return QString("s");
		case kAudioUnitParameterUnit_SampleFrames:
			return QString("samples");
		case kAudioUnitParameterUnit_Phase:
			return QString("rad");
		case kAudioUnitParameterUnit_Rate:
			return QString("Hz");
		case kAudioUnitParameterUnit_Hertz:
			return QString("Hz");
		case kAudioUnitParameterUnit_Cents:
			return QString("cents");
		case kAudioUnitParameterUnit_AbsoluteCents:
			return QString("acents");
		case kAudioUnitParameterUnit_Decibels:
			return QString("dB");
		case kAudioUnitParameterUnit_MIDIValue:
			return QString("MIDI");
		default:
			return QString();
		}
	}
#else
	Q_UNUSED(iParam);
#endif

	return QString();
}


// Parameter range accessors.
float qtractorAuPlugin::parameterMin(unsigned int iParam) const
{
#ifdef __APPLE__
	if (!m_audioUnit) return 0.0f;

	AUParameterInfo paramInfo;
	UInt32 size = sizeof(paramInfo);
	
	OSStatus status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_ParameterInfo,
		kAudioUnitScope_Global, iParam,
		&paramInfo, &size);
	
	if (status == noErr) {
		return paramInfo.minValue;
	}
#else
	Q_UNUSED(iParam);
#endif

	return 0.0f;
}

float qtractorAuPlugin::parameterMax(unsigned int iParam) const
{
#ifdef __APPLE__
	if (!m_audioUnit) return 1.0f;

	AUParameterInfo paramInfo;
	UInt32 size = sizeof(paramInfo);
	
	OSStatus status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_ParameterInfo,
		kAudioUnitScope_Global, iParam,
		&paramInfo, &size);
	
	if (status == noErr) {
		return paramInfo.maxValue;
	}
#else
	Q_UNUSED(iParam);
#endif

	return 1.0f;
}

float qtractorAuPlugin::parameterDefault(unsigned int iParam) const
{
#ifdef __APPLE__
	if (!m_audioUnit) return 0.5f;

	AUParameterInfo paramInfo;
	UInt32 size = sizeof(paramInfo);
	
	OSStatus status = AudioUnitGetProperty(m_audioUnit,
		kAudioUnitProperty_ParameterInfo,
		kAudioUnitScope_Global, iParam,
		&paramInfo, &size);
	
	if (status == noErr) {
		return paramInfo.defaultValue;
	}
#else
	Q_UNUSED(iParam);
#endif

	return 0.5f;
}


// Static factory methods.
bool qtractorAuPlugin::scan(const QString& sPath,
	QList<qtractorPluginFile *>& files)
{
#ifdef __APPLE__
	Q_UNUSED(sPath);

	// Find all AudioUnit components
	AudioComponentDescription desc;
	desc.componentType = kAudioUnitType_Effect;
	desc.componentSubType = 0; // Any subtype
	desc.componentManufacturer = 0; // Any manufacturer
	desc.componentFlags = 0;
	desc.componentFlagsMask = 0;

	void *iterator = nullptr;
	AudioComponent comp;
	
	while ((comp = AudioComponentFindNext(iterator, &desc)) != nullptr) {
		iterator = comp;
		
		// Get component info
		ComponentDescription compDesc;
		if (GetComponentInfo(reinterpret_cast<Component>(comp),
			&compDesc, nullptr, nullptr, nullptr) == noErr) {
			
			// Create plugin file entry
			qtractorPluginFile *pFile = new qtractorPluginFile();
			pFile->setName(QString("AU Plugin"));
			files.append(pFile);
		}
	}

	return !files.isEmpty();
#else
	Q_UNUSED(sPath);
	Q_UNUSED(files);
	return false;
#endif
}


// end of qtractorAuPlugin.cpp
