// qtractorAudioPeak.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include <QFile>
#include <QHash>

#include <QMutex>

#include <QStringList>


// Forward declarations.
class qtractorAudioPeakThread;


//----------------------------------------------------------------------
// class qtractorAudioPeakFile -- Audio peak file (ref'counted)
//

class qtractorAudioPeakFile
{
public:

	// Constructor.
	qtractorAudioPeakFile(const QString& sFilename, float fTimeStretch);

	// Default destructor.
	~qtractorAudioPeakFile();

	// Audio properties accessors.
	const QString& filename() const;
	float timeStretch() const;

	QString peakName() const;

	// Peak cache properties accessors.
	QString name() const;
	unsigned short period();
	unsigned short channels();

	// Audio peak file header.
	struct Header
	{
		unsigned short period;
		unsigned short channels;
	};

	// Audio peak file frame record.
	struct Frame
	{
		unsigned char max;
		unsigned char min;
		unsigned char rms;
	};

	// Peak cache file methods.
	bool openRead();
	Frame *read(unsigned long iPeakOffset, unsigned int iPeakFrames);
	void closeRead();

	// Write peak from audio frame methods.
	bool openWrite(unsigned short iChannels, unsigned int iSampleRate);
	void write(float **ppAudioFrames, unsigned int iAudioFrames);
	void closeWrite();

	// Reference count methods.
	void addRef();
	void removeRef();

	// Physical removal.
	void remove();

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync);
	bool isWaitSync() const;

protected:

	// Internal creational methods.
	void writeFrame();

	// Read frames from peak file into local buffer cache.
	unsigned int readBuffer(unsigned int iBuffOffset,
		unsigned long iPeakOffset, unsigned int iPeakFrames);

private:

	// Instance variables.
	QString        m_sFilename;
	float          m_fTimeStretch;

	QFile          m_peakFile;

	enum { None = 0, Read = 1, Write = 2 } m_openMode;

	Header         m_peakHeader;

	Frame         *m_pBuffer;
	unsigned int   m_iBuffSize;
	unsigned int   m_iBuffLength;
	unsigned long  m_iBuffOffset;

	unsigned long  m_iWriteOffset;

	float         *m_peakMax;
	float         *m_peakMin;
	float         *m_peakRms;
	unsigned short m_iPeakPeriod;
	unsigned short m_iPeak;

	QMutex         m_mutex;

	volatile bool  m_bWaitSync;

	// Current reference count.
	unsigned int   m_iRefCount;
};


//----------------------------------------------------------------------
// class qtractorAudioPeak -- Audio Peak file pseudo-cache.
//

class qtractorAudioPeak
{
public:

	// Constructor.
	qtractorAudioPeak(qtractorAudioPeakFile *pPeakFile)
		{ m_pPeakFile = pPeakFile; m_pPeakFile->addRef(); }
	// Copy consructor.
	qtractorAudioPeak(const qtractorAudioPeak& peak)
		{ m_pPeakFile = peak.m_pPeakFile; m_pPeakFile->addRef(); }

	// Default destructor.
	~qtractorAudioPeak() { m_pPeakFile->removeRef(); }

	// Reference accessor.
	qtractorAudioPeakFile *peakFile() const
		{ return m_pPeakFile; }

	// Peak file accessors.
	const QString& filename() const
		{ return m_pPeakFile->filename(); }

	// Peak cache properties.
	unsigned short period() const
		{ return m_pPeakFile->period(); }
	unsigned short channels() const
		{ return m_pPeakFile->channels(); }

	// Peak cache file methods.
	bool openRead() { return m_pPeakFile->openRead(); }
	qtractorAudioPeakFile::Frame *read(
		unsigned long iPeakOffset, unsigned int iPeakFrames)
		{ return m_pPeakFile->read(iPeakOffset, iPeakFrames); }
	void closeRead() { m_pPeakFile->closeRead(); }

	// Write peak from audio frame methods.
	bool openWrite(unsigned short iChannels, unsigned int iSampleRate)
		{ return m_pPeakFile->openWrite(iChannels, iSampleRate); }
	void write(float **ppAudioFrames, unsigned int iAudioFrames)
		{ m_pPeakFile->write(ppAudioFrames, iAudioFrames); }
	void closeWrite() { m_pPeakFile->closeWrite(); }

private:

	// Instance variable (ref'counted).
	qtractorAudioPeakFile *m_pPeakFile;
};


//----------------------------------------------------------------------
// class qtractorAudioPeakFactory -- Audio peak file factory (singleton).
//

class qtractorAudioPeakFactory : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorAudioPeakFactory(QObject *pParent = NULL);
	// Default destructor.
	~qtractorAudioPeakFactory();

	// The peak file factory-method.
	static QString peakName(const QString& sFilename, float fTimeStretch);
	qtractorAudioPeak *createPeak(const QString& sFilename, float fTimeStretch);
	void removePeak(qtractorAudioPeakFile *pPeakFile, bool bAborted);

	// Auto-delete property.
	void setAutoRemove(bool bAutoRemove);
	bool isAutoRemove() const;

	// Peak ready event notification.
	void notifyPeakEvent();

	// Base sync method.
	void sync(qtractorAudioPeakFile *pPeakFile = NULL);

	// Cleanup method.
	void cleanup();

signals:

	// Peak ready signal.
	void peakEvent();

private:

	// Factory mutex.
	QMutex m_mutex;

	// The list of managed peak files.
	QHash<QString, qtractorAudioPeakFile *> m_peaks;

	// Auto-delete property.
	bool m_bAutoRemove;

	// The queue of discardable peak files.
	QStringList m_files;

	// The peak file creation detached thread.
	qtractorAudioPeakThread *m_pPeakThread;
};


#endif  // __qtractorAudioPeak_h


// end of qtractorAudioPeak.h
