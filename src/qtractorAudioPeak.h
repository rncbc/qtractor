// qtractorAudioPeak.h
//
/****************************************************************************
   Copyright (C) 2005-2007, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorAudioPeak_h
#define __qtractorAudioPeak_h

#include <QString>
#include <QEvent>
#include <QFile>
#include <QHash>

// Forward declarations.
class qtractorAudioPeakThread;
class qtractorAudioPeakFactory;


//----------------------------------------------------------------------
// struct qtractorAudioPeakFrame -- Audio Peak frame record.
//

struct qtractorAudioPeakFrame
{
	unsigned char peakMax;
	unsigned char peakMin;
	unsigned char peakRms;
};


//----------------------------------------------------------------------
// class qtractorAudioPeakFile -- Audio peak file (ref'counted)
//

class qtractorAudioPeakFile
{
public:

	// Constructor.
	qtractorAudioPeakFile(qtractorAudioPeakFactory *pFactory,
		const QString& sFilename, unsigned int iSampleRate,
		const QString& sSessionDir);
	// Default destructor.
	~qtractorAudioPeakFile();

	// Audio file accessors.
	const QString& filename() const;
	unsigned int   sampleRate() const;

	// Peak file accessors.
	QString name() const { return m_peakFile.fileName(); }

	// Lazy-evaluated properties.
	unsigned short period();
	unsigned short channels();
	unsigned long  frames();

	// Peak file methods.
	void getPeak(qtractorAudioPeakFrame *pframes,
		unsigned long iframe, unsigned int nframes);

	// Event notifier widget settings.
	QWidget     *notifyWidget() const;
	QEvent::Type notifyPeakType() const;

	// Auto-delete property.
	bool isAutoRemove() const;

	// Reference count methods.
	void addRef();
	void removeRef();

protected:

	// Internal pseudo-cache methods.
	bool openPeakFile();
	void readPeak(char *pBuffer, unsigned long iOffset, unsigned int iLength);
	void readPeakChunk();
	void closePeakFile();
	
private:

	// Reference to master peak manager.
	qtractorAudioPeakFactory *m_pFactory;

	// Instance variables.
	QString        m_sFilename;
	unsigned int   m_iSampleRate;

	QFile          m_peakFile;

	unsigned short m_iPeakPeriod;
	unsigned short m_iPeakChannels;
	unsigned long  m_iPeakFrames;
	unsigned long  m_iPeakOffset;
	unsigned int   m_iPeakBufSize;
	char          *m_pPeakBuffer;

	// The peak file creation detached thread.
	qtractorAudioPeakThread *m_pPeakThread;
	
	// Current reference count.
	unsigned int  m_iRefCount;
};


//----------------------------------------------------------------------
// class qtractorAudioPeak -- Audio Peak file pseudo-cache.
//

class qtractorAudioPeak
{
public:

	// Constructor.
	qtractorAudioPeak(qtractorAudioPeakFile *pPeakFile);
	// Copy consructor.
	qtractorAudioPeak(const qtractorAudioPeak& peak);
	// Default destructor.
	~qtractorAudioPeak();

	// Peak file accessors.
	const QString& filename() const;
	unsigned short period() const;
	// Lazy-evaluated properties.
	unsigned short channels() const;
	unsigned long  frames() const;

	// Peak file methods.
	void getPeak(qtractorAudioPeakFrame *pframes,
		unsigned long iframe, unsigned int nframes);

private:

	// Instance variable (ref'counted).
	qtractorAudioPeakFile *m_pPeakFile;
};


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

class qtractorAudioPeakFactory
{
public:

	// Constructor.
	qtractorAudioPeakFactory();
	// Default destructor.
	~qtractorAudioPeakFactory();

	// The peak file factory-method.
	qtractorAudioPeak* createPeak(const QString& sFilename,
		unsigned int iSampleRate, const QString& sSessionDir);
	void removePeak(qtractorAudioPeakFile *pPeakFile);

	// Event notifier widget settings.
	void setNotifyWidget   (QWidget *pNotifyWidget);
	void setNotifyPeakType (QEvent::Type eNotifyPeakType);

	QWidget     *notifyWidget() const;
	QEvent::Type notifyPeakType() const;

	// Auto-delete property.
	void setAutoRemove(bool bAutoRemove);
	bool isAutoRemove() const;

private:

	// The list of managed peak files.
	QHash<QString, qtractorAudioPeakFile *> m_peaks;

	// The event notifier widget.
	QWidget      *m_pNotifyWidget;
	QEvent::Type  m_eNotifyPeakType;

	// Auto-delete property.
	bool m_bAutoRemove;
};


#endif  // __qtractorAudioPeak_h


// end of qtractorAudioPeak.h
