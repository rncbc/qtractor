// qtractorOscControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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

/* This code was originally borrowed, stirred, mangled and finally
 * adapted from Arnold Krille's libofqf, OSC for Qt4 implementation (GPL).
 *
 * Copyright (C) 2007, Arnold Krille <arnold@arnoldarts.de>
 */

#include "qtractorOscControl.h"


//---------------------------------------------------------------------------
// qtractorOscBase

// Constructor.
qtractorOscBase::qtractorOscBase ( QObject *pParent ) : QUdpSocket(pParent)
{
}


// Destructor.
qtractorOscBase::~qtractorOscBase (void)
{
}


// Basic data converters.
QByteArray qtractorOscBase::fromString ( const QString& s )
{
	QByteArray ret = s.toUtf8();
	ret.append(char(0));
	while (ret.length() % 4)
		ret.append(char(0));
	return ret;
}

QByteArray qtractorOscBase::fromInt ( int i )
{
	return reverse(QByteArray((char *) (&i), 4));
}

QByteArray qtractorOscBase::fromFloat ( float f )
{
	return reverse(QByteArray((char *) (&f), 4));
}


QString qtractorOscBase::toString ( const QByteArray& a )
{
	return QString(a.data());
}

int qtractorOscBase::toInt ( const QByteArray& a )
{
	return *(int *) reverse(a.left(4)).data();
}

float qtractorOscBase::toFloat( const QByteArray& a )
{
	return *(float *) reverse(a.left(4)).data();
}


// Specific data converters.
QByteArray qtractorOscBase::reverse ( const QByteArray& a )
{
	QByteArray ret;
	for (int i = 0; i < a.size(); ++i)
		ret.prepend(a.at(i));
	return ret;
}


void qtractorOscBase::parseArgs (
	const QVariant& v, QString& types, QByteArray& args )
{
	switch (v.type()) {
	case QVariant::Int:
		types += 'i';
		args += fromInt(v.toInt());
		break;
	case QVariant::Double:
		types += 'f';
		args += fromFloat(float(v.toDouble()));
		break;
	case QVariant::String:
		types += 's';
		args += fromString(v.toString());
		break;
	case QVariant::List:
	{
		QListIterator<QVariant> iter(v.toList());
		while (iter.hasNext())
			parseArgs(iter.next(), types, args);
	}
	default:
		break;
	}
}


QByteArray qtractorOscBase::message ( const QString& sPath, const QVariant& v )
{
	QString types(',');
	QByteArray args;

	parseArgs(v, types, args);

	QByteArray ret = fromString(sPath);
	ret.append(fromString(types));
	ret.append(args);
	return ret;
}


//---------------------------------------------------------------------------
// qtractorOscPath

// Constructor.
qtractorOscPath::qtractorOscPath (
	const QString& sPath, QVariant::Type vtype, qtractorOscServer *pServer )
	: QObject(pServer), m_pServer(pServer), m_sPath(sPath), m_vtype(vtype),
		m_host(QHostAddress::Null), m_port(0)
{
}


// Instance properties accessors.
qtractorOscServer *qtractorOscPath::server (void) const
{
	return m_pServer;
}

const QString& qtractorOscPath::path (void) const
{
	return m_sPath;
}

QVariant::Type qtractorOscPath::type (void) const
{
	return m_vtype;
}


// Transient properties accessors.
const QHostAddress& qtractorOscPath::host (void) const
{
	return m_host;
}

unsigned short qtractorOscPath::port (void) const
{
	return m_port;
}


// Data notifier.
void qtractorOscPath::signalData ( const QVariant& v,
	const QHostAddress& host, unsigned short port )
{
	if (v.type() == m_vtype) {
		m_host = host;
		m_port = port;
		emit data(v);
	}
}


//---------------------------------------------------------------------------
// qtractorOscServer

// Constructor.
qtractorOscServer::qtractorOscServer ( unsigned short port, QObject *pParent )
	: qtractorOscBase(pParent)
{
	QUdpSocket::bind(port);

	QObject::connect(this, SIGNAL(readyRead()), SLOT(readyReadSlot()));
}


// Destructor.
qtractorOscServer::~qtractorOscServer (void)
{
	clear();
}


// Path registry methods.
qtractorOscPath *qtractorOscServer::addPath (
	const QString& sPath, QVariant::Type vtype )
{
	qtractorOscPath *pOscPath = new qtractorOscPath(sPath, vtype, this);
	m_paths.insert(pOscPath->path(), pOscPath);
	return pOscPath;
}

qtractorOscPath *qtractorOscServer::addPath (
	const QString& sPath, QVariant::Type vtype,
	const QObject *pReceiver, const char *pMember )
{
	qtractorOscPath *pOscPath = addPath(sPath, vtype);
	QObject::connect(pOscPath,
		SIGNAL(data(const QVariant&)), pReceiver, pMember);
	return pOscPath;
}


void qtractorOscServer::removePath ( qtractorOscPath *pOscPath )
{
	m_paths.remove(pOscPath->path());
	delete pOscPath;
}


void qtractorOscServer::clear (void)
{
	qDeleteAll(m_paths);
	m_paths.clear();
}


// Data receiver.
void qtractorOscServer::readyReadSlot (void)
{
	while (QUdpSocket::hasPendingDatagrams()) {
		QHostAddress host;
		unsigned short port;
		const int nsize = 1024;
		QByteArray data(nsize, char(0));
		int nread = QUdpSocket::readDatagram(data.data(), nsize, &host, &port);
		QString sPath;
		QString types;
		QVariant args;
		int i = 0;
		if (data[i] == '/') {
			for ( ; i < nread && data[i] != char(0); ++i)
				sPath += data[i];
			while (data[i++] != ',');
			while (data[i] != char(0))
				types += data[i++];
			if (!types.isEmpty()) {
				++i; // Skip char(0)...
				QList<QVariant> list;
				for (int j = 0; j < types.size(); ++j) {
					QChar type = types.at(j);
					while (i % 4) ++i;
					QByteArray a = data.right(data.size() - i);
					QVariant v;
					if (type == 's') {
						QString s = toString(a);
						v = s;
						i += s.size();
					}
					else
					if (type == 'i') {
						v = toInt(a);
						i += 4;
					}
					else
					if (type == 'f') {
						v = double(toFloat(a));
						i += 4;
					}
					if (types.size() > 1)
						list.append(v);
					else
						args = v;
				}

				if (types.size() > 1)
					args = list;
			}
		}

		qtractorOscPath *pOscPath = m_paths.value(sPath, NULL);
		if (pOscPath)
			pOscPath->signalData(args, host, port);
	}
}


//---------------------------------------------------------------------------
// qtractorOscClient

// Constructor.
qtractorOscClient::qtractorOscClient (
	const QHostAddress& host, unsigned short port, QObject* pParent )
	: qtractorOscBase(pParent), m_host(host), m_port(port)
{
}

// Destructor.
qtractorOscClient::~qtractorOscClient (void)
{
}


// Instance properties accessors.
const QHostAddress& qtractorOscClient::host (void) const
{
	return m_host;
}

unsigned short qtractorOscClient::port (void) const
{
	return m_port;
}


// Data senders.
void qtractorOscClient::sendData ( const QString& sPath, const QVariant& v )
{
	QUdpSocket::writeDatagram(message(sPath, v), m_host, m_port);
}


//---------------------------------------------------------------------------
// OSC command slots implementation...

#include "qtractorSession.h"
#include "qtractorSessionCommand.h"
#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"
#include "qtractorAudioClip.h"


// Kind of singleton reference.
qtractorOscControl* qtractorOscControl::g_pOscControl = NULL;

// Contructor.
qtractorOscControl::qtractorOscControl (void)
{
	m_pOscServer = new qtractorOscServer(QTRACTOR_OSC_SERVER_PORT);

	// Add some command action slots
	m_pOscServer->addPath("/AddAudioTrack", QVariant::String,
		this, SLOT(addAudioTrackSlot(const QVariant&)));
	m_pOscServer->addPath("/AddAudioClip", QVariant::List,
		this, SLOT(addAudioClipSlot(const QVariant&)));
	m_pOscServer->addPath("/AddAudioClipOnUniqueTrack", QVariant::List,
		this, SLOT(addAudioClipOnUniqueTrackSlot(const QVariant&)));
	m_pOscServer->addPath("/EnsureUniqueTrack", QVariant::String,
		this, SLOT(ensureUniqueTrackSlot(const QVariant&)));
	m_pOscServer->addPath("/SetGlobalTempo", QVariant::List,
		this, SLOT(setGlobalTempoSlot(const QVariant&)));
	m_pOscServer->addPath("/AdvanceLoopRange", QVariant::List,
		this, SLOT(advanceLoopRangeSlot(const QVariant&)));

	// Pseudo-singleton reference setup.
	g_pOscControl = this;	
}


// Destructor.
qtractorOscControl::~qtractorOscControl (void)
{
	// Pseudo-singleton reference shut-down.
	g_pOscControl = NULL;

	delete m_pOscServer;	
}


// Kind of singleton reference.
qtractorOscControl *qtractorOscControl::getInstance (void)
{
	return g_pOscControl;
}


// AddAudioTrack s:track-name
//
void qtractorOscControl::addAudioTrackSlot ( const QVariant& v )
{
	// Parse arguments...
	const QString& sTrackName = v.toString();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::addAudioTrackSlot(\"%s\"",
		sTrackName.toUtf8().constData());
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Put it in the form of an undoable command...
	int iTrack = pSession->tracks().count() + 1;
	const QColor color = qtractorTrack::trackColor(iTrack);
	qtractorTrack *pTrack = new qtractorTrack(pSession, qtractorTrack::Audio);
	pTrack->setTrackName(sTrackName);
	pTrack->setBackground(color);
	pTrack->setForeground(color.darker());
	pSession->execute(new qtractorAddTrackCommand(pTrack));
}

// AddAudioClip i:trackno s:filename i:start i:offset i:length
//
void qtractorOscControl::addAudioClipSlot ( const QVariant& v )
{
	// Parse arguments...
	QList<QVariant> args = v.toList();

	int iTrack = args.at(0).toInt();
	// *** FIXME Sending two strings in one OSC command does not seem to work
	QString sFilename = args.at(1).toString();
	unsigned long iClipStart
		= (args.count() > 2 ? args.at(2).toULongLong() : 0);
	unsigned long iClipOffset
		= (args.count() > 3 ? args.at(3).toULongLong() : 0);
	unsigned long iClipLength
		= (args.count() > 4 ? args.at(4).toULongLong() : 0);

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::addAudioClipSlot(%d, \"%s\", %lu, %lu, %lu)",
		iTrack, sFilename.toUtf8().constData(), iClipStart, iClipOffset, iClipLength);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorTrack *pTrack = pSession->tracks().at(iTrack);
	if (pTrack == NULL)
		return;

	// Put it in the form of an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("add audio clip"));
	qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
	pAudioClip->setFilename(sFilename);
	pAudioClip->setClipStart(iClipStart);
	pAudioClip->setClipOffset(iClipOffset);
	if (iClipLength > 0)
		pAudioClip->setClipLength(iClipLength);
	pClipCommand->addClip(pAudioClip, pTrack);
	pSession->execute(pClipCommand);
}


// EnsureUniqueTrackExistsForClip s:filename of clip
//
// Prepare a track associated with a given audio clip.
// Subsequent calls to AddAudioClipOnUniqueTrack will use the newly created track.
//
void qtractorOscControl::ensureUniqueTrackSlot ( const QVariant& v )
{
	// Parse arguments...
	const QString& sFilename = v.toString();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::ensureUniqueTrackExistsForClipSlot(\"%s\")",
		sFilename.toUtf8().constData());
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Find a track with this clip on it already
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip) {
			if (pClip->filename() == sFilename)
				return;
			pClip = pClip->next();
		}
		pTrack = pTrack->next();
	}

	// Create a new track
	addAudioTrackSlot(sFilename);
}


// AddAudioClipOnUniqueTrack s:filename i:start i:offset i:length i:fade f:gain
//
// Put an audio clip on a unique track. All clips with this filename will go
// on the same track. Start location is relative to the current edit tail.
//
void qtractorOscControl::addAudioClipOnUniqueTrackSlot ( const QVariant& v )
{
	// Parse arguments...
	QList<QVariant> args = v.toList();

	const QString& sFilename = args.at(5).toString();
	unsigned long iClipStart
		= (args.count() > 1 ? args.at(0).toULongLong() : 0);
	unsigned long iClipOffset
		= (args.count() > 2 ? args.at(1).toULongLong() : 0);
	unsigned long iClipLength
		= (args.count() > 3 ? args.at(2).toULongLong() : 0);
	unsigned long iClipFade
		= (args.count() > 4 ? args.at(3).toULongLong() : 0);
	float fClipGain
		= (args.count() > 5 ? args.at(4).toFloat() : 0.0f);

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::addAudioClipUniqueTrackSlot(\"%s\", %lu, %lu, %lu, %lu, %g)",
		sFilename.toUtf8().constData(), iClipStart, iClipOffset, iClipLength, iClipFade, fClipGain);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Relative position
	unsigned long iEditTail = pSession->editTail();
	iClipStart += iEditTail;

	// Find a track with this clip on it already
	bool bFound = false;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack&& !bFound) {
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip&& !bFound) {
			if (pClip->filename() == sFilename) {
				bFound = true;
				break;
			}
			pClip = pClip->next();
		}
		if (!bFound)
			pTrack = pTrack->next();
	}

	if (!bFound) {
		addAudioTrackSlot(sFilename);
		// Use the last (newest) track
		pTrack = pSession->tracks().last();
	}

	// Put it in the form of an undoable command...
	qtractorClipCommand *pClipCommand
		= new qtractorClipCommand(tr("add audio clip"));
	qtractorAudioClip *pAudioClip = new qtractorAudioClip(pTrack);
	pAudioClip->setFilename(sFilename);
	pAudioClip->setClipStart(iClipStart);
	pAudioClip->setClipOffset(iClipOffset);
	if (iClipLength > 0)
		pAudioClip->setClipLength(iClipLength);
	pAudioClip->setFadeInType(qtractorClip::Linear);
	pAudioClip->setFadeOutType(qtractorClip::Linear);
	pAudioClip->setFadeInLength(iClipFade);
	pAudioClip->setFadeOutLength(iClipFade);
	pAudioClip->setClipGain(fClipGain);
	pClipCommand->addClip(pAudioClip, pTrack);
	pSession->execute(pClipCommand);
}


// SetGlobalTempo f:tempo-bpm, i:beats-per-bar
//
// Sets the global tempo for the session.
//
void qtractorOscControl::setGlobalTempoSlot ( const QVariant& v )
{
	// Parse arguments...
	QList<QVariant> args = v.toList();

	float fTempo = args.at(0).toFloat();
	int iBeatsPerBar = args.at(1).toInt();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::setGlobalTempoSlot(%f, %d)", fTempo, iBeatsPerBar);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorSessionTempoCommand *pTempoCommand
		= new qtractorSessionTempoCommand(pSession, fTempo, 0, iBeatsPerBar);
	pSession->execute(pTempoCommand);
}


// advanceLoopRange i:edit-head, i:edit-tail
//
// Advances the edit head and tail forward.
//
void qtractorOscControl::advanceLoopRangeSlot ( const QVariant& v )
{
	// Parse arguments...
	QList<QVariant> args = v.toList();

	unsigned long iEditHead = args.at(0).toULongLong();
	unsigned long iEditTail = args.at(1).toULongLong();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::advanceLoopRangeSlot(%ul, %ul)", iEditHead, iEditTail);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorSessionLoopCommand *pLoopCommand
		= new qtractorSessionLoopCommand(pSession,
			pSession->loopEnd() + iEditHead,
			pSession->loopEnd() + iEditTail);
	pSession->execute(pLoopCommand);
}


// end of qtractorOscControl.cpp
