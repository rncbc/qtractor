// qtractorOscControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2015, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QUdpSocket>
#include <QTcpSocket>
#include <QTcpServer>


//---------------------------------------------------------------------------
// qtractorOscPath - impl.

// Constructor.
qtractorOscPath::qtractorOscPath (
	const QString& path, QVariant::Type vtype, QObject *pParent)
	: QObject(pParent), m_path(path), m_vtype(vtype),
		m_host(QHostAddress::Null), m_port(0)
{
}


// Instance properties accessors.
const QString& qtractorOscPath::path (void) const
{
	return m_path;
}

QVariant::Type qtractorOscPath::vtype (void) const
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
void qtractorOscPath::notifyData (
	const QVariant& v, const QHostAddress& host, unsigned short port )
{
	if (v.type() == m_vtype) {
		m_host = host;
		m_port = port;
		emit dataSignal(v);
	}
}


//---------------------------------------------------------------------------
// qtractorOscSocket - impl.

// Constructor.
qtractorOscSocket::qtractorOscSocket ( qtractorOscSocket::Type stype, QAbstractSocket *pSocket )
	: m_stype(stype), m_pSocket(pSocket), m_bAutoDelete(pSocket == 0)
{
	if (m_bAutoDelete) {
		if (m_stype == qtractorOscSocket::Tcp) {
			m_pSocket = new QTcpSocket();
		} else {
			m_pSocket = new QUdpSocket();
		}
	}
}


// Destructor.
qtractorOscSocket::~qtractorOscSocket (void)
{
	if (m_bAutoDelete) delete m_pSocket;
}


// Socket accessors.
qtractorOscSocket::Type qtractorOscSocket::stype (void) const
{
	return m_stype;
}


void qtractorOscSocket::connectToHost (
	const QHostAddress& host, unsigned short port, qtractorOscNode *pOscNode )
{
	if (m_stype == qtractorOscSocket::Tcp) {
		QTcpSocket *pTcpSocket = static_cast<QTcpSocket *> (m_pSocket);
		if (pTcpSocket) {
			pTcpSocket->connectToHost(host, port);
		//	pTcpSocket->waitForConnected(3000);
		}
	} else {
		QUdpSocket *pUdpSocket = static_cast<QUdpSocket *> (m_pSocket);
		if (pUdpSocket)
			pUdpSocket->bind(host, port);
	}

	QObject::connect(
		m_pSocket, SIGNAL(readyRead()),
		pOscNode, SLOT(readyReadSlot()));
}


bool qtractorOscSocket::hasPendingData (void) const
{
	bool ret = false;

	if (m_stype == qtractorOscSocket::Tcp) {
		QTcpSocket *pTcpSocket = static_cast<QTcpSocket *> (m_pSocket);
		if (pTcpSocket)
			ret = pTcpSocket->bytesAvailable();
	} else {
		QUdpSocket *pUdpSocket = static_cast<QUdpSocket *> (m_pSocket);
		if (pUdpSocket)
			ret = pUdpSocket->hasPendingDatagrams();
	}

	return ret;
}


// Socket I/O.
int qtractorOscSocket::readData (
	char *data, int size, QHostAddress *host, unsigned short *port )
{
	int ret = 0;

	if (m_stype == qtractorOscSocket::Tcp) {
		QTcpSocket *pTcpSocket = static_cast<QTcpSocket *> (m_pSocket);
		if (pTcpSocket) {
			ret = pTcpSocket->read(data, size);
			if (host) *host = pTcpSocket->peerAddress();
			if (port) *port = pTcpSocket->peerPort();
		}
	} else {
		QUdpSocket *pUdpSocket = static_cast<QUdpSocket *> (m_pSocket);
		if (pUdpSocket)
			ret = pUdpSocket->readDatagram(data, size, host, port);
	}

	return ret;
}


int qtractorOscSocket::writeData (
	const QByteArray& data, const QHostAddress& host, unsigned short port )
{
	int ret = 0;

	if (m_stype == qtractorOscSocket::Tcp) {
		QTcpSocket *pTcpSocket = static_cast<QTcpSocket *> (m_pSocket);
		if (pTcpSocket)
			ret = pTcpSocket->write(data);
	} else {
		QUdpSocket *pUdpSocket = static_cast<QUdpSocket *> (m_pSocket);
		if (pUdpSocket)
			ret = pUdpSocket->writeDatagram(data, host, port);
	}

	return ret;
}


//---------------------------------------------------------------------------
// qtractorOscNode - impl.

// Constructor.
qtractorOscNode::qtractorOscNode ( qtractorOscSocket::Type stype, QObject *pParent )
	: QObject(pParent), m_socket(stype)
{
}


// Destructor.
qtractorOscNode::~qtractorOscNode (void)
{
	clear();
}


// Basic data converters.
QByteArray qtractorOscNode::fromString ( const QString& s )
{
	QByteArray ret = s.toUtf8();
	ret.append(char(0));
	while (ret.length() % 4)
		ret.append(char(0));
	return ret;
}

QByteArray qtractorOscNode::fromInt ( int i )
{
	return reverse(QByteArray((char *) (&i), 4));
}

QByteArray qtractorOscNode::fromFloat ( float f )
{
	return reverse(QByteArray((char *) (&f), 4));
}


QString qtractorOscNode::toString ( const QByteArray& a )
{
	return QString(a.data());
}

int qtractorOscNode::toInt ( const QByteArray& a )
{
	return *(int *) reverse(a.left(4)).data();
}

float qtractorOscNode::toFloat( const QByteArray& a )
{
	return *(float *) reverse(a.left(4)).data();
}


// Specific data converters.
QByteArray qtractorOscNode::reverse ( const QByteArray& a )
{
	QByteArray ret;
	for (int i = 0; i < a.size(); ++i)
		ret.prepend(a.at(i));
	return ret;
}


void qtractorOscNode::parseArgs (
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


QByteArray qtractorOscNode::message ( const QString& path, const QVariant& v )
{
	QString types(',');
	QByteArray args;

	parseArgs(v, types, args);

	QByteArray ret = fromString(path);
	ret.append(fromString(types));
	ret.append(args);
	return ret;
}


// Path registry methods.
qtractorOscPath *qtractorOscNode::addPath (
	const QString& path, QVariant::Type vtype )
{
	qtractorOscPath *pOscPath = new qtractorOscPath(path, vtype, this);
	m_paths.insert(pOscPath->path(), pOscPath);
	return pOscPath;
}

qtractorOscPath *qtractorOscNode::addPath (
	const QString& path, QVariant::Type vtype,
	const QObject *receiver, const char *method )
{
	qtractorOscPath *pOscPath = addPath(path, vtype);
	QObject::connect(pOscPath,
		SIGNAL(dataSignal(const QVariant&)),
		receiver, method);
	return pOscPath;
}


void qtractorOscNode::removePath ( qtractorOscPath *pOscPath )
{
	m_paths.remove(pOscPath->path());
	delete pOscPath;
}


void qtractorOscNode::clear (void)
{
	qDeleteAll(m_paths);
	m_paths.clear();
}


// Node methods.
void qtractorOscNode::connectToHost (
	const QHostAddress& host, unsigned short port )
{
	m_socket.connectToHost(host, port, this);
}


int qtractorOscNode::writeData ( const QByteArray& data,
	const QHostAddress& host, unsigned short port )
{
	return m_socket.writeData(data, host, port);
}


// Data receiver.
void qtractorOscNode::readyReadSlot (void)
{
	QAbstractSocket *pSocket = qobject_cast<QAbstractSocket *> (sender());
	if (pSocket == 0)
		return;

	qtractorOscSocket socket(m_socket.stype(), pSocket);

	while (socket.hasPendingData()) {
		QHostAddress host;
		unsigned short port = 0;
		const int nsize = 1024;
		QByteArray data(nsize, char(0));
		int nread = socket.readData(data.data(), nsize, &host, &port);
		int i = 0;
		while (i < nread && data[i] == '/') {
			QString path;
			QString types;
			QVariant args;
			for ( ; i < nread && data[i] != char(0); ++i)
				path += data[i];
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
			qtractorOscPath *pOscPath = m_paths.value(path, 0);
			if (pOscPath)
				pOscPath->notifyData(args, host, port);
			while (i < nread && data[++i] != '/');
		}
	}
}


//---------------------------------------------------------------------------
// qtractorOscServer - impl.

// Constructor.
qtractorOscServer::qtractorOscServer ( qtractorOscSocket::Type stype,
	const QHostAddress& host, unsigned short port, QObject *pParent )
	: qtractorOscNode(stype, pParent), m_pTcpServer(0)
{
	if (stype == qtractorOscSocket::Tcp) {
		m_pTcpServer = new QTcpServer(this);
		m_pTcpServer->listen(host, port);
		QObject::connect(m_pTcpServer,
			SIGNAL(newConnection()),
			SLOT(newConnectionSlot()));
	}
	else connectToHost(host, port);
}


// Destructor
qtractorOscServer::~qtractorOscServer (void)
{
	if (m_pTcpServer)
		delete m_pTcpServer;
}


// New connection receiver.
void qtractorOscServer::newConnectionSlot (void)
{
	while (m_pTcpServer && m_pTcpServer->hasPendingConnections()) {
		QTcpSocket *pTcpSocket = m_pTcpServer->nextPendingConnection();
		QObject::connect(pTcpSocket,
			SIGNAL(readyRead()),
			SLOT(readyReadSlot()));
	}
}


//---------------------------------------------------------------------------
// qtractorOscClient - impl.

// Constructor.
qtractorOscClient::qtractorOscClient ( qtractorOscSocket::Type stype,
	const QHostAddress& host, unsigned short port, QObject *pParent )
	: qtractorOscNode(stype, pParent), m_host(host), m_port(port)
{
	if (stype == qtractorOscSocket::Tcp)
		qtractorOscNode::connectToHost(host, port);
	else
		qtractorOscNode::connectToHost(QHostAddress::LocalHost, 0);
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
void qtractorOscClient::sendData ( const QString& path, const QVariant& v )
{
	qtractorOscNode::writeData(message(path, v), m_host, m_port);
}


//---------------------------------------------------------------------------
// OSC command slots implementation...

#include "qtractorSession.h"
#include "qtractorSessionCommand.h"
#include "qtractorTimeScaleCommand.h"
#include "qtractorTrackCommand.h"
#include "qtractorClipCommand.h"
#include "qtractorAudioClip.h"


// Kind of singleton reference.
qtractorOscControl* qtractorOscControl::g_pOscControl = NULL;


// Contructor.
qtractorOscControl::qtractorOscControl ( unsigned short port )
{
	m_pOscServer = new qtractorOscServer(
		qtractorOscSocket::Udp, QHostAddress::LocalHost, port);

	// Add some command action slots
	m_pOscServer->addPath("/AddAudioTrack", QVariant::String,
		this, SLOT(addAudioTrackSlot(const QVariant&)));
	m_pOscServer->addPath("/AddAudioClip", QVariant::List,
		this, SLOT(addAudioClipSlot(const QVariant&)));
	m_pOscServer->addPath("/AddAudioClipUniqueTrack", QVariant::List,
		this, SLOT(addAudioClipUniqueTrackSlot(const QVariant&)));
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
	const int iTrack = pSession->tracks().count() + 1;
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
	const QList<QVariant>& args = v.toList();

	const int iTrack = args.at(0).toInt();
	// *** FIXME Sending two strings in one OSC command does not seem to work
	const QString& sFilename = args.at(1).toString();
	const unsigned long iClipStart
		= (args.count() > 2 ? args.at(2).toUInt() : 0);
	const unsigned long iClipOffset
		= (args.count() > 3 ? args.at(3).toUInt() : 0);
	const unsigned long iClipLength
		= (args.count() > 4 ? args.at(4).toUInt() : 0);

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


// EnsureUniqueTrackExistsForClip s:filename
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


// AddAudioClipUniqueTrack s:filename i:start i:offset i:length i:fade f:gain
//
// Put an audio clip on a unique track. All clips with this filename will go
// on the same track. Start location is relative to the current edit tail.
//
void qtractorOscControl::addAudioClipUniqueTrackSlot ( const QVariant& v )
{
	// Parse arguments...
	const QList<QVariant>& args = v.toList();

	const QString& sFilename = args.at(0).toString();
	const unsigned long iClipStart
		= (args.count() > 1 ? args.at(1).toUInt() : 0);
	const unsigned long iClipOffset
		= (args.count() > 2 ? args.at(2).toUInt() : 0);
	const unsigned long iClipLength
		= (args.count() > 3 ? args.at(3).toUInt() : 0);
	const unsigned long iClipFade
		= (args.count() > 4 ? args.at(4).toUInt() : 0);
	const float fClipGain
		= (args.count() > 5 ? args.at(5).toFloat() : 0.0f);

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::addAudioClipUniqueTrackSlot(\"%s\", %lu, %lu, %lu, %lu, %g)",
		sFilename.toUtf8().constData(), iClipStart, iClipOffset, iClipLength, iClipFade, fClipGain);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Relative position
	const unsigned long iClipStartEx = iClipStart + pSession->editTail();

	// Find a track with this clip on it already
	bool bFound = false;
	qtractorTrack *pTrack = pSession->tracks().first();
	while (pTrack) {
		qtractorClip *pClip = pTrack->clips().first();
		while (pClip) {
			if (pClip->filename() == sFilename) {
				bFound = true;
				break;
			}
			pClip = pClip->next();
		}
		if (bFound)
			break;
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
	pAudioClip->setClipStart(iClipStartEx);
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
	const QList<QVariant>& args = v.toList();

	const float fTempo = args.at(0).toFloat();
	const int iBeatsPerBar = args.at(1).toInt();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::setGlobalTempoSlot(%f, %d)", fTempo, iBeatsPerBar);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	// Find appropriate node...
	qtractorTimeScale *pTimeScale = pSession->timeScale();
	qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
	qtractorTimeScale::Node *pNode = cursor.seekFrame(pSession->playHead());

	// Now, express the change as a undoable command...
	pSession->execute(new qtractorTimeScaleUpdateNodeCommand(
		pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, pNode->beatDivisor));
}


// advanceLoopRange i:edit-head, i:edit-tail
//
// Advances the edit head and tail forward.
//
void qtractorOscControl::advanceLoopRangeSlot ( const QVariant& v )
{
	// Parse arguments...
	const QList<QVariant>& args = v.toList();

	const unsigned long iEditHead = args.at(0).toUInt();
	const unsigned long iEditTail = args.at(1).toUInt();

#ifdef CONFIG_DEBUG
	qDebug("qtractorOscControl::advanceLoopRangeSlot(%lu, %lu)", iEditHead, iEditTail);
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
