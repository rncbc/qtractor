// qtractorOscControl.h
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

/* This code was originally borrowed, stirred, mangled and finally
 * adapted from Arnold Krille's libofqf, OSC for Qt4 implementation (GPL).
 *
 * Copyright (C) 2007, Arnold Krille <arnold@arnoldarts.de>
 */

#ifndef __qtractorOscControl_h
#define __qtractorOscControl_h

#include <QUdpSocket>
#include <QVariant>


// Forward declarations.
class qtractorOscServer;
class qtractorOscClient;


//---------------------------------------------------------------------------
// qtractorOscBase

class qtractorOscBase : public QUdpSocket
{
	Q_OBJECT

protected:

	// Constructor.
	qtractorOscBase(QObject *pParent = 0);

	// Destructor.
	~qtractorOscBase();

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
	static QByteArray message(const QString& sPath, const QVariant& v);
};


//---------------------------------------------------------------------------
// qtractorOscPath

class qtractorOscPath : public QObject
{
	Q_OBJECT

public:

	// Constructors.
	qtractorOscPath(const QString& sPath,
		QVariant::Type vtype, qtractorOscServer *pServer);

	// Instance properties accessors.
	const QString& path() const;
	QVariant::Type type() const;
	qtractorOscServer *server() const;

	// Transient properties accessors.
	const QHostAddress& host() const;
	unsigned short port() const;

	// Data notifier.
	void signalData(const QVariant& v,
		const QHostAddress& host, unsigned short port);

signals:

	void data(const QVariant& v);

private:

	// Instance properties.
	qtractorOscServer *m_pServer;
	QString            m_sPath;
	QVariant::Type     m_vtype;

	// Transient properties.
	QHostAddress       m_host;
	unsigned short     m_port;
};


//---------------------------------------------------------------------------
// qtractorOscServer

class qtractorOscServer : public qtractorOscBase
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOscServer(unsigned short port, QObject *pParent = 0);

	// Destructor.
	~qtractorOscServer();

	// Path registry methods.
	qtractorOscPath *addPath(const QString& sPath,
		QVariant::Type vtype = QVariant::Invalid);
	qtractorOscPath *addPath(const QString& sPath,
		QVariant::Type vtype, const QObject *pReceiver, const char *pMember);

	void removePath(qtractorOscPath *pOscPath);

	void clear();

signals:

	void data(const QString& sPath, const QVariant& v);

private slots:

	// Data receiver.
	void readyReadSlot();

private:

	// Path registry.
	QHash<QString, qtractorOscPath *> m_paths;
};


//---------------------------------------------------------------------------
// qtractorOscClient

class qtractorOscClient : public qtractorOscBase
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOscClient(
		const QHostAddress& host, unsigned short port, QObject *pParent = 0);

	// Desstructor.
	~qtractorOscClient();

	// Instance accessors.
	const QHostAddress& host() const;
	unsigned short port() const;

public slots:

	// Data senders.
	void sendData(const QString& sPath, const QVariant& v = QVariant::Invalid);

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
	qtractorOscControl();

	// Destructor.
	~qtractorOscControl();

	// Pseudo-singleton instance accessor.
	static qtractorOscControl *getInstance();

public slots:

	void addAudioTrackSlot(const QVariant& v);
	void addAudioClipSlot(const QVariant& v);
	void addAudioClipOnUniqueTrackSlot(const QVariant& v);
	void ensureUniqueTrackSlot(const QVariant& v);

private:

	// Instance variables.
	qtractorOscServer *m_pOscServer;

	// Pseudo-singleton instance.
	static qtractorOscControl *g_pOscControl;	
};


#endif  // __qtractorOscControl_h

// end of qtractorOscControl.h
