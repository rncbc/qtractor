// qtractorOscControl.h
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

/* This code was originally borrowed, stirred, mangled and finally
 * adapted from Arnold Krille's libofqf, OSC for Qt4 implementation (GPL).
 *
 * Copyright (C) 2007, Arnold Krille <arnold@arnoldarts.de>
 */

#ifndef __qtractorOscControl_h
#define __qtractorOscControl_h

#include <QHostAddress>
#include <QVariant>


// Forward declarations.
class qtractorOscServer;
class qtractorOscClient;


// Forward declarations.
class QAbstractSocket;

class QTcpServer;

class qtractorOscNode;


//---------------------------------------------------------------------------
// qtractorOscPath - decl.

class qtractorOscPath : public QObject
{
	Q_OBJECT

public:

	// Constructors.
	qtractorOscPath(const QString& path, QVariant::Type vtype, QObject *pParent = 0);

	// Instance properties accessors.
	const QString& path() const;
	QVariant::Type vtype() const;

	// Transient properties accessors.
	const QHostAddress& host() const;
	unsigned short port() const;

	// Data notifier.
	void notifyData(const QVariant& v,
		const QHostAddress& host, unsigned short port);

signals:

	void dataSignal(const QVariant& v);

private:

	// Instance properties.
	QString        m_path;
	QVariant::Type m_vtype;

	// Transient properties.
	QHostAddress   m_host;
	unsigned short m_port;
};


//---------------------------------------------------------------------------
// qtractorOscSocket - decl.

class qtractorOscSocket
{
public:

	// Type of OSC socket
	enum Type { Udp, Tcp };

	// Constructor.
	qtractorOscSocket(Type stype, QAbstractSocket *pSocket = 0);

	// Desstructor.
	~qtractorOscSocket();

	// Socket accessors.
	Type stype() const;

	void connectToHost(
		const QHostAddress& host, unsigned short port, qtractorOscNode *pOscNode);

	bool hasPendingData() const;

	// Socket I/O.
	int readData(char *data, int size,
		QHostAddress *host, unsigned short *port);

	int writeData(const QByteArray& data,
		const QHostAddress& host, unsigned short port);

private:

	// Instance variables.
	Type m_stype;
	QAbstractSocket *m_pSocket;
	bool m_bAutoDelete;
};


//---------------------------------------------------------------------------
// qtractorOscNode - decl.

class qtractorOscNode : public QObject
{
	Q_OBJECT

public:

	// Path registry methods.
	qtractorOscPath *addPath(const QString& path,
		QVariant::Type vtype = QVariant::Invalid);
	qtractorOscPath *addPath(const QString& path,
		QVariant::Type vtype, const QObject *receiver, const char *method);

	void removePath(qtractorOscPath *pOscPath);

	void clear();

signals:

	void dataSignal(const QString& path, const QVariant& v);

protected:

	// Constructor.
	qtractorOscNode(qtractorOscSocket::Type stype, QObject *pParent = 0);

	// Destructor.
	~qtractorOscNode();

	// Basic data converters.
	static QByteArray fromString (const QString& s);
	static QByteArray fromInt    (int i);
	static QByteArray fromFloat  (float f);

	static QString toString (const QByteArray& a);
	static int     toInt    (const QByteArray& a);
	static float   toFloat  (const QByteArray& a);

	// Specific data converters.
	static QByteArray reverse(const QByteArray& a);
	static void parseArgs(const QVariant& v, QString& s, QByteArray& a);
	static QByteArray message(const QString& path, const QVariant& v);

	// Node methods.
	void connectToHost(
		const QHostAddress& host, unsigned short port);

	int writeData(const QByteArray& data,
		const QHostAddress& host, unsigned short port);

protected slots:

	// Data receiver.
	void readyReadSlot();

private:

	// Instance variables.
	qtractorOscSocket m_socket;

	// Path registry.
	QHash<QString, qtractorOscPath *> m_paths;
};


//---------------------------------------------------------------------------
// qtractorOscServer - decl.

class qtractorOscServer : public qtractorOscNode
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOscServer(qtractorOscSocket::Type stype,
		const QHostAddress& host, unsigned short port, QObject *pParent = 0);

	// Destructor.
	~qtractorOscServer();

protected slots:

	// New connection receiver.
	void newConnectionSlot();

private:

	// Instance variables.
	QTcpServer *m_pTcpServer;
};


//---------------------------------------------------------------------------
// qtractorOscClient - decl.

class qtractorOscClient : public qtractorOscNode
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOscClient(qtractorOscSocket::Type stype,
		const QHostAddress& host, unsigned short port, QObject *pParent = 0);

	// Instance accessors.
	const QHostAddress& host() const;
	unsigned short port() const;

public slots:

	// Data senders.
	void sendData(const QString& path, const QVariant& v = QVariant::Invalid);

private:

	// Instance properties.
	QHostAddress   m_host;
	unsigned short m_port;
};


//---------------------------------------------------------------------------
// OSC command slots implementation...

#define QTRACTOR_OSC_SERVER_PORT 5000


class qtractorOscControl : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOscControl(unsigned short port = QTRACTOR_OSC_SERVER_PORT);

	// Destructor.
	~qtractorOscControl();

	// Pseudo-singleton instance accessor.
	static qtractorOscControl *getInstance();

public slots:

	void addAudioTrackSlot(const QVariant& v);
	void addAudioClipSlot(const QVariant& v);
	void addAudioClipUniqueTrackSlot(const QVariant& v);
	void ensureUniqueTrackSlot(const QVariant& v);
	void setGlobalTempoSlot(const QVariant& v);
	void advanceLoopRangeSlot(const QVariant& v);

private:

	// Instance variables.
	qtractorOscServer *m_pOscServer;

	// Pseudo-singleton instance.
	static qtractorOscControl *g_pOscControl;
};


#endif  // __qtractorOscControl_h

// end of qtractorOscControl.h
