// qtractorPluginFactory.h
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

#ifndef __qtractorPluginFactory_h
#define __qtractorPluginFactory_h

#include "qtractorPlugin.h"

#include <QProcess>


// Forward decls.
class qtractorPluginFactoryProxy;


//----------------------------------------------------------------------------
// qtractorPluginFactory -- Plugin path helper.
//

class qtractorPluginFactory : public QObject
{
	Q_OBJECT

public:

	// Constructor.
	qtractorPluginFactory(QObject *pParent = NULL);

	// Destructor.
	~qtractorPluginFactory();

	// Plugin files/paths resgistry.
	typedef QHash<qtractorPluginType::Hint, QStringList> Paths;

	// Plugin type hint accessors.
	void setTypeHint(qtractorPluginType::Hint typeHint)
		{ m_typeHint = typeHint; }
	qtractorPluginType::Hint typeHint() const
		{ return m_typeHint; }

	// Executive methods.
	void scan();

	// Plugin types registry.
	typedef QList<qtractorPluginType *> Types;
	const Types& types() const { return m_types; }
	// Type register method.
	void addType(qtractorPluginType *pType) { m_types.append(pType); }

	// Type list reset method.
	void clear();

	// Global plugin-paths executive methods.
	QStringList pluginPaths(qtractorPluginType::Hint typeHint);
	void updatePluginPaths();

	// Plugin factory method.
	static qtractorPlugin *createPlugin(
		qtractorPluginList *pList,
		const QString& sFilename, unsigned long iIndex,
		qtractorPluginType::Hint typeHint);

	// Singleton instance accessor.
	static qtractorPluginFactory *getInstance();

signals:

	// Scan progress feedback.
	void scanned(int iPercent);

protected:

	// Recursive plugin path/file inventory method (return partial file count).
	int addFiles(qtractorPluginType::Hint typeHint, const QStringList& paths);
	int addFiles(qtractorPluginType::Hint typeHint, const QString& sPath);

	// Plugin type listing.
	bool addTypes(qtractorPluginType::Hint typeHint, const QString& sFilename);

	// Plugin scan reset method.
	void reset();

private:

	// Instance variables.
	qtractorPluginType::Hint m_typeHint;

	// Internal plugin-paths.
	Paths m_paths;

	// Internal plugin-paths.
	Paths m_files;

	// Internal plugin types list.
	Types m_types;

	// Proxy (out-of-process) client.
	qtractorPluginFactoryProxy *m_pProxy;

	// Pseudo-singleton instance.
	static qtractorPluginFactory *g_pPluginFactory;
};


//----------------------------------------------------------------------------
// qtractorPluginFactoryProxy -- Plugin path proxy (out-of-process client).
//

class qtractorPluginFactoryProxy : public QProcess
{
	Q_OBJECT

public:

	// ctor.
	qtractorPluginFactoryProxy(qtractorPluginFactory *pPluginFactory);

	// Start method.
	bool start();

	// Service methods.
	bool addTypes(qtractorPluginType::Hint typeHint, const QString& sFilename);

protected slots:

	// Service slots.
	void stdout_slot();
	void stderr_slot();
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


#endif  // __qtractorPluginFactory_h

// end of qtractorPluginFactory.h
