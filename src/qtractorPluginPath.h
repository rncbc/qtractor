// qtractorPluginPath.h
//
/****************************************************************************
   Copyright (C) 2005-2016, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorPluginPath_h
#define __qtractorPluginPath_h

#include "qtractorPlugin.h"

#include <QProcess>


// Forward decls.
class qtractorPluginPathProxy;


//----------------------------------------------------------------------------
// qtractorPluginPath -- Plugin path helper.
//

class qtractorPluginPath
{
public:

	// Constructor.
	qtractorPluginPath()
		: m_typeHint(qtractorPluginType::Any), m_pProxy(NULL) {}

	// Destructor.
	~qtractorPluginPath()
		{ close(); clear(); }

	// Plugin type hint accessors.
	void setTypeHint(qtractorPluginType::Hint typeHint)
		{ m_typeHint = typeHint; }
	qtractorPluginType::Hint typeHint() const
		{ return m_typeHint; }

	// Executive methods.
	int open();
	void close();

	// Plugin path resgistry.
	typedef QHash<qtractorPluginType::Hint, QStringList> Paths;

	// Plugin filenames list.
	const Paths& files() const { return m_files; }

	// Plugin files/types list.
	const QList<qtractorPluginType *>& types() const { return m_types; }

	// Type list reset method.
	void clear() { qDeleteAll(m_types); m_types.clear(); }

	// Global plugin-paths executive methods.
	static QStringList pluginPaths(qtractorPluginType::Hint typeHint);
	static void updatePluginPaths();

	// Plugin factory method.
	static qtractorPlugin *createPlugin(
		qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex,
		qtractorPluginType::Hint typeHint);

	// Plugin type listing.
	bool addTypes(qtractorPluginType::Hint typeHint, const QString& sFilename);

	void addType(qtractorPluginType *pType)
		{ m_types.append(pType); }

	// Proxy (out-of-process) accessor.
	qtractorPluginPathProxy *proxy() const { return m_pProxy; }

protected:

	// Recursive plugin path/file inventory method (return estimated count).
	int addFiles(qtractorPluginType::Hint typeHint, const QStringList& paths);
	int addFiles(qtractorPluginType::Hint typeHint, const QString& sPath);

private:

	// Instance variables.
	qtractorPluginType::Hint m_typeHint;

	// Internal plugin filenames list.
	Paths m_files;

	// Estimated total number of files.
	int m_iFileCount;

	// Internal plugin types list.
	QList<qtractorPluginType *> m_types;

	// Proxy (out-of-process) client.
	qtractorPluginPathProxy *m_pProxy;

	// Global plugin-paths.
	static Paths g_paths;
};


//----------------------------------------------------------------------------
// qtractorPluginPathProxy -- Plugin path proxy (out-of-process client).
//

class qtractorPluginPathProxy : public QProcess
{
	Q_OBJECT

public:

	// ctor.
	qtractorPluginPathProxy(
		qtractorPluginPath *pPath, QObject *pParent = NULL);

	// Start method.
	bool start();

	// Service methods.
	bool addTypes(qtractorPluginType::Hint typeHint, const QString& sFilename);

protected slots:

	// Service slots.
	void stdout_slot();
	void stderr_slot();

private:

	// Instance variables.
	qtractorPluginPath *m_pPath;
};


//----------------------------------------------------------------------------
// qtractorDummyPluginType -- Dummy plugin type instance.
//

class qtractorDummyPluginType : public qtractorPluginType
{
public:

	// Constructor.
	qtractorDummyPluginType(
		const QString& sText, unsigned long iIndex, Hint typeHint);

	// Must be overriden methods.
	bool open();
	void close();

	// Dummy plugin filename (virtual override).
	QString filename() const
		{ return m_sFilename; }

	// Factory method (static)
	static qtractorDummyPluginType *createType(const QString& sText);

private:

	// Instance variables.
	QString m_sFilename;
};


#endif  // __qtractorPluginPath_h

// end of qtractorPluginPath.h
