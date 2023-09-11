// qtractorPluginFactory.cpp
//
/****************************************************************************
   Copyright (C) 2005-2023, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPluginFactory.h"

#ifdef CONFIG_LADSPA
#include "qtractorLadspaPlugin.h"
#endif
#ifdef CONFIG_DSSI
#include "qtractorDssiPlugin.h"
#endif
#ifdef CONFIG_VST2
#include "qtractorVst2Plugin.h"
#endif
#ifdef CONFIG_VST3
#include "qtractorVst3Plugin.h"
#endif
#ifdef CONFIG_CLAP
#include "qtractorClapPlugin.h"
#endif
#ifdef CONFIG_LV2
#include "qtractorLv2Plugin.h"
#endif

#include "qtractorInsertPlugin.h"

#include "qtractorOptions.h"

#include <QApplication>

#include <QLibrary>
#include <QTextStream>
#include <QFileInfo>
#include <QDateTime>
#include <QDir>

#include <QRegularExpression>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
#include <QDesktopServices>
#else
#include <QStandardPaths>
#endif

// Deprecated QTextStreamFunctions/Qt namespaces workaround.
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
#define endl	Qt::endl
#endif


//----------------------------------------------------------------------------
// qtractorPluginFactory -- Plugin path helper.
//

// Singleton instance pointer.
qtractorPluginFactory *qtractorPluginFactory::g_pPluginFactory = nullptr;

// Singleton instance accessor (static).
qtractorPluginFactory *qtractorPluginFactory::getInstance (void)
{
	return g_pPluginFactory;
}


// Contructor.
qtractorPluginFactory::qtractorPluginFactory ( QObject *pParent )
	: QObject(pParent), m_typeHint(qtractorPluginType::Any)
{
	// Load persistent blacklist...
	//m_blacklist.clear();
	QFile data_file(blacklistDataFilePath());
	if (data_file.exists())
		readBlacklist(data_file);
	QFile temp_file(blacklistTempFilePath());
	if (temp_file.exists()) {
		readBlacklist(temp_file);
		temp_file.remove();
	}

	g_pPluginFactory = this;
}


// Destructor.
qtractorPluginFactory::~qtractorPluginFactory (void)
{
	g_pPluginFactory = nullptr;

	reset();
	clear();

	m_paths.clear();

	// Save persistent blacklist...
	QFile data_file(blacklistDataFilePath());
	if (!m_blacklist.isEmpty())
		writeBlacklist(data_file, m_blacklist);
	else
	if (data_file.exists())
		data_file.remove();
	m_blacklist.clear();
}


// A common scheme for (a default) plugin serach paths...
//
#if defined(__WIN32__) || defined(_WIN32) || defined(WIN32)
#define PATH_SEP ";"
#else
#define PATH_SEP ":"
#endif

static QStringList default_paths ( const QString& suffix )
{
	const QString& sep  = QDir::separator();

	const QString& home = QDir::homePath();

	const QString& pre1 = QDir::rootPath() + "usr";
	const QString& pre2 = pre1 + sep + "local";

	const QString& lib0 = "lib";
	const QString& lib1 = pre1 + sep + lib0;
	const QString& lib2 = pre2 + sep + lib0;

#if defined(__x86_64__)
	const QString& x64  = "64";
	const QString& lib3 = lib1 + x64;
	const QString& lib4 = lib2 + x64;
#endif

	QStringList paths;

	paths << home + sep + '.' + suffix;

#if defined(__x86_64__)
//	paths << home + sep + lib0 + x64 + sep + suffix;
	paths << lib4 + sep + suffix;
	paths << lib3 + sep + suffix;
#endif

//	paths << home + sep + lib0 + sep + suffix;
	paths << lib2 + sep + suffix;
	paths << lib1 + sep + suffix;

	// Get rid of duplicate symlinks (canonical paths)...
	QStringList ret;
	QStringListIterator iter(paths);
	while (iter.hasNext()) {
		const QFileInfo info(iter.next());
		if (info.exists() && info.isDir()) {
			const QString& path
				= info.canonicalFilePath();
			if (!ret.contains(path))
				ret.append(path);
		}
	}

	return ret;
}


void qtractorPluginFactory::updatePluginPaths (
	qtractorPluginType::Hint typeHint )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();

#ifdef CONFIG_LADSPA
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Ladspa) {
		// LADSPA default paths...
		QStringList ladspa_paths;
		if (pOptions)
			ladspa_paths = pOptions->ladspaPaths;
		if (ladspa_paths.isEmpty()) {
			QStringList sLadspaPaths;
			const char *ladspa_path = ::getenv("LADSPA_PATH");
			if (ladspa_path)
				sLadspaPaths << QString::fromUtf8(ladspa_path).split(PATH_SEP);
			if (sLadspaPaths.isEmpty())
				ladspa_paths << default_paths("ladspa");
			else
				ladspa_paths << sLadspaPaths;
		}
		m_paths.insert(qtractorPluginType::Ladspa, ladspa_paths);
	}
#endif

#ifdef CONFIG_DSSI
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Dssi) {
		// DSSI default paths...
		QStringList dssi_paths;
		if (pOptions)
			dssi_paths = pOptions->dssiPaths;
		if (dssi_paths.isEmpty()) {
			QStringList sDssiPaths;
			const char *dssi_path = ::getenv("DSSI_PATH");
			if (dssi_path)
				sDssiPaths << QString::fromUtf8(dssi_path).split(PATH_SEP);
			if (sDssiPaths.isEmpty())
				dssi_paths << default_paths("dssi");
			else
				dssi_paths << sDssiPaths;
		}
		m_paths.insert(qtractorPluginType::Dssi, dssi_paths);
	}
#endif

#ifdef CONFIG_VST2
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Vst2) {
		// VST2 default paths...
		QStringList vst2_paths;
		if (pOptions)
			vst2_paths = pOptions->vst2Paths;
		if (vst2_paths.isEmpty()) {
			QStringList sVst2Paths;
			const char *vst2_path = ::getenv("VST2_PATH");
			if (vst2_path)
				sVst2Paths << QString::fromUtf8(vst2_path).split(PATH_SEP);
			if (sVst2Paths.isEmpty())
				vst2_paths << default_paths("vst2");
			else
				vst2_paths << sVst2Paths;
			// LXVST_PATH...
			sVst2Paths.clear();
			vst2_path = ::getenv("LXVST_PATH");
			if (vst2_path)
				sVst2Paths << QString::fromUtf8(vst2_path).split(PATH_SEP);
			if (sVst2Paths.isEmpty())
				vst2_paths << default_paths("lxvst");
			else
				vst2_paths << sVst2Paths;
			// VST_PATH...
			sVst2Paths.clear();
			vst2_path = ::getenv("VST_PATH");
			if (vst2_path)
				sVst2Paths << QString::fromUtf8(vst2_path).split(PATH_SEP);
			if (sVst2Paths.isEmpty())
				vst2_paths << default_paths("vst");
			else
				vst2_paths << sVst2Paths;
		}
		m_paths.insert(qtractorPluginType::Vst2, vst2_paths);
	}
#endif

#ifdef CONFIG_VST3
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Vst3) {
		// VST3 default paths...
		QStringList vst3_paths;
		if (pOptions)
			vst3_paths = pOptions->vst3Paths;
		if (vst3_paths.isEmpty()) {
			QStringList sVst3Paths;
			const char *vst3_path = ::getenv("VST3_PATH");
			if (vst3_path)
				sVst3Paths << QString::fromUtf8(vst3_path).split(PATH_SEP);
			if (sVst3Paths.isEmpty())
				vst3_paths << default_paths("vst3");
			else
				vst3_paths << sVst3Paths;
		}
		m_paths.insert(qtractorPluginType::Vst3, vst3_paths);
	}
#endif

#ifdef CONFIG_CLAP
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Clap) {
		// CLAP default paths...
		QStringList clap_paths;
		if (pOptions)
			clap_paths = pOptions->clapPaths;
		if (clap_paths.isEmpty()) {
			QStringList sClapPaths;
			const char *clap_path = ::getenv("CLAP_PATH");
			if (clap_path)
				sClapPaths << QString::fromUtf8(clap_path).split(PATH_SEP);
			if (sClapPaths.isEmpty())
				clap_paths << default_paths("clap");
			else
				clap_paths << sClapPaths;
		}
		m_paths.insert(qtractorPluginType::Clap, clap_paths);
	}
#endif

#ifdef CONFIG_LV2
	if (typeHint == qtractorPluginType::Any ||
		typeHint == qtractorPluginType::Lv2) {
		// LV2 default paths...
		QStringList lv2_paths;
		if (pOptions)
			lv2_paths = pOptions->lv2Paths;
		if (lv2_paths.isEmpty()) {
			QStringList sLv2Paths;
			const char *lv2_path = ::getenv("LV2_PATH");
			if (lv2_path)
				sLv2Paths << QString::fromUtf8(lv2_path).split(PATH_SEP);
			if (sLv2Paths.isEmpty())
				lv2_paths << default_paths("lv2");
			else
				lv2_paths << sLv2Paths;
		}
	#ifdef CONFIG_LV2_PRESETS
		QString sLv2PresetDir;
		if (pOptions)
			sLv2PresetDir = pOptions->sLv2PresetDir;
		if (sLv2PresetDir.isEmpty())
			sLv2PresetDir = QDir::homePath() + QDir::separator() + ".lv2";
		if (!lv2_paths.contains(sLv2PresetDir))
			lv2_paths.append(sLv2PresetDir);
	#endif
		m_paths.insert(qtractorPluginType::Lv2, lv2_paths);
		// HACK: set special environment for LV2...
		::setenv("LV2_PATH", lv2_paths.join(PATH_SEP).toUtf8().constData(), 1);
	}
#endif
}


QStringList qtractorPluginFactory::pluginPaths (
	qtractorPluginType::Hint typeHint )
{
	if (m_paths.isEmpty())
		updatePluginPaths(qtractorPluginType::Any);

	return m_paths.value(typeHint);
}


// Blacklist accessors.
void qtractorPluginFactory::setBlacklist ( const QStringList&  blacklist )
{
	m_blacklist = blacklist;
}

const QStringList& qtractorPluginFactory::blacklist (void) const
{
	return m_blacklist;
}


// Absolute temporary blacklist file path.
QString qtractorPluginFactory::blacklistTempFilePath (void) const
{
	const QString& sTempName
		= "qtractor_plugin_blacklist.temp";
	const QString& sTempDir
	#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
		= QDesktopServices::storageLocation(QDesktopServices::TempLocation);
	#else
		= QStandardPaths::writableLocation(QStandardPaths::TempLocation);
	#endif
	const QFileInfo fi(sTempDir, sTempName);
	const QDir& dir = fi.absoluteDir();
	if (!dir.exists())
		dir.mkpath(dir.path());
	return fi.absoluteFilePath();
}


// Absolute persistent blacklist file path.
QString qtractorPluginFactory::blacklistDataFilePath (void) const
{
	const QString& sDataName
		= "qtractor_plugin_blacklist.data";
	const QString& sDataDir
	#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
		= QDesktopServices::storageLocation(QDesktopServices::DataLocation);
	#else
		= QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	#endif
	const QFileInfo fi(sDataDir, sDataName);
	const QDir& dir = fi.absoluteDir();
	if (!dir.exists())
		dir.mkpath(dir.absolutePath());
	return fi.absoluteFilePath();
}


// Read from file and append to blacklist.
bool qtractorPluginFactory::readBlacklist (	QFile& file )
{
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
		return false;

	QTextStream sin(&file);
	while (!sin.atEnd()) {
		const QString& line = sin.readLine();
		if (line.isEmpty())
			continue;
		m_blacklist.append(line);
	}
	file.close();

	return true;
}


// Write/append blacklist to file.
bool qtractorPluginFactory::writeBlacklist (
	QFile& file, const QStringList& blacklist ) const
{
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
		return false;

	QTextStream sout(&file);
	QStringListIterator iter(blacklist);
	while (iter.hasNext())
		sout << iter.next() << endl;
	file.close();

	return true;
}


// Generic plugin-scan factory method.
bool qtractorPluginFactory::startScan ( qtractorPluginType::Hint typeHint )
{
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions == nullptr)
		return false;

	bool bDummyPluginScan = pOptions->bDummyPluginScan;
	int  iDummyPluginHash = 0;

	switch (typeHint) {
	case qtractorPluginType::Ladspa:
		iDummyPluginHash = pOptions->iDummyLadspaHash;
		break;
	case qtractorPluginType::Dssi:
		iDummyPluginHash = pOptions->iDummyDssiHash;
		break;
	case qtractorPluginType::Vst2:
		iDummyPluginHash = pOptions->iDummyVst2Hash;
		break;
	case qtractorPluginType::Vst3:
		iDummyPluginHash = pOptions->iDummyVst3Hash;
		break;
	case qtractorPluginType::Clap:
		iDummyPluginHash = pOptions->iDummyClapHash;
		break;
	case qtractorPluginType::Lv2:
		iDummyPluginHash = pOptions->iDummyLv2Hash;
		break;
	default:
		break;
	}

	if (bDummyPluginScan) {
		const int iNewDummyPluginHash
			= m_files.value(typeHint).count();
		Scanner *pScanner = new Scanner(typeHint, this);
		if (pScanner->open(iDummyPluginHash != iNewDummyPluginHash)) {
			m_scanners.insert(typeHint, pScanner);
			switch (typeHint) {
			case qtractorPluginType::Ladspa:
				pOptions->iDummyLadspaHash = iNewDummyPluginHash;
				break;
			case qtractorPluginType::Dssi:
				pOptions->iDummyDssiHash = iNewDummyPluginHash;
				break;
			case qtractorPluginType::Vst2:
				pOptions->iDummyVst2Hash = iNewDummyPluginHash;
				break;
			case qtractorPluginType::Vst3:
				pOptions->iDummyVst3Hash = iNewDummyPluginHash;
				break;
			case qtractorPluginType::Clap:
				pOptions->iDummyClapHash = iNewDummyPluginHash;
				break;
			case qtractorPluginType::Lv2:
				pOptions->iDummyLv2Hash = iNewDummyPluginHash;
				break;
			default:
				break;
			}
			// Remember to cleanup cache later, when applicable...
			const QString& sCacheFilePath = pScanner->cacheFilePath();
			m_cacheFilePaths.insert(typeHint, sCacheFilePath);
			// Done.
			return true;
		}
	}

	return false;
}


// Executive methods.
void qtractorPluginFactory::scan (void)
{
	// Start clean.
	reset();

	if (pluginPaths(m_typeHint).isEmpty()) // Just in case...
		updatePluginPaths(m_typeHint);

	// Get paths based on hints...
	int iFileCount = 0;

#ifdef CONFIG_LADSPA
	// LADSPA default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Ladspa) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Ladspa;
		const QStringList& paths = m_paths.value(typeHint);
		if (!paths.isEmpty()) {
			iFileCount += addFiles(typeHint, paths);
			startScan(typeHint);
		}
	}
#endif
#ifdef CONFIG_DSSI
	// DSSI default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Dssi) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Dssi;
		const QStringList& paths = m_paths.value(typeHint);
		if (!paths.isEmpty()) {
			iFileCount += addFiles(typeHint, paths);
			startScan(typeHint);
		}
	}
#endif
#ifdef CONFIG_VST2
	// VST default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Vst2) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Vst2;
		const QStringList& paths = m_paths.value(typeHint);
		if (!paths.isEmpty()) {
			iFileCount += addFiles(typeHint, paths);
			startScan(typeHint);
		}
	}
#endif
#ifdef CONFIG_VST3
	// VST3 default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Vst3) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Vst3;
		const QStringList& paths = m_paths.value(typeHint);
		if (!paths.isEmpty()) {
			iFileCount += addFiles(typeHint, paths);
			startScan(typeHint);
		}
	}
#endif
#ifdef CONFIG_CLAP
	// CLAP default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Clap) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Clap;
		const QStringList& paths = m_paths.value(typeHint);
		if (!paths.isEmpty()) {
			iFileCount += addFiles(typeHint, paths);
			startScan(typeHint);
		}
	}
#endif
#ifdef CONFIG_LV2
	// LV2 default path...
	if (m_typeHint == qtractorPluginType::Any ||
		m_typeHint == qtractorPluginType::Lv2) {
		const qtractorPluginType::Hint typeHint
			= qtractorPluginType::Lv2;
		QStringList& files = m_files[typeHint];
		files.append(qtractorLv2PluginType::lv2_plugins());
		iFileCount += files.count();
		startScan(typeHint);
	}
#endif

	// Do the real scan...
	int iFile = 0;
	Paths::ConstIterator files_iter = m_files.constBegin();
	const Paths::ConstIterator& files_end = m_files.constEnd();
	for ( ; files_iter != files_end; ++files_iter) {
		const qtractorPluginType::Hint typeHint = files_iter.key();
		QStringListIterator file_iter(files_iter.value());
		while (file_iter.hasNext()) {
			addTypes(typeHint, file_iter.next());
			emit scanned((++iFile * 100) / iFileCount);
			QApplication::processEvents(
				QEventLoop::ExcludeUserInputEvents);
		}
	}

	// Done.
	reset();
}


void qtractorPluginFactory::reset (void)
{
	// Check the proxy (out-of-process) client closure...
	Scanners::ConstIterator iter = m_scanners.constBegin();
	const Scanners::ConstIterator& iter_end = m_scanners.constEnd();
	for ( ; iter != iter_end; ++iter) {
		Scanner *pScanner = iter.value();
		if (pScanner)
			pScanner->close();
	}

	qDeleteAll(m_scanners);
	m_scanners.clear();

	m_files.clear();
}


void qtractorPluginFactory::clear (void)
{
	qDeleteAll(m_types);
	m_types.clear();
}


void qtractorPluginFactory::clearAll ( qtractorPluginType::Hint typeHint )
{
	if (typeHint == qtractorPluginType::Any) {
		CacheFilePaths::ConstIterator iter = m_cacheFilePaths.constBegin();
		const CacheFilePaths::ConstIterator& iter_end = m_cacheFilePaths.constEnd();
		for ( ; iter != iter_end; ++iter) {
			const QString& sCacheFilePath = iter.value();
			if (!sCacheFilePath.isEmpty())
				QFile::remove(sCacheFilePath);
		}
	} else {
		const QString& sCacheFilePath = m_cacheFilePaths.value(typeHint);
		if (!sCacheFilePath.isEmpty())
			QFile::remove(sCacheFilePath);
	}

	clear();
}


// Recursive plugin file/path inventory method.
int qtractorPluginFactory::addFiles (
	qtractorPluginType::Hint typeHint, const QStringList& paths )
{
	int iFileCount = 0;

	QStringListIterator path_iter(paths);
	while (path_iter.hasNext())
		iFileCount += addFiles(typeHint, path_iter.next());

	return iFileCount;
}

int qtractorPluginFactory::addFiles (
	qtractorPluginType::Hint typeHint, const QString& sPath )
{
	int iFileCount = 0;
	QStringList& files = m_files[typeHint];
	const QDir dir(sPath);
	QDir::Filters filters = QDir::Files;
	if (typeHint == qtractorPluginType::Vst2 ||
		typeHint == qtractorPluginType::Vst3 ||
		typeHint == qtractorPluginType::Clap)
		filters = filters | QDir::AllDirs | QDir::NoDotAndDotDot;
	const QFileInfoList& info_list = dir.entryInfoList(filters);
	QListIterator<QFileInfo> info_iter(info_list);
	while (info_iter.hasNext()) {
		const QFileInfo& info = info_iter.next();
		const QString& sFilename = info.absoluteFilePath();
		if (info.isDir() && info.isReadable())
			iFileCount += addFiles(typeHint, sFilename);
		else
		if (QLibrary::isLibrary(sFilename)
		#ifdef CONFIG_CLAP
			|| (typeHint == qtractorPluginType::Clap && info.suffix() == "clap")
		#endif
		) {
			files.append(sFilename);
			++iFileCount;
		}
	}

	return iFileCount;
}


// Plugin factory method (static).
qtractorPlugin *qtractorPluginFactory::createPlugin (
	qtractorPluginList *pList,
	const QString& sFilename, unsigned long iIndex,
	qtractorPluginType::Hint typeHint )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorPluginFactory::createPlugin(%p, \"%s\", %lu, %d)",
		pList, sFilename.toUtf8().constData(), iIndex, int(typeHint));
#endif

	// Attend to insert pseudo-plugin hints...
	if (sFilename.isEmpty()) {
		if (typeHint == qtractorPluginType::Insert)
			return qtractorInsertPluginType::createPlugin(pList, iIndex);
		else
		if (typeHint == qtractorPluginType::AuxSend)
			return qtractorAuxSendPluginType::createPlugin(pList, iIndex);
		else
		// Don't bother with anything else.
		return nullptr;
	}

#ifdef CONFIG_LV2
	// Try LV2 plugins hints before anything else...
	if (typeHint == qtractorPluginType::Lv2) {
		qtractorLv2PluginType *pLv2Type
			= qtractorLv2PluginType::createType(sFilename);
		if (pLv2Type) {
			if (pLv2Type->open())
				return new qtractorLv2Plugin(pList, pLv2Type);
			delete pLv2Type;
		}
		// Bail out.
		return nullptr;
	}
#endif

	// Try to fill the types list at this moment...
	qtractorPluginFile *pFile = qtractorPluginFile::addFile(sFilename);
	if (pFile == nullptr)
		return nullptr;

#ifdef CONFIG_DSSI
	// Try DSSI plugin types first...
	if (typeHint == qtractorPluginType::Dssi) {
		qtractorDssiPluginType *pDssiType
			= qtractorDssiPluginType::createType(pFile, iIndex);
		if (pDssiType) {
			pFile->addRef();
			if (pDssiType->open())
				return new qtractorDssiPlugin(pList, pDssiType);
			delete pDssiType;
		}
	}
#endif

#ifdef CONFIG_LADSPA
	// Try LADSPA plugin types...
	if (typeHint == qtractorPluginType::Ladspa) {
		qtractorLadspaPluginType *pLadspaType
			= qtractorLadspaPluginType::createType(pFile, iIndex);
		if (pLadspaType) {
			pFile->addRef();
			if (pLadspaType->open())
				return new qtractorLadspaPlugin(pList, pLadspaType);
			delete pLadspaType;
		}
	}
#endif

#ifdef CONFIG_VST2
	// Try VST2 plugin types...
	if (typeHint == qtractorPluginType::Vst2) {
		qtractorVst2PluginType *pVst2Type
			= qtractorVst2PluginType::createType(pFile, iIndex);
		if (pVst2Type) {
			pFile->addRef();
			if (pVst2Type->open())
				return new qtractorVst2Plugin(pList, pVst2Type);
			delete pVst2Type;
		}
	}
#endif

#ifdef CONFIG_VST3
	// Try VST3 plugin types...
	if (typeHint == qtractorPluginType::Vst3) {
		qtractorVst3PluginType *pVst3Type
			= qtractorVst3PluginType::createType(pFile, iIndex);
		if (pVst3Type) {
			pFile->addRef();
			if (pVst3Type->open())
				return new qtractorVst3Plugin(pList, pVst3Type);
			delete pVst3Type;
		}
	}
#endif

#ifdef CONFIG_CLAP
	// Try CLAP plugin types...
	if (typeHint == qtractorPluginType::Clap) {
		qtractorClapPluginType *pClapType
			= qtractorClapPluginType::createType(pFile, iIndex);
		if (pClapType) {
			pFile->addRef();
			if (pClapType->open())
				return new qtractorClapPlugin(pList, pClapType);
			delete pClapType;
		}
	}
#endif

	// Bad luck, no valid plugin found...
	qtractorPluginFile::removeFile(pFile);

	return nullptr;
}


// Plugin type listing methods.
bool qtractorPluginFactory::addTypes (
	qtractorPluginType::Hint typeHint, const QString& sFilename )
{
	// Check if already blacklisted...
	if (m_blacklist.contains(sFilename))
		return false;

	// Try first out-of-process scans, if any...
	Scanner *pScanner = m_scanners.value(typeHint, nullptr);
	if (pScanner)
		return pScanner->addTypes(typeHint, sFilename);

#ifdef CONFIG_LV2
	// Try first URI-based plugin types (LV2...)
	if (typeHint == qtractorPluginType::Lv2) {
		qtractorPluginType *pType
			= qtractorLv2PluginType::createType(sFilename);
		if (pType == nullptr)
			return false;
		if (pType->open()) {
			addType(pType);
			pType->close();
			return true;
		} else {
			delete pType;
			return false;
		}
	}
#endif

	// Try all other file-based types (LADSPA, DSSI, VST...)
	qtractorPluginFile *pFile = qtractorPluginFile::addFile(sFilename);
	if (pFile == nullptr)
		return false;

	// Add to temporary blacklist...
	QFile temp_file(blacklistTempFilePath());
	if (temp_file.exists())
		readBlacklist(temp_file);
	writeBlacklist(temp_file, QStringList() << sFilename);

	unsigned long iIndex = 0;
	while (addTypes(typeHint, pFile, iIndex))
		++iIndex;

	// If it reaches here safely, then there's
	// no use to temporary blacklist anymore...
	temp_file.remove();

	if (iIndex > 0) {
		pFile->close();
		return true;
	}

	// We probably have nothing here.
	qtractorPluginFile::removeFile(pFile);

	return false;
}


// Plugin type listing method.
bool qtractorPluginFactory::addTypes (
	qtractorPluginType::Hint typeHint,
	qtractorPluginFile *pFile, unsigned long iIndex )
{
	qtractorPluginType *pType = nullptr;

	switch (typeHint) {
#ifdef CONFIG_LADSPA
	case qtractorPluginType::Ladspa:
		// Try LADSPA plugin type...
		pType = qtractorLadspaPluginType::createType(pFile, iIndex);
		break;
#endif
#ifdef CONFIG_DSSI
	case qtractorPluginType::Dssi:
		// Try DSSI plugin type...
		pType = qtractorDssiPluginType::createType(pFile, iIndex);
		break;
#endif
#ifdef CONFIG_VST2
	case qtractorPluginType::Vst2:
		// Try VST2 plugin type...
		pType = qtractorVst2PluginType::createType(pFile, iIndex);
		break;
#endif
#ifdef CONFIG_VST3
	case qtractorPluginType::Vst3:
		// Try VST3 plugin type...
		pType = qtractorVst3PluginType::createType(pFile, iIndex);
		break;
#endif
#ifdef CONFIG_CLAP
	case qtractorPluginType::Clap:
		// Try CLAP plugin type...
		pType = qtractorClapPluginType::createType(pFile, iIndex);
		break;
#endif
	default:
		break;
	}

	if (pType == nullptr)
		return false;

	if (pType->open()) {
		pFile->addRef();
		addType(pType);
		pType->close();
		return true;
	} else {
		delete pType;
		return false;
	}
}


//----------------------------------------------------------------------------
// qtractorPluginFactory::Scanner -- Plugin path proxy (out-of-process client).
//

// Constructor.
qtractorPluginFactory::Scanner::Scanner (
	qtractorPluginType::Hint typeHint, QObject *pParent )
		: QProcess(pParent), m_typeHint(typeHint), m_iExitStatus(-1), m_bReset(false)
{
	QObject::connect(this,
		SIGNAL(readyReadStandardOutput()),
		SLOT(stdout_slot()));
	QObject::connect(this,
		SIGNAL(readyReadStandardError()),
		SLOT(stderr_slot()));
	QObject::connect(this,
		SIGNAL(finished(int, QProcess::ExitStatus)),
		SLOT(exit_slot(int, QProcess::ExitStatus)));
}


// Open/start method.
bool qtractorPluginFactory::Scanner::open ( bool bReset )
{
	// Cache file setup...
	m_file.setFileName(cacheFilePath());
	m_list.clear();

	m_bReset = bReset;

	// Open and read cache file, whether applicable...
	if (m_file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		// Read from cache...
		QTextStream sin(&m_file);
		while (!sin.atEnd()) {
			const QString& sText = sin.readLine();
			if (sText.isEmpty())
				continue;
			const QStringList& props = sText.split('|');
			if (props.count() >= 6) // get filename...
				m_list[props.at(6)].append(sText);
		}
		// May close the file.
		m_file.close();
	} else {
		// Force reset...
		m_bReset = true;
	}

	if (!m_bReset)
		return true;

	// Make sure cache file location do exists...
	const QFileInfo fi(m_file);
	if (!fi.dir().mkpath(fi.absolutePath()))
		return false;

	// Open cache file for writing...
	if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate))
		return false;

	// LV2 plugins are dang special,
	// need no out-of-process scanning whatsoever...
	if (m_typeHint == qtractorPluginType::Lv2) {
		m_iExitStatus = 0;
		return true;
	}

	// Go go go...
	return start();
}


// Close/stop method.
void qtractorPluginFactory::Scanner::close (void)
{
	// We're we scanning hard?...
	if (QProcess::state() != QProcess::NotRunning || m_iExitStatus < 0) {
		QProcess::closeWriteChannel();
		while (QProcess::state() != QProcess::NotRunning)
			QProcess::waitForFinished(200);
	}

	// Done anyway.
	QProcess::terminate();

	// Close cache file...
	if (m_file.isOpen())
		m_file.close();

	// Cleanup cache...
	m_list.clear();
}


// Scan start method.
bool qtractorPluginFactory::Scanner::start (void)
{
	// Maybe we're still running, doh!
	if (QProcess::state() != QProcess::NotRunning)
		return false;

	// Start from scratch...
	m_iExitStatus = -1;

	// Get the main scanner executable...
	const QString sName("qtractor_plugin_scan");
	QString sLibPath = QApplication::applicationDirPath();
	QFileInfo fi(sLibPath, sName);
	if (!fi.isExecutable()) {
		sLibPath.remove(CONFIG_BINDIR);
		sLibPath.append(CONFIG_LIBDIR);
		sLibPath.append(QDir::separator());
		sLibPath.append(PACKAGE_TARNAME);
		fi = QFileInfo(sLibPath, sName);
	}

	if (!fi.isExecutable())
		return false;

	// Go go go!
	QProcess::start(fi.filePath(), QStringList());
	return true;
}


// Service slots.
void qtractorPluginFactory::Scanner::stdout_slot (void)
{
	qtractorPluginFactory *pPluginFactory
		= static_cast<qtractorPluginFactory *> (QObject::parent());
	if (pPluginFactory == nullptr)
		return;

	const QString sData(QProcess::readAllStandardOutput());
	addTypes(sData.split('\n'));
}


void qtractorPluginFactory::Scanner::stderr_slot (void)
{
	QTextStream(stderr) << QProcess::readAllStandardError();
}


void qtractorPluginFactory::Scanner::exit_slot (
	int exitCode, QProcess::ExitStatus exitStatus )
{
	if (m_iExitStatus < 0)
		m_iExitStatus = 0;

	if (exitCode || exitStatus != QProcess::NormalExit)
		++m_iExitStatus;
}


// Service methods.
bool qtractorPluginFactory::Scanner::addTypes (
	qtractorPluginType::Hint typeHint, const QString& sFilename )
{
	// See if it's already cached in...
	const QStringList& list = m_list.value(sFilename);
	if (!list.isEmpty() && (
	#ifdef CONFIG_LV2
		m_typeHint == qtractorPluginType::Lv2 ||
	#endif
		QFileInfo(sFilename).exists())) {
		return addTypes(list);
	}

	if (!m_bReset)
		return false;

	qtractorPluginFactory *pPluginFactory
		= static_cast<qtractorPluginFactory *> (QObject::parent());
	if (pPluginFactory == nullptr)
		return false;

#ifdef CONFIG_LV2
	// LV2 plugins are dang special...
	if (typeHint == qtractorPluginType::Lv2) {
		qtractorPluginType *pType
			= qtractorLv2PluginType::createType(sFilename);
		if (pType == nullptr)
			return false;
		if (pType->open()) {
			pPluginFactory->addType(pType);
			pType->close();
			// Cache out...
			if (m_file.isOpen()) {
				QTextStream sout(&m_file);
				sout << "LV2|";
				sout << pType->name() << '|';
				sout << pType->audioIns()   << ':' << pType->audioOuts()   << '|';
				sout << pType->midiIns()    << ':' << pType->midiOuts()    << '|';
				sout << pType->controlIns() << ':' << pType->controlOuts() << '|';
				QStringList flags;
				if (pType->isEditor())
					flags.append("GUI");
				if (pType->isConfigure())
					flags.append("EXT");
				if (pType->isRealtime())
					flags.append("RT");
				sout << flags.join(",") << '|';
				sout << sFilename << '|' << 0 << '|';
				sout << "0x" << QString::number(pType->uniqueID(), 16) << endl;
			}
			// Success.
			return true;
		} else {
			// Fail.
			delete pType;
			return false;
		}
	}
#endif

	// Add to temporary blacklist...
	QFile temp_file(pPluginFactory->blacklistTempFilePath());
	if (temp_file.exists())
		pPluginFactory->readBlacklist(temp_file);
	pPluginFactory->writeBlacklist(temp_file, QStringList() << sFilename);

	// Not cached, yet...
	const QString& sHint = qtractorPluginType::textFromHint(typeHint);
	const QString& sLine = sHint + ':' + sFilename + '\n';
	const QByteArray& data = sLine.toUtf8();

	bool bResult = (QProcess::write(data) == data.size());

	// Check for hideous scan crashes...
	if (!QProcess::waitForReadyRead(3000)) {
		if (m_iExitStatus > 0) {
			QProcess::waitForFinished(200);
			start(); // Restart the crashed scan...
			QProcess::waitForStarted(200);
			bResult = false;
		}
	}

	// If it reaches here safely, then there's
	// no use to temporary blacklist anymore...
	if (bResult)
		temp_file.remove();

	return bResult;
}


bool qtractorPluginFactory::Scanner::addTypes ( const QStringList& list )
{
	qtractorPluginFactory *pPluginFactory
		= static_cast<qtractorPluginFactory *> (QObject::parent());
	if (pPluginFactory == nullptr)
		return false;

	QStringListIterator iter(list);
	while (iter.hasNext()) {
		const QString& sText = iter.next().simplified();
		if (sText.isEmpty())
			continue;
		qtractorPluginType *pType = qtractorDummyPluginType::createType(sText);
		if (pType) {
			// Brand new type, add to inventory...
			pPluginFactory->addType(pType);
			// Cache in...
			if (m_bReset && m_file.isOpen())
				QTextStream(&m_file) << sText << endl;
			// Done.
		} else {
			// Possibly some mistake occurred...
			QTextStream(stderr) << sText << endl;
		}
	}

	return true;
}


// Absolute cache file path.
QString qtractorPluginFactory::Scanner::cacheFilePath (void) const
{
	const QString& sCacheName = "qtractor_"
		+ qtractorPluginType::textFromHint(m_typeHint).toLower()
		+ "_scan.cache";
	const QString& sCacheDir
	#if QT_VERSION < QT_VERSION_CHECK(5, 5, 0)
		= QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
	#else
		= QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
	#endif
	const QFileInfo fi(sCacheDir, sCacheName);
	const QDir& dir = fi.absoluteDir();
	if (!dir.exists())
		dir.mkpath(dir.path());
	return fi.absoluteFilePath();
}


//----------------------------------------------------------------------------
// qtractorDummyPluginType -- Dummy plugin type instance.
//

// Constructor.
qtractorDummyPluginType::qtractorDummyPluginType (
	const QString& sText, unsigned long iIndex, Hint typeHint )
	: qtractorPluginType(nullptr, iIndex, typeHint)
{
	const QStringList& props = sText.split('|');

	m_sName  = props.at(1);
	m_sLabel = m_sName.simplified().replace(QRegularExpression("[\\s|\\.|\\-]+"), "_");

	const QStringList& audios = props.at(2).split(':');
	m_iAudioIns  = audios.at(0).toUShort();
	m_iAudioOuts = audios.at(1).toUShort();

	const QStringList& midis = props.at(3).split(':');
	m_iMidiIns  = midis.at(0).toUShort();
	m_iMidiOuts = midis.at(1).toUShort();

	const QStringList& controls = props.at(4).split(':');
	m_iControlIns  = controls.at(0).toUShort();
	m_iControlOuts = controls.at(1).toUShort();

	const QStringList& flags = props.at(5).split(',');
	m_bEditor = flags.contains("GUI");
	m_bConfigure = flags.contains("EXT");
	m_bRealtime = flags.contains("RT");

	m_sFilename = props.at(6);

	bool bOk = false;
	QString sUniqueID = props.at(8);
	m_iUniqueID = qHash(sUniqueID.remove("0x").toULong(&bOk, 16));
}


// Must be overridden methods.
bool qtractorDummyPluginType::open (void)
{
	return true;
}


void qtractorDummyPluginType::close (void)
{
}


// Factory method (static)
qtractorDummyPluginType *qtractorDummyPluginType::createType (
	const QString& sText )
{
	// Sanity checks...
	const QStringList& props = sText.split('|');
	if (props.count() < 9)
		return nullptr;

	const Hint typeHint
		= qtractorPluginType::hintFromText(props.at(0));
	if (typeHint == Any)
		return nullptr;

	const unsigned long iIndex = props.at(7).toULong();
	return new qtractorDummyPluginType(sText, iIndex, typeHint);
}


// end of qtractorPluginFactory.cpp
