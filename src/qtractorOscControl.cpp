// qtractorOscControl.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include <QAction>
#include <QMenu>


//---------------------------------------------------------------------------
// qtractorOscPath - impl.

// Constructors.
qtractorOscPath::qtractorOscPath (
	const QString& path, QMetaType::Type vtype, QObject *pParent)
	: QObject(pParent), m_path(path), m_vtype(vtype),
		m_action(nullptr), m_host(QHostAddress::Null), m_port(0)
{
}

qtractorOscPath::qtractorOscPath (
	const QString& path, QAction *pAction, QObject *pParent)
	: QObject(pParent), m_path(path), m_vtype(QMetaType::QObjectStar),
		m_action(pAction), m_host(QHostAddress::Null), m_port(0)
{
}


// Instance properties accessors.
const QString& qtractorOscPath::path (void) const
{
	return m_path;
}

QMetaType::Type qtractorOscPath::vtype (void) const
{
	return m_vtype;
}


QAction *qtractorOscPath::action (void) const
{
	return m_action;
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
	if (m_vtype == vtype(v)) { 
		m_host = host;
		m_port = port;
		emit dataSignal(v);
	}
}


// Meta-type id caster. (static)
QMetaType::Type qtractorOscPath::vtype ( const QVariant& v )
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	return static_cast<QMetaType::Type> (v.metaType().id());
#else
	return static_cast<QMetaType::Type> (v.type());
#endif
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
	: qtractorOscPath(QString(), QMetaType::UnknownType, pParent), m_socket(stype)
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
	switch (vtype(v)) {
	case QMetaType::Int:
		types += 'i';
		args += fromInt(v.toInt());
		break;
	case QMetaType::Double:
		types += 'f';
		args += fromFloat(float(v.toDouble()));
		break;
	case QMetaType::QString:
		types += 's';
		args += fromString(v.toString());
		break;
	case QMetaType::QVariantList:
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
	const QString& path, QMetaType::Type vtype )
{
	qtractorOscPath *pOscPath = new qtractorOscPath(path, vtype, this);
	m_paths.insert(pOscPath->path(), pOscPath);
	return pOscPath;
}

qtractorOscPath *qtractorOscNode::addPath (
	const QString& path, QMetaType::Type vtype,
	const QObject *receiver, const char *method )
{
	qtractorOscPath *pOscPath = addPath(path, vtype);
	QObject::connect(pOscPath,
		SIGNAL(dataSignal(const QVariant&)),
		receiver, method);
	return pOscPath;
}


qtractorOscPath *qtractorOscNode::addPath (
	const QString& path, QAction *pAction )
{
	qtractorOscPath *pOscPath = new qtractorOscPath(path, pAction, this);
	m_paths.insert(pOscPath->path(), pOscPath);
	return pOscPath;
}


void qtractorOscNode::removePath ( qtractorOscPath *pOscPath )
{
	if (pOscPath) {
		m_paths.remove(pOscPath->path());
		delete pOscPath;
	}
}


void qtractorOscNode::removePath ( const QString& path )
{
	removePath(m_paths.value(path, nullptr));
}


qtractorOscPath *qtractorOscNode::findPath ( const QString& path )
{
	return m_paths.value(path, nullptr);
}


QList<qtractorOscPath *> qtractorOscNode::paths (void) const
{
	return m_paths.values();
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
			qtractorOscPath *pOscPath = m_paths.value(path, nullptr);
			if (pOscPath) {
				QAction *pAction = pOscPath->action();
				if (pAction)
					pAction->trigger();
				else
					pOscPath->notifyData(args, host, port);
			}
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
qtractorOscControl* qtractorOscControl::g_pOscControl = nullptr;


// Contructor.
qtractorOscControl::qtractorOscControl ( unsigned short port )
{
	m_pOscServer = new qtractorOscServer(
		qtractorOscSocket::Udp, QHostAddress::LocalHost, port);

	// Pseudo-singleton reference setup.
	g_pOscControl = this;
}


// Destructor.
qtractorOscControl::~qtractorOscControl (void)
{
	// Pseudo-singleton reference shut-down.
	g_pOscControl = nullptr;

	delete m_pOscServer;
}


// Kind of singleton reference.
qtractorOscControl *qtractorOscControl::getInstance (void)
{
	return g_pOscControl;
}


// Action path registry.
void qtractorOscControl::addAction ( QAction *pAction )
{
	m_pOscServer->addPath(actionPath(pAction), pAction);
}

void qtractorOscControl::removeAction ( QAction *pAction )
{
	m_pOscServer->removePath(actionPath(pAction));
}

void qtractorOscControl::clearActions (void)
{
	const QList<qtractorOscPath *> paths(m_pOscServer->paths());
	QListIterator<qtractorOscPath *> iter(paths);
	while (iter.hasNext()) {
		qtractorOscPath *pOscPath = iter.next();
		if (pOscPath && pOscPath->action())
			m_pOscServer->removePath(pOscPath);
	}
}


QAction *qtractorOscControl::findAction ( const QString& sPath ) const
{
	QAction *pAction = nullptr;

	qtractorOscPath *pOscPath = m_pOscServer->findPath(sPath);
	if (pOscPath)
		pAction = pOscPath->action();

	return pAction;
}


QList<QAction *> qtractorOscControl::actions (void) const
{
	QList<QAction *> actions;

	QListIterator<qtractorOscPath *> iter(m_pOscServer->paths());
	while (iter.hasNext()) {
		qtractorOscPath *pOscPath = iter.next();
		if (pOscPath) {
			QAction *pAction = pOscPath->action();
			if (pAction)
				actions.append(pAction);
		}
	}

	return actions;
}


QString qtractorOscControl::actionPath ( QAction *pAction, const QString& sPath )
{
	QString sActionPath = sPath;
	if (sActionPath.isEmpty())
		sActionPath = '/' + pAction->text();

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 2)
	QListIterator<QObject *> iter(pAction->associatedObjects());
#else
	QListIterator<QWidget *> iter(pAction->associatedWidgets());
#endif
	while (iter.hasNext()) {
		QMenu *pMenu = qobject_cast<QMenu *> (iter.next());
		if (pMenu) {
			sActionPath = '/' + pMenu->title() + sActionPath;
			pAction = pMenu->menuAction();
			if (pAction)
				sActionPath = actionPath(pAction, sActionPath);
		}
	}

	return sActionPath.remove(' ').remove('&').remove('.');
}


// end of qtractorOscControl.cpp
