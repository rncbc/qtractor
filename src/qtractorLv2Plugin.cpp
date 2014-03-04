// qtractorLv2Plugin.cpp
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

#include "qtractorAbout.h"

#ifdef CONFIG_LV2

#include "qtractorLv2Plugin.h"

#if defined(CONFIG_LV2_EVENT) || defined(CONFIG_LV2_ATOM)
#include "qtractorMidiBuffer.h"
#endif

#ifdef CONFIG_LV2_PRESETS
// LV2 Presets: standard directory access.
#include "qtractorOptions.h"
// For local file vs. URI manipulations.
#include <QDir>
#include <QUrl>
#endif

#include <math.h>


// URI map/unmap features.
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

static QHash<QString, uint32_t>    g_uri_map;
static QHash<uint32_t, QByteArray> g_ids_map;


static uint32_t qtractor_lv2_urid_map (
	LV2_URID_Map_Handle /*handle*/, const char *uri )
{
#ifdef CONFIG_LV2_EVENT
	if (strcmp(uri, LILV_URI_MIDI_EVENT) == 0)
	    return QTRACTOR_LV2_MIDI_EVENT_ID;
#endif

	return qtractorLv2Plugin::lv2_urid_map(uri);
}

static LV2_URID_Map g_lv2_urid_map =
	{ NULL, qtractor_lv2_urid_map };
static const LV2_Feature g_lv2_urid_map_feature =
	{ LV2_URID_MAP_URI, &g_lv2_urid_map };


static const char *qtractor_lv2_urid_unmap (
	LV2_URID_Unmap_Handle /*handle*/, uint32_t id )
{
#ifdef CONFIG_LV2_EVENT
	if (id == QTRACTOR_LV2_MIDI_EVENT_ID)
		return LILV_URI_MIDI_EVENT;
#endif

	return qtractorLv2Plugin::lv2_urid_unmap(id);
}

static LV2_URID_Unmap g_lv2_urid_unmap =
	{ NULL, qtractor_lv2_urid_unmap };
static const LV2_Feature g_lv2_urid_unmap_feature =
	{ LV2_URID_UNMAP_URI, &g_lv2_urid_unmap };


// URI map (uri_to_id) feature (DEPRECATED)
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"

static uint32_t qtractor_lv2_uri_to_id (
	LV2_URI_Map_Callback_Data /*data*/, const char *map, const char *uri )
{
#ifdef CONFIG_LV2_EVENT
	if ((map && strcmp(map, LV2_EVENT_URI) == 0) &&
		strcmp(uri, LILV_URI_MIDI_EVENT) == 0)
		return QTRACTOR_LV2_MIDI_EVENT_ID;
#endif
	return qtractorLv2Plugin::lv2_urid_map(uri);
}

static LV2_URI_Map_Feature g_lv2_uri_map =
	{ NULL, qtractor_lv2_uri_to_id };
static const LV2_Feature g_lv2_uri_map_feature =
	{ LV2_URI_MAP_URI, &g_lv2_uri_map };


#ifdef CONFIG_LV2_STATE

static const LV2_Feature g_lv2_state_feature =
	{ LV2_STATE_URI, NULL };

static LV2_State_Status qtractor_lv2_state_store ( LV2_State_Handle handle,
	uint32_t key, const void *value, size_t size, uint32_t type, uint32_t flags )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return LV2_STATE_ERR_UNKNOWN;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_state_store(%p, %d, %d, %d, %d)", pLv2Plugin,
		int(key), int(size), int(type), int(flags));
#endif

	return pLv2Plugin->lv2_state_store(key, value, size, type, flags);
}

static const void *qtractor_lv2_state_retrieve ( LV2_State_Handle handle,
	uint32_t key, size_t *size, uint32_t *type, uint32_t *flags )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return NULL;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_state_retrieve(%p, %d)", pLv2Plugin, int(key));
#endif

	return pLv2Plugin->lv2_state_retrieve(key, size, type, flags);
}

#endif	// CONFIG_LV2_STATE


// URI map helpers (static).
uint32_t qtractorLv2Plugin::lv2_urid_map ( const char *uri )
{
	const QString sUri(uri);

	QHash<QString, uint32_t>::ConstIterator iter
		= g_uri_map.constFind(sUri);
	if (iter == g_uri_map.constEnd()) {
		uint32_t id = g_uri_map.size() + 1000;
		g_uri_map.insert(sUri, id);
		g_ids_map.insert(id, sUri.toUtf8());
		return id;
	}

	return iter.value();
}

const char *qtractorLv2Plugin::lv2_urid_unmap ( uint32_t id )
{
	QHash<uint32_t, QByteArray>::ConstIterator iter
		= g_ids_map.constFind(id);
	if (iter == g_ids_map.constEnd())
		return NULL;

	return iter.value().constData();
}


#ifdef CONFIG_LV2_WORKER

// LV2 Worker/Schedule support.
#include <QThread>
#include <QMutex>
#include <QWaitCondition>

#include <jack/ringbuffer.h>


//----------------------------------------------------------------------
// class qtractorLv2Worker -- LV2 Worker/Schedule item decl.
//
class qtractorLv2WorkerThread;

class qtractorLv2Worker
{
public:

	// Constructor.
	qtractorLv2Worker(qtractorLv2Plugin *pLv2Plugin,
		const LV2_Feature *const *features);

	// Destructor.
	~qtractorLv2Worker();

	// Instance copy of worker schedule feature.
	LV2_Feature **lv2_features() const
		{ return m_lv2_features; }

	// Schedule work.
	void schedule(uint32_t size, const void *data);

	// Respond work.
	void respond(uint32_t size, const void *data);

	// Commit work.
	void commit();

	// Process work.
	void process();

	// Sync thread state flags accessors.
	void setWaitSync(bool bWaitSync)
		{ m_bWaitSync = bWaitSync; }
	bool isWaitSync() const
		{ return m_bWaitSync; }

private:

	// Instance members.
	qtractorLv2Plugin  *m_pLv2Plugin;

	LV2_Feature       **m_lv2_features;

	LV2_Feature         m_lv2_schedule_feature;
	LV2_Worker_Schedule m_lv2_schedule;

	volatile bool       m_bWaitSync;

	jack_ringbuffer_t  *m_pRequests;
	jack_ringbuffer_t  *m_pResponses;
	void               *m_pResponse;

	static qtractorLv2WorkerThread *g_pWorkerThread;
	static unsigned int             g_iWorkerRefCount;
};

static LV2_Worker_Status qtractor_lv2_worker_schedule (
	LV2_Worker_Schedule_Handle handle, uint32_t size, const void *data )
{
	qtractorLv2Worker *pLv2Worker
		= static_cast<qtractorLv2Worker *> (handle);
	if (pLv2Worker == NULL)
		return LV2_WORKER_ERR_UNKNOWN;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_worker_schedule(%p, %u, %p)", pLv2Worker, size, data);
#endif

	pLv2Worker->schedule(size, data);
	return LV2_WORKER_SUCCESS;
}

static LV2_Worker_Status qtractor_lv2_worker_respond (
	LV2_Worker_Respond_Handle handle, uint32_t size, const void *data )
{
	qtractorLv2Worker *pLv2Worker
		= static_cast<qtractorLv2Worker *> (handle);
	if (pLv2Worker == NULL)
		return LV2_WORKER_ERR_UNKNOWN;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_worker_respond(%p, %u, %p)", pLv2Worker, size, data);
#endif

	pLv2Worker->respond(size, data);
	return LV2_WORKER_SUCCESS;
}

//----------------------------------------------------------------------
// class qtractorLv2WorkerThread -- LV2 Worker/Schedule thread.
//
class qtractorLv2WorkerThread : public QThread
{
public:

	// Constructor.
	qtractorLv2WorkerThread(unsigned int iSyncSize = 128);
	// Destructor.
	~qtractorLv2WorkerThread();

	// Thread run state accessors.
	void setRunState(bool bRunState);
	bool runState() const;

	// Wake from executive wait condition.
	void sync(qtractorLv2Worker *pLv2Worker = NULL);

protected:

	// The main thread executive.
	void run();

private:

	// The peak file queue instance reference.
	unsigned int          m_iSyncSize;
	unsigned int          m_iSyncMask;
	qtractorLv2Worker   **m_ppSyncItems;

	volatile unsigned int m_iSyncRead;
	volatile unsigned int m_iSyncWrite;

	// Whether the thread is logically running.
	volatile bool m_bRunState;

	// Thread synchronization objects.
	QMutex m_mutex;
	QWaitCondition m_cond;
};

// Constructor.
qtractorLv2WorkerThread::qtractorLv2WorkerThread ( unsigned int iSyncSize )
{
	m_iSyncSize = (64 << 1);
	while (m_iSyncSize < iSyncSize)
		m_iSyncSize <<= 1;
	m_iSyncMask = (m_iSyncSize - 1);
	m_ppSyncItems = new qtractorLv2Worker * [m_iSyncSize];
	m_iSyncRead   = 0;
	m_iSyncWrite  = 0;

	::memset(m_ppSyncItems, 0, m_iSyncSize * sizeof(qtractorLv2Worker *));

	m_bRunState = false;
}

// Destructor.
qtractorLv2WorkerThread::~qtractorLv2WorkerThread (void)
{
	delete [] m_ppSyncItems;
}

// Run state accessor.
void qtractorLv2WorkerThread::setRunState ( bool bRunState )
{
	QMutexLocker locker(&m_mutex);

	m_bRunState = bRunState;
}

bool qtractorLv2WorkerThread::runState (void) const
{
	return m_bRunState;
}

// Wake from executive wait condition.
void qtractorLv2WorkerThread::sync ( qtractorLv2Worker *pLv2Worker )
{
	if (pLv2Worker == NULL) {
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			qtractorLv2Worker *pSyncItem = m_ppSyncItems[r];
			if (pSyncItem)
				pSyncItem->setWaitSync(false);
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
	//	m_iSyncRead = r;
	} else {
		// !pLv2Worker->isWaitSync()
		unsigned int n;
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		if (w > r) {
			n = ((r - w + m_iSyncSize) & m_iSyncMask) - 1;
		} else if (r > w) {
			n = (r - w) - 1;
		} else {
			n = m_iSyncSize - 1;
		}
		if (n > 0) {
			pLv2Worker->setWaitSync(true);
			m_ppSyncItems[w] = pLv2Worker;
			m_iSyncWrite = (w + 1) & m_iSyncMask;
		}
	}

	if (m_mutex.tryLock()) {
		m_cond.wakeAll();
		m_mutex.unlock();
	}
#ifdef CONFIG_DEBUG_0
	else qDebug("qtractorLv2WorkerThread[%p]::sync(): tryLock() failed.", this);
#endif
}

// The main thread executive cycle.
void qtractorLv2WorkerThread::run (void)
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2WorkerThread[%p]::run(): started...", this);
#endif

	m_mutex.lock();

	m_bRunState = true;

	while (m_bRunState) {
		// Do whatever we must, then wait for more...
		unsigned int r = m_iSyncRead;
		unsigned int w = m_iSyncWrite;
		while (r != w) {
			m_ppSyncItems[r]->process();
			++r &= m_iSyncMask;
			w = m_iSyncWrite;
		}
		m_iSyncRead = r;
		// Wait for sync...
		m_cond.wait(&m_mutex);
	}

	m_mutex.unlock();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2WorkerThread[%p]::run(): stopped.\n", this);
#endif
}

//----------------------------------------------------------------------
// class qtractorLv2Worker -- LV2 Worker/Schedule item impl.
//
qtractorLv2WorkerThread *qtractorLv2Worker::g_pWorkerThread   = NULL;
unsigned int             qtractorLv2Worker::g_iWorkerRefCount = 0;

// Constructor.
qtractorLv2Worker::qtractorLv2Worker (
	qtractorLv2Plugin *pLv2Plugin, const LV2_Feature *const *features )
{
	m_pLv2Plugin = pLv2Plugin;

	int iFeatures = 0;
	while (features && features[iFeatures]) { ++iFeatures; }

	m_lv2_features = new LV2_Feature * [iFeatures + 2];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_features[i] = (LV2_Feature *) features[i];

	m_lv2_schedule.handle = this;
	m_lv2_schedule.schedule_work = &qtractor_lv2_worker_schedule;

	m_lv2_schedule_feature.URI  = LV2_WORKER__schedule;
	m_lv2_schedule_feature.data = &m_lv2_schedule;
	m_lv2_features[iFeatures++] = &m_lv2_schedule_feature;

	m_lv2_features[iFeatures] = NULL;

	m_bWaitSync  = false;

	m_pRequests  = ::jack_ringbuffer_create(1024);
	m_pResponses = ::jack_ringbuffer_create(1024);
	m_pResponse  = (void *) ::malloc(1024);

	if (++g_iWorkerRefCount == 1) {
		g_pWorkerThread = new qtractorLv2WorkerThread();
		g_pWorkerThread->start();
	}
}

// Destructor.
qtractorLv2Worker::~qtractorLv2Worker (void)
{
	m_bWaitSync = false;

	if (--g_iWorkerRefCount == 0) {
		if (g_pWorkerThread->isRunning()) do {
			g_pWorkerThread->setRunState(false);
		//	g_pWorkerThread->terminate();
			g_pWorkerThread->sync();
		} while (!g_pWorkerThread->wait(100));
		delete g_pWorkerThread;
		g_pWorkerThread = NULL;
	}

	::jack_ringbuffer_free(m_pRequests);
	::jack_ringbuffer_free(m_pResponses);
	::free(m_pResponse);

	delete [] m_lv2_features;
}

// Schedule work.
void qtractorLv2Worker::schedule ( uint32_t size, const void *data )
{
	::jack_ringbuffer_write(m_pRequests, (const char *) &size, sizeof(size));
	::jack_ringbuffer_write(m_pRequests, (const char *) data, size);

	if (g_pWorkerThread)
		g_pWorkerThread->sync(this);
}

// Response work.
void qtractorLv2Worker::respond ( uint32_t size, const void *data )
{
	::jack_ringbuffer_write(m_pResponses, (const char *) &size, sizeof(size));
	::jack_ringbuffer_write(m_pResponses, (const char *) data, size);
}

// Commit work.
void qtractorLv2Worker::commit (void)
{
	const LV2_Worker_Interface *worker
		= m_pLv2Plugin->lv2_worker_interface(0);
	if (worker == NULL)
		return;

	const unsigned short iInstances = m_pLv2Plugin->instances();
	unsigned short i;

	uint32_t read_space = ::jack_ringbuffer_read_space(m_pResponses);
	while (read_space > 0) {
		uint32_t size = 0;
		::jack_ringbuffer_read(m_pResponses, (char *) &size, sizeof(size));
		::jack_ringbuffer_read(m_pResponses, (char *) m_pResponse, size);
		if (worker->work_response) {
			for (i = 0; i < iInstances; ++i) {
				LV2_Handle handle = m_pLv2Plugin->lv2_handle(i);
				if (handle)
					(*worker->work_response)(handle, size, m_pResponse);
			}
		}
		read_space -= sizeof(size) + size;
	}

	if (worker->end_run) {
		for (i = 0; i < iInstances; ++i) {
			LV2_Handle handle = m_pLv2Plugin->lv2_handle(i);
			if (handle)
				(*worker->end_run)(handle);
		}
	}
}

// Process work.
void qtractorLv2Worker::process (void)
{
	if (!m_bWaitSync)
		return;

	const LV2_Worker_Interface *worker
		= m_pLv2Plugin->lv2_worker_interface(0);
	if (worker == NULL)
		return;

	const unsigned short iInstances = m_pLv2Plugin->instances();
	unsigned short i;

	void *buf = NULL;
	uint32_t size = 0;

	uint32_t read_space = ::jack_ringbuffer_read_space(m_pRequests);
	if (read_space > 0)
		buf = ::malloc(read_space);

	while (read_space > 0) {
		::jack_ringbuffer_read(m_pRequests, (char *) &size, sizeof(size));
		::jack_ringbuffer_read(m_pRequests, (char *) buf, size);
		if (worker->work) {
			for (i = 0; i < iInstances; ++i) {
				LV2_Handle handle = m_pLv2Plugin->lv2_handle(i);
				if (handle)
					(*worker->work)(handle,
						qtractor_lv2_worker_respond, this, size, buf);
			}
		}
		read_space -= sizeof(size) + size;
	}

	if (buf) ::free(buf);

	m_bWaitSync = false;
}

#endif	// CONFIG_LV2_WORKER


#ifdef CONFIG_LV2_STATE_FILES

#include "qtractorSession.h"
#include "qtractorDocument.h"

#include <QFileInfo>
#include <QDir>

static char *qtractor_lv2_state_abstract_path (
	LV2_State_Map_Path_Handle handle, const char *absolute_path )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_state_abstract_path(%p, \"%s\")", pLv2Plugin, absolute_path);
#endif

	// abstract_path from absolute_path...
	const QString& sAbstractPath = qtractorDocument::addArchiveFile(
		QDir(pSession->sessionDir()).relativeFilePath(absolute_path));
	return ::strdup(sAbstractPath.toUtf8().constData());
}

static char *qtractor_lv2_state_absolute_path (
    LV2_State_Map_Path_Handle handle, const char *abstract_path )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_state_absolute_path(%p, \"%s\")", pLv2Plugin, abstract_path);
#endif

	QFileInfo fi(abstract_path);
	if (fi.isRelative())
		fi.setFile(QDir(pSession->sessionDir()), fi.filePath());

	// absolute_path from abstract_path...
	const QString& sAbsolutePath = fi.absoluteFilePath();
	return ::strdup(sAbsolutePath.toUtf8().constData());
}

#ifdef CONFIG_LV2_STATE_MAKE_PATH

static QString qtractor_lv2_state_prefix ( qtractorLv2Plugin *pLv2Plugin )
{
	QString sPrefix;

	const QString& sPresetName = pLv2Plugin->preset();
	if (sPresetName.isEmpty()) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			const QString& sSessionName = pSession->sessionName();
			if (!sSessionName.isEmpty()) {
				sPrefix += qtractorSession::sanitize(sSessionName);
				sPrefix += '-';
			}
		}
		const QString& sListName = pLv2Plugin->list()->name();
		if (!sListName.isEmpty()) {
			sPrefix += qtractorSession::sanitize(sListName);
			sPrefix += '-';
		}
	}

	sPrefix += pLv2Plugin->type()->label();
	sPrefix += '-';

	if (sPresetName.isEmpty()) {
		sPrefix += QString::number(pLv2Plugin->uniqueID(), 16);
	} else {
		sPrefix += qtractorSession::sanitize(sPresetName);
	}

	return sPrefix;
}

static char *qtractor_lv2_state_make_path (
    LV2_State_Make_Path_Handle handle, const char *relative_path )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return NULL;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return NULL;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_state_make_path(%p, \"%s\")", pLv2Plugin, relative_path);
#endif

	QFileInfo fi(relative_path);

	const QDir dir(pSession->sessionDir());
	if (fi.isAbsolute())
		fi.setFile(dir, fi.fileName());
	else
		fi.setFile(dir, fi.filePath());

	const QString& sFilePath = fi.filePath();
	if (!QDir(sFilePath).exists())
		dir.mkpath(sFilePath);

	const QString& sFileName
		= qtractor_lv2_state_prefix(pLv2Plugin)
		+ ".lv2_state";	// '.' + fi.fileName();

	// make_path...
	const QString& sMakePath
		= QFileInfo(QDir(sFilePath), sFileName).filePath();
	return ::strdup(sMakePath.toUtf8().constData());
}

#endif	// CONFIG_LV2_STATE_MAKE_PATH

#endif	// CONFIG_LV2_STATE_FILES


#ifdef CONFIG_LV2_BUF_SIZE

#include "qtractorAudioEngine.h"

#include "lv2/lv2plug.in/ns/ext/buf-size/buf-size.h"

static const LV2_Feature g_lv2_buf_size_feature =
	{ LV2_BUF_SIZE__boundedBlockLength, NULL };

#endif	// CONFIG_LV2_BUF_SIZE


static const LV2_Feature *g_lv2_features[] =
{
	&g_lv2_urid_map_feature,
	&g_lv2_urid_unmap_feature,
	&g_lv2_uri_map_feature,	// deprecated
#ifdef CONFIG_LV2_STATE
	&g_lv2_state_feature,
#endif
#ifdef CONFIG_LV2_BUF_SIZE
	&g_lv2_buf_size_feature,
#endif
	NULL
};


#ifdef CONFIG_LV2_UI

#include "qtractorPluginForm.h"

#define LV2_UI_TYPE_NONE       0
#define LV2_UI_TYPE_QT4        1
#define LV2_UI_TYPE_EXTERNAL   2
#define LV2_UI_TYPE_GTK        3
#define LV2_UI_TYPE_X11        4

#if QT_VERSION < 0x050000
#define LV2_UI_HOST_URI	LV2_UI__Qt4UI
#else
#define LV2_UI_HOST_URI	LV2_UI_PREFIX "Qt5UI"
#endif


static void qtractor_lv2_ui_write (
	SuilController ui_controller,
	uint32_t port_index,
	uint32_t buffer_size,
	uint32_t protocol,
	const void *buffer )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return;

	pLv2Plugin->lv2_ui_write(port_index, buffer_size, protocol, buffer);
}


#ifdef CONFIG_LV2_EXTERNAL_UI

static void qtractor_lv2_ui_closed ( LV2UI_Controller ui_controller )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_ui_closed(%p)", pLv2Plugin);
#endif

	// Just flag up the closure...
	pLv2Plugin->setEditorClosed(true);
}

#endif	// CONFIG_LV2_EXTERNAL_UI


#if QT_VERSION < 0x050000

#include <QResizeEvent>

class qtractorLv2Plugin::EventFilter : public QObject
{
public:

	// Constructor.
	EventFilter(qtractorLv2Plugin *pLv2Plugin, QWidget *pQt4Widget)
		: QObject(), m_pLv2Plugin(pLv2Plugin), m_pQt4Widget(pQt4Widget)
		{ m_pQt4Widget->installEventFilter(this); }

	bool eventFilter(QObject *pObject, QEvent *pEvent)
	{
		if (pObject == static_cast<QObject *> (m_pQt4Widget)) {
			switch (pEvent->type()) {
			case QEvent::Close: {
				// Defer widget close!
				m_pQt4Widget->removeEventFilter(this);
				m_pQt4Widget = NULL;
				m_pLv2Plugin->closeEditorEx();
				pEvent->ignore();
				return true;
			}
			case QEvent::Resize: {
				// LV2 UI resize control...
				QResizeEvent *pResizeEvent
					= static_cast<QResizeEvent *> (pEvent);
				if (pResizeEvent)
					m_pLv2Plugin->resizeEditor(pResizeEvent->size());
				// Fall thru...
			}
			default:
				break;
			}
		}

		return QObject::eventFilter(pObject, pEvent);
	}

private:
	
	// Instance variables.
	qtractorLv2Plugin *m_pLv2Plugin;
	QWidget           *m_pQt4Widget;
};

#endif

#endif	// CONFIG_LV2_UI


// LV2 World stuff (ref. counted).
static LilvWorld   *g_lv2_world   = NULL;
static LilvPlugins *g_lv2_plugins = NULL;

// Supported port classes.
static LilvNode *g_lv2_input_class   = NULL;
static LilvNode *g_lv2_output_class  = NULL;
static LilvNode *g_lv2_control_class = NULL;
static LilvNode *g_lv2_audio_class   = NULL;

#ifdef CONFIG_LV2_EVENT
static LilvNode *g_lv2_event_class = NULL;
static LilvNode *g_lv2_midi_class  = NULL;
#endif

#ifdef CONFIG_LV2_ATOM
static LilvNode *g_lv2_atom_port_class    = NULL;
static uint32_t  g_lv2_atom_event_type    = 0;
static uint32_t  g_lv2_atom_chunk_type    = 0;
static uint32_t  g_lv2_atom_sequence_type = 0;
static uint32_t  g_lv2_atom_string_type   = 0;
static uint32_t  g_lv2_atom_float_type    = 0;
static uint32_t  g_lv2_atom_int_type      = 0;
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
static LilvNode *g_lv2_external_ui_class = NULL;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
static LilvNode *g_lv2_external_ui_deprecated_class = NULL;
#endif
#endif
#if QT_VERSION < 0x050000
static LilvNode *g_lv2_x11_ui_class = NULL;
static LilvNode *g_lv2_gtk_ui_class = NULL;
static LilvNode *g_lv2_qt4_ui_class = NULL;
#endif
#endif	// CONFIG_LV2_UI

// Supported plugin features.
static LilvNode *g_lv2_realtime_hint = NULL;
static LilvNode *g_lv2_extension_data_hint = NULL;

#ifdef CONFIG_LV2_WORKER
static LilvNode *g_lv2_worker_schedule_hint = NULL;
#endif

#ifdef CONFIG_LV2_STATE
static LilvNode *g_lv2_state_interface_hint = NULL;
#endif

// Supported port properties (hints).
static LilvNode *g_lv2_toggled_prop     = NULL;
static LilvNode *g_lv2_integer_prop     = NULL;
static LilvNode *g_lv2_sample_rate_prop = NULL;
static LilvNode *g_lv2_logarithmic_prop = NULL;

#ifdef CONFIG_LV2_ATOM
#include "lv2/lv2plug.in/ns/ext/resize-port/resize-port.h"
static LilvNode *g_lv2_minimum_size_prop = NULL;
#endif

#ifdef CONFIG_LV2_PROGRAMS

void qtractor_lv2_program_changed ( LV2_Programs_Handle handle, int32_t index )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_program_changed(%p, %d)", pLv2Plugin, index);
#endif

	pLv2Plugin->lv2_program_changed(index);
}

#endif	// CONFIG_LV2_PROGRAMS


#ifdef CONFIG_LV2_PRESETS

// LV2 Presets: port value setter.
static void qtractor_lv2_set_port_value ( const char *port_symbol,
	void *user_data, const void *value, uint32_t size, uint32_t type )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (user_data);
	if (pLv2Plugin == NULL)
		return;

	const LilvPlugin *plugin = pLv2Plugin->lv2_plugin();
	if (plugin == NULL)
		return;

	if (size != sizeof(float) || type != g_lv2_atom_float_type)
		return;

	LilvNode *symbol = lilv_new_string(g_lv2_world, port_symbol);

	const LilvPort *port
		= lilv_plugin_get_port_by_symbol(plugin, symbol);
	if (port) {
		const float val = *(float *) value;
		const unsigned long port_index = lilv_port_get_index(plugin, port);
		pLv2Plugin->updateParamValue(port_index, val, true);
	}

	lilv_node_free(symbol);
}


// LV2 Presets: port value getter.
static const void *qtractor_lv2_get_port_value ( const char *port_symbol,
	void *user_data, uint32_t *size, uint32_t *type )
{
	const void *retv = NULL;

	*size = 0;
	*type = 0;

	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (user_data);
	if (pLv2Plugin == NULL)
		return retv;

	const LilvPlugin *plugin = pLv2Plugin->lv2_plugin();
	if (plugin == NULL)
		return retv;

	LilvNode *symbol = lilv_new_string(g_lv2_world, port_symbol);

	const LilvPort *port
		= lilv_plugin_get_port_by_symbol(plugin, symbol);
	if (port) {
		unsigned long iIndex = lilv_port_get_index(plugin, port);
		qtractorPluginParam *pParam = pLv2Plugin->findParam(iIndex);
		if (pParam) {
			*size = sizeof(float);
			*type = g_lv2_atom_float_type;
			retv = (const void *) (pParam->subject())->data();
		}
	}

	lilv_node_free(symbol);

	return retv;
}

// Remove specific dir/file path.
static void qtractor_lv2_remove_dir(const QString& sDir);

static void qtractor_lv2_remove_file ( const QFileInfo& info )
{
	if (info.exists()) {
		const QString& sPath = info.absoluteFilePath();
		if (info.isDir()) {
			qtractor_lv2_remove_dir(sPath);
		} else {
			QFile::remove(sPath);
		}
	}
}

static void qtractor_lv2_remove_dir_list ( const QList<QFileInfo>& list )
{
	QListIterator<QFileInfo> iter(list);
	while (iter.hasNext())
		qtractor_lv2_remove_file(iter.next());
}

static void qtractor_lv2_remove_dir ( const QString& sDir )
{
	const QDir dir(sDir);

	qtractor_lv2_remove_dir_list(
		dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot));

	QDir cwd = QDir::current();
	if (cwd.absolutePath() == dir.absolutePath()) {
		cwd.cdUp();
		QDir::setCurrent(cwd.path());
	}

	dir.rmdir(sDir);
}

#endif	// CONFIG_LV2_PRESETS


#ifdef CONFIG_LV2_TIME

// LV2 Time-position control structure.
static struct qtractorLv2Time
{
	enum Index {

		position = 0,
		bar,
		beat,
		beatUnit,
		beatsPerBar,
		beatsPerMinute,
		frame,
		framesPerSecond,
		speed,

		numOfMembers
	};

	const char *uri;
	LilvNode   *node;
	float       value;

	QList<qtractorLv2PluginParam *> *params;

} g_lv2_time[] = {

	{ LV2_TIME__position,        NULL, 0.0f, NULL },
	{ LV2_TIME__bar,             NULL, 0.0f, NULL },
	{ LV2_TIME__beat,            NULL, 0.0f, NULL },
	{ LV2_TIME__beatUnit,        NULL, 0.0f, NULL },
	{ LV2_TIME__beatsPerBar,     NULL, 0.0f, NULL },
	{ LV2_TIME__beatsPerMinute,  NULL, 0.0f, NULL },
	{ LV2_TIME__frame,           NULL, 0.0f, NULL },
	{ LV2_TIME__framesPerSecond, NULL, 0.0f, NULL },
	{ LV2_TIME__speed,           NULL, 0.0f, NULL }
};

#endif	// CONFIG_LV2_TIME


#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
static uint32_t g_lv2_bufsz_min_block_length = 0;
static uint32_t g_lv2_bufsz_max_block_length = 0;
static uint32_t g_lv2_bufsz_sequence_size    = 0;
#endif
#endif


//----------------------------------------------------------------------------
// qtractorLv2PluginType -- LV2 plugin type instance.
//

// Derived methods.
bool qtractorLv2PluginType::open (void)
{
	// Do we have a descriptor already?
	if (m_lv2_plugin == NULL)
		m_lv2_plugin = lv2_plugin(m_sUri);
	if (m_lv2_plugin == NULL)
		return false;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType[%p]::open() uri=\"%s\"",
		this, filename().toUtf8().constData());
#endif

	// Retrieve plugin type names.
	LilvNode *name = lilv_plugin_get_name(m_lv2_plugin);
	if (name) {
		m_sName = lilv_node_as_string(name);
		lilv_node_free(name);
	} else {
		m_sName = filename();
		int iIndex = m_sName.lastIndexOf('/');
		if (iIndex > 0)
			m_sName = m_sName.right(m_sName.length() - iIndex - 1);
	}

	// Sanitize plugin label.
	m_sLabel = m_sName.simplified().replace(QRegExp("[\\s|\\.|\\-]+"), "_");

	// Retrieve plugin unique identifier.
	m_iUniqueID = qHash(m_sUri);

	// Compute and cache port counts...
	m_iControlIns  = 0;
	m_iControlOuts = 0;
	m_iAudioIns    = 0;
	m_iAudioOuts   = 0;
	m_iMidiIns     = 0;
	m_iMidiOuts    = 0;

#ifdef CONFIG_LV2_EVENT
	m_iMidiEventIns  = 0;
	m_iMidiEventOuts = 0;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iMidiAtomIns  = 0;
	m_iMidiAtomOuts = 0;
#endif

	const unsigned long iNumPorts = lilv_plugin_get_num_ports(m_lv2_plugin);

	for (unsigned long i = 0; i < iNumPorts; ++i) {
		const LilvPort *port = lilv_plugin_get_port_by_index(m_lv2_plugin, i);
		if (port) {
			if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_control_class)) {
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_input_class))
					++m_iControlIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iControlOuts;
			}
			else
			if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_audio_class)) {
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_input_class))
					++m_iAudioIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iAudioOuts;
			}
		#ifdef CONFIG_LV2_EVENT
			else
			if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_event_class) ||
				lilv_port_is_a(m_lv2_plugin, port, g_lv2_midi_class)) {
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_input_class))
					++m_iMidiEventIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iMidiEventOuts;
			}
		#endif
		#ifdef CONFIG_LV2_ATOM
			else
			if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_atom_port_class)) {
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_input_class))
					++m_iMidiAtomIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iMidiAtomOuts;
			}
		#endif
		}
	}

#ifdef CONFIG_LV2_EVENT
	m_iMidiIns  += m_iMidiEventIns;
	m_iMidiOuts += m_iMidiEventOuts;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iMidiIns  += m_iMidiAtomIns;
	m_iMidiOuts += m_iMidiAtomOuts;
#endif

	// Cache flags.
	m_bRealtime = lilv_plugin_has_feature(m_lv2_plugin, g_lv2_realtime_hint);

	m_bConfigure = false;
#ifdef CONFIG_LV2_STATE
	// Query for state interface extension data...
	LilvNodes *nodes = lilv_plugin_get_value(m_lv2_plugin,
		g_lv2_extension_data_hint);
	LILV_FOREACH(nodes, i, nodes) {
		const LilvNode *node = lilv_nodes_get(nodes, i);
		if (lilv_node_equals(node, g_lv2_state_interface_hint)) {
			m_bConfigure = true;
			break;
		}
	}
#endif
#ifdef CONFIG_LV2_UI
	// Check the UI inventory...
	LilvUIs *uis = lilv_plugin_get_uis(m_lv2_plugin);
	if (uis) {
		LILV_FOREACH(uis, i, uis) {
			const LilvUI *ui = lilv_uis_get(uis, i);
		#ifdef CONFIG_LV2_EXTERNAL_UI
			if (lilv_ui_is_a(ui, g_lv2_external_ui_class)
			#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
				|| lilv_ui_is_a(ui, g_lv2_external_ui_deprecated_class)
			#endif
			) {
				m_bEditor = true;
				break;
			}
		#endif
		#if QT_VERSION < 0x050000
			if (lilv_ui_is_a(ui, g_lv2_x11_ui_class)) {
				m_bEditor = true;
				break;
			}
			if (lilv_ui_is_a(ui, g_lv2_gtk_ui_class)) {
				m_bEditor = true;
				break;
			}
			if (lilv_ui_is_a(ui, g_lv2_qt4_ui_class)) {
				m_bEditor = true;
				break;
			}
		#endif
		}
		lilv_uis_free(uis);
	}
#endif

	// Done.
	return true;
}


void qtractorLv2PluginType::close (void)
{
	m_lv2_plugin = NULL;
}


// Factory method (static)
qtractorLv2PluginType *qtractorLv2PluginType::createType (
	const QString& sUri, LilvPlugin *plugin )
{
	// Sanity check...
	if (sUri.isEmpty())
		return NULL;

	if (plugin == NULL)
		plugin = lv2_plugin(sUri);
	if (plugin == NULL)
		return NULL;

	// Yep, most probably its a valid plugin descriptor...
	return new qtractorLv2PluginType(sUri, plugin);
}


// Descriptor method (static)
LilvPlugin *qtractorLv2PluginType::lv2_plugin ( const QString& sUri )
{
	if (g_lv2_plugins == NULL)
		return NULL;

	// Retrieve plugin descriptor if any...
	LilvNode *uri = lilv_new_uri(g_lv2_world, sUri.toUtf8().constData());
	if (uri == NULL)
		return NULL;

	LilvPlugin *plugin = const_cast<LilvPlugin *> (
		lilv_plugins_get_by_uri(g_lv2_plugins, uri));
#if 0
	LilvNodes *list = lilv_plugin_get_required_features(
		static_cast<LilvPlugin *> (plugin));
	if (list) {
		LILV_FOREACH(nodes, iter, list) {
			const LilvNode *node = lilv_nodes_get(list, iter);
			bool bSupported = false;
			for (int i = 0; !bSupported && g_lv2_features[i]; ++i) {
				const LilvNode *impl
					= lilv_new_uri(g_lv2_world, g_lv2_features[i]->URI);
				bSupported = lilv_node_equals(impl, node);
			}
			if (!bSupported) {
			#ifdef CONFIG_DEBUG
				qDebug("qtractorLv2PluginType::lilv_plugin: node %s not supported.",
					lilv_node_as_string(lilv_nodes_get(node, iter)));
			#endif
				plugin = NULL;
				break;
			}
		}
	}
#endif
	lilv_node_free(uri);
	return plugin;
}


// LV2 World stuff (ref. counted).
void qtractorLv2PluginType::lv2_open (void)
{
	if (g_lv2_plugins)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType::lv2_open()");
#endif

	g_lv2_world = lilv_world_new();

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bLv2DynManifest) {
		// Set dyn-manifest support option...
		LilvNode *dyn_manifest = lilv_new_bool(g_lv2_world, true);
		lilv_world_set_option(g_lv2_world, LILV_OPTION_DYN_MANIFEST, dyn_manifest);
		lilv_node_free(dyn_manifest);
	}

	// Find all installed plugins.
	lilv_world_load_all(g_lv2_world);

	g_lv2_plugins = const_cast<LilvPlugins *> (
		lilv_world_get_all_plugins(g_lv2_world));

	// Set up the port classes we support.
	g_lv2_input_class   = lilv_new_uri(g_lv2_world, LILV_URI_INPUT_PORT);
	g_lv2_output_class  = lilv_new_uri(g_lv2_world, LILV_URI_OUTPUT_PORT);
	g_lv2_control_class = lilv_new_uri(g_lv2_world, LILV_URI_CONTROL_PORT);
	g_lv2_audio_class   = lilv_new_uri(g_lv2_world, LILV_URI_AUDIO_PORT);
#ifdef CONFIG_LV2_EVENT
	g_lv2_event_class   = lilv_new_uri(g_lv2_world, LILV_URI_EVENT_PORT);
	g_lv2_midi_class    = lilv_new_uri(g_lv2_world, LILV_URI_MIDI_EVENT);
#endif
#ifdef CONFIG_LV2_ATOM
	g_lv2_atom_port_class    = lilv_new_uri(g_lv2_world, LV2_ATOM__AtomPort);
	g_lv2_atom_event_type    = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__eventTransfer);
	g_lv2_atom_chunk_type    = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Chunk);
	g_lv2_atom_sequence_type = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Sequence);
	g_lv2_atom_string_type   = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__String);
	g_lv2_atom_float_type    = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Float);
	g_lv2_atom_int_type      = qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Int);
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	g_lv2_external_ui_class
		= lilv_new_uri(g_lv2_world, LV2_EXTERNAL_UI__Widget);
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	g_lv2_external_ui_deprecated_class
		= lilv_new_uri(g_lv2_world, LV2_EXTERNAL_UI_DEPRECATED_URI);
#endif
#endif
#if QT_VERSION < 0x050000
	g_lv2_x11_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__X11UI);
	g_lv2_gtk_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__GtkUI);
	g_lv2_qt4_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__Qt4UI);
#endif
#endif	// CONFIG_LV2_UI

	// Set up the feature we may want to know (as hints).
	g_lv2_realtime_hint = lilv_new_uri(g_lv2_world,
		LV2_CORE_PREFIX "hardRtCapable");
	g_lv2_extension_data_hint = lilv_new_uri(g_lv2_world,
		LV2_CORE_PREFIX "extensionData");

#ifdef CONFIG_LV2_WORKER
	// LV2 Worker/Schedule support hints...
	g_lv2_worker_schedule_hint = lilv_new_uri(g_lv2_world,
		LV2_WORKER__schedule);
#endif

#ifdef CONFIG_LV2_STATE
	// LV2 State: set up supported interface and types...
	g_lv2_state_interface_hint = lilv_new_uri(g_lv2_world,
		LV2_STATE__interface);
#endif

	// Set up the port properties we support (as hints).
	g_lv2_toggled_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE_PREFIX "toggled");
	g_lv2_integer_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE_PREFIX "integer");
	g_lv2_sample_rate_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE_PREFIX "sampleRate");
	g_lv2_logarithmic_prop = lilv_new_uri(g_lv2_world,
		"http://lv2plug.in/ns/dev/extportinfo#logarithmic");

#ifdef CONFIG_LV2_ATOM
	g_lv2_minimum_size_prop = lilv_new_uri(g_lv2_world,
		LV2_RESIZE_PORT__minimumSize);
#endif

#ifdef CONFIG_LV2_TIME
	// LV2 Time: set up supported port designations...
	for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
		qtractorLv2Time& member = g_lv2_time[i];
		member.node = lilv_new_uri(g_lv2_world, member.uri);
		member.value = 0.0f;
		member.params = new QList<qtractorLv2PluginParam *> ();
	}
#endif

#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	g_lv2_bufsz_min_block_length
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__minBlockLength);
	g_lv2_bufsz_max_block_length
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__maxBlockLength);
	g_lv2_bufsz_sequence_size
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__sequenceSize);
#endif
#endif
}


void qtractorLv2PluginType::lv2_close (void)
{
	if (g_lv2_plugins == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType::lv2_close()");
#endif


#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	g_lv2_bufsz_min_block_length = 0;
	g_lv2_bufsz_max_block_length = 0;
	g_lv2_bufsz_sequence_size    = 0;
#endif
#endif

#ifdef CONFIG_LV2_TIME
	for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
		qtractorLv2Time& member = g_lv2_time[i];
		lilv_node_free(member.node);
		member.node = NULL;
		member.value = 0.0f;
		delete member.params;
		member.params = NULL;
	}
#endif

	// Clean up.
	lilv_node_free(g_lv2_toggled_prop);
	lilv_node_free(g_lv2_integer_prop);
	lilv_node_free(g_lv2_sample_rate_prop);
	lilv_node_free(g_lv2_logarithmic_prop);

#ifdef CONFIG_LV2_ATOM
	lilv_node_free(g_lv2_minimum_size_prop);
#endif

#ifdef CONFIG_LV2_STATE
	lilv_node_free(g_lv2_state_interface_hint);
#endif

#ifdef CONFIG_LV2_WORKER
	lilv_node_free(g_lv2_worker_schedule_hint);
#endif

	lilv_node_free(g_lv2_extension_data_hint);
	lilv_node_free(g_lv2_realtime_hint);

	lilv_node_free(g_lv2_input_class);
	lilv_node_free(g_lv2_output_class);
	lilv_node_free(g_lv2_control_class);
	lilv_node_free(g_lv2_audio_class);
#ifdef CONFIG_LV2_EVENT
	lilv_node_free(g_lv2_event_class);
	lilv_node_free(g_lv2_midi_class);
#endif
#ifdef CONFIG_LV2_ATOM
	lilv_node_free(g_lv2_atom_port_class);
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	lilv_node_free(g_lv2_external_ui_class);
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	lilv_node_free(g_lv2_external_ui_deprecated_class);
#endif
#endif
#if QT_VERSION < 0x050000
	lilv_node_free(g_lv2_x11_ui_class);
	lilv_node_free(g_lv2_gtk_ui_class);
	lilv_node_free(g_lv2_qt4_ui_class);
#endif
#endif	// CONFIG_LV2_UI

	lilv_world_free(g_lv2_world);

	g_lv2_input_class   = NULL;
	g_lv2_output_class  = NULL;
	g_lv2_control_class = NULL;
	g_lv2_audio_class   = NULL;
#ifdef CONFIG_LV2_EVENT
	g_lv2_event_class   = NULL;
	g_lv2_midi_class    = NULL;
#endif
#ifdef CONFIG_LV2_ATOM
	g_lv2_atom_port_class    = NULL;
	g_lv2_atom_event_type    = 0;
	g_lv2_atom_chunk_type    = 0;
	g_lv2_atom_sequence_type = 0;
	g_lv2_atom_string_type   = 0;
	g_lv2_atom_float_type    = 0;
	g_lv2_atom_int_type      = 0;
#endif

#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	g_lv2_bufsz_min_block_length = 0;
	g_lv2_bufsz_max_block_length = 0;
	g_lv2_bufsz_sequence_size    = 0;
#endif
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	g_lv2_external_ui_class = NULL;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	g_lv2_external_ui_deprecated_class = NULL;
#endif
#endif
#if QT_VERSION < 0x050000
	g_lv2_x11_ui_class = NULL;
	g_lv2_gtk_ui_class = NULL;
	g_lv2_qt4_ui_class = NULL;
#endif
#endif

	g_lv2_plugins = NULL;
	g_lv2_world   = NULL;
}


// Plugin type listing (static).
bool qtractorLv2PluginType::getTypes ( qtractorPluginPath& path )
{
	if (g_lv2_plugins == NULL)
		return false;

	unsigned long iIndex = 0;

	LILV_FOREACH(plugins, i, g_lv2_plugins) {
		LilvPlugin *plugin = const_cast<LilvPlugin *> (
			lilv_plugins_get(g_lv2_plugins, i));
		const char *pszUri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(pszUri, plugin);
		if (pLv2Type) {
			if (pLv2Type->open()) {
				path.addType(pLv2Type);
				pLv2Type->close();
				++iIndex;
			} else {
				delete pLv2Type;
			}
		}
	}

	return (iIndex > 0);
}


// Instance cached-deferred accesors.
const QString& qtractorLv2PluginType::aboutText (void)
{
	if (m_sAboutText.isEmpty() && m_lv2_plugin) {
		LilvNode *node = lilv_plugin_get_project(m_lv2_plugin);
		if (node) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Project: ");
			m_sAboutText += lilv_node_as_string(node);
			lilv_node_free(node);
		}
		node = lilv_plugin_get_author_name(m_lv2_plugin);
		if (node) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
			m_sAboutText += QObject::tr("Author: ");
			m_sAboutText += lilv_node_as_string(node);
			lilv_node_free(node);
		}
		node = lilv_plugin_get_author_email(m_lv2_plugin);
		if (node) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
		//	m_sAboutText += QObject::tr("Email: ");
			m_sAboutText += lilv_node_as_string(node);
			lilv_node_free(node);
		}
		node = lilv_plugin_get_author_homepage(m_lv2_plugin);
		if (node) {
			if (!m_sAboutText.isEmpty())
				m_sAboutText += '\n';
		//	m_sAboutText += QObject::tr("Homepage: ");
			m_sAboutText += lilv_node_as_string(node);
			lilv_node_free(node);
		}
	}

	return m_sAboutText;
}


//----------------------------------------------------------------------------
// qtractorLv2Plugin -- LV2 plugin instance.
//

// Dynamic singleton list of LV2 plugins with external UIs open.
static QList<qtractorLv2Plugin *> g_lv2Plugins;

// Constructors.
qtractorLv2Plugin::qtractorLv2Plugin ( qtractorPluginList *pList,
	qtractorLv2PluginType *pLv2Type )
	: qtractorPlugin(pList, pLv2Type)
		, m_ppInstances(NULL)
		, m_piControlOuts(NULL)
		, m_pfControlOuts(NULL)
		, m_pfControlOutsLast(NULL)
		, m_piAudioIns(NULL)
		, m_piAudioOuts(NULL)
	#ifdef CONFIG_LV2_EVENT
		, m_piMidiEventIns(NULL)
		, m_piMidiEventOuts(NULL)
	#endif
	#ifdef CONFIG_LV2_ATOM
		, m_piMidiAtomIns(NULL)
		, m_piMidiAtomOuts(NULL)
		, m_lv2_atom_buffer_ins(NULL)
		, m_lv2_atom_port_ins(NULL)
		, m_lv2_atom_buffer_outs(NULL)
		, m_lv2_atom_port_outs(NULL)
	#endif
		, m_lv2_features(NULL)
	#ifdef CONFIG_LV2_WORKER
		, m_lv2_worker(NULL)
	#endif
	#ifdef CONFIG_LV2_UI
		, m_lv2_ui_type(LV2_UI_TYPE_NONE)
		, m_bEditorVisible(false)
		, m_bEditorClosed(false)
		, m_lv2_uis(NULL)
		, m_lv2_ui(NULL)
		, m_lv2_ui_features(NULL)
		, m_suil_host(NULL)
		, m_suil_instance(NULL)
		, m_lv2_ui_widget(NULL)
	#ifdef CONFIG_LV2_ATOM
		, m_ui_events(NULL)
		, m_plugin_events(NULL)
	#endif
	#if QT_VERSION < 0x050000
		, m_pQt4Filter(NULL)
		, m_pQt4Widget(NULL)
	#endif
	#endif	// CONFIG_LV2_UI
	#ifdef CONFIG_LV2_OPTIONS
	#ifdef CONFIG_LV2_BUF_SIZE
		, m_iMinBlockLength(0)
		, m_iMaxBlockLength(0)
		, m_iSequenceSize(0)
	#endif
	#endif	// CONFIG_LV2_OPTIONS
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p] uri=\"%s\"",
		this, pLv2Type->filename().toUtf8().constData());
#endif

	int iFeatures = 0;
	while (g_lv2_features[iFeatures]) { ++iFeatures; }

	m_lv2_features = new LV2_Feature * [iFeatures + 5];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_features[i] = (LV2_Feature *) g_lv2_features[i];

#ifdef CONFIG_LV2_STATE_FILES

	m_lv2_state_map_path.handle = this;
	m_lv2_state_map_path.abstract_path = &qtractor_lv2_state_abstract_path;
	m_lv2_state_map_path.absolute_path = &qtractor_lv2_state_absolute_path;

	m_lv2_state_map_path_feature.URI = LV2_STATE__mapPath;
	m_lv2_state_map_path_feature.data = &m_lv2_state_map_path;
	m_lv2_features[iFeatures++] = &m_lv2_state_map_path_feature;

#ifdef CONFIG_LV2_STATE_MAKE_PATH

	m_lv2_state_make_path.handle = this;
	m_lv2_state_make_path.path = &qtractor_lv2_state_make_path;

	m_lv2_state_make_path_feature.URI = LV2_STATE__makePath;
	m_lv2_state_make_path_feature.data = &m_lv2_state_make_path;
	m_lv2_features[iFeatures++] = &m_lv2_state_make_path_feature;

#endif	// CONFIG_LV2_STATE_MAKE_PATH

#endif	// CONFIG_LV2_STATE_FILES

#ifdef CONFIG_LV2_PROGRAMS

	m_lv2_programs_host.handle = this;
	m_lv2_programs_host.program_changed = &qtractor_lv2_program_changed;

	m_lv2_programs_host_feature.URI = LV2_PROGRAMS__Host;
	m_lv2_programs_host_feature.data = &m_lv2_programs_host;
	m_lv2_features[iFeatures++] = &m_lv2_programs_host_feature;

#endif	// CONFIG_LV2_PROGRAMS

	qtractorMidiManager *pMidiManager = list()->midiManager();

#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE

	m_iMinBlockLength = 0;
	m_iMaxBlockLength = 0;
	m_iSequenceSize   = 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine)
			m_iMaxBlockLength = pAudioEngine->bufferSize();
	}

	if (pMidiManager) {
		const uint32_t MaxMidiEvents = (pMidiManager->bufferSize() << 1);
	#ifdef CONFIG_LV2_EVENT
		const uint32_t Lv2EventBufferSize
			= (sizeof(LV2_Event) + 4) * MaxMidiEvents;
		if (m_iSequenceSize > Lv2EventBufferSize || m_iSequenceSize < 1)
			m_iSequenceSize = Lv2EventBufferSize;
	#endif
	#ifdef CONFIG_LV2_ATOM
		const uint32_t Lv2AtomBufferSize
			= (sizeof(LV2_Atom_Event) + 4) * MaxMidiEvents;
		if (m_iSequenceSize > Lv2AtomBufferSize || m_iSequenceSize < 1)
			m_iSequenceSize = Lv2AtomBufferSize;
	#endif
	}

	// Build options array to pass to plugin
	const LV2_Options_Option options[] = {
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_bufsz_min_block_length,
		  sizeof(int32_t), g_lv2_atom_int_type, &m_iMinBlockLength },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_bufsz_max_block_length,
		  sizeof(int32_t), g_lv2_atom_int_type, &m_iMaxBlockLength },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_bufsz_sequence_size,
		  sizeof(int32_t), g_lv2_atom_int_type, &m_iSequenceSize },
		{ LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL }
	};

	::memcpy(&m_lv2_options, &options, sizeof(options));

	m_lv2_options_feature.URI   = LV2_OPTIONS__options;
	m_lv2_options_feature.data  = &m_lv2_options;
	m_lv2_features[iFeatures++] = &m_lv2_options_feature;

#endif	// CONFIG_LV2_BUF_SIZE
#endif	// CONFIG_LV2_OPTIONS

	m_lv2_features[iFeatures] = NULL;

	// Get some structural data first...
	const LilvPlugin *plugin = pLv2Type->lv2_plugin();
	if (plugin) {
		unsigned short iControlOuts = pLv2Type->controlOuts();
		unsigned short iAudioIns    = pLv2Type->audioIns();
		unsigned short iAudioOuts   = pLv2Type->audioOuts();
		if (iAudioIns > 0)
			m_piAudioIns = new unsigned long [iAudioIns];
		if (iAudioOuts > 0)
			m_piAudioOuts = new unsigned long [iAudioOuts];
		if (iControlOuts > 0) {
			m_piControlOuts = new unsigned long [iControlOuts];
			m_pfControlOuts = new float [iControlOuts];
			m_pfControlOutsLast = new float [iControlOuts];
		}
		iControlOuts = iAudioIns = iAudioOuts = 0;
	#ifdef CONFIG_LV2_EVENT
		unsigned short iMidiEventIns  = pLv2Type->midiEventIns();
		unsigned short iMidiEventOuts = pLv2Type->midiEventOuts();
		if (iMidiEventIns > 0)
			m_piMidiEventIns = new unsigned long [iMidiEventIns];
		if (iMidiEventOuts > 0)
			m_piMidiEventOuts = new unsigned long [iMidiEventOuts];
		iMidiEventIns = iMidiEventOuts = 0;
	#endif	// CONFIG_LV2_EVENT
	#ifdef CONFIG_LV2_ATOM
		unsigned short iMidiAtomIns  = pLv2Type->midiAtomIns();
		unsigned short iMidiAtomOuts = pLv2Type->midiAtomOuts();
		if (iMidiAtomIns > 0)
			m_piMidiAtomIns = new unsigned long [iMidiAtomIns];
		if (iMidiAtomOuts > 0)
			m_piMidiAtomOuts = new unsigned long [iMidiAtomOuts];
		iMidiAtomIns = iMidiAtomOuts = 0;
	#endif	// CONFIG_LV2_ATOM
		const unsigned long iNumPorts = lilv_plugin_get_num_ports(plugin);
		for (unsigned long i = 0; i < iNumPorts; ++i) {
			const LilvPort *port = lilv_plugin_get_port_by_index(plugin, i);
			if (port) {
				if (lilv_port_is_a(plugin, port, g_lv2_input_class)) {
					if (lilv_port_is_a(plugin, port, g_lv2_audio_class))
						m_piAudioIns[iAudioIns++] = i;
					else
				#ifdef CONFIG_LV2_EVENT
					if (lilv_port_is_a(plugin, port, g_lv2_event_class) ||
						lilv_port_is_a(plugin, port, g_lv2_midi_class))
						m_piMidiEventIns[iMidiEventIns++] = i;
					else
				#endif
				#ifdef CONFIG_LV2_ATOM
					if (lilv_port_is_a(plugin, port, g_lv2_atom_port_class))
						m_piMidiAtomIns[iMidiAtomIns++] = i;
					else
				#endif
					if (lilv_port_is_a(plugin, port, g_lv2_control_class))
						addParam(new qtractorLv2PluginParam(this, i));
				}
				else
				if (lilv_port_is_a(plugin, port, g_lv2_output_class)) {
					if (lilv_port_is_a(plugin, port, g_lv2_audio_class))
						m_piAudioOuts[iAudioOuts++] = i;
					else
				#ifdef CONFIG_LV2_EVENT
					if (lilv_port_is_a(plugin, port, g_lv2_event_class) ||
						lilv_port_is_a(plugin, port, g_lv2_midi_class))
						m_piMidiEventOuts[iMidiEventOuts++] = i;
					else
				#endif
				#ifdef CONFIG_LV2_ATOM
					if (lilv_port_is_a(plugin, port, g_lv2_atom_port_class))
						m_piMidiAtomOuts[iMidiAtomOuts++] = i;
					else
				#endif
					if (lilv_port_is_a(plugin, port, g_lv2_control_class)) {
						m_piControlOuts[iControlOuts] = i;
						m_pfControlOuts[iControlOuts] = 0.0f;
						m_pfControlOutsLast[iControlOuts] = 0.0f;
						++iControlOuts;
					}
				}
			}
		}
	#ifdef CONFIG_LV2_ATOM
		unsigned int iMidiAtomInsCapacity = 0;
		if (iMidiAtomIns > 0) {
			m_lv2_atom_buffer_ins = new LV2_Atom_Buffer * [iMidiAtomIns];
			m_lv2_atom_port_ins = new LV2_Atom_Buffer * [iMidiAtomIns];
			for (unsigned long j = 0; j < iMidiAtomIns; ++j) {
				unsigned int iMinBufferCapacity = 1024;
				const LilvPort *port
					= lilv_plugin_get_port_by_index(plugin, m_piMidiAtomIns[j]);
				if (port) {
					LilvNode *minimum_size
						= lilv_port_get(plugin, port, g_lv2_minimum_size_prop);
					if (minimum_size) {
						if (lilv_node_is_int(minimum_size)) {
							const unsigned int iMinimumSize
								= lilv_node_as_int(minimum_size);
							if (iMinBufferCapacity  < iMinimumSize)
								iMinBufferCapacity += iMinimumSize;
						}
						lilv_node_free(minimum_size);
					}
				}
				m_lv2_atom_buffer_ins[j]
					= lv2_atom_buffer_new(iMinBufferCapacity,
						g_lv2_atom_chunk_type,
						g_lv2_atom_sequence_type, true);
				m_lv2_atom_port_ins[j] = m_lv2_atom_buffer_ins[j];
				if (pMidiManager && j == 0)
					pMidiManager->lv2_atom_buffer_resize(iMinBufferCapacity);
				iMidiAtomInsCapacity += iMinBufferCapacity;
			}
		}
		unsigned int iMidiAtomOutsCapacity = 0;
		if (iMidiAtomOuts > 0) {
			m_lv2_atom_buffer_outs = new LV2_Atom_Buffer * [iMidiAtomOuts];
			m_lv2_atom_port_outs = new LV2_Atom_Buffer * [iMidiAtomOuts];
			for (unsigned long j = 0; j < iMidiAtomOuts; ++j) {
				unsigned int iMinBufferCapacity = 1024;
				const LilvPort *port
					= lilv_plugin_get_port_by_index(plugin, m_piMidiAtomOuts[j]);
				if (port) {
					LilvNode *minimum_size
						= lilv_port_get(plugin, port, g_lv2_minimum_size_prop);
					if (minimum_size) {
						if (lilv_node_is_int(minimum_size)) {
							const unsigned int iMinimumSize
								= lilv_node_as_int(minimum_size);
							if (iMinBufferCapacity  < iMinimumSize)
								iMinBufferCapacity += iMinimumSize;
						}
						lilv_node_free(minimum_size);
					}
				}
				m_lv2_atom_buffer_outs[j]
					= lv2_atom_buffer_new(iMinBufferCapacity,
						g_lv2_atom_chunk_type,
						g_lv2_atom_sequence_type, false);
				m_lv2_atom_port_outs[j] = m_lv2_atom_buffer_outs[j];
				if (pMidiManager && j == 0)
					pMidiManager->lv2_atom_buffer_resize(iMinBufferCapacity);
				iMidiAtomOutsCapacity += iMinBufferCapacity;
			}
		}
	#ifdef CONFIG_LV2_UI
		m_ui_events = ::jack_ringbuffer_create(iMidiAtomInsCapacity);
		m_plugin_events = ::jack_ringbuffer_create(iMidiAtomOutsCapacity);
	#endif
	#endif	// CONFIG_LV2_ATOM
	#ifdef CONFIG_LV2_PRESETS
		LilvNode *label_uri  = lilv_new_uri(g_lv2_world, LILV_NS_RDFS "label");
		LilvNode *preset_uri = lilv_new_uri(g_lv2_world, LV2_PRESETS__Preset);
		LilvNodes *presets = lilv_plugin_get_related(lv2_plugin(), preset_uri);
		if (presets) {
			LILV_FOREACH(nodes, i, presets) {
				const LilvNode *preset = lilv_nodes_get(presets, i);
				lilv_world_load_resource(g_lv2_world, preset);
				LilvNodes *labels = lilv_world_find_nodes(
					g_lv2_world, preset, label_uri, NULL);
				if (labels) {
					const LilvNode *label = lilv_nodes_get_first(labels);
					const QString sPreset(lilv_node_as_string(label));
					const QString sUri(lilv_node_as_string(preset));
					m_lv2_presets.insert(sPreset, sUri);
					lilv_nodes_free(labels);
				}
			}
			lilv_nodes_free(presets);
		}
		lilv_node_free(preset_uri);
		lilv_node_free(label_uri);
	#endif	// CONFIG_LV2_PRESETS
	#ifdef CONFIG_LV2_TIME
		// Set up time-pos designated port indexes, if any...
		for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
			qtractorLv2Time& member = g_lv2_time[i];
			if (member.node) {
				const LilvPort *port = lilv_plugin_get_port_by_designation(
					plugin, g_lv2_input_class, member.node);
				if (port) {
					const unsigned long iIndex = lilv_port_get_index(plugin, port);
					qtractorLv2PluginParam *pParam
						= static_cast<qtractorLv2PluginParam *> (findParam(iIndex));
					if (pParam) {
						m_lv2_time_ports.insert(iIndex, i);
						member.params->append(pParam);
					}
				}
			}
		}
	#endif	// CONFIG_LV2_TIME
		// FIXME: instantiate each instance properly...
		qtractorLv2Plugin::setChannels(channels());
	}
}


// Destructor.
qtractorLv2Plugin::~qtractorLv2Plugin (void)
{
	// Cleanup all plugin instances...
	setChannels(0);

#ifdef CONFIG_LV2_TIME
	// Unsubscribe mapped params...
	QHash<unsigned long, int>::ConstIterator iter
		= m_lv2_time_ports.constBegin();
	const QHash<unsigned long, int>::ConstIterator& iter_end
		= m_lv2_time_ports.constEnd();
	for ( ; iter != iter_end; ++iter) {
		qtractorLv2PluginParam *pParam
			= static_cast<qtractorLv2PluginParam *> (findParam(iter.key()));
		if (pParam)
			g_lv2_time[iter.value()].params->removeAll(pParam);
	}
#endif	// CONFIG_LV2_TIME

	// Free up all the rest...
#ifdef CONFIG_LV2_ATOM
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	const unsigned short iMidiAtomOuts
		= (pLv2Type ? pLv2Type->midiAtomOuts() : 0);
	if (m_lv2_atom_buffer_outs && m_lv2_atom_port_outs) {
		for (unsigned long j = 0; j < iMidiAtomOuts; ++j)
			lv2_atom_buffer_free(m_lv2_atom_buffer_outs[j]);
		delete [] m_lv2_atom_buffer_outs;
		delete [] m_lv2_atom_port_outs;
	}
	const unsigned short iMidiAtomIns
		= (pLv2Type ? pLv2Type->midiAtomIns() : 0);
	if (m_lv2_atom_buffer_ins && m_lv2_atom_port_ins) {
		for (unsigned long j = 0; j < iMidiAtomIns; ++j)
			lv2_atom_buffer_free(m_lv2_atom_buffer_ins[j]);
		delete [] m_lv2_atom_buffer_ins;
		delete [] m_lv2_atom_port_ins;
	}
#ifdef CONFIG_LV2_UI
	if (m_plugin_events)
		::jack_ringbuffer_free(m_plugin_events);
	if (m_ui_events)
		::jack_ringbuffer_free(m_ui_events);
#endif
	if (m_piMidiAtomOuts)
		delete [] m_piMidiAtomOuts;
	if (m_piMidiAtomIns)
		delete [] m_piMidiAtomIns;
#endif	// CONFIG_LV2_ATOM
#ifdef CONFIG_LV2_EVENT
	if (m_piMidiEventOuts)
		delete [] m_piMidiEventOuts;
	if (m_piMidiEventIns)
		delete [] m_piMidiEventIns;
#endif	// CONFIG_LV2_EVENT
	if (m_piAudioOuts)
		delete [] m_piAudioOuts;
	if (m_piAudioIns)
		delete [] m_piAudioIns;
	if (m_piControlOuts)
		delete [] m_piControlOuts;
	if (m_pfControlOuts)
		delete [] m_pfControlOuts;
	if (m_pfControlOutsLast)
		delete [] m_pfControlOutsLast;

	if (m_lv2_features)
		delete [] m_lv2_features;
}


// Channel/instance number accessors.
void qtractorLv2Plugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorPluginType *pType = type();
	if (pType == NULL)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	const unsigned short iInstances
		= pType->instances(iChannels, list()->isMidi());

	// Now see if instance count changed anyhow...
	if (iInstances == iOldInstances)
		return;

	const LilvPlugin *plugin = lv2_plugin();
	if (plugin == NULL)
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setActivated(false);

	// Set new instance number...
	setInstances(iInstances);

	// Close old instances, all the way...
#ifdef CONFIG_LV2_WORKER
	if (m_lv2_worker) {
		delete m_lv2_worker;
		m_lv2_worker = NULL;
	}
#endif
	if (m_ppInstances) {
		for (unsigned short i = 0; i < iOldInstances; ++i) {
			LilvInstance *instance = m_ppInstances[i];
			if (instance)
				lilv_instance_free(instance);
		}
		delete [] m_ppInstances;
		m_ppInstances = NULL;
	}

	// Bail out, if none are about to be created...
	if (iInstances < 1) {
		setActivated(bActivated);
		return;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::setChannels(%u) instances=%u",
		this, iChannels, iInstances);
#endif

#ifdef CONFIG_LV2_WORKER
	if (lilv_plugin_has_feature(lv2_plugin(), g_lv2_worker_schedule_hint))
		m_lv2_worker = new qtractorLv2Worker(this, m_lv2_features);
#endif

	LV2_Feature **features = m_lv2_features;
#ifdef CONFIG_LV2_WORKER
	if (m_lv2_worker)
		features = m_lv2_worker->lv2_features();
#endif

	// We'll need output control (not dummy anymore) port indexes...
	const unsigned short iControlOuts = pType->controlOuts();
	// Allocate new instances...
	m_ppInstances = new LilvInstance * [iInstances];
	for (unsigned short i = 0; i < iInstances; ++i) {
		// Instantiate them properly first...
		LilvInstance *instance
			= lilv_plugin_instantiate(plugin, sampleRate(), features);
		if (instance) {
			// (Dis)connect all ports...
			const unsigned long iNumPorts = lilv_plugin_get_num_ports(plugin);
			for (unsigned long k = 0; k < iNumPorts; ++k)
				lilv_instance_connect_port(instance, k, NULL);
			// Connect all existing input control ports...
			const qtractorPlugin::Params& params = qtractorPlugin::params();
			qtractorPlugin::Params::ConstIterator param = params.constBegin();
			const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
			for ( ; param != param_end; ++param) {
				qtractorPluginParam *pParam = param.value();
				lilv_instance_connect_port(instance,
					pParam->index(), pParam->subject()->data());
			}
			// Connect all existing output control ports...
			for (unsigned short j = 0; j < iControlOuts; ++j) {
				lilv_instance_connect_port(instance,
					m_piControlOuts[j], &m_pfControlOuts[j]);
			}
		#if 0//def CONFIG_LV2_TIME
			// Connect time-pos designated ports, if any...
			QHash<unsigned long, int>::ConstIterator iter
				= m_lv2_time_ports.constBegin();
			const QHash<unsigned long, int>::ConstIterator& iter_end
				= m_lv2_time_ports.constEnd();
			for ( ; iter != iter_end; ++iter) {
				lilv_instance_connect_port(instance,
					iter.key(), &(g_lv2_time[iter.value()].data));
			}
		#endif
		}
	#ifdef CONFIG_DEBUG
		qDebug("qtractorLv2Plugin[%p]::setChannels(%u) instance[%u]=%p",
			this, iChannels, i, instance);
	#endif
		// This is it...
		m_ppInstances[i] = instance;
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
LilvPlugin *qtractorLv2Plugin::lv2_plugin (void) const
{
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	return (pLv2Type ? pLv2Type->lv2_plugin() : NULL);
}


LilvInstance *qtractorLv2Plugin::lv2_instance ( unsigned short iInstance ) const
{
	return (m_ppInstances ? m_ppInstances[iInstance] : NULL);
}


LV2_Handle qtractorLv2Plugin::lv2_handle ( unsigned short iInstance ) const
{
	const LilvInstance *instance = lv2_instance(iInstance);
	return (instance ? lilv_instance_get_handle(instance) : NULL);
}


// Do the actual activation.
void qtractorLv2Plugin::activate (void)
{
	if (m_ppInstances) {
		for (unsigned short i = 0; i < instances(); ++i) {
			LilvInstance *instance = m_ppInstances[i];
			if (instance)
				lilv_instance_activate(instance);
		}
	}
}


// Do the actual deactivation.
void qtractorLv2Plugin::deactivate (void)
{
	if (m_ppInstances) {
		for (unsigned short i = 0; i < instances(); ++i) {
			LilvInstance *instance = m_ppInstances[i];
			if (instance)
				lilv_instance_deactivate(instance);
		}
	}
}


// The main plugin processing procedure.
void qtractorLv2Plugin::process (
	float **ppIBuffer, float **ppOBuffer, unsigned int nframes )
{
	if (m_ppInstances == NULL)
		return;

	const LilvPlugin *plugin = lv2_plugin();
	if (plugin == NULL)
		return;

#if defined(CONFIG_LV2_EVENT) || defined(CONFIG_LV2_ATOM)
	qtractorMidiManager *pMidiManager = NULL;
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type->midiIns() > 0)
		pMidiManager = list()->midiManager();
#ifdef CONFIG_LV2_EVENT
	const unsigned short iMidiEventIns  = pLv2Type->midiEventIns();
	const unsigned short iMidiEventOuts = pLv2Type->midiEventOuts();
#endif
#ifdef CONFIG_LV2_ATOM
	const unsigned short iMidiAtomIns  = pLv2Type->midiAtomIns();
	const unsigned short iMidiAtomOuts = pLv2Type->midiAtomOuts();
#endif
#endif

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
		LilvInstance *instance = m_ppInstances[i];
		if (instance) {
			// For each instance audio input port...
			for (j = 0; j < iAudioIns; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioIns[j], ppIBuffer[iIChannel]);
				if (++iIChannel >= iChannels)
					iIChannel = 0;
			}
			// For each instance audio output port...
			for (j = 0; j < iAudioOuts; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioOuts[j], ppOBuffer[iOChannel]);
				if (++iOChannel >= iChannels)
					iOChannel = 0;
			}
		#ifdef CONFIG_LV2_EVENT
			// Connect all existing input event/MIDI ports...
			if (pMidiManager) {
				for (j = 0; j < iMidiEventIns; ++j) {
					lilv_instance_connect_port(instance,
						m_piMidiEventIns[j], pMidiManager->lv2_events_in());
				}
				for (j = 0; j < iMidiEventOuts; ++j) {
					lilv_instance_connect_port(instance,
						m_piMidiEventOuts[j], pMidiManager->lv2_events_out());
				}
			}
		#endif
		#ifdef CONFIG_LV2_ATOM
			// Connect all existing input atom/MIDI ports...
			for (j = 0; j < iMidiAtomIns; ++j) {
				if (pMidiManager && j == 0)
					m_lv2_atom_port_ins[j] = pMidiManager->lv2_atom_buffer_in();
				else
					lv2_atom_buffer_reset(m_lv2_atom_port_ins[j], true);
				lilv_instance_connect_port(instance,
					m_piMidiAtomIns[j], &m_lv2_atom_port_ins[j]->atoms);
			}
			for (j = 0; j < iMidiAtomOuts; ++j) {
				if (pMidiManager && j == 0)
					m_lv2_atom_port_outs[j] = pMidiManager->lv2_atom_buffer_out();
				else
					lv2_atom_buffer_reset(m_lv2_atom_port_outs[j], false);
				lilv_instance_connect_port(instance,
					m_piMidiAtomOuts[j], &m_lv2_atom_port_outs[j]->atoms);
			}
		#ifdef CONFIG_LV2_UI
			// Read and apply control change events from UI...
			if (m_lv2_ui_widget) {
				uint32_t read_space = ::jack_ringbuffer_read_space(m_ui_events);
				while (read_space > 0) {
					ControlEvent ev;
					::jack_ringbuffer_read(m_ui_events, (char *) &ev, sizeof(ev));
					char buf[ev.size];
					if (::jack_ringbuffer_read(m_ui_events, buf, ev.size) < ev.size)
						break;
					if (ev.protocol == g_lv2_atom_event_type) {
						for (j = 0; j < iMidiAtomIns; ++j) {
							if (m_piMidiAtomIns[j] == ev.index) {
								LV2_Atom_Buffer_Iterator aiter;
								lv2_atom_buffer_end(&aiter, m_lv2_atom_port_ins[j]);
								const LV2_Atom *atom = (const LV2_Atom *) buf;
								lv2_atom_buffer_write(&aiter, nframes, 0,
									atom->type, atom->size,
									(const uint8_t *) LV2_ATOM_BODY(atom));
								break;
							}
						}
					}
					read_space -= sizeof(ev) + ev.size;
				}
			}
		#endif	// CONFIG_LV2_UI
		#endif	// CONFIG_LV2_ATOM
			// Make it run...
			lilv_instance_run(instance, nframes);
			// Done.
			// Wrap channels?...
			if (iIChannel < iChannels - 1)
				++iIChannel;
			if (iOChannel < iChannels - 1)
				++iOChannel;
		}
	}

#ifdef CONFIG_LV2_WORKER
	if (m_lv2_worker)
		m_lv2_worker->commit();
#endif

#ifdef CONFIG_LV2_ATOM
#ifdef CONFIG_LV2_UI
	for (j = 0; j < iMidiAtomOuts; ++j) {
		LV2_Atom_Buffer_Iterator aiter;
		lv2_atom_buffer_begin(&aiter, m_lv2_atom_port_outs[j]);
		while (true) {
			uint8_t *data;
			LV2_Atom_Event *pLv2AtomEvent
				= lv2_atom_buffer_get(&aiter, &data);
			if (pLv2AtomEvent == NULL)
				break;
		//	if (j > 0 || pLv2AtomEvent->body.type != QTRACTOR_LV2_MIDI_EVENT_ID) {
				char buf[sizeof(ControlEvent) + sizeof(LV2_Atom)];
				const uint32_t type = pLv2AtomEvent->body.type;
				const uint32_t size = pLv2AtomEvent->body.size;
				ControlEvent *ev = (ControlEvent *) buf;
				ev->index    = m_piMidiAtomOuts[j];
				ev->protocol = g_lv2_atom_event_type;
				ev->size     = sizeof(LV2_Atom) + size;
				LV2_Atom *atom = (LV2_Atom *) ev->body;
				atom->type = type;
				atom->size = size;
				if (::jack_ringbuffer_write_space(m_plugin_events)
					< sizeof(buf) + size)
					break;
				::jack_ringbuffer_write(m_plugin_events,
					(const char *) buf, sizeof(buf));
				::jack_ringbuffer_write(m_plugin_events,
					(const char *) data, size);
		//	}
			lv2_atom_buffer_increment(&aiter);
		}
	}
#endif	// CONFIG_LV2_UI
#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_EVENT
	if (pMidiManager && iMidiEventOuts > 0)
		pMidiManager->lv2_events_swap();
#endif
#ifdef CONFIG_LV2_ATOM
	if (pMidiManager && iMidiAtomOuts > 0)
		pMidiManager->lv2_atom_buffer_swap();
#endif
}


#ifdef CONFIG_LV2_UI

// Open editor.
void qtractorLv2Plugin::openEditor ( QWidget */*pParent*/ )
{
	if (m_lv2_ui_widget) {
		setEditorVisible(true);
		return;
	}

	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return;

	// Check the UI inventory...
	m_lv2_uis = lilv_plugin_get_uis(pLv2Type->lv2_plugin());
	if (m_lv2_uis == NULL)
		return;

	QMap<int, LilvUI *> ui_map;

	LILV_FOREACH(uis, i, m_lv2_uis) {
		LilvUI *ui = const_cast<LilvUI *> (lilv_uis_get(m_lv2_uis, i));
	#ifdef CONFIG_LV2_EXTERNAL_UI
		if (lilv_ui_is_a(ui, g_lv2_external_ui_class)
		#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
			|| lilv_ui_is_a(ui, g_lv2_external_ui_deprecated_class)
		#endif
		)	ui_map.insert(LV2_UI_TYPE_EXTERNAL, ui);
	#if QT_VERSION < 0x050000
		else
	#endif
	#endif
	#if QT_VERSION < 0x050000
		if (lilv_ui_is_a(ui, g_lv2_x11_ui_class))
			ui_map.insert(LV2_UI_TYPE_X11, ui);
		else
		if (lilv_ui_is_a(ui, g_lv2_gtk_ui_class))
			ui_map.insert(LV2_UI_TYPE_GTK, ui);
		else
		if (lilv_ui_is_a(ui, g_lv2_qt4_ui_class))
			ui_map.insert(LV2_UI_TYPE_QT4, ui);
	#endif
	}

	if (ui_map.isEmpty())
		return;

	QMap<int, LilvUI *>::ConstIterator ui_iter = ui_map.constBegin();

	m_lv2_ui_type = ui_iter.key();
	m_lv2_ui = ui_iter.value();

	const LilvInstance *instance = lv2_instance(0);
	if (instance == NULL)
		return;
	
	const LV2_Descriptor *descriptor = lilv_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return;

	updateEditorTitle();

	int iFeatures = 0;
	while (m_lv2_features[iFeatures]) { ++iFeatures; }

	m_lv2_ui_features = new LV2_Feature * [iFeatures + 5];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_ui_features[i] = (LV2_Feature *) m_lv2_features[i];

	m_lv2_data_access.data_access = descriptor->extension_data;
	m_lv2_data_access_feature.URI = LV2_DATA_ACCESS_URI;
	m_lv2_data_access_feature.data = &m_lv2_data_access;
	m_lv2_ui_features[iFeatures++] = &m_lv2_data_access_feature;

	m_lv2_instance_access_feature.URI = LV2_INSTANCE_ACCESS_URI;
	m_lv2_instance_access_feature.data = lilv_instance_get_handle(instance);
	m_lv2_ui_features[iFeatures++] = &m_lv2_instance_access_feature;

#ifdef CONFIG_LV2_EXTERNAL_UI
	m_lv2_ui_external_host.ui_closed = qtractor_lv2_ui_closed;
	m_lv2_ui_external_host.plugin_human_id = m_aEditorTitle.constData();
	m_lv2_ui_external_feature.URI = LV2_EXTERNAL_UI__Host;
	m_lv2_ui_external_feature.data = &m_lv2_ui_external_host;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_external_feature;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	m_lv2_ui_external_deprecated_feature.URI = LV2_EXTERNAL_UI_DEPRECATED_URI;
	m_lv2_ui_external_deprecated_feature.data = &m_lv2_ui_external_host;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_external_deprecated_feature;
#endif
#endif

	m_lv2_ui_features[iFeatures] = NULL;

	const char *ui_type_uri = NULL;
	switch (m_lv2_ui_type) {
#ifdef CONFIG_LV2_EXTERNAL_UI
	case LV2_UI_TYPE_EXTERNAL:
	#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
		if (lilv_ui_is_a(m_lv2_ui, g_lv2_external_ui_deprecated_class))
			ui_type_uri = LV2_EXTERNAL_UI_DEPRECATED_URI;
		else
	#endif
		ui_type_uri = LV2_EXTERNAL_UI__Widget;
		break;
#endif
#if QT_VERSION < 0x050000
	case LV2_UI_TYPE_X11:
		ui_type_uri = LV2_UI__X11UI;
		break;
	case LV2_UI_TYPE_GTK:
		ui_type_uri = LV2_UI__GtkUI;
		break;
	case LV2_UI_TYPE_QT4:
		ui_type_uri = LV2_UI__Qt4UI;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::openEditor(\"%s\")", this, ui_type_uri);
#endif

	m_suil_host = suil_host_new(qtractor_lv2_ui_write, NULL, NULL, NULL);
	m_suil_instance = suil_instance_new(m_suil_host, this, LV2_UI_HOST_URI,
		lilv_node_as_uri(lilv_plugin_get_uri(pLv2Type->lv2_plugin())),
		lilv_node_as_uri(lilv_ui_get_uri(m_lv2_ui)), ui_type_uri,
		lilv_uri_to_path(lilv_node_as_uri(lilv_ui_get_bundle_uri(m_lv2_ui))),
		lilv_uri_to_path(lilv_node_as_uri(lilv_ui_get_binary_uri(m_lv2_ui))),
		m_lv2_ui_features);

	if (m_suil_instance) {
		const qtractorPlugin::Params& params = qtractorPlugin::params();
		qtractorPlugin::Params::ConstIterator param = params.constBegin();
		const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
		for ( ; param != param_end; ++param) {
			qtractorPluginParam *pParam = param.value();
			const float fValue = pParam->value();
			suil_instance_port_event(m_suil_instance,
				pParam->index(), sizeof(float), 0, &fValue);
		}
		const unsigned long iControlOuts = pLv2Type->controlOuts();
		for (unsigned long j = 0; j < iControlOuts; ++j) {
			suil_instance_port_event(m_suil_instance,
				m_piControlOuts[j], sizeof(float), 0, &m_pfControlOuts[j]);
		}

		m_lv2_ui_widget = suil_instance_get_widget(m_suil_instance);
	#if QT_VERSION < 0x050000
		if (m_lv2_ui_widget && m_lv2_ui_type != LV2_UI_TYPE_EXTERNAL) {
			m_pQt4Widget = static_cast<QWidget *> (m_lv2_ui_widget);
			m_pQt4Widget->setWindowTitle(m_aEditorTitle.constData());
			m_pQt4Filter = new EventFilter(this, m_pQt4Widget);
			// LV2 UI resize control...
			resizeEditor(m_pQt4Widget->sizeHint());
		//	m_pQt4Widget->show();
		}
	#endif

		g_lv2Plugins.append(this);
	}

	setEditorVisible(true);

//	idleEditor();
}


// Close editor.
void qtractorLv2Plugin::closeEditor (void)
{
	if (m_lv2_ui_widget == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	m_ui_params.clear();

#if QT_VERSION < 0x050000
	if (m_pQt4Filter) {
		delete m_pQt4Filter;
		m_pQt4Filter = NULL;
	}
	if (m_pQt4Widget) {
	//	delete m_pQt4Widget;
		m_pQt4Widget = NULL;
	}
#endif

	m_lv2_ui_type = LV2_UI_TYPE_NONE;
	
	const int iLv2Plugin = g_lv2Plugins.indexOf(this);
	if (iLv2Plugin >= 0)
		g_lv2Plugins.removeAt(iLv2Plugin);

	if (m_suil_instance) {
		suil_instance_free(m_suil_instance);
		m_suil_instance = NULL;
	}

	if (m_suil_host) {
		suil_host_free(m_suil_host);
		m_suil_host = NULL;
	}

	m_lv2_ui_widget = NULL;
	
	if (m_lv2_ui_features) {
		delete [] m_lv2_ui_features;
		m_lv2_ui_features = NULL;
	}

	if (m_lv2_uis) {
		lilv_uis_free(m_lv2_uis);
		m_lv2_uis = NULL;
		m_lv2_ui = NULL;
	}
}


// Idle editor.
void qtractorLv2Plugin::idleEditor (void)
{
	if (m_lv2_ui_widget == NULL)
		return;

	// Do we need some clean-up...?
	if (isEditorClosed()) {
		setEditorClosed(false);
		if (isFormVisible())
			form()->toggleEditor(false);
		m_bEditorVisible = false;
		// Do really close now.
		closeEditor();
		return;
	}

	if (m_piControlOuts && m_pfControlOuts && m_pfControlOutsLast) {
		if (m_suil_instance) {
			const unsigned long iControlOuts = type()->controlOuts();
			for (unsigned short j = 0; j < iControlOuts; ++j) {
				if (m_pfControlOutsLast[j] != m_pfControlOuts[j]) {
					suil_instance_port_event(m_suil_instance,
						m_piControlOuts[j], sizeof(float), 0, &m_pfControlOuts[j]);
					m_pfControlOutsLast[j] = m_pfControlOuts[j];
				}
			}
		}
	}

#ifdef CONFIG_LV2_EXTERNAL_UI
	if (m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
		LV2_EXTERNAL_UI_RUN((LV2_External_UI_Widget *) m_lv2_ui_widget);
#endif

#ifdef CONFIG_LV2_ATOM

	uint32_t read_space = ::jack_ringbuffer_read_space(m_plugin_events);
	while (read_space > 0) {
		ControlEvent ev;
		::jack_ringbuffer_read(m_plugin_events, (char *) &ev, sizeof(ev));
		char buf[ev.size];
		if (::jack_ringbuffer_read(m_plugin_events, buf, ev.size) < ev.size)
			break;
		if (m_suil_instance) {
			suil_instance_port_event(m_suil_instance,
				ev.index, ev.size, ev.protocol, buf);
		}
		read_space -= sizeof(ev) + ev.size;
	}

#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_UI

	if (m_ui_params.count() > 0) {
		QHash<unsigned long, float>::ConstIterator iter
			= m_ui_params.constBegin();
		const QHash<unsigned long, float>::ConstIterator& iter_end
			= m_ui_params.constEnd();
		for ( ; iter != iter_end; ++iter) {
			const unsigned long iIndex = iter.key();
			const float fValue = iter.value();
		#if 0//def CONFIG_LV2_TIME
			const int i = m_lv2_time_ports.value(iIndex, -1);
			if (i >= 0) g_lv2_time[i].value = fValue;
		#endif
			updateParamValue(iIndex, fValue, false);
		}
		m_ui_params.clear();
	}

#endif	// CONFIG_LV2_UI
}


void qtractorLv2Plugin::closeEditorEx (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::closeEditorEx()", this);
#endif

#if QT_VERSION < 0x050000
	if (m_pQt4Widget) {
		m_pQt4Widget = NULL;
		setEditorClosed(true);
	}
#endif
}


// GUI editor visibility state.
void qtractorLv2Plugin::setEditorVisible ( bool bVisible )
{
	if (m_lv2_ui_widget == NULL)
		return;

	if (/*!m_bEditorVisible && */bVisible) {
	#ifdef CONFIG_LV2_EXTERNAL_UI
		if (m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
			LV2_EXTERNAL_UI_SHOW((LV2_External_UI_Widget *) m_lv2_ui_widget);
	#endif
	#if QT_VERSION < 0x050000
		if (m_pQt4Widget) {
			// Guaranteeing a reasonable window type:
			// ie. not Qt::Dialog or Qt::Popup from the seemingly good choices.
			if (m_lv2_ui_type == LV2_UI_TYPE_QT4) {
				const Qt::WindowFlags wflags
					= m_pQt4Widget->windowFlags() & ~Qt::WindowType_Mask;
				m_pQt4Widget->setWindowFlags(wflags | Qt::Widget);
			}
			m_pQt4Widget->show();
			m_pQt4Widget->raise();
			m_pQt4Widget->activateWindow();
		}
	#endif
		m_bEditorVisible = true;
	}
	else
	if (/*m_bEditorVisible && */!bVisible) {
	#ifdef CONFIG_LV2_EXTERNAL_UI
		if (m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
			LV2_EXTERNAL_UI_HIDE((LV2_External_UI_Widget *) m_lv2_ui_widget);
	#endif
	#if QT_VERSION < 0x050000
		if (m_pQt4Widget)
			m_pQt4Widget->hide();
	#endif
		m_bEditorVisible = false;
	}

	if (isFormVisible())
		form()->toggleEditor(m_bEditorVisible);
}

bool qtractorLv2Plugin::isEditorVisible (void) const
{
	return m_bEditorVisible;
}


// Update editor widget caption.
void qtractorLv2Plugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	m_aEditorTitle = editorTitle().toUtf8();

#ifdef CONFIG_LV2_EXTERNAL_UI
	m_lv2_ui_external_host.plugin_human_id = m_aEditorTitle.constData();
#endif
#if QT_VERSION < 0x050000
	if (m_pQt4Widget)
		m_pQt4Widget->setWindowTitle(m_aEditorTitle.constData());
#endif
}


// Parameter update method.
void qtractorLv2Plugin::updateParam (
	qtractorPluginParam *pParam, float fValue, bool bUpdate )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::updateParam(%lu, %g, %d)",
		this, pParam->index(), fValue, int(bUpdate));
#endif

	if (bUpdate && m_suil_instance) {
		suil_instance_port_event(m_suil_instance,
			pParam->index(), sizeof(float), 0, &fValue);
	}
}


// Idle editor (static).
void qtractorLv2Plugin::idleEditorAll (void)
{
	QListIterator<qtractorLv2Plugin *> iter(g_lv2Plugins);
	while (iter.hasNext())
		iter.next()->idleEditor();
}


// LV2 UI control change method.
void qtractorLv2Plugin::lv2_ui_write ( uint32_t port_index,
	uint32_t buffer_size, uint32_t protocol, const void *buffer )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_write(%u, %u, %u, %p)",
		this, port_index, buffer_size, protocol, buffer);
#endif

#ifdef CONFIG_LV2_ATOM
	if (protocol == g_lv2_atom_event_type) {
		char buf[sizeof(ControlEvent) + buffer_size];
		ControlEvent *ev = (ControlEvent *) buf;
		ev->index    = port_index;
		ev->protocol = protocol;
		ev->size     = buffer_size;
		::memcpy(ev->body, buffer, buffer_size);
		::jack_ringbuffer_write(m_ui_events, buf, sizeof(buf));
		return;
	}
#endif

	if (buffer_size != sizeof(float) || protocol != 0)
		return;

	const float val = *(float *) buffer;

	// FIXME: Update plugin params...
	// updateParamValue(port_index, val, false);
	m_ui_params.insert(port_index, val);
}


// LV2 UI resize control (host->ui).
void qtractorLv2Plugin::resizeEditor ( const QSize& size ) const
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::resizeEditor(%d, %d)",
		this, size.width(), size.height());
#endif

	if (m_suil_instance) {
		const LV2UI_Resize *resize
			= (const LV2UI_Resize *) suil_instance_extension_data(
				m_suil_instance, LV2_UI__resize);
		if (resize && resize->ui_resize) {
			(*resize->ui_resize)(resize->handle,
				size.width(), size.height());
		}
	}
}

#endif	// CONFIG_LV2_UI


// Plugin configuration/state (save) snapshot.
void qtractorLv2Plugin::freezeConfigs (void)
{
	if (!type()->isConfigure())
		return;

#ifdef CONFIG_LV2_STATE

	for (unsigned short i = 0; i < instances(); ++i) {
		const LV2_State_Interface *state = lv2_state_interface(i);
		if (state) {
			LV2_Handle handle = lv2_handle(i);
			if (handle)
				(*state->save)(handle, qtractor_lv2_state_store, this,
					LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, m_lv2_features);
		}
	}

#endif	// CONFIG_LV2_STATE
}


// Plugin configuration/state (load) realization.
void qtractorLv2Plugin::realizeConfigs (void)
{
	if (!type()->isConfigure())
		return;

#ifdef CONFIG_LV2_STATE

	m_lv2_state_configs.clear();
	m_lv2_state_ctypes.clear();

	const Configs& configs = qtractorPlugin::configs();
	const ConfigTypes& ctypes = qtractorPlugin::configTypes();

	Configs::ConstIterator config = configs.constBegin();
	const Configs::ConstIterator& config_end = configs.constEnd();
	for ( ; config != config_end; ++config) {
		const QString& sKey = config.key();
		QByteArray aType;
		ConfigTypes::ConstIterator ctype = ctypes.constFind(sKey);
		if (ctype != ctypes.constEnd())
			aType = ctype.value().toUtf8();
		const char *pszType = aType.constData();
		if (aType.isEmpty()
			|| ::strcmp(pszType, LV2_ATOM__Path)   == 0
			|| ::strcmp(pszType, LV2_ATOM__String) == 0) {
			m_lv2_state_configs.insert(sKey, config.value().toUtf8());
		} else {
			m_lv2_state_configs.insert(sKey, qUncompress(
				QByteArray::fromBase64(config.value().toUtf8())));
			m_lv2_state_ctypes.insert(sKey, lv2_urid_map(pszType));
		}
	}

	const unsigned short iInstances = instances();
	for (unsigned short i = 0; i < iInstances; ++i) {
		const LV2_State_Interface *state = lv2_state_interface(i);
		if (state) {
			LV2_Handle handle = lv2_handle(i);
			if (handle)
				(*state->restore)(handle, qtractor_lv2_state_retrieve, this,
					LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, m_lv2_features);
		}
	}

#endif	// CONFIG_LV2_STATE

	qtractorPlugin::realizeConfigs();
}


// Plugin configuration/state release.
void qtractorLv2Plugin::releaseConfigs (void)
{
	if (!type()->isConfigure())
		return;

#ifdef CONFIG_LV2_STATE
	m_lv2_state_configs.clear();
	m_lv2_state_ctypes.clear();
#endif

	qtractorPlugin::releaseConfigs();
}


#ifdef CONFIG_LV2_WORKER

// LV2 Worker/Schedule extension data interface accessor.
const LV2_Worker_Interface *qtractorLv2Plugin::lv2_worker_interface (
	unsigned short iInstance ) const
{
	const LilvInstance *instance = lv2_instance(iInstance);
	if (instance == NULL)
		return NULL;

	const LV2_Descriptor *descriptor = lilv_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return NULL;
	if (descriptor->extension_data == NULL)
		return NULL;

	return (const LV2_Worker_Interface *)
		(*descriptor->extension_data)(LV2_WORKER__interface);
}

#endif	// CONFIG_LV2_WORKER


#ifdef CONFIG_LV2_STATE

// LV2 State extension data descriptor accessor.
const LV2_State_Interface *qtractorLv2Plugin::lv2_state_interface (
	unsigned short iInstance ) const
{
	const LilvInstance *instance = lv2_instance(iInstance);
	if (instance == NULL)
		return NULL;

	const LV2_Descriptor *descriptor = lilv_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return NULL;
	if (descriptor->extension_data == NULL)
		return NULL;

	return (const LV2_State_Interface *)
		(*descriptor->extension_data)(LV2_STATE__interface);
}


LV2_State_Status qtractorLv2Plugin::lv2_state_store (
	uint32_t key, const void *value, size_t size, uint32_t type, uint32_t flags )
{
	if ((flags & (LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE)) == 0)
		return LV2_STATE_ERR_BAD_FLAGS;

	const char *pszKey = lv2_urid_unmap(key);
	if (pszKey == NULL)
		return LV2_STATE_ERR_UNKNOWN;

	const char *pszType = lv2_urid_unmap(type);
	if (pszType == NULL)
		return LV2_STATE_ERR_BAD_TYPE;

	const char *pchValue = (const char *) value;
	if (pchValue == NULL)
		return LV2_STATE_ERR_UNKNOWN;

	const QString& sKey = QString::fromUtf8(pszKey);
	if (::strcmp(pszType, LV2_ATOM__Path)   == 0 ||
		::strcmp(pszType, LV2_ATOM__String) == 0) {
		setConfig(sKey, QString::fromUtf8(pchValue, ::strlen(pchValue)));
	} else {
		QByteArray data = qCompress(
			QByteArray(pchValue, size)).toBase64();
		for (int i = data.size() - (data.size() % 72); i >= 0; i -= 72)
			data.insert(i, "\n       "); // Indentation.
		setConfig(sKey, data.constData());
		setConfigType(sKey, QString::fromUtf8(pszType));
	}

	return LV2_STATE_SUCCESS;
}


const void *qtractorLv2Plugin::lv2_state_retrieve (
	uint32_t key, size_t *size, uint32_t *type, uint32_t *flags )
{
	const char *pszKey = lv2_urid_unmap(key);
	if (pszKey == NULL)
		return NULL;

	const QString& sKey = QString::fromUtf8(pszKey);
	if (sKey.isEmpty())
		return NULL;

	QHash<QString, QByteArray>::ConstIterator iter
		= m_lv2_state_configs.constFind(sKey);
	if (iter == m_lv2_state_configs.constEnd())
		return NULL;

	const QByteArray& data = iter.value();

	if (size)
		*size = data.size();
	if (type) {
		QHash<QString, uint32_t>::ConstIterator ctype
			= m_lv2_state_ctypes.constFind(sKey);
		if (ctype != m_lv2_state_ctypes.constEnd())
			*type = ctype.value();
		else
			*type = g_lv2_atom_string_type;
	}
	if (flags)
		*flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

	return data.constData();
}


#endif	// CONFIG_LV2_STATE


#ifdef CONFIG_LV2_PROGRAMS

// LV2 Programs extension data descriptor accessor.
const LV2_Programs_Interface *qtractorLv2Plugin::lv2_programs_descriptor (
	unsigned short iInstance ) const
{
	const LilvInstance *instance = lv2_instance(iInstance);
	if (instance == NULL)
		return NULL;

	const LV2_Descriptor *descriptor = lilv_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return NULL;
	if (descriptor->extension_data == NULL)
		return NULL;

	return (const LV2_Programs_Interface *)
		(*descriptor->extension_data)(LV2_PROGRAMS__Interface);
}


// Bank/program selector.
void qtractorLv2Plugin::selectProgram ( int iBank, int iProg )
{
	if (iBank < 0 || iProg < 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	// For each plugin instance...
	for (unsigned short i = 0; i < instances(); ++i) {
		const LV2_Programs_Interface *programs = lv2_programs_descriptor(i);
		if (programs && programs->select_program) {
			LV2_Handle handle = lv2_handle(i);
			if (handle) {
				(*programs->select_program)(handle, iBank, iProg);
			}
		}
	}

#ifdef CONFIG_LV2_UI
	if (m_suil_instance) {
		const LV2_Programs_UI_Interface *ui_programs
			= (const LV2_Programs_UI_Interface *)
				suil_instance_extension_data(
					m_suil_instance, LV2_PROGRAMS__UIInterface);
		if (ui_programs && ui_programs->select_program) {
		#ifdef CONFIG_SUIL_INSTANCE_GET_HANDLE
			LV2UI_Handle ui_handle = (LV2UI_Handle)
				suil_instance_get_handle(m_suil_instance);
		#else
			struct SuilInstanceHead {	// HACK!
				void                   *ui_lib_handle;
				const LV2UI_Descriptor *ui_descriptor;
				LV2UI_Handle            ui_handle;
			} *suil_instance_head = (SuilInstanceHead *) m_suil_instance;
			LV2UI_Handle ui_handle = suil_instance_head->ui_handle;
		#endif	//  CONFIG_SUIL_INSTANCE_GET_HANDLE
		#ifdef CONFIG_DEBUG
			qDebug("qtractorLv2Plugin[%p]::selectProgram(%d, %d)"
				" LV2UI_Handle=%p", this, iBank, iProg, ui_handle);
		#endif
			if (ui_handle) {
				(*ui_programs->select_program)(ui_handle, iBank, iProg);
			}
		}
	}
#endif	// CONFIG_LV2_UI

	// Reset parameters default value...
	const qtractorPlugin::Params& params = qtractorPlugin::params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		pParam->setDefaultValue(pParam->value());
	}
}


// Provisional program/patch accessor.
bool qtractorLv2Plugin::getProgram ( int iIndex, Program& program ) const
{
	// Only first one instance should matter...
	const LV2_Programs_Interface *programs = lv2_programs_descriptor(0);
	if (programs == NULL)
		return false;
	if (programs->get_program == NULL)
		return false;

	LV2_Handle handle = lv2_handle(0);
	if (handle == NULL)
		return false;

	const LV2_Program_Descriptor *pLv2Program
		= (*programs->get_program)(handle, iIndex);
	if (pLv2Program == NULL)
		return false;

	// Map this to that...
	program.bank = pLv2Program->bank;
	program.prog = pLv2Program->program;
	program.name = pLv2Program->name;

	return true;
}


// Program/patch notification.
void qtractorLv2Plugin::lv2_program_changed ( int iIndex )
{
	qtractorPluginList *pList = list();

	if (iIndex < 0) {
		qtractorMidiManager *pMidiManager = pList->midiManager();
		if (pMidiManager)
			pMidiManager->updateInstruments();
	} else {
		qtractorPlugin::Program program;
		if (getProgram(iIndex, program)) {
			qtractorPluginList::MidiProgramSubject *pMidiProgramSubject
				= pList->midiProgramSubject();
			if (pMidiProgramSubject)
				pMidiProgramSubject->setProgram(program.bank, program.prog);
		}
	}
}

#endif	// CONFIG_LV2_PROGRAMS


#ifdef CONFIG_LV2_TIME

// Update LV2 Time from JACK transport position. (static)
inline void qtractor_lv2_time_update ( int i, float fValue )
{
	qtractorLv2Time& member = g_lv2_time[i];

	if (member.value != fValue
		&& member.params && member.params->count() > 0) {
		member.value  = fValue;
		QListIterator<qtractorLv2PluginParam *> iter(*member.params);
		while (iter.hasNext())
			iter.next()->setValue(fValue, true);
	}
}

void qtractorLv2Plugin::updateTime (
	const jack_transport_state_t state, const jack_position_t *pPos )
{
	if (pPos->valid & JackPositionBBT) {
		qtractor_lv2_time_update(
			qtractorLv2Time::position,
			float(pPos->tick));
		qtractor_lv2_time_update(
			qtractorLv2Time::bar,
			float(pPos->bar));
		qtractor_lv2_time_update(
			qtractorLv2Time::beat,
			float(pPos->beat));
		qtractor_lv2_time_update(
			qtractorLv2Time::beatUnit,
			float(pPos->beat_type));
		qtractor_lv2_time_update(
			qtractorLv2Time::beatsPerBar,
			float(pPos->beats_per_bar));
		qtractor_lv2_time_update(
			qtractorLv2Time::beatsPerMinute,
			float(pPos->beats_per_minute));
	}

	qtractor_lv2_time_update(
		qtractorLv2Time::frame,
		float(pPos->frame));
	qtractor_lv2_time_update(
		qtractorLv2Time::framesPerSecond,
		float(pPos->frame_rate));
	qtractor_lv2_time_update(
		qtractorLv2Time::speed,
		(state == JackTransportRolling ? 1.0f : 0.0f));
}

#endif	// CONFIG_LV2_TIME

#ifdef CONFIG_LV2_PRESETS

// Refresh and load preset labels listing. (virtual)
QStringList qtractorLv2Plugin::presetList (void) const
{
	QStringList list(qtractorPlugin::presetList());

	QHash<QString, QString>::ConstIterator iter
		= m_lv2_presets.constBegin();
	const QHash<QString, QString>::ConstIterator& iter_end
		= m_lv2_presets.constEnd();
	for ( ; iter != iter_end; ++iter)
		list.append(iter.key());

	return list;
}

// Load plugin state from a named preset.
bool qtractorLv2Plugin::loadPreset ( const QString& sPreset )
{
	const QString& sUri = m_lv2_presets.value(sPreset);
	if (sUri.isEmpty())
		return false;

	LilvNode *preset
		= lilv_new_uri(g_lv2_world, sUri.toUtf8().constData());

	LilvState *state = NULL;
	const QString& sPath = QUrl(sUri).toLocalFile();
	if (!sPath.isEmpty() && QFileInfo(sPath).exists()) {
		state = lilv_state_new_from_file(g_lv2_world,
			&g_lv2_urid_map, preset,
			sPath.toUtf8().constData());
	} else {
		state = lilv_state_new_from_world(g_lv2_world,
			&g_lv2_urid_map, preset);
	}
	if (state == NULL) {
		lilv_node_free(preset);
		return false;
	}

	for (unsigned short i = 0; i < instances(); ++i) {
		lilv_state_restore(state, m_ppInstances[i],
			qtractor_lv2_set_port_value, this, 0, NULL);
	}

	lilv_state_free(state);
	lilv_node_free(preset);

	return true;
}

// Save current plugin state as a named preset.
bool qtractorLv2Plugin::savePreset ( const QString& sPreset )
{
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return false;

	LilvState *state = lilv_state_new_from_instance(
		lv2_plugin(), m_ppInstances[0], &g_lv2_urid_map,
		NULL, NULL, NULL, NULL,
		qtractor_lv2_get_port_value, this,
		LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, m_lv2_features);

	if (state == NULL)
		return false;

	lilv_state_set_label(state, sPreset.toUtf8().constData());

	const QString sDotLv2(".lv2");
	const QString& sep = QDir::separator();

	QString sDir;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		sDir = pOptions->sLv2PresetDir;
	if (sDir.isEmpty())
		sDir = QDir::homePath() + sep + sDotLv2;
	sDir += sep + pLv2Type->label();
	sDir += '_' + QString::number(pLv2Type->uniqueID(), 16);
	sDir += '-' + sPreset + sDotLv2;

	const QString& sFile = sPreset + ".ttl";

	int ret = lilv_state_save(g_lv2_world,
		&g_lv2_urid_map, &g_lv2_urid_unmap, state, NULL,
		sDir.toUtf8().constData(), sFile.toUtf8().constData());

	lilv_state_free(state);

	if (ret == 0) {
		m_lv2_presets.insert(sPreset,
			QUrl::fromLocalFile(sDir + sep + sFile).toString());
	}

	return (ret == 0);
}

// Delete plugin state preset (from file-system).
bool qtractorLv2Plugin::deletePreset ( const QString& sPreset )
{
	const QString& sUri = m_lv2_presets.value(sPreset);
	if (sUri.isEmpty())
		return false;

	const QString& sPath = QUrl(sUri).toLocalFile();
	if (!sPath.isEmpty()) {
		QFileInfo info(sPath);
		if (!info.isDir())
			info.setFile(info.absolutePath());
		qtractor_lv2_remove_file(info);
		m_lv2_presets.remove(sPreset);
	}

	return true;
}

// Whether given preset is internal/read-only.
bool qtractorLv2Plugin::isReadOnlyPreset ( const QString& sPreset ) const
{
	const QString& sUri = m_lv2_presets.value(sPreset);
	if (sUri.isEmpty())
		return false;

	const QString& sPath = QUrl(sUri).toLocalFile();
	if (sPath.isEmpty())
		return true;

	const QFileInfo info(sPath);
	return !info.exists() || !info.isWritable();
}

#endif	// CONFIG_LV2_PRESETS


//----------------------------------------------------------------------------
// qtractorLv2PluginParam -- LV2 plugin control input port instance.
//

// Constructors.
qtractorLv2PluginParam::qtractorLv2PluginParam (
	qtractorLv2Plugin *pLv2Plugin, unsigned long iIndex )
	: qtractorPluginParam(pLv2Plugin, iIndex), m_iPortHints(None)
{
	const LilvPlugin *plugin = pLv2Plugin->lv2_plugin();
	const LilvPort *port = lilv_plugin_get_port_by_index(plugin, iIndex);

	// Set nominal parameter name...
	LilvNode *name = lilv_port_get_name(plugin, port);
	setName(lilv_node_as_string(name));
	lilv_node_free(name);

	// Get port properties and set hints...
	if (lilv_port_has_property(plugin, port, g_lv2_toggled_prop))
		m_iPortHints |= Toggled;
	if (lilv_port_has_property(plugin, port, g_lv2_integer_prop))
		m_iPortHints |= Integer;
	if (lilv_port_has_property(plugin, port, g_lv2_sample_rate_prop))
		m_iPortHints |= SampleRate;
	if (lilv_port_has_property(plugin, port, g_lv2_logarithmic_prop))
		m_iPortHints |= Logarithmic;

	// Initialize range values...
	LilvNode *vdef;
	LilvNode *vmin;
	LilvNode *vmax;

	lilv_port_get_range(plugin, port, &vdef, &vmin, &vmax);

	setMinValue(vmin ? lilv_node_as_float(vmin) : 0.0f);
	setMaxValue(vmax ? lilv_node_as_float(vmax) : 1.0f);

	setDefaultValue(vdef ? lilv_node_as_float(vdef) : 0.0f);

	if (vdef) lilv_node_free(vdef);
	if (vmin) lilv_node_free(vmin);
	if (vmax) lilv_node_free(vmax);

	// Have scale points (display values)
	// m_display.clear();
	LilvScalePoints *points = lilv_port_get_scale_points(plugin, port);
	if (points) {
		LILV_FOREACH(scale_points, i, points) {
			const LilvScalePoint *point = lilv_scale_points_get(points, i);
			const LilvNode *value = lilv_scale_point_get_value(point);
			const LilvNode *label = lilv_scale_point_get_label(point);
			if (value && label) {
				float   fValue = lilv_node_as_float(value);
				QString sLabel = lilv_node_as_string(label);
				m_display.insert(QString::number(fValue), sLabel);
			}
		}
		lilv_scale_points_free(points);
	}

	// Initialize port value...
	reset();
}


// Destructor.
qtractorLv2PluginParam::~qtractorLv2PluginParam (void)
{
}


// Port range hints predicate methods.
bool qtractorLv2PluginParam::isBoundedBelow (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isBoundedAbove (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isDefaultValue (void) const
{
	return true;
}

bool qtractorLv2PluginParam::isLogarithmic (void) const
{
	return (m_iPortHints & Logarithmic);
}

bool qtractorLv2PluginParam::isSampleRate (void) const
{
	return (m_iPortHints & SampleRate);
}

bool qtractorLv2PluginParam::isInteger (void) const
{
	return (m_iPortHints & Integer);
}

bool qtractorLv2PluginParam::isToggled (void) const
{
	return (m_iPortHints & Toggled);
}

bool qtractorLv2PluginParam::isDisplay (void) const
{
	return !m_display.isEmpty();
}


// Current display value.
QString qtractorLv2PluginParam::display (void) const
{
	// Check if current value is mapped...
	if (isDisplay()) {
		float fValue = value();
		if (isInteger())
			fValue = ::rintf(fValue);
		const QString& sValue = QString::number(fValue);
		if (m_display.contains(sValue))
			return m_display.value(sValue);
	}

	// Default parameter display value...
	return qtractorPluginParam::display();
}


#endif	// CONFIG_LV2

// end of qtractorLv2Plugin.cpp
