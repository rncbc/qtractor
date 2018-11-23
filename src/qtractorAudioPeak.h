// qtractorAudioPeak.h
//
/****************************************************************************
   Copyright (C) 2005-2018, rncbc aka Rui Nuno Capela. All rights reserved.

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
	Frame *read(unsigned long iPeakOffset, unsigned int iPeakLength);
	void closeRead();

	// Write peak from audio frame methods.
	bool openWrite(unsigned short iChannels, unsigned int iSampleRate);
	int write(float **ppAudioFrames, unsigned int iAudioFrames);
	void closeWrite();

	// Reference count methods.
	void addRef();
	void removeRef();

	// Physical removal.
	void remove();

	// Clean/close method.
	void cleanup(bool bAutoRemove = false);

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync);
	bool isWaitSync() const;

	// Peak filename standard.
	static QString peakName(const QString& sFilename, float fTimeStretch);

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

	QMutex         m_mutex;

	volatile bool  m_bWaitSync;

	// Current reference count.
	unsigned int   m_iRefCount;

	// Peak-writer context state.
	struct Writer
	{
		unsigned long  offset;
		float         *amax;
		float         *amin;
		float         *arms;
		unsigned long  period_p;
		unsigned int   period_q;
		unsigned int   period_r;
		unsigned long  nframe;
		unsigned short npeak;
		unsigned long  nread;
		unsigned long  nwrite;

	} *m_pWriter;
};


//----------------------------------------------------------------------
// class qtractorAudioPeak -- Audio Peak file pseudo-cache.
//

class qtractorAudioPeak
{
public:

	// Constructor.
	qtractorAudioPeak(qtractorAudioPeakFile *pPeakFile);

	// Copy onstructor.
	qtractorAudioPeak(const qtractorAudioPeak& peak);

	// Default destructor.
	~qtractorAudioPeak();

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

	// Peak frame buffer reader-cache executive.
	qtractorAudioPeakFile::Frame *peakFrames(
		unsigned long iFrameOffset, unsigned long iFrameLength, int width);
	// Peak frame buffer length (in frames).
	unsigned int peakLength() const
		{ return m_iPeakLength; }

private:

	// Instance variable (ref'counted).
	qtractorAudioPeakFile *m_pPeakFile;

	// Interim scaling buffer and hash.
	qtractorAudioPeakFile::Frame *m_pPeakFrames;

	unsigned int m_iPeakLength;
	unsigned int m_iPeakHash;
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

	// The peak period accessors.
	void setPeakPeriod(unsigned short iPeakPeriod);
	unsigned short peakPeriod() const;

	// The peak file factory-methods.
	qtractorAudioPeak *createPeak(
		const QString& sFilename, float fTimeStretch = 1.0f);

	// Auto-delete property.
	void setAutoRemove(bool bAutoRemove);
	bool isAutoRemove() const;

	// Peak ready event notification.
	void notifyPeakEvent();

	// Base sync method.
	void sync(qtractorAudioPeakFile *pPeakFile = NULL);

	// Cleanup method.
	void cleanup();

	// Singleton instance accessor.
	static qtractorAudioPeakFactory *getInstance();

signals:

	// Peak ready signal.
	void peakEvent();

private:

	// Factory mutex.
	QMutex m_mutex;

	// The list of managed peak files.
	typedef QHash<QString, qtractorAudioPeakFile *> PeakFiles;

	PeakFiles m_peaks;

	// Auto-delete property.
	bool m_bAutoRemove;

	// The peak file creation detached thread.
	qtractorAudioPeakThread *m_pPeakThread;

	// The current running peak-period.
	unsigned short m_iPeakPeriod;

	// The pseudo-singleton instance.
	static qtractorAudioPeakFactory *g_pPeakFactory;
};


#endif  // __qtractorAudioPeak_h


// end of qtractorAudioPeak.h
