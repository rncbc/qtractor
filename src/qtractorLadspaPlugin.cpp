// qtractorLadspaPlugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifdef CONFIG_LADSPA

#include "qtractorLadspaPlugin.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"

#include <math.h>


//----------------------------------------------------------------------------
// qtractorLadspaPluginType -- LADSPA plugin type instance.
//

// Derived methods.
bool qtractorLadspaPluginType::open (void)
{
	// Do we have a descriptor already?
	if (m_pLadspaDescriptor == NULL)
		m_pLadspaDescriptor = ladspa_descriptor(file(), index());
	if (m_pLadspaDescriptor == NULL)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLadspaPluginType[%p]::open() filename=\"%s\" index=%lu",
		this, filename().toUtf8().constData(), index());
#endif

	// Retrieve plugin type names.
	m_sName  = m_pLadspaDescriptor->Name;
	m_sLabel = m_pLadspaDescriptor->Label;

	// Retrieve plugin unique identifier.
	m_iUniqueID = m_pLadspaDescriptor->UniqueID;

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

	for (unsigned long i = 0; i < m_pLadspaDescriptor->PortCount; ++i) {
		const LADSPA_PortDescriptor portType
			= m_pLadspaDescriptor->PortDescriptors[i];
		if (LADSPA_IS_PORT_INPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioIns;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlIns;
		}
		else
		if (LADSPA_IS_PORT_OUTPUT(portType)) {
			if (LADSPA_IS_PORT_AUDIO(portType))
				++m_iAudioOuts;
			else
			if (LADSPA_IS_PORT_CONTROL(portType))
				++m_iControlOuts;
		}
	}

	// Cache flags.
	m_bRealtime = LADSPA_IS_HARD_RT_CAPABLE(m_pLadspaDescriptor->Properties);

	// Done.
	return true;
}


void qtractorLadspaPluginType::close (void)
{
	m_pLadspaDescriptor = NULL;
}


// Factory method (static)
qtractorLadspaPluginType *qtractorLadspaPluginType::createType (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Sanity check...
	if (pFile == NULL)
		return NULL;

	// Retrieve descriptor if any...
	const LADSPA_Descriptor *pLadspaDescriptor
		= ladspa_descriptor(pFile, iIndex);
	if (pLadspaDescriptor == NULL)
		return NULL;

	// Yep, most probably its a valid plugin descriptor...
	return new qtractorLadspaPluginType(pFile, iIndex,
		qtractorPluginType::Ladspa, pLadspaDescriptor);
}


// Descriptor method (static)
const LADSPA_Descriptor *qtractorLadspaPluginType::ladspa_descriptor (
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	// Retrieve the descriptor function, if any...
	LADSPA_Descriptor_Function pfnLadspaDescriptor
		= (LADSPA_Descriptor_Function) pFile->resolve("ladspa_descriptor");
	if (pfnLadspaDescriptor == NULL)
		return NULL;

	// Retrieve descriptor if any...
	return (*pfnLadspaDescriptor)(iIndex);
}


// Instance cached-deferred accesors.
const QString& qtractorLadspaPluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty() && m_pLadspaDescriptor) {
		if (m_pLadspaDescriptor->Maker) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Author: ");
			m_sAboutText += m_pLadspaDescriptor->Maker;
		}
		if (m_pLadspaDescriptor->Copyright) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Copyright: ");
			m_sAboutText += m_pLadspaDescriptor->Copyright;
		}
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorLadspaPlugin -- LADSPA plugin instance.
//

// Constructors.
qtractorLadspaPlugin::qtractorLadspaPlugin ( qtractorPluginList *pList,
	qtractorLadspaPluginType *pLadspaType )
	: qtractorPlugin(pList, pLadspaType), m_phInstances(NULL),
		m_piControlOuts(NULL), m_pfControlOuts(NULL),
		m_piAudioIns(NULL), m_piAudioOuts(NULL),
		m_pfIDummy(NULL), m_pfODummy(NULL), m_pfLatency(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLadspaPlugin[%p] filename=\"%s\" index=%lu typeHint=%d",
		this, type()->filename().toUtf8().constData(),
		type()->index(), int(type()->typeHint()));
#endif

	// Get some structural data first...
	const LADSPA_Descriptor *pLadspaDescriptor
		= pLadspaType->ladspa_descriptor();
	if (pLadspaDescriptor) {
		unsigned short iControlOuts = pLadspaType->controlOuts();
		unsigned short iAudioIns    = pLadspaType->audioIns();
		unsigned short iAudioOuts   = pLadspaType->audioOuts();
		if (iAudioIns > 0)
			m_piAudioIns = new unsigned long [iAudioIns];
		if (iAudioOuts > 0)
			m_piAudioOuts = new unsigned long [iAudioOuts];
		if (iControlOuts > 0) {
			m_piControlOuts = new unsigned long [iControlOuts];
			m_pfControlOuts = new float [iControlOuts];
		}
		iControlOuts = iAudioIns = iAudioOuts = 0;
		for (unsigned long i = 0; i < pLadspaDescriptor->PortCount; ++i) {
			const LADSPA_PortDescriptor portType
				= pLadspaDescriptor->PortDescriptors[i];
			if (LADSPA_IS_PORT_INPUT(portType)) {
				if (LADSPA_IS_PORT_AUDIO(portType))
					m_piAudioIns[iAudioIns++] = i;
				else
				if (LADSPA_IS_PORT_CONTROL(portType))
					addParam(new qtractorLadspaPluginParam(this, i));
			}
			else
			if (LADSPA_IS_PORT_OUTPUT(portType)) {
				if (LADSPA_IS_PORT_AUDIO(portType))
					m_piAudioOuts[iAudioOuts++] = i;
				else
				if (LADSPA_IS_PORT_CONTROL(portType)) {
					m_piControlOuts[iControlOuts] = i;
					m_pfControlOuts[iControlOuts] = 0.0f;
					if (::strcmp(pLadspaDescriptor->PortNames[i], "latency") == 0)
						m_pfLatency = &m_pfControlOuts[iControlOuts];
					++iControlOuts;
				}
			}
		}
		// FIXME: instantiate each instance properly...
		qtractorLadspaPlugin::setChannels(channels());
	}
}


// Destructor.
qtractorLadspaPlugin::~qtractorLadspaPlugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

	// Free up all the rest...
	if (m_piAudioOuts)
		delete [] m_piAudioOuts;
	if (m_piAudioIns)
		delete [] m_piAudioIns;
	if (m_piControlOuts)
		delete [] m_piControlOuts;
	if (m_pfControlOuts)
		delete [] m_pfControlOuts;

	if (m_pfIDummy)
		delete [] m_pfIDummy;
	if (m_pfODummy)
		delete [] m_pfODummy;
}


// Channel/instance number accessors.
void qtractorLadspaPlugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorLadspaPluginType *pLadspaType
		= static_cast<qtractorLadspaPluginType *> (type());
	if (pLadspaType == NULL)
		return;
		
	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	const unsigned short iInstances
		= pLadspaType->instances(iChannels, list()->isMidi());
	// Now see if instance count changed anyhow...
	if (iInstances == iOldInstances)
		return;

	const LADSPA_Descriptor *pLadspaDescriptor
		= pLadspaType->ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setActivated(false);

	// Set new instance number...
	setInstances(iInstances);

	if (m_phInstances) {
		if (pLadspaDescriptor->cleanup) {
			for (unsigned short i = 0; i < iOldInstances; ++i)
				(*pLadspaDescriptor->cleanup)(m_phInstances[i]);
		}
		delete [] m_phInstances;
		m_phInstances = NULL;
	}

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorLadspaPlugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

	// Allocate the dummy audio I/O buffers...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	const unsigned int iSampleRate = pAudioEngine->sampleRate();
	const unsigned int iBufferSize = pAudioEngine->bufferSize();

	const unsigned short iAudioIns = pLadspaType->audioIns();
	const unsigned short iAudioOuts = pLadspaType->audioOuts();

	if (iChannels < iAudioIns) {
		if (m_pfIDummy)
			delete [] m_pfIDummy;
		m_pfIDummy = new float [iBufferSize];
		::memset(m_pfIDummy, 0, iBufferSize * sizeof(float));
	}

	if (iChannels < iAudioOuts) {
		if (m_pfODummy)
			delete [] m_pfODummy;
		m_pfODummy = new float [iBufferSize];
	//	::memset(m_pfODummy, 0, iBufferSize * sizeof(float));
	}

	// We'll need output control (not dummy anymore) port indexes...
	const unsigned short iControlOuts = pLadspaType->controlOuts();

	unsigned short i, j;

	// Allocate new instances...
	m_phInstances = new LADSPA_Handle [iInstances];
	for (i = 0; i < iInstances; ++i) {
		// Instantiate them properly first...
		LADSPA_Handle handle
			= (*pLadspaDescriptor->instantiate)(pLadspaDescriptor, iSampleRate);
		// Connect all existing input control ports...
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			// Just in case the plugin decides
			// to set the port value at this time...
			float *pfValue = pParam->subject()->data();
			float   fValue = *pfValue;
			(*pLadspaDescriptor->connect_port)(handle,
				pParam->index(), pfValue);
			// Make new one the default and restore port value...
			pParam->setDefaultValue(*pfValue);
			*pfValue = fValue;
		}
		// Connect all existing output control ports...
		for (j = 0; j < iControlOuts; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piControlOuts[j], &m_pfControlOuts[j]);
		}
		// Connect all dummy input ports...
		if (m_pfIDummy) for (j = iChannels; j < iAudioIns; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioIns[j], m_pfIDummy); // dummy input port!
		}
		// Connect all dummy output ports...
		if (m_pfODummy) for (j = iChannels; j < iAudioOuts; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioOuts[j], m_pfODummy); // dummy input port!
		}
		// This is it...
		m_phInstances[i] = handle;
	}

	// (Re)issue all configuration as needed...
	realizeConfigs();
	realizeValues();

	// But won't need it anymore.
	releaseConfigs();
	releaseValues();

	// (Re)activate instance if necessary...
	setActivated(bActivated);
}


// Specific accessors.
const LADSPA_Descriptor *qtractorLadspaPlugin::ladspa_descriptor (void) const
{
	qtractorLadspaPluginType *pLadspaType
		= static_cast<qtractorLadspaPluginType *> (type());
	return (pLadspaType ? pLadspaType->ladspa_descriptor() : NULL);
}


LADSPA_Handle qtractorLadspaPlugin::ladspa_handle ( unsigned short iInstance ) const
{
	return (m_phInstances ? m_phInstances[iInstance] : NULL);
}


// Do the actual activation.
void qtractorLadspaPlugin::activate (void)
{
	const LADSPA_Descriptor *pLadspaDescriptor = ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	if (m_phInstances && pLadspaDescriptor->activate) {
		for (unsigned short i = 0; i < instances(); ++i)
			(*pLadspaDescriptor->activate)(m_phInstances[i]);
	}
}


// Do the actual deactivation.
void qtractorLadspaPlugin::deactivate (void)
{
	const LADSPA_Descriptor *pLadspaDescriptor = ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	if (m_phInstances && pLadspaDescriptor->deactivate) {
		for (unsigned short i = 0; i < instances(); ++i)
			(*pLadspaDescriptor->deactivate)(m_phInstances[i]);
	}
}


// The main plugin processing procedure.
void qtractorLadspaPlugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_phInstances == NULL)
		return;

	const LADSPA_Descriptor *pLadspaDescriptor = ladspa_descriptor();
	if (pLadspaDescriptor == NULL)
		return;

	// We'll cross channels over instances...
	const unsigned short iInstances = instances();
	const unsigned short iChannels  = channels();
	const unsigned short iAudioIns  = audioIns();
	const unsigned short iAudioOuts = audioOuts();

	unsigned short iIChannel = 0;
	unsigned short iOChannel = 0;
	unsigned short i, j;

	// For each plugin instance...
	for (i = 0; i < iInstances; ++i) {
		LADSPA_Handle handle = m_phInstances[i];
		// For each instance audio input port...
		for (j = 0; j < iAudioIns && iIChannel < iChannels; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioIns[j], ppIBuffer[iIChannel++]);
		}
		// For each instance audio output port...
		for (j = 0; j < iAudioOuts && iOChannel < iChannels; ++j) {
			(*pLadspaDescriptor->connect_port)(handle,
				m_piAudioOuts[j], ppOBuffer[iOChannel++]);
		}
		// Make it run...
		(*pLadspaDescriptor->run)(handle, nframes);
		// Wrap dangling output channels?...
		for (j = iOChannel; j < iChannels; ++j)
			::memset(ppOBuffer[j], 0, nframes * sizeof(float));
	}
}


//----------------------------------------------------------------------------
// qtractorLadspaPluginParam -- LADSPA plugin control input port instance.
//

// Constructors.
qtractorLadspaPluginParam::qtractorLadspaPluginParam (
	qtractorLadspaPlugin *pLadspaPlugin, unsigned long iIndex )
	: qtractorPluginParam(pLadspaPlugin, iIndex)
{
	const LADSPA_Descriptor *pLadspaDescriptor
		= pLadspaPlugin->ladspa_descriptor();
	const LADSPA_PortRangeHint *pPortRangeHint
		= &(pLadspaDescriptor->PortRangeHints[iIndex]);
	m_portHints = pPortRangeHint->HintDescriptor;

	// Set nominal parameter name...
	setName(pLadspaDescriptor->PortNames[iIndex]);

	// Initialize default sample-rate...
	unsigned int iSampleRate = 44100;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine)
			iSampleRate = pAudioEngine->sampleRate();
	}

	// Initialize range values...
	float fMinValue = 0.0f;
	if (isBoundedBelow()) {
		fMinValue = pPortRangeHint->LowerBound;
		if (isSampleRate())
			fMinValue *= float(iSampleRate);
	}
	setMinValue(fMinValue);

	float fMaxValue = 1.0f;
	if (isBoundedAbove()) {
		fMaxValue = pPortRangeHint->UpperBound;
		if (isSampleRate())
			fMaxValue *= float(iSampleRate);
	}
	setMaxValue(fMaxValue);

	// Port default value...
	float fDefaultValue = value();
	if (isDefaultValue()) {
		switch (m_portHints & LADSPA_HINT_DEFAULT_MASK) {
		case LADSPA_HINT_DEFAULT_MINIMUM:
			fDefaultValue = fMinValue;
			break;
		case LADSPA_HINT_DEFAULT_LOW:
			if (isLogarithmic()) {
				fDefaultValue = ::expf(
					::logf(fMinValue) * 0.75f + ::logf(fMaxValue) * 0.25f);
			} else {
				fDefaultValue = (fMinValue * 0.75f + fMaxValue * 0.25f);
			}
			break;
		case LADSPA_HINT_DEFAULT_MIDDLE:
			if (isLogarithmic()) {
				fDefaultValue = ::sqrt(fMinValue * fMaxValue);
			} else {
				fDefaultValue = (fMinValue + fMaxValue) * 0.5f;
			}
			break;
		case LADSPA_HINT_DEFAULT_HIGH:
			if (isLogarithmic()) {
				fDefaultValue = ::expf(
					::logf(fMinValue) * 0.25f + ::logf(fMaxValue) * 0.75f);
			} else {
				fDefaultValue = (fMinValue * 0.25f + fMaxValue * 0.75f);
			}
			break;
		case LADSPA_HINT_DEFAULT_MAXIMUM:
			fDefaultValue = fMaxValue;
			break;
		case LADSPA_HINT_DEFAULT_0:
			fDefaultValue = 0.0f;
			break;
		case LADSPA_HINT_DEFAULT_1:
			fDefaultValue = 1.0f;
			break;
		case LADSPA_HINT_DEFAULT_100:
			fDefaultValue = 100.0f;
			break;
		case LADSPA_HINT_DEFAULT_440:
			fDefaultValue = 440.0f;
			break;
		}
	}
	setDefaultValue(fDefaultValue);

	// Initialize port value...
	// reset(); -- deferred to qtractorPlugin::addParam();
}


// Destructor.
qtractorLadspaPluginParam::~qtractorLadspaPluginParam (void)
{
}


// Port range hints predicate methods.
bool qtractorLadspaPluginParam::isBoundedBelow (void) const
{
	return LADSPA_IS_HINT_BOUNDED_BELOW(m_portHints);
}

bool qtractorLadspaPluginParam::isBoundedAbove (void) const
{
	return LADSPA_IS_HINT_BOUNDED_ABOVE(m_portHints);
}

bool qtractorLadspaPluginParam::isDefaultValue (void) const
{
	return LADSPA_IS_HINT_HAS_DEFAULT(m_portHints);
}

bool qtractorLadspaPluginParam::isLogarithmic (void) const
{
	return LADSPA_IS_HINT_LOGARITHMIC(m_portHints);
}

bool qtractorLadspaPluginParam::isSampleRate (void) const
{
	return LADSPA_IS_HINT_SAMPLE_RATE(m_portHints);
}

bool qtractorLadspaPluginParam::isInteger (void) const
{
	return LADSPA_IS_HINT_INTEGER(m_portHints);
}

bool qtractorLadspaPluginParam::isToggled (void) const
{
	return LADSPA_IS_HINT_TOGGLED(m_portHints);
}

bool qtractorLadspaPluginParam::isDisplay (void) const
{
	return false;
}


#endif	// CONFIG_LADSPA

// end of qtractorLadspaPlugin.cpp
