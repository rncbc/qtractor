// qtractorLv2Plugin.cpp
//
/****************************************************************************
   Copyright (C) 2005-2019, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorPluginFactory.h"

#include "qtractorSession.h"
#include "qtractorAudioEngine.h"
#include "qtractorMidiManager.h"

#include "qtractorSessionCursor.h"

#include "qtractorOptions.h"

#ifdef CONFIG_LV2_STATE
// LV2 State/Dirty (StateChanged) notification.
#include "qtractorMainForm.h"
// LV2 State/Presets: standard directory access.
// For local file vs. URI manipulations.
#include <QFileInfo>
#include <QDir>
#include <QUrl>
#endif

#include <math.h>

#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif


// URI map/unmap features.
#include "lv2/lv2plug.in/ns/ext/urid/urid.h"

static QHash<QString, LV2_URID>    g_uri_map;
static QHash<LV2_URID, QByteArray> g_ids_map;


static LV2_URID qtractor_lv2_urid_map (
	LV2_URID_Map_Handle /*handle*/, const char *uri )
{
	if (strcmp(uri, LILV_URI_MIDI_EVENT) == 0)
		return QTRACTOR_LV2_MIDI_EVENT_ID;
	else
		return qtractorLv2Plugin::lv2_urid_map(uri);
}

static LV2_URID_Map g_lv2_urid_map =
	{ NULL, qtractor_lv2_urid_map };
static const LV2_Feature g_lv2_urid_map_feature =
	{ LV2_URID_MAP_URI, &g_lv2_urid_map };


static const char *qtractor_lv2_urid_unmap (
	LV2_URID_Unmap_Handle /*handle*/, LV2_URID id )
{
	if (id == QTRACTOR_LV2_MIDI_EVENT_ID)
		return LILV_URI_MIDI_EVENT;
	else
		return qtractorLv2Plugin::lv2_urid_unmap(id);
}

static LV2_URID_Unmap g_lv2_urid_unmap =
	{ NULL, qtractor_lv2_urid_unmap };
static const LV2_Feature g_lv2_urid_unmap_feature =
	{ LV2_URID_UNMAP_URI, &g_lv2_urid_unmap };


#ifdef CONFIG_LV2_EVENT

// URI map (uri_to_id) feature (DEPRECATED)
#include "lv2/lv2plug.in/ns/ext/uri-map/uri-map.h"

static LV2_URID qtractor_lv2_uri_to_id (
	LV2_URI_Map_Callback_Data /*data*/, const char *map, const char *uri )
{
	if ((map && strcmp(map, LV2_EVENT_URI) == 0)
		&& strcmp(uri, LILV_URI_MIDI_EVENT) == 0)
		return QTRACTOR_LV2_MIDI_EVENT_ID;
	else
		return qtractorLv2Plugin::lv2_urid_map(uri);
}

static LV2_URI_Map_Feature g_lv2_uri_map =
	{ NULL, qtractor_lv2_uri_to_id };
static const LV2_Feature g_lv2_uri_map_feature =
	{ LV2_URI_MAP_URI, &g_lv2_uri_map };

#endif	// CONFIG_LV2_EVENT

#ifdef CONFIG_LV2_STATE

#ifndef LV2_STATE__StateChanged
#define LV2_STATE__StateChanged LV2_STATE_PREFIX "StateChanged"
#endif

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
LV2_URID qtractorLv2Plugin::lv2_urid_map ( const char *uri )
{
	const QString sUri(uri);

	QHash<QString, uint32_t>::ConstIterator iter
		= g_uri_map.constFind(sUri);
	if (iter == g_uri_map.constEnd()) {
		LV2_URID id = g_uri_map.size() + 1000;
		g_uri_map.insert(sUri, id);
		g_ids_map.insert(id, sUri.toUtf8());
		return id;
	}

	return iter.value();
}

const char *qtractorLv2Plugin::lv2_urid_unmap ( LV2_URID id )
{
	QHash<LV2_URID, QByteArray>::ConstIterator iter
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

private:

	// Instance members.
	qtractorLv2Plugin  *m_pLv2Plugin;

	LV2_Feature       **m_lv2_features;

	LV2_Feature         m_lv2_schedule_feature;
	LV2_Worker_Schedule m_lv2_schedule;

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

#ifdef CONFIG_DEBUG_0
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

#ifdef CONFIG_DEBUG_0
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
	if (pLv2Worker) {
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

	m_pRequests  = ::jack_ringbuffer_create(4096);
	m_pResponses = ::jack_ringbuffer_create(4096);
	m_pResponse  = (void *) ::malloc(4096);

	if (++g_iWorkerRefCount == 1) {
		g_pWorkerThread = new qtractorLv2WorkerThread();
		g_pWorkerThread->start();
	}
}

// Destructor.
qtractorLv2Worker::~qtractorLv2Worker (void)
{
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
	const uint32_t request_size = size + sizeof(size);

	if (::jack_ringbuffer_write_space(m_pRequests) >= request_size) {
		char request_data[request_size];
		::memcpy(request_data, &size, sizeof(size));
		::memcpy(request_data + sizeof(size), data, size);
		::jack_ringbuffer_write(m_pRequests,
			(const char *) &request_data, request_size);
	}

	if (g_pWorkerThread)
		g_pWorkerThread->sync(this);
}

// Response work.
void qtractorLv2Worker::respond ( uint32_t size, const void *data )
{
	const uint32_t response_size = size + sizeof(size);

	if (::jack_ringbuffer_write_space(m_pResponses) >= response_size) {
		char response_data[response_size];
		::memcpy(response_data, &size, sizeof(size));
		::memcpy(response_data + sizeof(size), data, size);
		::jack_ringbuffer_write(m_pResponses,
			(const char *) &response_data, response_size);
	}
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
}

#endif	// CONFIG_LV2_WORKER


#ifdef CONFIG_LV2_STATE_FILES

#include "qtractorDocument.h"

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

	QString sDir = pLv2Plugin->lv2_state_save_dir();
	const bool bSessionDir = sDir.isEmpty();
	if (bSessionDir)
		sDir = pSession->sessionDir();

	const QFileInfo fi(absolute_path);
	const QString& sAbsolutePath = fi.absoluteFilePath();

	QString sAbstractPath = QDir(sDir).relativeFilePath(sAbsolutePath);
	if (bSessionDir)
		sAbstractPath = qtractorDocument::addFile(sDir, sAbstractPath);
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

	// absolute_path from abstract_path...

	QString sDir = pLv2Plugin->lv2_state_save_dir();
	const bool bSessionDir = sDir.isEmpty();
	if (bSessionDir)
		sDir = pSession->sessionDir();

	QFileInfo fi(abstract_path);
	if (fi.isRelative())
		fi.setFile(QDir(sDir), fi.filePath());

	const QString& sAbsolutePath = fi.absoluteFilePath();
	return ::strdup(sAbsolutePath.toUtf8().constData());
}

#ifdef CONFIG_LV2_STATE_MAKE_PATH

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

	// make_path from relative_path...

	QString sDir = pLv2Plugin->lv2_state_save_dir();
	if (sDir.isEmpty()) {
		sDir  = pSession->sessionDir();
		sDir += QDir::separator();
	#if 0
		const QString& sSessionName = pSession->sessionName();
		if (!sSessionName.isEmpty()) {
			sDir += qtractorSession::sanitize(sSessionName);
			sDir += '-';
		}
		const QString& sListName = pLv2Plugin->list()->name();
		if (!sListName.isEmpty()) {
			sDir += qtractorSession::sanitize(sListName);
			sDir += '-';
		}
	#endif
		sDir += pLv2Plugin->type()->label();
		sDir += '-';
		sDir += QString::number(pLv2Plugin->uniqueID(), 16);
	}

	QDir dir; int i = 0;
	const QString sPath = sDir + "-%1";
	do dir.setPath(sPath.arg(++i));
	while (dir.exists());

	QFileInfo fi(relative_path);
	if (fi.isRelative())
		fi.setFile(dir, fi.filePath());
	if (fi.isSymLink())
		fi.setFile(fi.symLinkTarget());

	const QString& sMakeDir = fi.absolutePath();
	if (!QDir(sMakeDir).exists())
		dir.mkpath(sMakeDir);

	const QString& sMakePath = fi.absoluteFilePath();
	return ::strdup(sMakePath.toUtf8().constData());
}

#endif	// CONFIG_LV2_STATE_MAKE_PATH

#endif	// CONFIG_LV2_STATE_FILES


#ifdef CONFIG_LV2_BUF_SIZE

// LV2 Buffer size option.
#include "lv2/lv2plug.in/ns/ext/buf-size/buf-size.h"

#ifndef LV2_BUF_SIZE__nominalBlockLength
#define LV2_BUF_SIZE__nominalBlockLength LV2_BUF_SIZE_PREFIX "nominalBlockLength"
#endif

static const LV2_Feature g_lv2_buf_size_fixed_feature =
	{ LV2_BUF_SIZE__fixedBlockLength, NULL };
static const LV2_Feature g_lv2_buf_size_bounded_feature =
	{ LV2_BUF_SIZE__boundedBlockLength, NULL };

#endif	// CONFIG_LV2_BUF_SIZE


static const LV2_Feature *g_lv2_features[] =
{
	&g_lv2_urid_map_feature,
	&g_lv2_urid_unmap_feature,
#ifdef CONFIG_LV2_EVENT
	&g_lv2_uri_map_feature,	// deprecated.
#endif
#ifdef CONFIG_LV2_STATE
	&g_lv2_state_feature,
#endif
#ifdef CONFIG_LV2_BUF_SIZE
	&g_lv2_buf_size_fixed_feature,
	&g_lv2_buf_size_bounded_feature,
#endif
	NULL
};


#ifdef CONFIG_LV2_UI

#include "qtractorPluginForm.h"

#include "qtractorMessageBox.h"

#include <QButtonGroup>
#include <QRadioButton>
#include <QCheckBox>

#define LV2_UI_TYPE_NONE       0
#define LV2_UI_TYPE_QT4        1
#define LV2_UI_TYPE_QT5        2
#define LV2_UI_TYPE_GTK        3
#define LV2_UI_TYPE_X11        4
#define LV2_UI_TYPE_EXTERNAL   5
#define LV2_UI_TYPE_OTHER      6

#ifndef LV2_UI__Qt5UI
#define LV2_UI__Qt5UI	LV2_UI_PREFIX "Qt5UI"
#endif

#if QT_VERSION < 0x050000
#define LV2_UI_HOST_URI	LV2_UI__Qt4UI
#else
#define LV2_UI_HOST_URI	LV2_UI__Qt5UI
#endif

#ifndef LV2_UI__windowTitle
#define LV2_UI__windowTitle	LV2_UI_PREFIX "windowTitle"
#endif

#ifndef LV2_UI__updateRate
#define LV2_UI__updateRate	LV2_UI_PREFIX "updateRate"
#endif

static void qtractor_lv2_ui_port_write (
	LV2UI_Controller ui_controller,
	uint32_t port_index,
	uint32_t buffer_size,
	uint32_t protocol,
	const void *buffer )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return;

	pLv2Plugin->lv2_ui_port_write(port_index, buffer_size, protocol, buffer);
}

static uint32_t qtractor_lv2_ui_port_index (
	LV2UI_Controller ui_controller,
	const char *port_symbol )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (ui_controller);
	if (pLv2Plugin == NULL)
		return LV2UI_INVALID_PORT_INDEX;

	return pLv2Plugin->lv2_ui_port_index(port_symbol);
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


#ifdef CONFIG_LV2_UI_TOUCH

static void qtractor_lv2_ui_touch (
	LV2UI_Feature_Handle handle, uint32_t port_index, bool grabbed )
{
	qtractorLv2Plugin *pLv2Plugin
		= static_cast<qtractorLv2Plugin *> (handle);
	if (pLv2Plugin == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_ui_touch(%p, %u, %d)", pLv2Plugin, port_index, int(grabbed));
#endif

	// Just flag up the closure...
	pLv2Plugin->lv2_ui_touch(port_index, grabbed);
}

#endif	// CONFIG_LV2_UI_TOUCH


#include <QResizeEvent>

class qtractorLv2Plugin::EventFilter : public QObject
{
public:

	// Constructor.
	EventFilter(qtractorLv2Plugin *pLv2Plugin, QWidget *pQtWidget)
		: QObject(), m_pLv2Plugin(pLv2Plugin), m_pQtWidget(pQtWidget)
		{ m_pQtWidget->installEventFilter(this); }

	bool eventFilter(QObject *pObject, QEvent *pEvent)
	{
		if (pObject == static_cast<QObject *> (m_pQtWidget)) {
			switch (pEvent->type()) {
			case QEvent::Close: {
				// Defer widget close!
				m_pQtWidget->removeEventFilter(this);
				m_pQtWidget = NULL;
				m_pLv2Plugin->closeEditorEx();
				pEvent->ignore();
				return true;
			}
			case QEvent::Resize: {
				// LV2 UI resize control...
				QResizeEvent *pResizeEvent
					= static_cast<QResizeEvent *> (pEvent);
				if (pResizeEvent)
					m_pLv2Plugin->lv2_ui_resize(pResizeEvent->size());
				break;
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
	QWidget           *m_pQtWidget;
};


#endif	// CONFIG_LV2_UI


// LV2 World stuff (ref. counted).
static LilvWorld   *g_lv2_world   = NULL;
static LilvPlugins *g_lv2_plugins = NULL;

// Supported port classes.
static LilvNode *g_lv2_input_class   = NULL;
static LilvNode *g_lv2_output_class  = NULL;
static LilvNode *g_lv2_control_class = NULL;
static LilvNode *g_lv2_audio_class   = NULL;
static LilvNode *g_lv2_midi_class    = NULL;

#ifdef CONFIG_LV2_EVENT
static LilvNode *g_lv2_event_class = NULL;
#endif

#ifdef CONFIG_LV2_ATOM
static LilvNode *g_lv2_atom_class = NULL;
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
static LilvNode *g_lv2_external_ui_class = NULL;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
static LilvNode *g_lv2_external_ui_deprecated_class = NULL;
#endif
#endif
static LilvNode *g_lv2_x11_ui_class = NULL;
static LilvNode *g_lv2_gtk_ui_class = NULL;
static LilvNode *g_lv2_qt4_ui_class = NULL;
static LilvNode *g_lv2_qt5_ui_class = NULL;
#endif	// CONFIG_LV2_UI

// Supported plugin features.
static LilvNode *g_lv2_realtime_hint = NULL;
static LilvNode *g_lv2_extension_data_hint = NULL;

#ifdef CONFIG_LV2_WORKER
static LilvNode *g_lv2_worker_schedule_hint = NULL;
#endif

#ifdef CONFIG_LV2_STATE
static LilvNode *g_lv2_state_interface_hint = NULL;
static LilvNode *g_lv2_state_load_default_hint = NULL;
#endif

// Supported port properties (hints).
static LilvNode *g_lv2_toggled_prop     = NULL;
static LilvNode *g_lv2_integer_prop     = NULL;
static LilvNode *g_lv2_sample_rate_prop = NULL;

#include "lv2/lv2plug.in/ns/ext/port-props/port-props.h"

static LilvNode *g_lv2_logarithmic_prop = NULL;


#ifdef CONFIG_LV2_ATOM

#include "lv2/lv2plug.in/ns/ext/atom/forge.h"
#include "lv2/lv2plug.in/ns/ext/atom/util.h"

static LV2_Atom_Forge *g_lv2_atom_forge = NULL;

#ifndef CONFIG_LV2_ATOM_FORGE_OBJECT
#define lv2_atom_forge_object(forge, frame, id, otype) \
		lv2_atom_forge_blank(forge, frame, id, otype)
#endif

#ifndef CONFIG_LV2_ATOM_FORGE_KEY
#define lv2_atom_forge_key(forge, key) \
		lv2_atom_forge_property_head(forge, key, 0)
#endif

static LilvNode *g_lv2_minimum_prop = NULL;
static LilvNode *g_lv2_maximum_prop = NULL;
static LilvNode *g_lv2_default_prop = NULL;

#include "lv2/lv2plug.in/ns/ext/resize-port/resize-port.h"

static LilvNode *g_lv2_minimum_size_prop = NULL;

#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_PATCH
static LilvNode *g_lv2_patch_message_class = NULL;
#endif

// LV2 URIDs stock.
static struct qtractorLv2Urids
{
#ifdef CONFIG_LV2_ATOM
	LV2_URID atom_eventTransfer;
	LV2_URID atom_Chunk;
	LV2_URID atom_Sequence;
	LV2_URID atom_Object;
	LV2_URID atom_Blank;
	LV2_URID atom_Bool;
	LV2_URID atom_Int;
	LV2_URID atom_Long;
	LV2_URID atom_Float;
	LV2_URID atom_Double;
	LV2_URID atom_String;
	LV2_URID atom_Path;
#endif
#ifdef CONFIG_LV2_PATCH
	LV2_URID patch_Get;
	LV2_URID patch_Put;
	LV2_URID patch_Set;
	LV2_URID patch_body;
	LV2_URID patch_property;
	LV2_URID patch_value;
#endif
#ifdef CONFIG_LV2_TIME
#ifdef CONFIG_LV2_TIME_POSITION
	LV2_URID time_Position;
#endif
#endif	// CONFIG_LV2_TIME
#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	LV2_URID bufsz_minBlockLength;
	LV2_URID bufsz_maxBlockLength;
	LV2_URID bufsz_nominalBlockLength;
	LV2_URID bufsz_sequenceSize;
#endif
#ifdef CONFIG_LV2_UI
	LV2_URID ui_windowTitle;
	LV2_URID ui_updateRate;
	LV2_URID ui_sampleRate;
#endif
#endif	// CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_STATE
	LV2_URID state_StateChanged;
#endif

} g_lv2_urids;


#ifdef CONFIG_LV2_PROGRAMS

#include "qtractorPluginCommand.h"

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


#ifdef CONFIG_LV2_STATE

// LV2 State/Presets: port value setter.
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

	if (size != sizeof(float) || type != g_lv2_urids.atom_Float)
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

#endif	// CONFIG_LV2_STATE

#ifdef CONFIG_LV2_PRESETS

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
			*type = g_lv2_urids.atom_Float;
			retv = (const void *) (pParam->subject())->data();
		}
	}

	lilv_node_free(symbol);

	return retv;
}

// Remove specific dir/file path.
static void qtractor_lv2_remove_file (const QFileInfo& info);

static void qtractor_lv2_remove_dir ( const QString& sDir )
{
	const QDir dir(sDir);

	const QList<QFileInfo>& list
		= dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot);
	QListIterator<QFileInfo> iter(list);
	while (iter.hasNext())
		qtractor_lv2_remove_file(iter.next());

	QDir cwd = QDir::current();
	if (cwd.absolutePath() == dir.absolutePath()) {
		cwd.cdUp();
		QDir::setCurrent(cwd.path());
	}

	dir.rmdir(sDir);
}

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

#endif	// CONFIG_LV2_PRESETS


#ifdef CONFIG_LV2_PATCH

//----------------------------------------------------------------------
// class qtractorLv2Plugin::Property -- LV2 Patch/property registry item.
//
qtractorLv2Plugin::Property::Property ( const LilvNode *property )
{
	static const char *s_types[] = {
		LV2_ATOM__Bool,
		LV2_ATOM__Int,
		LV2_ATOM__Long,
		LV2_ATOM__Float,
		LV2_ATOM__Double,
		LV2_ATOM__String,
		LV2_ATOM__Path,
		NULL
	};

	LilvNode *label_uri = lilv_new_uri(g_lv2_world, LILV_NS_RDFS "label");
	LilvNode *range_uri = lilv_new_uri(g_lv2_world, LILV_NS_RDFS "range");

	const char *prop_uri = lilv_node_as_uri(property);
	m_key = qtractorLv2Plugin::lv2_urid_map(prop_uri);
	m_uri = prop_uri;

	LilvNodes *nodes = lilv_world_find_nodes(
		g_lv2_world, property, label_uri, NULL);
	LilvNode *label = (nodes ? lilv_nodes_get_first(nodes) : label_uri);
	m_name = (label ? lilv_node_as_string(label) : prop_uri);
	m_type = 0;
	for (int i = 0; s_types[i]; ++i) {
		const char *type = s_types[i];
		LilvNode *range = lilv_new_uri(g_lv2_world, type);
		const bool has_range = lilv_world_ask(
			g_lv2_world, property, range_uri, range);
		lilv_node_free(range);
		if (has_range) {
			m_type = qtractorLv2Plugin::lv2_urid_map(type);
			break;
		}
	}

	LilvNode *prop_min = lilv_world_get(
		g_lv2_world, property, g_lv2_minimum_prop, NULL);
	LilvNode *prop_max = lilv_world_get(
		g_lv2_world, property, g_lv2_maximum_prop, NULL);
	LilvNode *prop_def = lilv_world_get(
		g_lv2_world, property, g_lv2_default_prop, NULL);

	if (m_type == g_lv2_urids.atom_Bool) {
		m_min = float(prop_min ? lilv_node_as_bool(prop_min) : false);
		m_max = float(prop_max ? lilv_node_as_bool(prop_max) : true);
		m_def = float(prop_def ? lilv_node_as_bool(prop_def) : false);
	}
	else
	if (m_type == g_lv2_urids.atom_Int ||
		m_type == g_lv2_urids.atom_Long) {
		m_min = float(prop_min ? lilv_node_as_int(prop_min) : 0);
		m_max = float(prop_max ? lilv_node_as_int(prop_max) : INT32_MAX);
		m_def = float(prop_def ? lilv_node_as_int(prop_def) : 0);
	}
	else
	if (m_type == g_lv2_urids.atom_Float ||
		m_type == g_lv2_urids.atom_Double) {
		m_min = (prop_min ? lilv_node_as_float(prop_min) : 0.0f);
		m_max = (prop_max ? lilv_node_as_float(prop_max) : 1.0f);
		m_def = (prop_def ? lilv_node_as_float(prop_def) : 0.0f);
	}
	else m_min = m_max = m_def = 0.0f;

	if (prop_min) lilv_node_free(prop_min);
	if (prop_max) lilv_node_free(prop_max);
	if (prop_def) lilv_node_free(prop_def);

	if (nodes) lilv_nodes_free(nodes);

	lilv_node_free(label_uri);
	lilv_node_free(range_uri);
}

bool qtractorLv2Plugin::Property::isToggled (void) const
	{ return (m_type == g_lv2_urids.atom_Bool); }

bool qtractorLv2Plugin::Property::isInteger (void) const
	{ return (m_type == g_lv2_urids.atom_Int || m_type == g_lv2_urids.atom_Long); }

bool qtractorLv2Plugin::Property::isString (void) const
	{ return (m_type == g_lv2_urids.atom_String); }

bool qtractorLv2Plugin::Property::isPath (void) const
	{ return (m_type == g_lv2_urids.atom_Path); }

#endif	// CONFIG_LV2_PATCH


#ifdef CONFIG_LV2_TIME

// JACK Transport position support.
#include <jack/transport.h>

// LV2 Time-position control structure.
static struct qtractorLv2Time
{
	enum Index {

		frame = 0,
		framesPerSecond,
		speed,
		bar,
		beat,
		barBeat,
		beatUnit,
		beatsPerBar,
		beatsPerMinute,

		numOfMembers
	};

	const char *uri;
	LV2_URID    urid;
	LilvNode   *node;
	float       value;
	uint32_t    changed;

	QList<qtractorLv2PluginParam *> *params;

} g_lv2_time[] = {

	{ LV2_TIME__frame,           0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__framesPerSecond, 0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__speed,           0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__bar,             0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__beat,            0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__barBeat,         0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__beatUnit,        0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__beatsPerBar,     0, NULL, 0.0f, 0, NULL },
	{ LV2_TIME__beatsPerMinute,  0, NULL, 0.0f, 0, NULL }
};

static uint32_t g_lv2_time_refcount = 0;

#ifdef CONFIG_LV2_TIME_POSITION

// LV2 Time position atoms...
static LilvNode *g_lv2_time_position_class   = NULL;
static uint8_t  *g_lv2_time_position_buffer  = NULL;
static uint32_t  g_lv2_time_position_changed = 0;

static QList<qtractorLv2Plugin *> *g_lv2_time_position_plugins = NULL;

static void qtractor_lv2_time_position_open ( qtractorLv2Plugin *pLv2Plugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_time_position_open(%p)", pLv2Plugin);
#endif

	if (g_lv2_time_position_plugins == NULL)
		g_lv2_time_position_plugins = new QList<qtractorLv2Plugin *> ();

	g_lv2_time_position_plugins->append(pLv2Plugin);

	if (g_lv2_time_position_buffer == NULL)
		g_lv2_time_position_buffer = new uint8_t [256];

	++g_lv2_time_refcount;
}

static void qtractor_lv2_time_position_close ( qtractorLv2Plugin *pLv2Plugin )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractor_lv2_time_position_close(%p)", pLv2Plugin);
#endif

	--g_lv2_time_refcount;

	if (g_lv2_time_position_plugins) {
		g_lv2_time_position_plugins->removeAll(pLv2Plugin);
		if (g_lv2_time_position_plugins->isEmpty()) {
			delete g_lv2_time_position_plugins;
			g_lv2_time_position_plugins = NULL;
		}
	}

	if (g_lv2_time_position_plugins == NULL) {
		if (g_lv2_time_position_buffer) {
			delete [] g_lv2_time_position_buffer;
			g_lv2_time_position_buffer = NULL;
		}
	}
}

#endif	// CONFIG_LV2_TIME_POSITION

#endif	// CONFIG_LV2_TIME


#ifdef CONFIG_LV2_UI
#if QT_VERSION >= 0x050100

#ifdef CONFIG_LV2_UI_GTK2

#undef signals // Collides with GTK symbology

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

static void qtractor_lv2_ui_gtk2_on_size_request (
	GtkWidget */*widget*/, GtkRequisition *req, gpointer user_data )
{
	QWidget *pQtWidget = static_cast<QWidget *> (user_data);
	pQtWidget->setMinimumSize(req->width, req->height);
}

static void qtractor_lv2_ui_gtk2_on_size_allocate (
	GtkWidget */*widget*/, GdkRectangle *rect, gpointer user_data )
{
	QWidget *pQtWidget = static_cast<QWidget *> (user_data);
	pQtWidget->resize(rect->width, rect->height);
}

static bool g_lv2_ui_gtk2_init = false;

#endif	// CONFIG_LV2_UI_GTK2

#ifdef CONFIG_LV2_UI_X11

static int qtractor_lv2_ui_resize (
	LV2UI_Feature_Handle handle, int width, int height )
{
	QWidget *pQtWidget = static_cast<QWidget *> (handle);
	if (pQtWidget) {
		pQtWidget->resize(width, height);
		return 0;
	} else {
		return 1;
	}
}

#endif	// CONFIG_LV2_UI_X11

#endif
#endif	// CONFIG_LV2_UI


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
		const int iIndex = m_sName.lastIndexOf('/');
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
	m_iEventIns    = 0;
	m_iEventOuts   = 0;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iAtomIns     = 0;
	m_iAtomOuts    = 0;
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
					++m_iEventIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iEventOuts;
			}
		#endif
		#ifdef CONFIG_LV2_ATOM
			else
			if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_atom_class)) {
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_input_class))
					++m_iAtomIns;
				else
				if (lilv_port_is_a(m_lv2_plugin, port, g_lv2_output_class))
					++m_iAtomOuts;
			}
		#endif
		}
	}

#ifdef CONFIG_LV2_EVENT
	m_iMidiIns  += m_iEventIns;
	m_iMidiOuts += m_iEventOuts;
#endif
#ifdef CONFIG_LV2_ATOM
	m_iMidiIns  += m_iAtomIns;
	m_iMidiOuts += m_iAtomOuts;
#endif

	// Cache flags.
	m_bRealtime = lilv_plugin_has_feature(m_lv2_plugin, g_lv2_realtime_hint);

	m_bConfigure = false;
#ifdef CONFIG_LV2_STATE
	// Query for state interface extension data...
	LilvNodes *nodes = lilv_plugin_get_value(m_lv2_plugin,
		g_lv2_extension_data_hint);
	if (nodes) {
		LILV_FOREACH(nodes, iter, nodes) {
			const LilvNode *node = lilv_nodes_get(nodes, iter);
			if (lilv_node_equals(node, g_lv2_state_interface_hint)) {
				m_bConfigure = true;
				break;
			}
		}
		lilv_nodes_free(nodes);
	}
#endif
#ifdef CONFIG_LV2_UI
	// Check the UI inventory...
	LilvUIs *uis = lilv_plugin_get_uis(m_lv2_plugin);
	if (uis) {
		int uis_count = 0;
		LILV_FOREACH(uis, iter, uis) {
			LilvUI *ui = const_cast<LilvUI *> (lilv_uis_get(uis, iter));
		#ifdef CONFIG_LV2_EXTERNAL_UI
			if (lilv_ui_is_a(ui, g_lv2_external_ui_class)
			#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
				|| lilv_ui_is_a(ui, g_lv2_external_ui_deprecated_class)
			#endif
			)	++uis_count;
			else
		#endif
			if (lilv_ui_is_a(ui, g_lv2_x11_ui_class))
				++uis_count;
			else
			if (lilv_ui_is_a(ui, g_lv2_gtk_ui_class))
				++uis_count;
		#if QT_VERSION < 0x050000
			else
			if (lilv_ui_is_a(ui, g_lv2_qt4_ui_class))
				++uis_count;
		#else
			else
			if (lilv_ui_is_a(ui, g_lv2_qt5_ui_class))
				++uis_count;
		#endif
		#ifdef CONFIG_LV2_UI_SHOW
			else
			if (lv2_ui_show_interface(ui))
				++uis_count;
		#endif
		}
		m_bEditor = (uis_count > 0);
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
qtractorLv2PluginType *qtractorLv2PluginType::createType ( const QString& sUri )
{
	// Sanity check...
	if (sUri.isEmpty())
		return NULL;

	LilvPlugin *plugin = lv2_plugin(sUri);
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

	// HACK: set special environment for LV2...
	qtractorPluginFactory *pPluginFactory
		= qtractorPluginFactory::getInstance();
	if (pPluginFactory) {
		const char *LV2_PATH = "LV2_PATH";
		const QStringList& lv2_paths
			= pPluginFactory->pluginPaths(qtractorPluginType::Lv2);
		if (lv2_paths.isEmpty()) {
			::unsetenv(LV2_PATH);
		} else {
		#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
			const QString sPathSep(';');
		#else
			const QString sPathSep(':');
		#endif
			::setenv(LV2_PATH, lv2_paths.join(sPathSep).toUtf8().constData(), 1);
		}
	}

	// Taking on all the world...
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
	g_lv2_midi_class    = lilv_new_uri(g_lv2_world, LILV_URI_MIDI_EVENT);
#ifdef CONFIG_LV2_EVENT
	g_lv2_event_class   = lilv_new_uri(g_lv2_world, LILV_URI_EVENT_PORT);
#endif
#ifdef CONFIG_LV2_ATOM
	g_lv2_atom_class    = lilv_new_uri(g_lv2_world, LV2_ATOM__AtomPort);
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	g_lv2_external_ui_class
		= lilv_new_uri(g_lv2_world, LV2_EXTERNAL_UI__Widget);
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	g_lv2_external_ui_deprecated_class
		= lilv_new_uri(g_lv2_world, LV2_EXTERNAL_UI_DEPRECATED_URI);
#endif
#endif	// CONFIG_LV2_EXTERNAL_UI
	g_lv2_x11_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__X11UI);
	g_lv2_gtk_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__GtkUI);
	g_lv2_qt4_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__Qt4UI);
	g_lv2_qt5_ui_class = lilv_new_uri(g_lv2_world, LV2_UI__Qt5UI);
#endif	// CONFIG_LV2_UI

	// Set up the feature we may want to know (as hints).
	g_lv2_realtime_hint = lilv_new_uri(g_lv2_world,
		LV2_CORE__hardRTCapable);
	g_lv2_extension_data_hint = lilv_new_uri(g_lv2_world,
		LV2_CORE__extensionData);

#ifdef CONFIG_LV2_WORKER
	// LV2 Worker/Schedule support hints...
	g_lv2_worker_schedule_hint = lilv_new_uri(g_lv2_world,
		LV2_WORKER__schedule);
#endif

#ifdef CONFIG_LV2_STATE
	// LV2 State: set up supported interface and types...
	g_lv2_state_interface_hint = lilv_new_uri(g_lv2_world,
		LV2_STATE__interface);
	g_lv2_state_load_default_hint = lilv_new_uri(g_lv2_world,
		LV2_STATE__loadDefaultState);
#endif

	// Set up the port properties we support (as hints).
	g_lv2_toggled_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE__toggled);
	g_lv2_integer_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE__integer);
	g_lv2_sample_rate_prop = lilv_new_uri(g_lv2_world,
		LV2_CORE__sampleRate);
	g_lv2_logarithmic_prop = lilv_new_uri(g_lv2_world,
		LV2_PORT_PROPS__logarithmic);

#ifdef CONFIG_LV2_ATOM

	g_lv2_atom_forge = new LV2_Atom_Forge();
	lv2_atom_forge_init(g_lv2_atom_forge, &g_lv2_urid_map);

	g_lv2_maximum_prop = lilv_new_uri(g_lv2_world, LV2_CORE__maximum);
	g_lv2_minimum_prop = lilv_new_uri(g_lv2_world, LV2_CORE__minimum);
	g_lv2_default_prop = lilv_new_uri(g_lv2_world, LV2_CORE__default);

	g_lv2_minimum_size_prop = lilv_new_uri(g_lv2_world,
		LV2_RESIZE_PORT__minimumSize);

#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_PATCH
	g_lv2_patch_message_class = lilv_new_uri(g_lv2_world, LV2_PATCH__Message);
#endif

	// LV2 URIDs stock setup...
#ifdef CONFIG_LV2_ATOM
	g_lv2_urids.atom_eventTransfer
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__eventTransfer);
	g_lv2_urids.atom_Chunk
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Chunk);
	g_lv2_urids.atom_Sequence
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Sequence);
	g_lv2_urids.atom_Object
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Object);
	g_lv2_urids.atom_Blank
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Blank);
	g_lv2_urids.atom_Bool
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Bool);
	g_lv2_urids.atom_Int
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Int);
	g_lv2_urids.atom_Long
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Long);
	g_lv2_urids.atom_Float
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Float);
	g_lv2_urids.atom_Double
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Double);
	g_lv2_urids.atom_String
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__String);
	g_lv2_urids.atom_Path
		= qtractorLv2Plugin::lv2_urid_map(LV2_ATOM__Path);
#endif
#ifdef CONFIG_LV2_PATCH
	g_lv2_urids.patch_Get
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__Get);
	g_lv2_urids.patch_Put
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__Put);
	g_lv2_urids.patch_Set
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__Set);
	g_lv2_urids.patch_body
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__body);
	g_lv2_urids.patch_property
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__property);
	g_lv2_urids.patch_value
		= qtractorLv2Plugin::lv2_urid_map(LV2_PATCH__value);
#endif
#ifdef CONFIG_LV2_TIME
#ifdef CONFIG_LV2_TIME_POSITION
	g_lv2_urids.time_Position
		= qtractorLv2Plugin::lv2_urid_map(LV2_TIME__Position);
#endif
#endif	// CONFIG_LV2_TIME
#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE
	g_lv2_urids.bufsz_minBlockLength
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__minBlockLength);
	g_lv2_urids.bufsz_maxBlockLength
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__maxBlockLength);
	g_lv2_urids.bufsz_nominalBlockLength
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__nominalBlockLength);
	g_lv2_urids.bufsz_sequenceSize
		= qtractorLv2Plugin::lv2_urid_map(LV2_BUF_SIZE__sequenceSize);
#endif
#ifdef CONFIG_LV2_UI
	g_lv2_urids.ui_windowTitle
		= qtractorLv2Plugin::lv2_urid_map(LV2_UI__windowTitle);
	g_lv2_urids.ui_updateRate
		= qtractorLv2Plugin::lv2_urid_map(LV2_UI__updateRate);
	g_lv2_urids.ui_sampleRate
		= qtractorLv2Plugin::lv2_urid_map(LV2_CORE__sampleRate);
#endif
#ifdef CONFIG_LV2_STATE
	g_lv2_urids.state_StateChanged
		= qtractorLv2Plugin::lv2_urid_map(LV2_STATE__StateChanged);
#endif
#endif	// CONFIG_LV2_OPTIONS

#ifdef CONFIG_LV2_TIME
	// LV2 Time: set up supported port designations...
	for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
		qtractorLv2Time& member = g_lv2_time[i];
		member.node = lilv_new_uri(g_lv2_world, member.uri);
		member.urid = qtractorLv2Plugin::lv2_urid_map(member.uri);
		member.value = 0.0f;
		member.changed = 0;
		member.params = new QList<qtractorLv2PluginParam *> ();
	}
#ifdef CONFIG_LV2_TIME_POSITION
	// LV2 Time: set up for atom port event notifications...
	g_lv2_time_position_class
		= lilv_new_uri(g_lv2_world, LV2_TIME__Position);
#endif
#endif	// CONFIG_LV2_TIME
}


void qtractorLv2PluginType::lv2_close (void)
{
	if (g_lv2_plugins == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2PluginType::lv2_close()");
#endif

	// LV2 URIDs stock reset.
	::memset(&g_lv2_urids, 0, sizeof(g_lv2_urids));

#ifdef CONFIG_LV2_TIME
	for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
		qtractorLv2Time& member = g_lv2_time[i];
		lilv_node_free(member.node);
		member.node = NULL;
		member.urid = 0;
		member.value = 0.0f;
		member.changed = 0;
		delete member.params;
		member.params = NULL;
	}
#ifdef CONFIG_LV2_TIME_POSITION
	lilv_node_free(g_lv2_time_position_class);
#endif
#endif	// CONFIG_LV2_TIME

	// Clean up.
	lilv_node_free(g_lv2_toggled_prop);
	lilv_node_free(g_lv2_integer_prop);
	lilv_node_free(g_lv2_sample_rate_prop);
	lilv_node_free(g_lv2_logarithmic_prop);

#ifdef CONFIG_LV2_ATOM

	if (g_lv2_atom_forge) {
		delete g_lv2_atom_forge;
		g_lv2_atom_forge = NULL;
	}

	lilv_node_free(g_lv2_maximum_prop);
	lilv_node_free(g_lv2_minimum_prop);
	lilv_node_free(g_lv2_default_prop);

	lilv_node_free(g_lv2_minimum_size_prop);

#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_PATCH
	lilv_node_free(g_lv2_patch_message_class);
#endif

#ifdef CONFIG_LV2_STATE
	lilv_node_free(g_lv2_state_interface_hint);
	lilv_node_free(g_lv2_state_load_default_hint);
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
	lilv_node_free(g_lv2_midi_class);
#ifdef CONFIG_LV2_EVENT
	lilv_node_free(g_lv2_event_class);
#endif
#ifdef CONFIG_LV2_ATOM
	lilv_node_free(g_lv2_atom_class);
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	lilv_node_free(g_lv2_external_ui_class);
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	lilv_node_free(g_lv2_external_ui_deprecated_class);
#endif
#endif	// CONFIG_LV2_EXTERNAL_UI
	lilv_node_free(g_lv2_x11_ui_class);
	lilv_node_free(g_lv2_gtk_ui_class);
	lilv_node_free(g_lv2_qt4_ui_class);
	lilv_node_free(g_lv2_qt5_ui_class);
#endif	// CONFIG_LV2_UI

	lilv_world_free(g_lv2_world);

#ifdef CONFIG_LV2_TIME
#ifdef CONFIG_LV2_TIME_POSITION
	g_lv2_time_position_class = NULL;
#endif
#endif

	g_lv2_toggled_prop     = NULL;
	g_lv2_integer_prop     = NULL;
	g_lv2_sample_rate_prop = NULL;
	g_lv2_logarithmic_prop = NULL;

#ifdef CONFIG_LV2_ATOM

	g_lv2_maximum_prop = NULL;
	g_lv2_minimum_prop = NULL;
	g_lv2_default_prop = NULL;

	g_lv2_minimum_size_prop = NULL;

#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_PATCH
	g_lv2_patch_message_class = NULL;
#endif

#ifdef CONFIG_LV2_STATE
	g_lv2_state_interface_hint    = NULL;
	g_lv2_state_load_default_hint = NULL;
#endif

#ifdef CONFIG_LV2_WORKER
	g_lv2_worker_schedule_hint = NULL;
#endif

	g_lv2_extension_data_hint = NULL;
	g_lv2_realtime_hint = NULL;

	g_lv2_input_class   = NULL;
	g_lv2_output_class  = NULL;
	g_lv2_control_class = NULL;
	g_lv2_audio_class   = NULL;
	g_lv2_midi_class    = NULL;
#ifdef CONFIG_LV2_EVENT
	g_lv2_event_class   = NULL;
#endif
#ifdef CONFIG_LV2_ATOM
	g_lv2_atom_class    = NULL;
#endif

#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_EXTERNAL_UI
	g_lv2_external_ui_class = NULL;
#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
	g_lv2_external_ui_deprecated_class = NULL;
#endif
#endif	// CONFIG_LV2_EXTERNAL_UI
	g_lv2_x11_ui_class = NULL;
	g_lv2_gtk_ui_class = NULL;
	g_lv2_qt4_ui_class = NULL;
	g_lv2_qt5_ui_class = NULL;
#endif	// CONFIG_LV2_UI

	g_lv2_plugins = NULL;
	g_lv2_world   = NULL;
}


// Plugin URI listing (static).
QStringList qtractorLv2PluginType::lv2_plugins (void)
{
	QStringList list;

	if (g_lv2_plugins) {
		LILV_FOREACH(plugins, iter, g_lv2_plugins) {
			const LilvPlugin *plugin = lilv_plugins_get(g_lv2_plugins, iter);
			const char *pszUri = lilv_node_as_uri(lilv_plugin_get_uri(plugin));
			list.append(QString::fromLocal8Bit(pszUri));
		}
	}

	return list;
}


#ifdef CONFIG_LV2_UI_SHOW

// Check for LV2 UI Show interface.
bool qtractorLv2PluginType::lv2_ui_show_interface ( LilvUI *ui ) const
{
	if (m_lv2_plugin == NULL)
		return false;

	const LilvNode *ui_uri = lilv_ui_get_uri(ui);
	lilv_world_load_resource(g_lv2_world, ui_uri);
	LilvNode *extension_data_uri
		= lilv_new_uri(g_lv2_world, LV2_CORE__extensionData);
	LilvNode *show_interface_uri
		= lilv_new_uri(g_lv2_world, LV2_UI__showInterface);
	const bool ui_show_interface
		= lilv_world_ask(g_lv2_world, ui_uri,
			extension_data_uri, show_interface_uri);
	lilv_node_free(extension_data_uri);
	lilv_node_free(show_interface_uri);
#ifdef CONFIG_LILV_WORLD_UNLOAD_RESOURCE
	lilv_world_unload_resource(g_lv2_world, ui_uri);
#endif

	return ui_show_interface;
}

#endif	// CONFIG_LV2_UI_SHOW


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

// Dynamic singleton list of LV2 plugins.
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
		, m_pfIDummy(NULL)
		, m_pfODummy(NULL)
	#ifdef CONFIG_LV2_EVENT
		, m_piEventIns(NULL)
		, m_piEventOuts(NULL)
	#endif
	#ifdef CONFIG_LV2_ATOM
		, m_piAtomIns(NULL)
		, m_piAtomOuts(NULL)
		, m_lv2_atom_buffer_ins(NULL)
		, m_lv2_atom_buffer_outs(NULL)
		, m_lv2_atom_midi_port_in(0)
		, m_lv2_atom_midi_port_out(0)
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
		, m_lv2_ui_library(NULL)
		, m_lv2_ui_descriptor(NULL)
		, m_lv2_ui_handle(NULL)
		, m_lv2_ui_widget(NULL)
	#ifdef CONFIG_LIBSUIL
		, m_suil_host(NULL)
		, m_suil_instance(NULL)
	#endif
	#ifdef CONFIG_LV2_ATOM
		, m_ui_events(NULL)
		, m_plugin_events(NULL)
	#endif
		, m_pQtFilter(NULL)
		, m_pQtWidget(NULL)
		, m_bQtDelete(false)
	#ifdef CONFIG_LV2_UI_IDLE
		, m_lv2_ui_idle_interface(NULL)
	#endif
	#ifdef CONFIG_LV2_UI_SHOW
		, m_lv2_ui_show_interface(NULL)
	#endif
	#if QT_VERSION >= 0x050100
	#ifdef CONFIG_LV2_UI_GTK2
		, m_pGtkWindow(NULL)
		, m_pQtWindow(NULL)
	#endif	// CONFIG_LV2_UI_GTK2
	#endif
	#endif	// CONFIG_LV2_UI
	#ifdef CONFIG_LV2_TIME
	#ifdef CONFIG_LV2_TIME_POSITION
		, m_lv2_time_position_enabled(false)
		, m_lv2_time_position_port_in(0)
		, m_lv2_time_position_changed(0)
	#endif
	#endif	// CONFIG_LV2_TIME
	#ifdef CONFIG_LV2_PATCH
		, m_lv2_patch_port_in(0)
		, m_lv2_patch_changed(0)
	#endif	// CONFIG_LV2_PATCH
	#ifdef CONFIG_LV2_OPTIONS
	#ifdef CONFIG_LV2_BUF_SIZE
		, m_iMinBlockLength(0)
		, m_iMaxBlockLength(0)
		, m_iNominalBlockLength(0)
		, m_iSequenceSize(0)
	#endif
	#endif	// CONFIG_LV2_OPTIONS
		, m_pfLatency(NULL)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p] uri=\"%s\"",
		this, pLv2Type->filename().toUtf8().constData());
#endif

	int iFeatures = 0;
	while (g_lv2_features[iFeatures]) { ++iFeatures; }

	m_lv2_features = new LV2_Feature * [iFeatures + 6];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_features[i] = (LV2_Feature *) g_lv2_features[i];

#ifdef CONFIG_LV2_STATE

	m_lv2_state_load_default_feature.URI = LV2_STATE__loadDefaultState;
	m_lv2_state_load_default_feature.data = NULL;
	m_lv2_features[iFeatures++] = &m_lv2_state_load_default_feature;

#endif	// CONFIG_LV2_STATE

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

#if defined(CONFIG_LV2_EVENT) || defined(CONFIG_LV2_ATOM)
	qtractorMidiManager *pMidiManager = list()->midiManager();
#endif

#ifdef CONFIG_LV2_OPTIONS
#ifdef CONFIG_LV2_BUF_SIZE

	m_iMinBlockLength     = 0;
	m_iMaxBlockLength     = 0;
	m_iNominalBlockLength = 0;
	m_iSequenceSize       = 0;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine) {
			m_iMinBlockLength     = pAudioEngine->bufferSize();
			m_iMaxBlockLength     = m_iMinBlockLength;
			m_iNominalBlockLength = m_iMaxBlockLength;
		}
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
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.bufsz_minBlockLength,
		  sizeof(int32_t), g_lv2_urids.atom_Int, &m_iMinBlockLength },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.bufsz_maxBlockLength,
		  sizeof(int32_t), g_lv2_urids.atom_Int, &m_iMaxBlockLength },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.bufsz_nominalBlockLength,
		  sizeof(int32_t), g_lv2_urids.atom_Int, &m_iNominalBlockLength },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.bufsz_sequenceSize,
		  sizeof(int32_t), g_lv2_urids.atom_Int, &m_iSequenceSize },
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
		unsigned short iEventIns  = pLv2Type->eventIns();
		unsigned short iEventOuts = pLv2Type->eventOuts();
		if (iEventIns > 0)
			m_piEventIns = new unsigned long [iEventIns];
		if (iEventOuts > 0)
			m_piEventOuts = new unsigned long [iEventOuts];
		iEventIns = iEventOuts = 0;
	#endif	// CONFIG_LV2_EVENT
	#ifdef CONFIG_LV2_ATOM
		unsigned short iAtomIns  = pLv2Type->atomIns();
		unsigned short iAtomOuts = pLv2Type->atomOuts();
		if (iAtomIns > 0)
			m_piAtomIns = new unsigned long [iAtomIns];
		if (iAtomOuts > 0)
			m_piAtomOuts = new unsigned long [iAtomOuts];
		iAtomIns = iAtomOuts = 0;
	#endif	// CONFIG_LV2_ATOM
		const bool bLatency = lilv_plugin_has_latency(plugin);
		const uint32_t iLatencyPort
			= (bLatency ? lilv_plugin_get_latency_port_index(plugin) : 0);
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
						m_piEventIns[iEventIns++] = i;
					else
				#endif
				#ifdef CONFIG_LV2_ATOM
					if (lilv_port_is_a(plugin, port, g_lv2_atom_class))
						m_piAtomIns[iAtomIns++] = i;
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
						m_piEventOuts[iEventOuts++] = i;
					else
				#endif
				#ifdef CONFIG_LV2_ATOM
					if (lilv_port_is_a(plugin, port, g_lv2_atom_class))
						m_piAtomOuts[iAtomOuts++] = i;
					else
				#endif
					if (lilv_port_is_a(plugin, port, g_lv2_control_class)) {
						m_piControlOuts[iControlOuts] = i;
						m_pfControlOuts[iControlOuts] = 0.0f;
						m_pfControlOutsLast[iControlOuts] = 0.0f;
						if (bLatency && iLatencyPort == i)
							m_pfLatency = &m_pfControlOuts[iControlOuts];
						++iControlOuts;
					}
				}
			}
		}
	#ifdef CONFIG_LV2_ATOM
		unsigned int iAtomInsCapacity = 0;
		if (iAtomIns > 0) {
			unsigned short iMidiAtomIns = 0;
			m_lv2_atom_buffer_ins = new LV2_Atom_Buffer * [iAtomIns];
			for (unsigned short j = 0; j < iAtomIns; ++j) {
				unsigned int iMinBufferCapacity = 1024;
				const LilvPort *port
					= lilv_plugin_get_port_by_index(plugin, m_piAtomIns[j]);
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
					if (pMidiManager &&
						lilv_port_supports_event(plugin,
							port, g_lv2_midi_class)) {
						if (++iMidiAtomIns == 1)
							m_lv2_atom_midi_port_in = j; // First wins input.
						pMidiManager->lv2_atom_buffer_resize(iMinBufferCapacity);
					}
				#ifdef CONFIG_LV2_TIME_POSITION
					if (lilv_port_supports_event(plugin,
							port, g_lv2_time_position_class)) {
						m_lv2_time_position_enabled = true;
						m_lv2_time_position_port_in = j;
						qtractor_lv2_time_position_open(this);
					}
				#endif
				#ifdef CONFIG_LV2_PATCH
					if (lilv_port_supports_event(plugin,
							port, g_lv2_patch_message_class)) {
						m_lv2_patch_port_in = j;
					}
				#endif
				}
				m_lv2_atom_buffer_ins[j]
					= lv2_atom_buffer_new(iMinBufferCapacity,
						g_lv2_urids.atom_Chunk,
						g_lv2_urids.atom_Sequence, true);
				iAtomInsCapacity += iMinBufferCapacity;
			}
		}
		unsigned int iAtomOutsCapacity = 0;
		if (iAtomOuts > 0) {
			unsigned short iMidiAtomOuts = 0;
			m_lv2_atom_buffer_outs = new LV2_Atom_Buffer * [iAtomOuts];
			for (unsigned short j = 0; j < iAtomOuts; ++j) {
				unsigned int iMinBufferCapacity = 1024;
				const LilvPort *port
					= lilv_plugin_get_port_by_index(plugin, m_piAtomOuts[j]);
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
					if (pMidiManager &&
						lilv_port_supports_event(plugin,
							port, g_lv2_midi_class)) {
						if (++iMidiAtomOuts == 1)
							m_lv2_atom_midi_port_out = j; // First wins output.
						pMidiManager->lv2_atom_buffer_resize(iMinBufferCapacity);
					}
				}
				m_lv2_atom_buffer_outs[j]
					= lv2_atom_buffer_new(iMinBufferCapacity,
						g_lv2_urids.atom_Chunk,
						g_lv2_urids.atom_Sequence, false);
				iAtomOutsCapacity += iMinBufferCapacity;
			}
		}
	#ifdef CONFIG_LV2_UI
		m_ui_events = ::jack_ringbuffer_create(iAtomInsCapacity);
		m_plugin_events = ::jack_ringbuffer_create(iAtomOutsCapacity);
	#endif
	#endif	// CONFIG_LV2_ATOM
	#ifdef CONFIG_LV2_PRESETS
		LilvNode *label_uri = lilv_new_uri(g_lv2_world, LILV_NS_RDFS "label");
		LilvNode *preset_uri = lilv_new_uri(g_lv2_world, LV2_PRESETS__Preset);
		LilvNodes *presets = lilv_plugin_get_related(lv2_plugin(), preset_uri);
		if (presets) {
			LILV_FOREACH(nodes, iter, presets) {
				const LilvNode *preset = lilv_nodes_get(presets, iter);
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
				const LilvPort *port
					= lilv_plugin_get_port_by_designation(
						plugin, g_lv2_input_class, member.node);
				if (port) {
					const unsigned long iIndex
						= lilv_port_get_index(plugin, port);
					qtractorLv2PluginParam *pParam
						= static_cast<qtractorLv2PluginParam *> (
							findParam(iIndex));
					if (pParam) {
						m_lv2_time_ports.insert(iIndex, i);
						member.params->append(pParam);
					}
				}
			}
		}
		// Add to global running LV2 Time/position ref-count...
		if (!m_lv2_time_ports.isEmpty())
			++g_lv2_time_refcount;
	#endif	// CONFIG_LV2_TIME
	#ifdef CONFIG_LV2_PATCH
		lv2_patch_properties(LV2_PATCH__writable);
		lv2_patch_properties(LV2_PATCH__readable);
	#endif
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
	// Remove from global running LV2 Time/position ref-count...
	if (!m_lv2_time_ports.isEmpty()) {
		--g_lv2_time_refcount;
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
	}
#ifdef CONFIG_LV2_TIME_POSITION
	if (m_lv2_time_position_enabled)
		qtractor_lv2_time_position_close(this);
#endif
#endif	// CONFIG_LV2_TIME

	// Free up all the rest...
#ifdef CONFIG_LV2_PATCH
	qDeleteAll(m_lv2_properties);
	m_lv2_properties.clear();
#endif

#ifdef CONFIG_LV2_ATOM
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	const unsigned short iAtomOuts
		= (pLv2Type ? pLv2Type->atomOuts() : 0);
	if (m_lv2_atom_buffer_outs) {
		for (unsigned short j = 0; j < iAtomOuts; ++j)
			lv2_atom_buffer_free(m_lv2_atom_buffer_outs[j]);
		delete [] m_lv2_atom_buffer_outs;
	}
	const unsigned short iAtomIns
		= (pLv2Type ? pLv2Type->atomIns() : 0);
	if (m_lv2_atom_buffer_ins) {
		for (unsigned short j = 0; j < iAtomIns; ++j)
			lv2_atom_buffer_free(m_lv2_atom_buffer_ins[j]);
		delete [] m_lv2_atom_buffer_ins;
	}
	if (m_piAtomOuts)
		delete [] m_piAtomOuts;
	if (m_piAtomIns)
		delete [] m_piAtomIns;
#endif	// CONFIG_LV2_ATOM

#ifdef CONFIG_LV2_EVENT
	if (m_piEventOuts)
		delete [] m_piEventOuts;
	if (m_piEventIns)
		delete [] m_piEventIns;
#endif	// CONFIG_LV2_EVENT

#ifdef CONFIG_LV2_UI
	if (m_plugin_events)
		::jack_ringbuffer_free(m_plugin_events);
	if (m_ui_events)
		::jack_ringbuffer_free(m_ui_events);
#endif

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

	if (m_pfIDummy)
		delete [] m_pfIDummy;
	if (m_pfODummy)
		delete [] m_pfODummy;

	if (m_lv2_features)
		delete [] m_lv2_features;
}


// Channel/instance number accessors.
void qtractorLv2Plugin::setChannels ( unsigned short iChannels )
{
	// Check our type...
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return;

	// Estimate the (new) number of instances...
	const unsigned short iOldInstances = instances();
	const unsigned short iInstances
		= pLv2Type->instances(iChannels, list()->isMidi());

	// Now see if instance count changed anyhow...
	if (iInstances == iOldInstances)
		return;

	const LilvPlugin *plugin = pLv2Type->lv2_plugin();
	if (plugin == NULL)
		return;

	// Gotta go for a while...
	const bool bActivated = isActivated();
	setActivated(false);

	// Set new instance number...
	setInstances(iInstances);

	// Close old instances, all the way...
	const int iLv2Plugin = g_lv2Plugins.indexOf(this);
	if (iLv2Plugin >= 0)
		g_lv2Plugins.removeAt(iLv2Plugin);

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

	// Allocate the dummy audio I/O buffers...
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == NULL)
		return;

	qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
	if (pAudioEngine == NULL)
		return;

	const unsigned int iSampleRate = pAudioEngine->sampleRate();
	const unsigned int iBufferSize = pAudioEngine->bufferSize();

	const unsigned short iAudioIns = pLv2Type->audioIns();
	const unsigned short iAudioOuts = pLv2Type->audioOuts();

	if (iChannels < iAudioIns) {
		if (m_pfIDummy)
			delete [] m_pfIDummy;
		m_pfIDummy = new float [iBufferSize];
		::memset(m_pfIDummy, 0, iBufferSize * sizeof(float));
	}

	if (iChannels < iAudioOuts) {
		if (m_pfODummy)
			delete [] m_pfODummy;
		m_pfODummy = new float [iBufferSize];
	//	::memset(m_pfODummy, 0, iBufferSize * sizeof(float));
	}

#ifdef CONFIG_LV2_WORKER
	if (lilv_plugin_has_feature(plugin, g_lv2_worker_schedule_hint))
		m_lv2_worker = new qtractorLv2Worker(this, m_lv2_features);
#endif

	LV2_Feature **features = m_lv2_features;
#ifdef CONFIG_LV2_WORKER
	if (m_lv2_worker)
		features = m_lv2_worker->lv2_features();
#endif

	// We'll need output control (not dummy anymore) port indexes...
	const unsigned short iControlOuts = pLv2Type->controlOuts();

	unsigned short i, j;

	// Allocate new instances...
	m_ppInstances = new LilvInstance * [iInstances];
	for (i = 0; i < iInstances; ++i) {
		// Instantiate them properly first...
		LilvInstance *instance
			= lilv_plugin_instantiate(plugin, iSampleRate, features);
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
			for (j = 0; j < iControlOuts; ++j) {
				lilv_instance_connect_port(instance,
					m_piControlOuts[j], &m_pfControlOuts[j]);
			}
			// Connect all dummy input ports...
			if (m_pfIDummy) for (j = iChannels; j < iAudioIns; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioIns[j], m_pfIDummy); // dummy input port!
			}
			// Connect all dummy output ports...
			if (m_pfODummy) for (j = iChannels; j < iAudioOuts; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioOuts[j], m_pfODummy); // dummy input port!
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

	// Finally add it to the LV2 plugin roster...
	g_lv2Plugins.append(this);

#ifdef CONFIG_LV2_STATE
	// Load default state as needed...
	if (lilv_plugin_has_feature(plugin, g_lv2_state_load_default_hint))
		lv2_state_load_default();
#endif

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
		const unsigned short iInstances = instances();
		for (unsigned short i = 0; i < iInstances; ++i) {
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
		const unsigned short iInstances = instances();
		for (unsigned short i = 0; i < iInstances; ++i) {
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
	const unsigned short iEventIns  = pLv2Type->eventIns();
	const unsigned short iEventOuts = pLv2Type->eventOuts();
#endif
#ifdef CONFIG_LV2_ATOM
	const unsigned short iAtomIns   = pLv2Type->atomIns();
	const unsigned short iAtomOuts  = pLv2Type->atomOuts();
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
			for (j = 0; j < iAudioIns && iIChannel < iChannels; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioIns[j], ppIBuffer[iIChannel++]);
			}
			// For each instance audio output port...
			for (j = 0; j < iAudioOuts && iOChannel < iChannels; ++j) {
				lilv_instance_connect_port(instance,
					m_piAudioOuts[j], ppOBuffer[iOChannel++]);
			}
		#ifdef CONFIG_LV2_EVENT
			// Connect all existing input event/MIDI ports...
			if (pMidiManager) {
				for (j = 0; j < iEventIns; ++j) {
					lilv_instance_connect_port(instance,
						m_piEventIns[j], pMidiManager->lv2_events_in());
				}
				for (j = 0; j < iEventOuts; ++j) {
					lilv_instance_connect_port(instance,
						m_piEventOuts[j], pMidiManager->lv2_events_out());
				}
			}
		#endif
		#ifdef CONFIG_LV2_ATOM
			// Connect all existing input atom/MIDI ports...
			for (j = 0; j < iAtomIns; ++j) {
				LV2_Atom_Buffer *abuf = m_lv2_atom_buffer_ins[j];
				if (pMidiManager && j == m_lv2_atom_midi_port_in)
					abuf = pMidiManager->lv2_atom_buffer_in();
				else
					lv2_atom_buffer_reset(abuf, true);
				lilv_instance_connect_port(instance,
					m_piAtomIns[j], &abuf->atoms);
			#ifdef CONFIG_LV2_TIME_POSITION
				// Time position has changed, provide an update...
				if (m_lv2_time_position_changed > 0 &&
					j == m_lv2_time_position_port_in) {
					LV2_Atom_Buffer_Iterator aiter;
					lv2_atom_buffer_end(&aiter, abuf);
					const LV2_Atom *atom
						= (const LV2_Atom *) g_lv2_time_position_buffer;
					lv2_atom_buffer_write(&aiter, 0, 0,
						atom->type, atom->size,
						(const uint8_t *) LV2_ATOM_BODY(atom));
					m_lv2_time_position_changed = 0;
				}
			#endif
			#ifdef CONFIG_LV2_PATCH
				// Plugin state has changed, request an update...
				if (m_lv2_patch_changed > 0 &&
					j == m_lv2_patch_port_in) {
					LV2_Atom_Buffer_Iterator aiter;
					lv2_atom_buffer_end(&aiter, abuf);
					LV2_Atom_Object obj;
					obj.atom.size  = sizeof(LV2_Atom_Object_Body);
					obj.atom.type  = g_lv2_urids.atom_Object;
					obj.body.id    = 0;
					obj.body.otype = g_lv2_urids.patch_Get;
					lv2_atom_buffer_write(&aiter, 0, 0,
						obj.atom.type, obj.atom.size,
						(const uint8_t *) LV2_ATOM_BODY(&obj));
					m_lv2_patch_changed = 0;
				}
			#endif
			}
			for (j = 0; j < iAtomOuts; ++j) {
				LV2_Atom_Buffer *abuf = m_lv2_atom_buffer_outs[j];
				if (pMidiManager && j == m_lv2_atom_midi_port_out)
					abuf = pMidiManager->lv2_atom_buffer_out();
				else
					lv2_atom_buffer_reset(abuf, false);
				lilv_instance_connect_port(instance,
					m_piAtomOuts[j], &abuf->atoms);
			}
		#ifdef CONFIG_LV2_UI
			// Read and apply control changes, eventually from an UI...
			uint32_t read_space = ::jack_ringbuffer_read_space(m_ui_events);
			while (read_space > 0) {
				ControlEvent ev;
				::jack_ringbuffer_read(m_ui_events, (char *) &ev, sizeof(ev));
				char ebuf[ev.size];
				if (::jack_ringbuffer_read(m_ui_events, ebuf, ev.size) < ev.size)
					break;
				if (ev.protocol == g_lv2_urids.atom_eventTransfer) {
					for (j = 0; j < iAtomIns; ++j) {
						if (m_piAtomIns[j] == ev.index) {
							LV2_Atom_Buffer *abuf = m_lv2_atom_buffer_ins[j];
							if (pMidiManager && j == m_lv2_atom_midi_port_in)
								abuf = pMidiManager->lv2_atom_buffer_in();
							LV2_Atom_Buffer_Iterator aiter;
							lv2_atom_buffer_end(&aiter, abuf);
							const LV2_Atom *atom = (const LV2_Atom *) ebuf;
							lv2_atom_buffer_write(&aiter, nframes, 0,
								atom->type, atom->size,
								(const uint8_t *) LV2_ATOM_BODY(atom));
							break;
						}
					}
				}
				read_space -= sizeof(ev) + ev.size;
			}
		#endif	// CONFIG_LV2_UI
		#endif	// CONFIG_LV2_ATOM
			// Make it run...
			lilv_instance_run(instance, nframes);
			// Wrap dangling output channels?...
			for (j = iOChannel; j < iChannels; ++j)
				::memset(ppOBuffer[j], 0, nframes * sizeof(float));
		}
	}

#ifdef CONFIG_LV2_WORKER
	if (m_lv2_worker)
		m_lv2_worker->commit();
#endif

#ifdef CONFIG_LV2_ATOM
#ifdef CONFIG_LV2_UI
	for (j = 0; j < iAtomOuts; ++j) {
		LV2_Atom_Buffer *abuf;
		if (pMidiManager && j == m_lv2_atom_midi_port_out)
			abuf = pMidiManager->lv2_atom_buffer_out();
		else
			abuf = m_lv2_atom_buffer_outs[j];
		LV2_Atom_Buffer_Iterator aiter;
		lv2_atom_buffer_begin(&aiter, abuf);
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
				ev->index    = m_piAtomOuts[j];
				ev->protocol = g_lv2_urids.atom_eventTransfer;
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
	if (pMidiManager && iEventOuts > 0)
		pMidiManager->lv2_events_swap();
#endif
#ifdef CONFIG_LV2_ATOM
	if (pMidiManager && iAtomOuts > 0)
		pMidiManager->lv2_atom_buffer_swap();
#endif
}


#ifdef CONFIG_LV2_UI

// Open editor.
void qtractorLv2Plugin::openEditor ( QWidget */*pParent*/ )
{
	if (m_lv2_ui) {
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

	LILV_FOREACH(uis, iter, m_lv2_uis) {
		LilvUI *ui = const_cast<LilvUI *> (lilv_uis_get(m_lv2_uis, iter));
	#ifdef CONFIG_LV2_EXTERNAL_UI
		if (lilv_ui_is_a(ui, g_lv2_external_ui_class)
		#ifdef LV2_EXTERNAL_UI_DEPRECATED_URI
			|| lilv_ui_is_a(ui, g_lv2_external_ui_deprecated_class)
		#endif
		)	ui_map.insert(LV2_UI_TYPE_EXTERNAL, ui);
		else
	#endif
		if (lilv_ui_is_a(ui, g_lv2_x11_ui_class))
			ui_map.insert(LV2_UI_TYPE_X11, ui);
		else
		if (lilv_ui_is_a(ui, g_lv2_gtk_ui_class))
			ui_map.insert(LV2_UI_TYPE_GTK, ui);
	#if QT_VERSION < 0x050000
		else
		if (lilv_ui_is_a(ui, g_lv2_qt4_ui_class))
			ui_map.insert(LV2_UI_TYPE_QT4, ui);
	#else
		else
		if (lilv_ui_is_a(ui, g_lv2_qt5_ui_class))
			ui_map.insert(LV2_UI_TYPE_QT5, ui);
	#endif
	#ifdef CONFIG_LV2_UI_SHOW
		else
		if (pLv2Type->lv2_ui_show_interface(ui))
			ui_map.insert(LV2_UI_TYPE_OTHER, ui);
	#endif
	}

	const QMap<int, LilvUI *>::ConstIterator& ui_begin = ui_map.constBegin();
	const QMap<int, LilvUI *>::ConstIterator& ui_end = ui_map.constEnd();
	QMap<int, LilvUI *>::ConstIterator ui_iter = ui_begin;

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bQueryEditorType) {
		const int iEditorType = editorType();
		if (iEditorType > 0) // Must be != LV2_UI_TYPE_NONE.
			ui_iter = ui_map.constFind(iEditorType);
		else
		if (ui_map.count() > 1) {
			const QString& sTitle
				= type()->name() + " - " QTRACTOR_TITLE;
			const QString& sText
				= QObject::tr("Select plug-in's editor (GUI):");
			qtractorMessageBox mbox(qtractorMainForm::getInstance());
			mbox.setIcon(QMessageBox::Question);
			mbox.setWindowTitle(sTitle);
			mbox.setText(sText);
			mbox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
			QButtonGroup group;
			for ( ; ui_iter != ui_end; ++ui_iter) {
				const int ui_type = ui_iter.key();
				QRadioButton *pRadioButton;
				switch (ui_type) {
				case LV2_UI_TYPE_EXTERNAL:
					pRadioButton = new QRadioButton(QObject::tr("External"));
					break;
				case LV2_UI_TYPE_X11:
					pRadioButton = new QRadioButton(QObject::tr("X11"));
					break;
				case LV2_UI_TYPE_GTK:
					pRadioButton = new QRadioButton(QObject::tr("Gtk2"));
					break;
			#if QT_VERSION < 0x050000
				case LV2_UI_TYPE_QT4:
					pRadioButton = new QRadioButton(QObject::tr("Qt4"));
					break;
			#else
				case LV2_UI_TYPE_QT5:
					pRadioButton = new QRadioButton(QObject::tr("Qt5"));
					break;
			#endif
				case LV2_UI_TYPE_OTHER:
				default:
					pRadioButton = new QRadioButton(QObject::tr("Other"));
					break;
				}
				pRadioButton->setChecked(ui_iter == ui_begin);
				group.addButton(pRadioButton, ui_type);
				mbox.addCustomButton(pRadioButton);
			}
			mbox.addCustomSpacer();
			QCheckBox cbox(QObject::tr("Don't ask this again"));
			cbox.setChecked(false);
			cbox.blockSignals(true);
			mbox.addButton(&cbox, QMessageBox::ActionRole);
			if (mbox.exec() == QMessageBox::Ok)
				ui_iter = ui_map.constFind(group.checkedId());
			else
				ui_iter = ui_end;
			if (ui_iter != ui_end && cbox.isChecked())
				setEditorType(ui_iter.key());
		}
	}

	if (ui_iter == ui_end) {
		lilv_uis_free(m_lv2_uis);
		m_lv2_uis = NULL;
		return;
	}

	m_lv2_ui_type = ui_iter.key();
	m_lv2_ui = ui_iter.value();

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
	case LV2_UI_TYPE_X11:
		ui_type_uri = LV2_UI__X11UI;
		break;
	case LV2_UI_TYPE_GTK:
		ui_type_uri = LV2_UI__GtkUI;
		break;
#if QT_VERSION < 0x050000
	case LV2_UI_TYPE_QT4:
		ui_type_uri = LV2_UI__Qt4UI;
		break;
#else
	case LV2_UI_TYPE_QT5:
		ui_type_uri = LV2_UI__Qt5UI;
		break;
#endif
	default:
		break;
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::openEditor(\"%s\")", this, ui_type_uri);
#endif

	const char *ui_host_uri = LV2_UI_HOST_URI;
	const char *plugin_uri
		= lilv_node_as_uri(lilv_plugin_get_uri(pLv2Type->lv2_plugin()));
	const char *ui_uri = lilv_node_as_uri(lilv_ui_get_uri(m_lv2_ui));

	const char *ui_bundle_uri = lilv_node_as_uri(lilv_ui_get_bundle_uri(m_lv2_ui));
	const char *ui_binary_uri = lilv_node_as_uri(lilv_ui_get_binary_uri(m_lv2_ui));
#ifdef CONFIG_LILV_FILE_URI_PARSE
	const char *ui_bundle_path = lilv_file_uri_parse(ui_bundle_uri, NULL);
	const char *ui_binary_path = lilv_file_uri_parse(ui_binary_uri, NULL);
#else
	const char *ui_bundle_path = lilv_uri_to_path(ui_bundle_uri);
	const char *ui_binary_path = lilv_uri_to_path(ui_binary_uri);
#endif

	// Do try to instantiate the UI...
	const bool ui_instantiate = lv2_ui_instantiate(
		ui_host_uri, plugin_uri, ui_uri, ui_type_uri,
		ui_bundle_path, ui_binary_path);

#ifdef CONFIG_LILV_FILE_URI_PARSE
	lilv_free((void *) ui_binary_path);
	lilv_free((void *) ui_bundle_path);
#endif

	// Did we failed miserably?
	if (!ui_instantiate)
		return;

#ifdef CONFIG_LIBSUIL
	const bool ui_supported = (m_suil_instance != NULL);
#else
	const bool ui_supported = false;
#endif

	const qtractorPlugin::Params& params = qtractorPlugin::params();
	qtractorPlugin::Params::ConstIterator param = params.constBegin();
	const qtractorPlugin::Params::ConstIterator& param_end = params.constEnd();
	for ( ; param != param_end; ++param) {
		qtractorPluginParam *pParam = param.value();
		const float fValue = pParam->value();
		lv2_ui_port_event(pParam->index(),
			sizeof(float), 0, &fValue);
	}

	const unsigned long iControlOuts = pLv2Type->controlOuts();
	for (unsigned long j = 0; j < iControlOuts; ++j) {
		lv2_ui_port_event(m_piControlOuts[j],
			sizeof(float), 0, &m_pfControlOuts[j]);
	}

#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_X11
	if (!ui_supported && m_pQtWidget
		&& m_lv2_ui_type == LV2_UI_TYPE_X11) {
		// Override widget handle...
		m_lv2_ui_widget = static_cast<LV2UI_Widget> (m_pQtWidget);
		m_pQtFilter = new EventFilter(this, m_pQtWidget);
	//	m_bQtDelete = true;
		// LV2 UI resize control...
		QSize size = m_pQtWidget->sizeHint();
		if (!size.isValid() || size.isNull())
			size = m_pQtWidget->size();
		if (size.isValid() && !size.isNull()) {
			m_pQtWidget->setMinimumSize(size);
			lv2_ui_resize(size);
		}
	//	m_pQtWidget->show();
	}
	else
#endif	// CONFIG_LV2_UI_X11
#ifdef CONFIG_LV2_UI_GTK2
	if (!ui_supported && m_lv2_ui_widget
		&& m_lv2_ui_type == LV2_UI_TYPE_GTK) {
		// Initialize GTK+ framework (one time only)...
		if (!g_lv2_ui_gtk2_init) {
			gtk_init(NULL, NULL);
			g_lv2_ui_gtk2_init = true;
		}
		// Create embeddable native window...
		GtkWidget *pGtkWidget = static_cast<GtkWidget *> (m_lv2_ui_widget);
		GtkWidget *pGtkWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	//	GtkWidget *pGtkWindow = gtk_plug_new(0);
		gtk_window_set_resizable(GTK_WINDOW(pGtkWindow), 0);
		// Add plugin widget into our new window container...
		gtk_container_add(GTK_CONTAINER(pGtkWindow), pGtkWidget);
		gtk_widget_show_all(pGtkWindow);
		// Embed native GTK+ window into a Qt widget...
		const WId wid = GDK_WINDOW_XID(gtk_widget_get_window(pGtkWindow));
	//	const WId wid = gtk_plug_get_id((GtkPlug *) pGtkWindow);
		QWindow *pQtWindow = QWindow::fromWinId(wid);
		// Create the new parent frame...
		QWidget *pQtWidget = new QWidget(NULL, Qt::Window);
		QWidget *pQtContainer = QWidget::createWindowContainer(pQtWindow, pQtWidget);
		QVBoxLayout *pVBoxLayout = new QVBoxLayout();
		pVBoxLayout->setMargin(0);
		pVBoxLayout->setSpacing(0);
		pVBoxLayout->addWidget(pQtContainer);
		pQtWidget->setLayout(pVBoxLayout);
		// Get initial window size...
		GtkAllocation alloc;
		gtk_widget_get_allocation(pGtkWidget, &alloc);
		pQtWidget->resize(alloc.width, alloc.height);
		// Set native GTK+ window size callbacks...
		g_signal_connect(G_OBJECT(pGtkWindow), "size-request",
			G_CALLBACK(qtractor_lv2_ui_gtk2_on_size_request), pQtWidget);
		g_signal_connect(G_OBJECT(pGtkWindow), "size-allocate",
			G_CALLBACK(qtractor_lv2_ui_gtk2_on_size_allocate), pQtWidget);
		m_pGtkWindow = pGtkWindow;
		m_pQtWindow = pQtWindow;
		// done.
		m_pQtWidget = pQtWidget;
		m_pQtFilter = new EventFilter(this, m_pQtWidget);
		m_bQtDelete = true; // owned!
		// LV2 UI resize control...
		lv2_ui_resize(QSize(alloc.width, alloc.height));
	//	m_pQtWidget->show();
	}
	else
#endif	// CONFIG_LV2_UI_GTK2
#endif
	if (ui_supported && m_lv2_ui_widget
		&& m_lv2_ui_type != LV2_UI_TYPE_EXTERNAL) {
		m_pQtWidget = static_cast<QWidget *> (m_lv2_ui_widget);
		m_pQtFilter = new EventFilter(this, m_pQtWidget);
		m_bQtDelete = false;
		// LV2 UI resize control...
		QSize size = m_pQtWidget->sizeHint();
		if (!size.isValid() || size.isNull())
			size = m_pQtWidget->size();
		if (size.isValid() && !size.isNull()) {
			m_pQtWidget->setMinimumSize(size);
			lv2_ui_resize(size);
		}
	//	m_pQtWidget->show();
	} else {
		m_pQtWidget = NULL;
		m_pQtFilter = NULL;
		m_bQtDelete = false;
	}
#ifdef CONFIG_LV2_UI_IDLE
	if (m_lv2_ui_type != LV2_UI_TYPE_EXTERNAL) {
		m_lv2_ui_idle_interface	= (const LV2UI_Idle_Interface *)
			lv2_ui_extension_data(LV2_UI__idleInterface);
	} else {
		m_lv2_ui_idle_interface	= NULL;
	}
#endif	// CONFIG_LV2_UI_IDLE
#ifdef CONFIG_LV2_UI_SHOW
	if (m_pQtWidget == NULL && m_lv2_ui_type != LV2_UI_TYPE_EXTERNAL) {
		m_lv2_ui_show_interface	= (const LV2UI_Show_Interface *)
			lv2_ui_extension_data(LV2_UI__showInterface);
	} else {
		m_lv2_ui_show_interface	= NULL;
	}
#endif	// CONFIG_LV2_UI_SHOW

	updateEditorTitle();

	setEditorVisible(true);
	loadEditorPos();
//	idleEditor();
}


// Close editor.
void qtractorLv2Plugin::closeEditor (void)
{
	if (m_lv2_ui == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::closeEditor()", this);
#endif

	setEditorVisible(false);

	m_ui_params.clear();

#ifdef CONFIG_LV2_UI_TOUCH
	m_ui_params_touch.clear();
#endif

#ifdef CONFIG_LV2_UI_SHOW
	m_lv2_ui_show_interface	= NULL;
#endif
#ifdef CONFIG_LV2_UI_IDLE
	m_lv2_ui_idle_interface	= NULL;
#endif

#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
	if (m_pQtWindow) {
		m_pQtWindow->setParent(NULL);
		delete m_pQtWindow;
		m_pQtWindow = NULL;
	}
	if (m_pGtkWindow) {
		gtk_widget_destroy(m_pGtkWindow);
		m_pGtkWindow = NULL;
	}
#endif	// CONFIG_LV2_UI_GTK2
#endif

	if (m_pQtWidget) {
		if (m_bQtDelete) {
			delete m_pQtWidget;
			m_bQtDelete = false;
		}
		m_pQtWidget = NULL;
	}

	if (m_pQtFilter) {
		delete m_pQtFilter;
		m_pQtFilter = NULL;
	}

#ifdef CONFIG_LIBSUIL
	if (m_suil_instance) {
		suil_instance_free(m_suil_instance);
		m_suil_instance = NULL;
	}
	if (m_suil_host) {
		suil_host_free(m_suil_host);
		m_suil_host = NULL;
	}
#endif

	if (m_lv2_ui_descriptor) {
		if (m_lv2_ui_handle)
			m_lv2_ui_descriptor->cleanup(m_lv2_ui_handle);
		m_lv2_ui_descriptor = NULL;
	}

	if (m_lv2_ui_library) {
		delete m_lv2_ui_library;
		m_lv2_ui_library = NULL;
	}

	if (m_lv2_ui_features) {
		delete [] m_lv2_ui_features;
		m_lv2_ui_features = NULL;
	}

	if (m_lv2_uis) {
		lilv_uis_free(m_lv2_uis);
		m_lv2_uis = NULL;
	}

	m_lv2_ui_widget = NULL;
	m_lv2_ui_handle = NULL;
	m_lv2_ui_type = LV2_UI_TYPE_NONE;
	m_lv2_ui = NULL;
}


// Idle editor.
void qtractorLv2Plugin::idleEditor (void)
{
#ifdef CONFIG_LV2_ATOM
	uint32_t read_space = ::jack_ringbuffer_read_space(m_plugin_events);
	while (read_space > 0) {
		ControlEvent ev;
		::jack_ringbuffer_read(m_plugin_events, (char *) &ev, sizeof(ev));
		char buf[ev.size];
		if (::jack_ringbuffer_read(m_plugin_events, buf, ev.size) < ev.size)
			break;
		lv2_ui_port_event(ev.index, ev.size, ev.protocol, buf);
		read_space -= sizeof(ev) + ev.size;
	}
#endif

	// Try to make all parameter changes into one single command...
	if (m_ui_params.count() > 0) {
		qtractorSession *pSession = qtractorSession::getInstance();
		if (pSession) {
			qtractorPluginParamValuesCommand *pParamValuesCommand
				= new qtractorPluginParamValuesCommand(
					QObject::tr("plugin parameters"));
			QHash<unsigned long, float>::ConstIterator iter
				= m_ui_params.constBegin();
			const QHash<unsigned long, float>::ConstIterator& iter_end
				= m_ui_params.constEnd();
			for ( ; iter != iter_end; ++iter) {
				const unsigned long iIndex = iter.key();
				const float fValue = iter.value();
				qtractorPluginParam *pParam = findParam(iIndex);
				if (pParam)
					pParamValuesCommand->updateParamValue(pParam, fValue, false);
			}
			if (pParamValuesCommand->isEmpty())
				delete pParamValuesCommand;
			else
				pSession->execute(pParamValuesCommand);
		}
		// Done.
		m_ui_params.clear();
	}

	// Now, the following only makes sense
	// iif you have an open custom GUI editor..
	if (m_lv2_ui == NULL)
		return;

	if (m_piControlOuts && m_pfControlOuts && m_pfControlOutsLast) {
		const unsigned long iControlOuts = type()->controlOuts();
		for (unsigned short j = 0; j < iControlOuts; ++j) {
			if (m_pfControlOutsLast[j] != m_pfControlOuts[j]) {
				lv2_ui_port_event(m_piControlOuts[j],
					sizeof(float), 0, &m_pfControlOuts[j]);
				m_pfControlOutsLast[j] = m_pfControlOuts[j];
			}
		}
	}

	// Do we need some clean-up...?
	if (isEditorClosed()) {
		setEditorClosed(false);
		toggleFormEditor(false);
		m_bEditorVisible = false;
		// Do really close now.
		closeEditor();
		return;
	}

#ifdef CONFIG_LV2_EXTERNAL_UI
	if (m_lv2_ui_widget && m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
		LV2_EXTERNAL_UI_RUN((LV2_External_UI_Widget *) m_lv2_ui_widget);
#endif
#ifdef CONFIG_LV2_UI_IDLE
	if (m_lv2_ui_handle
		&& m_lv2_ui_idle_interface
		&& m_lv2_ui_idle_interface->idle) {
		if ((*m_lv2_ui_idle_interface->idle)(m_lv2_ui_handle))
			closeEditorEx();
	}
#endif
}


void qtractorLv2Plugin::closeEditorEx (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::closeEditorEx()", this);
#endif

	// Save current editor position, if any...
	// saveEditorPos();

	setEditorClosed(true);
}


// GUI editor visibility state.
void qtractorLv2Plugin::setEditorVisible ( bool bVisible )
{
	if (m_lv2_ui == NULL)
		return;

	if (/*!m_bEditorVisible && */bVisible) {
		if (m_pQtWidget) {
			// Guaranteeing a reasonable window type:
			// ie. not Qt::Dialog or Qt::Popup from the seemingly good choices.
			if (m_lv2_ui_type == LV2_UI_TYPE_QT4 ||
				m_lv2_ui_type == LV2_UI_TYPE_QT5) {
				const Qt::WindowFlags wflags
					= m_pQtWidget->windowFlags() & ~Qt::WindowType_Mask;
				m_pQtWidget->setWindowFlags(wflags | Qt::Widget);
			}
			m_pQtWidget->show();
			m_pQtWidget->raise();
			m_pQtWidget->activateWindow();
		}
	#ifdef CONFIG_LV2_EXTERNAL_UI
		else
		if (m_lv2_ui_widget && m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
			LV2_EXTERNAL_UI_SHOW((LV2_External_UI_Widget *) m_lv2_ui_widget);
	#endif
	#ifdef CONFIG_LV2_UI_SHOW
		else
		if (m_lv2_ui_handle
			&& m_lv2_ui_show_interface
			&& m_lv2_ui_show_interface->show)
			(*m_lv2_ui_show_interface->show)(m_lv2_ui_handle);
	#endif
		m_bEditorVisible = true;
		// Restore editor last known position, if any...
		// loadEditorPos();
	}
	else
	if (/*m_bEditorVisible && */!bVisible) {
		// Save editor current position, if any...
		saveEditorPos();
		if (m_pQtWidget)
			m_pQtWidget->hide();
	#ifdef CONFIG_LV2_EXTERNAL_UI
		else
		if (m_lv2_ui_widget && m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
			LV2_EXTERNAL_UI_HIDE((LV2_External_UI_Widget *) m_lv2_ui_widget);
	#endif
	#ifdef CONFIG_LV2_UI_SHOW
		else
		if (m_lv2_ui_handle
			&& m_lv2_ui_show_interface
			&& m_lv2_ui_show_interface->hide)
			(*m_lv2_ui_show_interface->hide)(m_lv2_ui_handle);
	#endif
		m_bEditorVisible = false;
	}

	toggleFormEditor(m_bEditorVisible);
}


bool qtractorLv2Plugin::isEditorVisible (void) const
{
	return m_bEditorVisible;
}


// GUI editor window title methods.
void qtractorLv2Plugin::setEditorTitle ( const QString& sTitle )
{
	qtractorPlugin::setEditorTitle(sTitle);

	m_aEditorTitle = editorTitle().toUtf8();

	updateEditorTitleEx();
}


void qtractorLv2Plugin::updateEditorTitleEx (void)
{
	if (m_lv2_ui == NULL)
		return;

#ifdef CONFIG_LV2_OPTIONS
	for (int i = 0; m_lv2_ui_options[i].type; ++i) {
		if (m_lv2_ui_options[i].type == g_lv2_urids.ui_windowTitle) {
			m_lv2_ui_options[i].value = m_aEditorTitle.constData();
			break;
		}
	}
#endif

	if (m_pQtWidget)
		m_pQtWidget->setWindowTitle(m_aEditorTitle);
#ifdef CONFIG_LV2_EXTERNAL_UI
	else
	if (m_lv2_ui_widget && m_lv2_ui_type == LV2_UI_TYPE_EXTERNAL)
		m_lv2_ui_external_host.plugin_human_id = m_aEditorTitle.constData();
#endif
#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
#ifdef CONFIG_LV2_UI_SHOW
	else
	if (m_lv2_ui_widget
		&& m_lv2_ui_show_interface
		&& m_lv2_ui_type == LV2_UI_TYPE_GTK) {
		GtkWidget *pGtkWidget = static_cast<GtkWidget *> (m_lv2_ui_widget);
		gtk_window_set_title(
			GTK_WINDOW(gtk_widget_get_toplevel(pGtkWidget)),
			m_aEditorTitle.constData());
	}
#endif	// CONFIG_LV2_UI_SHOW
#endif	// CONFIG_LV2_UI_GTK2
#endif
}


// GUI editor window (re)position methods.
void qtractorLv2Plugin::saveEditorPos (void)
{
	if (m_lv2_ui == NULL)
		return;

	QPoint posEditor;

	if (m_pQtWidget && m_pQtWidget->isVisible())
		posEditor = m_pQtWidget->pos();
#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
#ifdef CONFIG_LV2_UI_SHOW
	else
	if (m_lv2_ui_widget
		&& m_lv2_ui_show_interface
		&& m_lv2_ui_type == LV2_UI_TYPE_GTK) {
		GtkWidget *pGtkWidget = static_cast<GtkWidget *> (m_lv2_ui_widget);
		gint x = 0;
		gint y = 0;
		gtk_window_get_position(
			GTK_WINDOW(gtk_widget_get_toplevel(pGtkWidget)), &x, &y);
		posEditor.setX(int(x));
		posEditor.setY(int(y));
	}
#endif	// CONFIG_LV2_UI_SHOW
#endif	// CONFIG_LV2_UI_GTK2
#endif

	setEditorPos(posEditor);
}


void qtractorLv2Plugin::loadEditorPos (void)
{
	if (m_lv2_ui == NULL)
		return;

	const QPoint& posEditor = editorPos();

	if (m_pQtWidget)
		moveWidgetPos(m_pQtWidget, posEditor);
#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_GTK2
#ifdef CONFIG_LV2_UI_SHOW
	else
	if (m_lv2_ui_widget
		&& m_lv2_ui_show_interface
		&& m_lv2_ui_type == LV2_UI_TYPE_GTK) {
		GtkWidget *pGtkWidget = static_cast<GtkWidget *> (m_lv2_ui_widget);
		const gint x = posEditor.x();
		const gint y = posEditor.y();
		gtk_window_move(
			GTK_WINDOW(gtk_widget_get_toplevel(pGtkWidget)), x, y);
	}
#endif	// CONFIG_LV2_UI_SHOW
#endif	// CONFIG_LV2_UI_GTK2
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

	if (bUpdate)
		lv2_ui_port_event(pParam->index(), sizeof(float), 0, &fValue);
}


// Idle editor (static).
void qtractorLv2Plugin::idleEditorAll (void)
{
	QListIterator<qtractorLv2Plugin *> iter(g_lv2Plugins);
	while (iter.hasNext())
		iter.next()->idleEditor();
}


// LV2 UI control change method.
void qtractorLv2Plugin::lv2_ui_port_write ( uint32_t port_index,
	uint32_t buffer_size, uint32_t protocol, const void *buffer )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_port_write(%u, %u, %u, %p)",
		this, port_index, buffer_size, protocol, buffer);
#endif

#ifdef CONFIG_LV2_ATOM
	if (protocol == g_lv2_urids.atom_eventTransfer) {
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

#ifdef CONFIG_LV2_UI_TOUCH
	if (m_ui_params_touch.contains(port_index)
		&& !m_ui_params_touch.value(port_index, false))
		return;
#endif

	const float val = *(float *) buffer;

	// FIXME: Update plugin params...
	// updateParamValue(port_index, val, false);
	m_ui_params.insert(port_index, val);
}

// LV2 UI portMap method.
uint32_t qtractorLv2Plugin::lv2_ui_port_index ( const char *port_symbol )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_port_index(%s)",
		this, port_symbol);
#endif

	LilvPlugin *plugin = lv2_plugin();
	if (plugin == NULL)
		return LV2UI_INVALID_PORT_INDEX;

	LilvNode *symbol_uri = lilv_new_string(g_lv2_world, port_symbol);
	const LilvPort *port = lilv_plugin_get_port_by_symbol(plugin, symbol_uri);
	lilv_node_free(symbol_uri);

	return port
		? lilv_port_get_index(plugin, port)
		: LV2UI_INVALID_PORT_INDEX;
}


#ifdef CONFIG_LV2_UI_TOUCH

// LV2 UI touch control (ui->host).
void qtractorLv2Plugin::lv2_ui_touch ( uint32_t port_index, bool grabbed )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_touch(%u, %d)",
		this, port_index, int(grabbed));
#endif

	m_ui_params_touch[port_index] = grabbed;
}

#endif	// CONFIG_LV2_UI_TOUCH


// LV2 UI resize control (host->ui).
void qtractorLv2Plugin::lv2_ui_resize ( const QSize& size )
{
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_resize(%d, %d)",
		this, size.width(), size.height());
#endif
	const LV2UI_Resize *resize
		= (const LV2UI_Resize *) lv2_ui_extension_data(LV2_UI__resize);
	if (resize && resize->ui_resize) {
		LV2UI_Feature_Handle handle = resize->handle;
		if (handle == NULL)
			handle = m_lv2_ui_handle;
		(*resize->ui_resize)(handle, size.width(), size.height());
	}
}


// Alternate UI instantiation stuff.
bool qtractorLv2Plugin::lv2_ui_instantiate (
	const char *ui_host_uri, const char *plugin_uri,
	const char *ui_uri,	const char *ui_type_uri,
	const char *ui_bundle_path, const char *ui_binary_path )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::lv2_ui_instantiate(\"%s\")", this, ui_uri);
#endif

	// Setup fundamental UI features...
	const LilvInstance *instance = lv2_instance(0);
	if (instance == NULL)
		return false;

	const LV2_Descriptor *descriptor = lilv_instance_get_descriptor(instance);
	if (descriptor == NULL)
		return false;

	int iFeatures = 0;
	while (m_lv2_features[iFeatures]) { ++iFeatures; }

	m_lv2_ui_features = new LV2_Feature * [iFeatures + 9];
	for (int i = 0; i < iFeatures; ++i)
		m_lv2_ui_features[i] = (LV2_Feature *) m_lv2_features[i];

	m_lv2_ui_data_access.data_access = descriptor->extension_data;
	m_lv2_ui_data_access_feature.URI = LV2_DATA_ACCESS_URI;
	m_lv2_ui_data_access_feature.data = &m_lv2_ui_data_access;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_data_access_feature;

	m_lv2_ui_instance_access_feature.URI = LV2_INSTANCE_ACCESS_URI;
	m_lv2_ui_instance_access_feature.data = lilv_instance_get_handle(instance);
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_instance_access_feature;

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

#ifdef CONFIG_LV2_OPTIONS
	m_fUpdateRate = 15.0f;
	m_dSampleRate = 44100.0;
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		qtractorAudioEngine *pAudioEngine = pSession->audioEngine();
		if (pAudioEngine)
			m_dSampleRate = double(pAudioEngine->sampleRate());
	}
	const LV2_Options_Option ui_options[] = {
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.ui_windowTitle,
		  sizeof(char *), g_lv2_urids.atom_String, m_aEditorTitle.constData() },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.ui_updateRate,
		  sizeof(float), g_lv2_urids.atom_Float, &m_fUpdateRate },
		{ LV2_OPTIONS_INSTANCE, 0, g_lv2_urids.ui_sampleRate,
		  sizeof(double), g_lv2_urids.atom_Double, &m_dSampleRate },
		{ LV2_OPTIONS_INSTANCE, 0, 0, 0, 0, NULL }
	};
	::memcpy(&m_lv2_ui_options, &ui_options, sizeof(ui_options));
	m_lv2_ui_options_feature.URI  = LV2_OPTIONS__options;
	m_lv2_ui_options_feature.data = &m_lv2_ui_options;
	// Find and override options feature...
	for (int i = 0; i < iFeatures; ++i) {
		if (::strcmp(m_lv2_ui_features[i]->URI, LV2_OPTIONS__options) == 0) {
			m_lv2_ui_features[i] = &m_lv2_ui_options_feature;
			break;
		}
	}
#endif

	m_lv2_ui_features[iFeatures] = NULL;

#ifdef CONFIG_LIBSUIL
	// Check whether special UI wrapping are supported...
	if (ui_type_uri && suil_ui_supported(ui_host_uri, ui_type_uri) > 0) {
		m_suil_host = suil_host_new(
			qtractor_lv2_ui_port_write,
			qtractor_lv2_ui_port_index,
			NULL, NULL);
		if (m_suil_host) {
		#ifdef CONFIG_LV2_UI_TOUCH
			suil_host_set_touch_func(m_suil_host, qtractor_lv2_ui_touch);
		#endif
			m_suil_instance = suil_instance_new(m_suil_host, this,
				ui_host_uri, plugin_uri, ui_uri, ui_type_uri,
				ui_bundle_path, ui_binary_path, m_lv2_ui_features);
			if (m_suil_instance) {
			#ifdef CONFIG_SUIL_INSTANCE_GET_HANDLE
				m_lv2_ui_handle = (LV2UI_Handle)
					suil_instance_get_handle(m_suil_instance);
			#else
				struct SuilInstanceHead {	// HACK!
					void                   *ui_lib_handle;
					const LV2UI_Descriptor *ui_descriptor;
					LV2UI_Handle            ui_handle;
				} *suil_instance_head = (SuilInstanceHead *) m_suil_instance;
				m_lv2_ui_handle = suil_instance_head->ui_handle;
			#endif	// CONFIG_SUIL_INSTANCE_GET_HANDLE
				m_lv2_ui_widget = suil_instance_get_widget(m_suil_instance);
				return true;
			}
			// Fall thru...
			suil_host_free(m_suil_host);
			m_suil_host = NULL;
		}
	}
#endif

	// Open UI library...
	m_lv2_ui_library = new QLibrary(ui_binary_path);

	// Get UI descriptor discovery function...
	LV2UI_DescriptorFunction pfnLv2UiDescriptor
		= (LV2UI_DescriptorFunction) m_lv2_ui_library->resolve("lv2ui_descriptor");
	if (pfnLv2UiDescriptor == NULL) {
		delete m_lv2_ui_library;
		m_lv2_ui_library = NULL;
		return false;
	}

	// Get UI descriptor...
	uint32_t ui_index = 0;
	m_lv2_ui_descriptor = (*pfnLv2UiDescriptor)(ui_index);
	while (m_lv2_ui_descriptor && ::strcmp(m_lv2_ui_descriptor->URI, ui_uri))
		m_lv2_ui_descriptor = (*pfnLv2UiDescriptor)(++ui_index);
	if (m_lv2_ui_descriptor == NULL) {
		delete m_lv2_ui_library;
		m_lv2_ui_library = NULL;
		return false;
	}

	// Add additional features implemented by host functions...
	m_lv2_ui_port_map.handle       = this;
	m_lv2_ui_port_map.port_index   = qtractor_lv2_ui_port_index;
	m_lv2_ui_port_map_feature.URI  = LV2_UI__portMap;
	m_lv2_ui_port_map_feature.data = &m_lv2_ui_port_map;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_port_map_feature;

#ifdef CONFIG_LV2_UI_TOUCH
	m_lv2_ui_touch.handle          = this;
	m_lv2_ui_touch.touch           = qtractor_lv2_ui_touch;
	m_lv2_ui_touch_feature.URI     = LV2_UI__touch;
	m_lv2_ui_touch_feature.data    = &m_lv2_ui_touch;
	m_lv2_ui_features[iFeatures++] = &m_lv2_ui_touch_feature;
#endif

#if QT_VERSION >= 0x050100
#ifdef CONFIG_LV2_UI_X11
	if (m_lv2_ui_type == LV2_UI_TYPE_X11) {
		// Create the new parent frame...
		QWidget *pQtWidget = new QWidget(NULL, Qt::Window);
		// Add/prepare some needed features...
		m_lv2_ui_resize.handle = pQtWidget;
		m_lv2_ui_resize.ui_resize = qtractor_lv2_ui_resize;
		m_lv2_ui_resize_feature.URI = LV2_UI__resize;
		m_lv2_ui_resize_feature.data = &m_lv2_ui_resize;
		m_lv2_ui_features[iFeatures++] = &m_lv2_ui_resize_feature;
		m_lv2_ui_parent_feature.URI = LV2_UI__parent;
		m_lv2_ui_parent_feature.data = (void *) pQtWidget->winId();
		m_lv2_ui_features[iFeatures++] = &m_lv2_ui_parent_feature;
		// Done.
		m_pQtWidget = pQtWidget;
		m_bQtDelete = true;
	}
#endif	// CONFIG_LV2_UI_X11
#endif

	m_lv2_ui_features[iFeatures] = NULL;

	// Instantiate UI...
	m_lv2_ui_widget = NULL;
	m_lv2_ui_handle = m_lv2_ui_descriptor->instantiate(
		m_lv2_ui_descriptor, plugin_uri, ui_bundle_path,
		qtractor_lv2_ui_port_write, this, &m_lv2_ui_widget,
		m_lv2_ui_features);

	// Failed to instantiate UI?
	if (m_lv2_ui_handle == NULL) {
		m_lv2_ui_widget = NULL;
		m_lv2_ui_descriptor = NULL;
		delete m_lv2_ui_library;
		m_lv2_ui_library = NULL;
		return false;
	}

	return true;
}


void qtractorLv2Plugin::lv2_ui_port_event ( uint32_t port_index,
	uint32_t buffer_size, uint32_t format, const void *buffer )
{
#ifdef CONFIG_LIBSUIL
	if (m_suil_instance) {
		suil_instance_port_event(m_suil_instance,
			port_index, buffer_size, format, buffer);
	}
	else
#endif
	if (m_lv2_ui_descriptor && m_lv2_ui_descriptor->port_event) {
		(*m_lv2_ui_descriptor->port_event)(m_lv2_ui_handle,
			port_index, buffer_size, format, buffer);
	}

#ifdef CONFIG_LV2_ATOM
	if (format == g_lv2_urids.atom_eventTransfer) {
		const LV2_Atom *atom = (const LV2_Atom *) buffer;
		if (atom->type == g_lv2_urids.atom_Blank ||
			atom->type == g_lv2_urids.atom_Object) {
			const LV2_Atom_Object *obj = (const LV2_Atom_Object *) buffer;
		#ifdef CONFIG_LV2_PATCH
			if (obj->body.otype == g_lv2_urids.patch_Set) {
				const LV2_Atom_URID *prop = NULL;
				const LV2_Atom *value = NULL;
				lv2_atom_object_get(obj,
					g_lv2_urids.patch_property, (const LV2_Atom *) &prop,
					g_lv2_urids.patch_value, &value, 0);
				if (prop && value && prop->atom.type == g_lv2_atom_forge->URID)
					lv2_property_changed(prop->body, value);
			}
			else
			if (obj->body.otype == g_lv2_urids.patch_Put) {
				const LV2_Atom_Object *body = NULL;
				lv2_atom_object_get(obj,
					g_lv2_urids.patch_body, (const LV2_Atom *) &body, 0);
				if (body == NULL) // HACK!
					body = obj;
				if (body && (
					body->atom.type == g_lv2_urids.atom_Blank ||
					body->atom.type == g_lv2_urids.atom_Object)) {
					LV2_ATOM_OBJECT_FOREACH(body, prop)
						lv2_property_changed(prop->key, &prop->value);
				}
			}
		#ifdef CONFIG_LV2_STATE
			else
		#endif
		#endif // CONFIG_LV2_PATCH
		#ifdef CONFIG_LV2_STATE
			if (obj->body.otype == g_lv2_urids.state_StateChanged) {
				qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
				if (pMainForm)
					pMainForm->dirtyNotifySlot();
				refreshForm();
			}
		#endif // CONFIG_LV2_STATE
		}
	}
#endif	// CONFIG_LV2_ATOM
}


#ifdef CONFIG_LV2_PATCH

// LV2 Patch/properties inventory.
void qtractorLv2Plugin::lv2_patch_properties ( const char *pszPatch )
{
	const LilvPlugin *plugin = lv2_plugin();
	if (plugin == NULL)
		return;

	LilvNode *patch_uri = lilv_new_uri(g_lv2_world, pszPatch);
	LilvNodes *properties = lilv_world_find_nodes(
		g_lv2_world, lilv_plugin_get_uri(plugin), patch_uri, NULL);
	LILV_FOREACH(nodes, iter, properties) {
		const LilvNode *property = lilv_nodes_get(properties, iter);
		const char *pszKey = lilv_node_as_uri(property);
		if (pszKey && !m_lv2_properties.contains(pszKey)) {
			Property *pProp = new Property(property);
			m_lv2_properties.insert(pProp->uri(), pProp);
			++m_lv2_patch_changed;
		}
	}
	lilv_nodes_free(properties);
	lilv_node_free(patch_uri);
}


// LV2 Patch/properties changed, eventually from UI->plugin...
void qtractorLv2Plugin::lv2_property_changed (
	LV2_URID key, const LV2_Atom *value )
{
	const char *pszKey = lv2_urid_unmap(key);
	if (pszKey == NULL)
		return;

	Property *pProp = m_lv2_properties.value(pszKey, NULL);
	if (pProp == NULL)
		return;

	const LV2_URID type = value->type;
	const uint32_t size = value->size;
	const void *body = value + 1;

	if (type != pProp->type())
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_property_changed(\"%s\") [%s]",
		this, pszKey, pProp->name().toUtf8().constData());
#endif

	if (type == g_lv2_urids.atom_Bool)
		pProp->setValue(bool(*(const int32_t *) body));
	else
	if (type == g_lv2_urids.atom_Int)
		pProp->setValue(int(*(const int32_t *) body));
	else
	if (type == g_lv2_urids.atom_Long)
		pProp->setValue(qlonglong(*(const int64_t *) body));
	else
	if (type == g_lv2_urids.atom_Float)
		pProp->setValue(*(const float *) body);
	else
	if (type == g_lv2_urids.atom_Double)
		pProp->setValue(*(const double *) body);
	else
	if (type == g_lv2_urids.atom_String || type == g_lv2_urids.atom_Path)
		pProp->setValue(QByteArray((const char *) body, size));

	// Update the stock/generic form if visible...
	refreshForm();
}

// LV2 Patch/property updated, eventually from plugin->UI...
void qtractorLv2Plugin::lv2_property_update ( LV2_URID key )
{
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return;

	if (m_lv2_patch_port_in >= pLv2Type->atomIns())
		return;

	const char *pszKey = lv2_urid_unmap(key);
	if (pszKey == NULL)
		return;

	Property *pProp = m_lv2_properties.value(pszKey, NULL);
	if (pProp == NULL)
		return;

	LV2_URID type = pProp->type();
	const QVariant& value = pProp->value();

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::lv2_property_update(\"%s\") [%s]",
		this, pszKey, pProp->name().toUtf8().constData());
#endif

	// Set up forge to write to temporary buffer on the stack
	LV2_Atom_Forge *forge = g_lv2_atom_forge;
	LV2_Atom_Forge_Frame frame;
	uint8_t buf[1024];
	lv2_atom_forge_set_buffer(forge, buf, sizeof(buf));

	// Serialize patch_Set message to set property...
	lv2_atom_forge_object(forge, &frame, 1, g_lv2_urids.patch_Set);
	lv2_atom_forge_key(forge, g_lv2_urids.patch_property);
	lv2_atom_forge_urid(forge, key);
	lv2_atom_forge_key(forge, g_lv2_urids.patch_value);

	if (type == g_lv2_urids.atom_Bool)
		lv2_atom_forge_bool(forge, value.toBool());
	else
	if (type == g_lv2_urids.atom_Int)
		lv2_atom_forge_int(forge, value.toInt());
	else
	if (type == g_lv2_urids.atom_Long)
		lv2_atom_forge_long(forge, value.toLongLong());
	else
	if (type == g_lv2_urids.atom_Float)
		lv2_atom_forge_float(forge, value.toFloat());
	else
	if (type == g_lv2_urids.atom_Double)
		lv2_atom_forge_double(forge, value.toDouble());
	else
	if (type == g_lv2_urids.atom_String) {
		const QByteArray& aString = value.toByteArray();
		lv2_atom_forge_string(forge, aString.data(), aString.size());
	}
	else
	if (type == g_lv2_urids.atom_Path) {
		const QByteArray& aPath = value.toByteArray();
		lv2_atom_forge_path(forge, aPath.data(), aPath.size());
	}

	// Write message to UI...
	const LV2_Atom *atom
		= lv2_atom_forge_deref(forge, frame.ref);

	lv2_ui_port_write(
		m_piAtomIns[m_lv2_patch_port_in],
		lv2_atom_total_size(atom),
		g_lv2_urids.atom_eventTransfer,
		(const void *) atom);
}

#endif	// CONFIG_LV2_PATCH


const void *qtractorLv2Plugin::lv2_ui_extension_data ( const char *uri )
{
#ifdef CONFIG_LIBSUIL
	if (m_suil_instance)
		return suil_instance_extension_data(m_suil_instance, uri);
	else
#endif
	if (m_lv2_ui_descriptor && m_lv2_ui_descriptor->extension_data)
		return (*m_lv2_ui_descriptor->extension_data)(uri);

	return NULL;
}


#endif	// CONFIG_LV2_UI


// Plugin configuration/state (save) snapshot.
void qtractorLv2Plugin::freezeConfigs (void)
{
#ifdef CONFIG_LV2_UI
	// Update current editor position...
	saveEditorPos();
#endif

	if (!type()->isConfigure())
		return;

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::freezeConfigs()", this);
#endif

#ifdef CONFIG_LV2_STATE

	const unsigned short iInstances = instances();
	for (unsigned short i = 0; i < iInstances; ++i) {
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

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::realizeConfigs()", this);
#endif

#ifdef CONFIG_LV2_STATE

	m_lv2_state_configs.clear();
	m_lv2_state_ctypes.clear();

	const Configs& configs = qtractorPlugin::configs();
	const ConfigTypes& ctypes = qtractorPlugin::configTypes();

	Configs::ConstIterator config = configs.constBegin();
	const Configs::ConstIterator& config_end = configs.constEnd();
	for ( ; config != config_end; ++config) {
		const QString& sKey = config.key();
		QByteArray aType(LV2_ATOM__String);
		ConfigTypes::ConstIterator ctype = ctypes.constFind(sKey);
		if (ctype != ctypes.constEnd())
			aType = ctype.value().toUtf8();
		const char *pszType = aType.constData();
		const LV2_URID type = lv2_urid_map(pszType);
		const bool bIsPath = (type == g_lv2_urids.atom_Path);
		const bool bIsString = (type == g_lv2_urids.atom_String);
		if (aType.isEmpty() || bIsPath || bIsString)
			m_lv2_state_configs.insert(sKey, config.value().toUtf8());
		else
		if (type == g_lv2_urids.atom_Bool || type == g_lv2_urids.atom_Int) {
			const int32_t val = config.value().toInt();
			m_lv2_state_configs.insert(sKey,
				QByteArray((const char *) &val, sizeof(int32_t)));
		}
		else
		if (type == g_lv2_urids.atom_Long) {
			const int64_t val = config.value().toLongLong();
			m_lv2_state_configs.insert(sKey,
				QByteArray((const char *) &val, sizeof(int64_t)));
		}
		else
		if (type == g_lv2_urids.atom_Float) {
			const float val = config.value().toFloat();
			m_lv2_state_configs.insert(sKey,
				QByteArray((const char *) &val, sizeof(float)));
		}
		else
		if (type == g_lv2_urids.atom_Double) {
			const double val = config.value().toDouble();
			m_lv2_state_configs.insert(sKey,
				QByteArray((const char *) &val, sizeof(double)));
		}
		else {
			m_lv2_state_configs.insert(sKey, qUncompress(
				QByteArray::fromBase64(config.value().toUtf8())));
		}
		if (!aType.isEmpty() && !bIsString)
			m_lv2_state_ctypes.insert(sKey, type);
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

#ifdef CONFIG_DEBUG_0
	qDebug("qtractorLv2Plugin[%p]::releaseConfigs()", this);
#endif

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

// Load default plugin state
void qtractorLv2Plugin::lv2_state_load_default (void)
{
	const LilvNode *default_state = lilv_plugin_get_uri(lv2_plugin());
	if (default_state == NULL)
		return;

	LilvState *state = lilv_state_new_from_world(g_lv2_world,
		&g_lv2_urid_map, default_state);
	if (state == NULL)
		return;

	const unsigned short iInstances = instances();
	for (unsigned short i = 0; i < iInstances; ++i) {
		lilv_state_restore(state, m_ppInstances[i],
			qtractor_lv2_set_port_value, this, 0, NULL);
	}

	lilv_state_free(state);

#ifdef CONFIG_LV2_PATCH
	++m_lv2_patch_changed;
#endif
}


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

	const bool bIsPath   = (type == g_lv2_urids.atom_Path);
	const bool bIsString = (type == g_lv2_urids.atom_String);

	const QString& sKey = QString::fromUtf8(pszKey);

	if (bIsPath || bIsString)
		setConfig(sKey, QString::fromUtf8(pchValue, ::strlen(pchValue)));
	else
	if (type == g_lv2_urids.atom_Bool || type == g_lv2_urids.atom_Int)
		setConfig(sKey, QString::number(int(*(const int32_t *) pchValue)));
	else
	if (type == g_lv2_urids.atom_Long)
		setConfig(sKey, QString::number(qlonglong(*(const int64_t *) pchValue)));
	else
	if (type == g_lv2_urids.atom_Float)
		setConfig(sKey, QString::number(*(const float *) pchValue));
	else
	if (type == g_lv2_urids.atom_Double)
		setConfig(sKey, QString::number(*(const double *) pchValue));
	else {
		QByteArray data = qCompress(
			QByteArray(pchValue, size)).toBase64();
		for (int i = data.size() - (data.size() % 72); i >= 0; i -= 72)
			data.insert(i, "\n       "); // Indentation.
		setConfig(sKey, data.constData());
	}

	if (!bIsString)
		setConfigType(sKey, QString::fromUtf8(pszType));

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
			*type = g_lv2_urids.atom_String;
	}
	if (flags)
		*flags = LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE;

	return data.constData();
}

#endif	// CONFIG_LV2_STATE


#ifdef CONFIG_LV2_STATE_FILES

// LV2 State save directory (when not the default session one).
const QString& qtractorLv2Plugin::lv2_state_save_dir (void) const
{
	return m_lv2_state_save_dir;
}

#endif	// CONFIG_LV2_STATE_FILES


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

	// HACK: We don't change program-preset when
	// we're supposed to be multi-timbral...
	if (list()->isMidiBus())
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorLv2Plugin[%p]::selectProgram(%d, %d)", this, iBank, iProg);
#endif

	// For each plugin instance...
	const unsigned short iInstances = instances();
	for (unsigned short i = 0; i < iInstances; ++i) {
		const LV2_Programs_Interface *programs = lv2_programs_descriptor(i);
		if (programs && programs->select_program) {
			LV2_Handle handle = lv2_handle(i);
			if (handle) {
				(*programs->select_program)(handle, iBank, iProg);
			}
		}
	}

#ifdef CONFIG_LV2_UI
	const LV2_Programs_UI_Interface *ui_programs
		= (const LV2_Programs_UI_Interface *)
			lv2_ui_extension_data(LV2_PROGRAMS__UIInterface);
	if (ui_programs && ui_programs->select_program)
		(*ui_programs->select_program)(m_lv2_ui_handle, iBank, iProg);
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
			qtractorSession *pSession = qtractorSession::getInstance();
			if (pSession) {
				pSession->execute(
					new qtractorPluginProgramCommand(
						this, program.bank, program.prog));
			}
		}
	}
}

#endif	// CONFIG_LV2_PROGRAMS


#ifdef CONFIG_LV2_TIME

// Update LV2 Time from JACK transport position. (static)
inline void qtractor_lv2_time_update ( int i, float fValue )
{
	qtractorLv2Time& member = g_lv2_time[i];

	if (member.value != fValue) {
		member.value  = fValue;
		++member.changed;
	#ifdef CONFIG_LV2_TIME_POSITION
		++g_lv2_time_position_changed;
	#endif
	}
}

void qtractorLv2Plugin::updateTime ( qtractorAudioEngine *pAudioEngine )
{
	if (g_lv2_time_refcount < 1)
		return;

#ifdef CONFIG_LV2_TIME_POSITION
	g_lv2_time_position_changed = 0;
#endif

	jack_position_t pos;
	jack_transport_state_t state;

	if (pAudioEngine->isFreewheel()) {
		pos.frame = pAudioEngine->sessionCursor()->frame();
		pAudioEngine->timebase(&pos, 0);
		state = JackTransportRolling; // Fake transport rolling...
	} else {
		state = jack_transport_query(pAudioEngine->jackClient(), &pos);
	}

#if 0//QTRACTOR_LV2_TIME_POSITION_FRAME_0
	qtractor_lv2_time_update(
		qtractorLv2Time::frame,
		float(pos.frame));
#endif
	qtractor_lv2_time_update(
		qtractorLv2Time::framesPerSecond,
		float(pos.frame_rate));
	qtractor_lv2_time_update(
		qtractorLv2Time::speed,
		(state == JackTransportRolling ? 1.0f : 0.0f));

	if (pos.valid & JackPositionBBT) {
		qtractor_lv2_time_update(
			qtractorLv2Time::bar,
			float(pos.bar));
		qtractor_lv2_time_update(
			qtractorLv2Time::beat,
			float(pos.beat));
	#if 0//QTRACTOR_LV2_TIME_POSITION_BARBEAT_0
		qtractor_lv2_time_update(
			qtractorLv2Time::barBeat,
			float(pos.beat + (pos.tick / pos.ticks_per_beat) - 1));
	#endif
		qtractor_lv2_time_update(
			qtractorLv2Time::beatUnit,
			float(pos.beat_type));
		qtractor_lv2_time_update(
			qtractorLv2Time::beatsPerBar,
			float(pos.beats_per_bar));
		qtractor_lv2_time_update(
			qtractorLv2Time::beatsPerMinute,
			float(pos.beats_per_minute));
	}

#ifdef CONFIG_LV2_TIME_POSITION
	if (g_lv2_time_position_changed > 0 &&
		g_lv2_time_position_plugins &&
		g_lv2_time_position_buffer) {
		// Build LV2 Time position object to report change to plugin.
		LV2_Atom_Forge *forge = g_lv2_atom_forge;
		uint8_t *buffer = g_lv2_time_position_buffer;
		lv2_atom_forge_set_buffer(forge, buffer, 256);
		LV2_Atom_Forge_Frame frame;
		lv2_atom_forge_object(forge, &frame, 0, g_lv2_urids.time_Position);
		qtractorLv2Time& time_frame
			= g_lv2_time[qtractorLv2Time::frame];
	#if 1//QTRACTOR_LV2_TIME_POSITION_FRAME_1
		time_frame.value = float(pos.frame);
	#endif
		lv2_atom_forge_key(forge, time_frame.urid);
		lv2_atom_forge_long(forge, long(time_frame.value));
		const qtractorLv2Time& time_speed
			= g_lv2_time[qtractorLv2Time::speed];
		lv2_atom_forge_key(forge, time_speed.urid);
		lv2_atom_forge_float(forge, time_speed.value);
		if (pos.valid & JackPositionBBT) {
			const qtractorLv2Time& time_bar
				= g_lv2_time[qtractorLv2Time::bar];
			lv2_atom_forge_key(forge, time_bar.urid);
			lv2_atom_forge_long(forge, long(time_bar.value) - 1); // WTF?
			const qtractorLv2Time& time_beat
				= g_lv2_time[qtractorLv2Time::beat];
			lv2_atom_forge_key(forge, time_beat.urid);
			lv2_atom_forge_double(forge, double(time_beat.value));
			qtractorLv2Time& time_barBeat
				= g_lv2_time[qtractorLv2Time::barBeat];
		#if 1//QTRACTOR_LV2_TIME_POSITION_BARBEAT_1
			time_barBeat.value = float(pos.beat + (pos.tick / pos.ticks_per_beat) - 1);
		#endif
			lv2_atom_forge_key(forge, time_barBeat.urid);
			lv2_atom_forge_float(forge, time_barBeat.value);
			const qtractorLv2Time& time_beatUnit
				= g_lv2_time[qtractorLv2Time::beatUnit];
			lv2_atom_forge_key(forge, time_beatUnit.urid);
			lv2_atom_forge_int(forge, int(time_beatUnit.value));
			const qtractorLv2Time& time_beatsPerBar
				= g_lv2_time[qtractorLv2Time::beatsPerBar];
			lv2_atom_forge_key(forge, time_beatsPerBar.urid);
			lv2_atom_forge_float(forge, time_beatsPerBar.value);
			const qtractorLv2Time& time_beatsPerMinute
				= g_lv2_time[qtractorLv2Time::beatsPerMinute];
			lv2_atom_forge_key(forge, time_beatsPerMinute.urid);
			lv2_atom_forge_float(forge, time_beatsPerMinute.value);
		}
		lv2_atom_forge_pop(forge, &frame);
		// Make all supporting plugins ready...
		QListIterator<qtractorLv2Plugin *> iter(*g_lv2_time_position_plugins);
		while (iter.hasNext())
			iter.next()->lv2_time_position_changed();
	}
#endif	// CONFIG_LV2_TIME_POSITION
}

void qtractorLv2Plugin::updateTimePost (void)
{
	if (g_lv2_time_refcount < 1)
		return;

	for (int i = 0; i < int(qtractorLv2Time::numOfMembers); ++i) {
		qtractorLv2Time& member = g_lv2_time[i];
		if (member.changed > 0 &&
			member.params && member.params->count() > 0) {
			QListIterator<qtractorLv2PluginParam *> iter(*member.params);
			while (iter.hasNext())
				iter.next()->setValue(member.value, true);
			member.changed = 0;
		}
	}
}


#ifdef CONFIG_LV2_TIME_POSITION
// Make ready LV2 Time position.
void qtractorLv2Plugin::lv2_time_position_changed (void)
{
	++m_lv2_time_position_changed;
}
#endif

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

	LilvNode *preset_uri
		= lilv_new_uri(g_lv2_world, sUri.toUtf8().constData());

	LilvState *state = NULL;
	const QString sPath = QUrl(sUri).toLocalFile();
	const QFileInfo fi(sPath);
	if (!sPath.isEmpty() && fi.exists()) {
	#ifdef CONFIG_LV2_STATE_FILES
		m_lv2_state_save_dir = fi.absolutePath();
	#endif
		state = lilv_state_new_from_file(g_lv2_world,
			&g_lv2_urid_map, preset_uri,
			sPath.toUtf8().constData());
	} else {
		state = lilv_state_new_from_world(g_lv2_world,
			&g_lv2_urid_map, preset_uri);
	}

	if (state == NULL) {
		lilv_node_free(preset_uri);
	#ifdef CONFIG_LV2_STATE_FILES
		m_lv2_state_save_dir.clear();
	#endif
		return false;
	}

	const unsigned short iInstances = instances();
	for (unsigned short i = 0; i < iInstances; ++i) {
		lilv_state_restore(state, m_ppInstances[i],
			qtractor_lv2_set_port_value, this, 0, m_lv2_features);
	}

	lilv_state_free(state);
	lilv_node_free(preset_uri);

#ifdef CONFIG_LV2_STATE_FILES
	m_lv2_state_save_dir.clear();
#endif
#ifdef CONFIG_LV2_PATCH
	++m_lv2_patch_changed;
#endif
	return true;
}

// Save current plugin state as a named preset.
bool qtractorLv2Plugin::savePreset ( const QString& sPreset )
{
	qtractorLv2PluginType *pLv2Type
		= static_cast<qtractorLv2PluginType *> (type());
	if (pLv2Type == NULL)
		return false;

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

#ifdef CONFIG_LV2_STATE_FILES
	m_lv2_state_save_dir = sDir;
#endif

	LilvState *state = lilv_state_new_from_instance(
		lv2_plugin(), m_ppInstances[0], &g_lv2_urid_map,
		NULL, NULL, NULL, NULL,
		qtractor_lv2_get_port_value, this,
		LV2_STATE_IS_POD | LV2_STATE_IS_PORTABLE, m_lv2_features);

	if (state == NULL) {
	#ifdef CONFIG_LV2_STATE_FILES
		m_lv2_state_save_dir.clear();
	#endif
		return false;
	}

	lilv_state_set_label(state, sPreset.toUtf8().constData());

	const QString sFile = sPreset + ".ttl";

	int ret = lilv_state_save(g_lv2_world,
		&g_lv2_urid_map, &g_lv2_urid_unmap, state, NULL,
		sDir.toUtf8().constData(), sFile.toUtf8().constData());

	lilv_state_free(state);

#ifdef CONFIG_LV2_STATE_FILES
	m_lv2_state_save_dir.clear();
#endif

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
		LILV_FOREACH(scale_points, iter, points) {
			const LilvScalePoint *point = lilv_scale_points_get(points, iter);
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
	// reset(); -- deferred to qtractorPlugin::addParam();
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
