// qtractorNsmClient.cpp
//
/****************************************************************************
   Copyright (C) 2005-2013, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorNsmClient.h"

#include <QApplication>
#include <QFileInfo>

#define NSM_API_VERSION_MAJOR 1
#define NSM_API_VERSION_MINOR 0


#ifdef CONFIG_LIBLO

//---------------------------------------------------------------------------
// qtractorNsmClient - OSC (liblo) callback methods.

static
int osc_nsm_error ( const char */*path*/, const char */*types*/,
	lo_arg **argv, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	if (strcmp(&argv[0]->s, "/nsm/server/announce"))
		return -1;

	pNsmClient->nsm_announce_error(&argv[2]->s);
	return 0;
}


static
int osc_nsm_reply ( const char */*path*/, const char */*types*/,
	lo_arg **argv, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	if (strcmp(&argv[0]->s, "/nsm/server/announce"))
		return -1;

	pNsmClient->nsm_announce_reply(&argv[1]->s, &argv[2]->s, &argv[3]->s);
	return 0;
}


static
int osc_nsm_open ( const char */*path*/, const char */*types*/,
	lo_arg **argv, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	pNsmClient->nsm_open(&argv[0]->s, &argv[1]->s, &argv[2]->s);
	return 0;
}


static
int osc_nsm_save ( const char */*path*/, const char */*types*/,
	lo_arg **/*argv*/, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	pNsmClient->nsm_save();
	return 0;
}


static
int osc_nsm_loaded ( const char */*path*/, const char */*types*/,
	lo_arg **/*argv*/, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	pNsmClient->nsm_loaded();
	return 0;
}


static
int osc_nsm_show ( const char */*path*/, const char */*types*/,
	lo_arg **/*argv*/, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	pNsmClient->nsm_show();
	return 0;
}


static
int osc_nsm_hide ( const char */*path*/, const char */*types*/,
	lo_arg **/*argv*/, int /*argc*/, lo_message /*msg*/, void *user_data )
{
	qtractorNsmClient *pNsmClient
		= static_cast<qtractorNsmClient *> (user_data);
	if (pNsmClient == NULL)
		return -1;

	pNsmClient->nsm_hide();
	return 0;
}

#endif	// CONFIG_LIBLO


//---------------------------------------------------------------------------
// qtractorNsmClient - NSM OSC client agent.

// Constructor.
qtractorNsmClient::qtractorNsmClient (
	const QString& nsm_url, QObject *pParent )
	: QObject(pParent),
	#ifdef CONFIG_LIBLO
		m_address(NULL),
		m_thread(NULL),
		m_server(NULL),
	#endif
		m_active(false)
{
#ifdef CONFIG_LIBLO
	m_address = lo_address_new_from_url(nsm_url.toUtf8().constData());
	int proto = lo_address_get_protocol(m_address);
	m_thread = lo_server_thread_new_with_proto(NULL, proto, NULL);
	if (m_thread) {
		m_server = lo_server_thread_get_server(m_thread);
		lo_server_thread_add_method(m_thread,
			"/error", "sis", osc_nsm_error, this);
		lo_server_thread_add_method(m_thread,
			"/reply", "ssss", osc_nsm_reply, this);
		lo_server_thread_add_method(m_thread,
			"/nsm/client/open", "sss", osc_nsm_open, this);
		lo_server_thread_add_method(m_thread,
			"/nsm/client/save", "", osc_nsm_save, this);
		lo_server_thread_add_method(m_thread,
			"/nsm/client/session_is_loaded", "", osc_nsm_loaded, this);
		lo_server_thread_add_method(m_thread,
			"/nsm/client/show_optional_gui", "", osc_nsm_show, this);
		lo_server_thread_add_method(m_thread,
			"/nsm/client/hide_optional_gui", "", osc_nsm_hide, this);
		lo_server_thread_start(m_thread);
	}
#endif
}


// Destructor.
qtractorNsmClient::~qtractorNsmClient (void)
{
#ifdef CONFIG_LIBLO
	if (m_thread) {
		lo_server_thread_stop(m_thread);
		lo_server_thread_free(m_thread);
	}
	if (m_address)
		lo_address_free(m_address);
#endif
}


// Session clieant methods.
void qtractorNsmClient::announce (
	const QString& app_name, const QString& capabilities )
{
#ifdef CONFIG_LIBLO
	if (m_address && m_server) {
		const QFileInfo fi(QApplication::applicationFilePath());
		lo_send_from(m_address,
			m_server, LO_TT_IMMEDIATE,
			"/nsm/server/announce", "sssiii",
			app_name.toUtf8().constData(),
			capabilities.toUtf8().constData(),
			fi.fileName().toUtf8().constData(),
			NSM_API_VERSION_MAJOR,
			NSM_API_VERSION_MINOR,
			int(QApplication::applicationPid()));
	}
#endif
}


// Session activation accessor.
bool qtractorNsmClient::is_active (void) const
{
	return m_active;
}


// Session manager accessors.
const QString& qtractorNsmClient::manager (void) const
{
	return m_manager;
}


const QString& qtractorNsmClient::capabilities (void) const
{
	return m_capabilities;
}


// Session client accessors.
const QString& qtractorNsmClient::path_name (void) const
{
	return m_path_name;
}


const QString& qtractorNsmClient::display_name (void) const
{
	return m_display_name;
}


const QString& qtractorNsmClient::client_id (void) const
{
	return m_client_id;
}


// Session client methods.
void qtractorNsmClient::dirty ( bool is_dirty )
{
#ifdef CONFIG_LIBLO
	if (m_address && m_server && m_active) {
		const char *path = is_dirty
			? "/nsm/client/is_dirty"
			: "/nsm/client/is_clean";
		lo_send_from(m_address,
			m_server, LO_TT_IMMEDIATE,
			path, "");
	}
#endif
}


void qtractorNsmClient::visible ( bool is_visible )
{
#ifdef CONFIG_LIBLO
	if (m_address && m_server && m_active) {
		const char *path = is_visible
			? "/nsm/client/gui_is_shown"
			: "/nsm/client/gui_is_hidden";
		lo_send_from(m_address,
			m_server, LO_TT_IMMEDIATE,
			path, "");
	}
#endif
}


void qtractorNsmClient::progress ( float percent )
{
#ifdef CONFIG_LIBLO
	if (m_address && m_server && m_active) {
		lo_send_from(m_address,
			m_server, LO_TT_IMMEDIATE,
			"/nsm/client/progress", "f", percent);
	}
#endif
}


void qtractorNsmClient::message ( int priority, const QString& mesg )
{
#ifdef CONFIG_LIBLO
	if (m_address && m_server && m_active) {
		lo_send_from(m_address,
			m_server, LO_TT_IMMEDIATE,
			"/nsm/client/message", "is", priority,
			mesg.toUtf8().constData());
	}
#endif
}


// Session client reply methods.
void qtractorNsmClient::open_reply ( ReplyCode reply_code )
{
	reply("/nsm/client/open", reply_code);
}


void qtractorNsmClient::save_reply ( ReplyCode reply_code )
{
	reply("/nsm/client/save", reply_code);
}


void qtractorNsmClient::reply ( const QString& path, ReplyCode reply_code )
{
	const char *reply_mesg;
	switch (reply_code) {
		case ERR_OK:               reply_mesg = "OK";                   break;
		case ERR_GENERAL:          reply_mesg = "ERR_GENERAL";          break;
		case ERR_INCOMPATIBLE_API: reply_mesg = "ERR_INCOMPATIBLE_API"; break;
		case ERR_BLACKLISTED:      reply_mesg = "ERR_BLACKLISTED";      break;
		case ERR_LAUNCH_FAILED:    reply_mesg = "ERR_LAUNCH_FAILED";    break;
		case ERR_NO_SUCH_FILE:     reply_mesg = "ERR_NO_SUCH_FILE";     break;
		case ERR_NO_SESSION_OPEN:  reply_mesg = "ERR_NO_SESSION_OPEN";  break;
		case ERR_UNSAVED_CHANGES:  reply_mesg = "ERR_UNSAVED_CHANGES";  break;
		case ERR_NOT_NOW:          reply_mesg = "ERR_NOT_NOW";          break;
		default:                   reply_mesg = "(UNKNOWN)";            break;
	}

#ifdef CONFIG_LIBLO
	if (m_address && m_server) {
		if (reply_code == ERR_OK) {
			lo_send_from(m_address,
				m_server, LO_TT_IMMEDIATE,
				"/reply", "ss",
				path.toUtf8().constData(),
				reply_mesg);
		} else {
			lo_send_from(m_address,
				m_server, LO_TT_IMMEDIATE,
				"/error", "sis",
				path.toUtf8().constData(),
				int(reply_code),
				reply_mesg);
		}
	}
#endif
}


// Server announce error.
void qtractorNsmClient::nsm_announce_error (
	const char *mesg )
{
	m_active = false;

	m_manager.clear();
	m_capabilities.clear();

	m_path_name.clear();
	m_display_name.clear();
	m_client_id.clear();

	emit active(false);

	qWarning("NSM: Failed to register with server: %s.", mesg);
}


// Server announce reply.
void qtractorNsmClient::nsm_announce_reply (
	const char *mesg,
	const char *manager,
	const char *capabilities )
{
	m_active = true;

	m_manager = manager;
	m_capabilities = capabilities;

	emit active(true);

	qWarning("NSM: Successfully registered with server: %s.", mesg);
}


// Client open callback.
void qtractorNsmClient::nsm_open (
	const char *path_name,
	const char *display_name,
	const char *client_id )
{
	m_path_name = path_name;
	m_display_name = display_name;
	m_client_id = client_id;

#ifdef CONFIG_DEBUG
	qDebug("qtractorNsmClient::nsm_open: "
		"path_name=\"%s\" display_name=\"%s\" client_id=\"%s\".",
		m_path_name.toUtf8().constData(),
		m_display_name.toUtf8().constData(),
		m_client_id.toUtf8().constData());
#endif

	emit open();
}


// Client save callback.
void qtractorNsmClient::nsm_save (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorNsmClient::nsm_save: "
		"path_name=\"%s\" display_name=\"%s\" client_id=\"%s\".",
		m_path_name.toUtf8().constData(),
		m_display_name.toUtf8().constData(),
		m_client_id.toUtf8().constData());
#endif

	emit save();
}


// Client loaded callback.
void qtractorNsmClient::nsm_loaded (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorNsmClient::nsm_loaded: "
		"path_name=\"%s\" display_name=\"%s\" client_id=\"%s\".",
		m_path_name.toUtf8().constData(),
		m_display_name.toUtf8().constData(),
		m_client_id.toUtf8().constData());
#endif

	emit loaded();
}


// Client show optional GUI.
void qtractorNsmClient::nsm_show (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorNsmClient::nsm_show: "
		"path_name=\"%s\" display_name=\"%s\" client_id=\"%s\".",
		m_path_name.toUtf8().constData(),
		m_display_name.toUtf8().constData(),
		m_client_id.toUtf8().constData());
#endif

	emit show();
}


// Client hide optional GUI.
void qtractorNsmClient::nsm_hide (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorNsmClient::nsm_hide: "
		"path_name=\"%s\" display_name=\"%s\" client_id=\"%s\".",
		m_path_name.toUtf8().constData(),
		m_display_name.toUtf8().constData(),
		m_client_id.toUtf8().constData());
#endif

	emit hide();
}


// end of qtractorNsmClient.cpp
