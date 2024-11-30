// qtractorPaletteForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2024, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPaletteForm.h"

#include "ui_qtractorPaletteForm.h"

#include "qtractorOptions.h"

#include <QMetaProperty>
#include <QToolButton>
#include <QHeaderView>
#include <QLabel>

#include <QHash>

#include <QPainter>
#include <QStyle>
#include <QStyleOption>

#include <QColorDialog>
#include <QFileDialog>
#include <QMessageBox>


// Local static consts.
static const char *ColorThemesGroup   = "/ColorThemes/";

static const char *PaletteEditorGroup = "/PaletteEditor/";
static const char *DefaultDirKey      = "DefaultDir";
static const char *ShowDetailsKey     = "ShowDetails";
static const char *DefaultSuffix      = "conf";


static struct
{
	const char *key;
	QPalette::ColorRole value;

} g_colorRoles[] = {

	{ "Window",          QPalette::Window          },
	{ "WindowText",      QPalette::WindowText      },
	{ "Button",          QPalette::Button          },
	{ "ButtonText",      QPalette::ButtonText      },
	{ "Light",           QPalette::Light           },
	{ "Midlight",        QPalette::Midlight        },
	{ "Dark",            QPalette::Dark            },
	{ "Mid",             QPalette::Mid             },
	{ "Text",            QPalette::Text            },
	{ "BrightText",      QPalette::BrightText      },
	{ "Base",            QPalette::Base            },
	{ "AlternateBase",   QPalette::AlternateBase   },
	{ "Shadow",          QPalette::Shadow          },
	{ "Highlight",       QPalette::Highlight       },
	{ "HighlightedText", QPalette::HighlightedText },
	{ "Link",            QPalette::Link            },
	{ "LinkVisited",     QPalette::LinkVisited     },
	{ "ToolTipBase",     QPalette::ToolTipBase     },
	{ "ToolTipText",     QPalette::ToolTipText     },
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
	{ "PlaceholderText", QPalette::PlaceholderText },
#endif
	{ "NoRole",          QPalette::NoRole          },

	{  nullptr,          QPalette::NoRole          }
};


//-------------------------------------------------------------------------
// qtractorPaletteForm

qtractorPaletteForm::qtractorPaletteForm ( QWidget *parent, const QPalette& pal )
	: QDialog(parent), p_ui(new Ui::qtractorPaletteForm), m_ui(*p_ui)
{
	m_ui.setupUi(this);

	m_settings = nullptr;
	m_owner = false;

	m_modelUpdated = false;
	m_paletteUpdated = false;
	m_dirtyCount = 0;
	m_dirtyTotal = 0;

	updateGenerateButton();

	m_paletteModel = new PaletteModel(this);
	m_ui.paletteView->setModel(m_paletteModel);
	ColorDelegate *delegate = new ColorDelegate(this);
	m_ui.paletteView->setItemDelegate(delegate);
	m_ui.paletteView->setEditTriggers(QAbstractItemView::AllEditTriggers);
//	m_ui.paletteView->setAlternatingRowColors(true);
	m_ui.paletteView->setSelectionBehavior(QAbstractItemView::SelectRows);
	m_ui.paletteView->setDragEnabled(true);
	m_ui.paletteView->setDropIndicatorShown(true);
	m_ui.paletteView->setRootIsDecorated(false);
	m_ui.paletteView->setColumnHidden(2, true);
	m_ui.paletteView->setColumnHidden(3, true);

	QObject::connect(m_ui.nameCombo,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(nameComboChanged(const QString&)));
	QObject::connect(m_ui.saveButton,
		SIGNAL(clicked()),
		SLOT(saveButtonClicked()));
	QObject::connect(m_ui.deleteButton,
		SIGNAL(clicked()),
		SLOT(deleteButtonClicked()));

	QObject::connect(m_ui.generateButton,
		SIGNAL(changed()),
		SLOT(generateButtonChanged()));
	QObject::connect(m_ui.resetButton,
		SIGNAL(clicked()),
		SLOT(resetButtonClicked()));
	QObject::connect(m_ui.detailsCheck,
		SIGNAL(clicked()),
		SLOT(detailsCheckClicked()));
	QObject::connect(m_ui.importButton,
		SIGNAL(clicked()),
		SLOT(importButtonClicked()));
	QObject::connect(m_ui.exportButton,
		SIGNAL(clicked()),
		SLOT(exportButtonClicked()));

	QObject::connect(m_paletteModel,
		SIGNAL(paletteChanged(const QPalette&)),
		SLOT(paletteChanged(const QPalette&)));

	QObject::connect(m_ui.dialogButtons,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.dialogButtons,
		SIGNAL(rejected()),
		SLOT(reject()));

	setPalette(pal, pal);

	QDialog::adjustSize();
}


qtractorPaletteForm::~qtractorPaletteForm (void)
{
	setSettings(nullptr);
}


void qtractorPaletteForm::setPalette ( const QPalette& pal )
{
	m_palette = pal;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	const uint mask = pal.resolveMask();
#else
	const uint mask = pal.resolve();
#endif
	for (int i = 0; g_colorRoles[i].key; ++i) {
		if ((mask & (1 << i)) == 0) {
			const QPalette::ColorRole cr = g_colorRoles[i].value;
			m_palette.setBrush(QPalette::Active, cr,
				m_parentPalette.brush(QPalette::Active, cr));
			m_palette.setBrush(QPalette::Inactive, cr,
				m_parentPalette.brush(QPalette::Inactive, cr));
			m_palette.setBrush(QPalette::Disabled, cr,
				m_parentPalette.brush(QPalette::Disabled, cr));
		}
	}
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	m_palette.setResolveMask(mask);
#else
	m_palette.resolve(mask);
#endif

	updateGenerateButton();

	m_paletteUpdated = true;
	if (!m_modelUpdated)
		m_paletteModel->setPalette(m_palette, m_parentPalette);
	m_paletteUpdated = false;
}


void qtractorPaletteForm::setPalette ( const QPalette& pal, const QPalette& parentPal )
{
	m_parentPalette = parentPal;

	setPalette(pal);
}


const QPalette& qtractorPaletteForm::palette (void) const
{
	return m_palette;
}


void qtractorPaletteForm::setSettings ( QSettings *settings, bool owner )
{
	if (m_settings && m_owner)
		delete m_settings;

	m_settings = settings;
	m_owner = owner;

	m_ui.detailsCheck->setChecked(isShowDetails());

	updateNamedPaletteList();
	updateDialogButtons();
}


QSettings *qtractorPaletteForm::settings (void) const
{
	return m_settings;
}


void qtractorPaletteForm::nameComboChanged ( const QString& name )
{
	if (m_dirtyCount > 0 && m_ui.nameCombo->findText(name) < 0) {
		updateDialogButtons();
	} else {
		resetButtonClicked();
		setPaletteName(name);
		++m_dirtyTotal;
	}
}


void qtractorPaletteForm::saveButtonClicked (void)
{
	const QString& name = m_ui.nameCombo->currentText();
	if (name.isEmpty())
		return;

	QString filename = namedPaletteConf(name);

	if (filename.isEmpty() || !QFileInfo(filename).isWritable()) {
		const QString& title
			= tr("Save Palette - %1").arg(QDialog::windowTitle());
		QStringList filters;
		filters.append(tr("Palette files (*.%1)").arg(DefaultSuffix));
		filters.append(tr("All files (*.*)"));
		QString dirname = defaultDir();
		if (!dirname.isEmpty())
			dirname.append(QDir::separator());
		dirname.append(paletteName() + '.' + DefaultSuffix);
		filename = QFileDialog::getSaveFileName(this,
			title, dirname, filters.join(";;"));
	}

	if (!filename.isEmpty()
		&& saveNamedPaletteConf(name, filename, m_palette)) {
		addNamedPaletteConf(name, filename);
		setPalette(m_palette, m_palette);
		updateNamedPaletteList();
		resetButtonClicked();
	}
}


void qtractorPaletteForm::deleteButtonClicked (void)
{
	const QString& name = m_ui.nameCombo->currentText();
	if (m_ui.nameCombo->findText(name) >= 0) {
		deleteNamedPaletteConf(name);
		updateNamedPaletteList();
		updateDialogButtons();
	}
}


void qtractorPaletteForm::generateButtonChanged (void)
{
	const QColor& color
		= m_ui.generateButton->brush().color();
	const QPalette& pal = QPalette(color);
	setPalette(pal);

	++m_dirtyCount;
	updateDialogButtons();
}


void qtractorPaletteForm::resetButtonClicked (void)
{
	const bool blocked = blockSignals(true);

	for (int i = 0; g_colorRoles[i].key; ++i) {
		const QPalette::ColorRole cr = g_colorRoles[i].value;
		const QModelIndex& index = m_paletteModel->index(cr, 0);
		m_paletteModel->setData(index, false, Qt::EditRole);
	}

	m_dirtyCount = 0;
	updateDialogButtons();

	blockSignals(blocked);
}


void qtractorPaletteForm::detailsCheckClicked (void)
{
	const int cw = (m_ui.paletteView->viewport()->width() >> 2);
	QHeaderView *header = m_ui.paletteView->header();
	header->resizeSection(0, cw);
	if (m_ui.detailsCheck->isChecked()) {
		m_ui.paletteView->setColumnHidden(2, false);
		m_ui.paletteView->setColumnHidden(3, false);
		header->resizeSection(1, cw);
		header->resizeSection(2, cw);
		header->resizeSection(3, cw);
		m_paletteModel->setGenerate(false);
	} else {
		m_ui.paletteView->setColumnHidden(2, true);
		m_ui.paletteView->setColumnHidden(3, true);
		header->resizeSection(1, cw * 3);
		m_paletteModel->setGenerate(true);
	}
}


void qtractorPaletteForm::importButtonClicked (void)
{
	const QString& title
		= tr("Import File - %1").arg(QDialog::windowTitle());

	QStringList filters;
	filters.append(tr("Palette files (*.%1)").arg(DefaultSuffix));
	filters.append(tr("All files (*.*)"));

	const QString& filename
		= QFileDialog::getOpenFileName(this,
			title, defaultDir(), filters.join(";;"));

	if (filename.isEmpty())
		return;

	QSettings conf(filename, QSettings::IniFormat);
	conf.beginGroup(ColorThemesGroup);
	const QStringList names = conf.childGroups();
	conf.endGroup();

	int imported = 0;
	QStringListIterator name_iter(names);
	while (name_iter.hasNext()) {
		const QString& name = name_iter.next();
		if (!name.isEmpty()) {
			addNamedPaletteConf(name, filename);
			setPaletteName(name);
			++imported;
		}
	}

	if (imported > 0) {
		updateNamedPaletteList();
		resetButtonClicked();
		setDefaultDir(QFileInfo(filename).absolutePath());
	} else {
		QMessageBox::warning(this,
			tr("Warning - %1").arg(QDialog::windowTitle()),
			tr("Could not import from file:\n\n"
			"%1\n\nSorry.").arg(filename));
	}
}


void qtractorPaletteForm::exportButtonClicked (void)
{
	const QString& title
		= tr("Export File - %1").arg(QDialog::windowTitle());

	QStringList filters;
	filters.append(tr("Palette files (*.%1)").arg(DefaultSuffix));
	filters.append(tr("All files (*.*)"));

	QString dirname = defaultDir();
	if (!dirname.isEmpty())
		dirname.append(QDir::separator());
	dirname.append(paletteName() + '.' + DefaultSuffix);

	const QString& filename
		= QFileDialog::getSaveFileName(this,
			title, dirname, filters.join(";;"));

	if (filename.isEmpty())
		return;

	const QFileInfo fi(filename);
	const QString& name = fi.baseName();
	if (saveNamedPaletteConf(name, filename, m_palette)) {
	//	addNamedPaletteConf(name, filename);
		setDefaultDir(fi.absolutePath());
	}
}


void qtractorPaletteForm::paletteChanged ( const QPalette& pal )
{
	m_modelUpdated = true;
	if (!m_paletteUpdated)
		setPalette(pal);
	m_modelUpdated = false;

	++m_dirtyCount;
	updateDialogButtons();
}


void qtractorPaletteForm::setPaletteName ( const QString& name )
{
	const bool blocked = m_ui.nameCombo->blockSignals(true);

	m_ui.nameCombo->setEditText(name);

	QPalette pal;

	if (namedPalette(m_settings, name, pal, true))
		setPalette(pal, pal);

	m_dirtyCount = 0;
	updateDialogButtons();

	m_ui.nameCombo->blockSignals(blocked);
}


QString qtractorPaletteForm::paletteName (void) const
{
	return m_ui.nameCombo->currentText();
}


void qtractorPaletteForm::updateNamedPaletteList (void)
{
	const bool blocked = m_ui.nameCombo->blockSignals(true);
	const QString old_name = m_ui.nameCombo->currentText();

	m_ui.nameCombo->clear();
	m_ui.nameCombo->insertItems(0, namedPaletteList());
//	m_ui.nameCombo->model()->sort(0);

	const int i = m_ui.nameCombo->findText(old_name);
	if (i >= 0)
		m_ui.nameCombo->setCurrentIndex(i);
	else
		m_ui.nameCombo->setEditText(old_name);

	m_ui.nameCombo->blockSignals(blocked);
}


void qtractorPaletteForm::updateGenerateButton (void)
{
	m_ui.generateButton->setBrush(
		m_palette.brush(QPalette::Active, QPalette::Button));
}


void qtractorPaletteForm::updateDialogButtons (void)
{
	const QString& name = m_ui.nameCombo->currentText();
	const QString& filename = namedPaletteConf(name);
	const int i = m_ui.nameCombo->findText(name);
	m_ui.saveButton->setEnabled(!name.isEmpty() && (m_dirtyCount > 0 || i < 0));
	m_ui.deleteButton->setEnabled(i >= 0);
	m_ui.resetButton->setEnabled(m_dirtyCount > 0);
	m_ui.exportButton->setEnabled(!name.isEmpty() || i >= 0);
	m_ui.dialogButtons->button(QDialogButtonBox::Ok)->setEnabled(i >= 0);
}


bool qtractorPaletteForm::namedPalette ( const QString& name, QPalette& pal ) const
{
	return namedPalette(m_settings, name, pal);
}


bool qtractorPaletteForm::namedPalette (
	QSettings *settings, const QString& name, QPalette& pal, bool fixup )
{
	int result = 0;

	if (!name.isEmpty()
		&& loadNamedPalette(settings, name, pal)) {
		++result;
	} else {
		const QString& filename
			= namedPaletteConf(settings, name);
		if (!filename.isEmpty()
			&& QFileInfo(filename).isReadable()
			&& loadNamedPaletteConf(name, filename, pal)) {
			++result;
		}
	}

	// Dark themes grayed/disabled color group fix...
	if (!fixup && pal.base().color().value() < 0x7f) {
		const QColor& color = pal.window().color();
		const int groups = int(QPalette::Active | QPalette::Inactive) + 1;
		for (int i = 0; i < groups; ++i) {
			const QPalette::ColorGroup cg = QPalette::ColorGroup(i);
			pal.setBrush(cg, QPalette::Light,    color.lighter(140));
			pal.setBrush(cg, QPalette::Midlight, color.lighter(100));
			pal.setBrush(cg, QPalette::Mid,      color.lighter(90));
			pal.setBrush(cg, QPalette::Dark,     color.darker(160));
			pal.setBrush(cg, QPalette::Shadow,   color.darker(180));
		}
		pal.setColorGroup(QPalette::Disabled,
			pal.windowText().color().darker(),
			pal.button(),
			pal.light(),
			pal.dark(),
			pal.mid(),
			pal.text().color().darker(),
			pal.text().color().lighter(),
			pal.base(),
			pal.window());
	#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
		pal.setColor(QPalette::Disabled,
			QPalette::Highlight, pal.mid().color());
		pal.setColor(QPalette::Disabled,
			QPalette::ButtonText, pal.mid().color());
	#endif
		++result;
	}

	return (result > 0);
}


QStringList qtractorPaletteForm::namedPaletteList (void) const
{
	return namedPaletteList(m_settings);
}


QStringList qtractorPaletteForm::namedPaletteList ( QSettings *settings )
{
	QStringList list;

	if (settings) {
		settings->beginGroup(ColorThemesGroup);
		list.append(settings->childKeys());
		list.append(settings->childGroups()); // legacy...
		settings->endGroup();
	}

	return list;
}


QString qtractorPaletteForm::namedPaletteConf ( const QString& name ) const
{
	return namedPaletteConf(m_settings, name);
}


QString qtractorPaletteForm::namedPaletteConf (
	QSettings *settings, const QString& name )
{
	QString ret;

	if (settings && !name.isEmpty()) {
		settings->beginGroup(ColorThemesGroup);
		ret = settings->value(name).toString();
		settings->endGroup();
	}

	return ret;
}


void qtractorPaletteForm::addNamedPaletteConf (
	const QString& name, const QString& filename )
{
	addNamedPaletteConf(m_settings, name, filename);

	++m_dirtyTotal;
}


void qtractorPaletteForm::addNamedPaletteConf (
	QSettings *settings, const QString& name, const QString& filename )
{
	if (settings) {
		settings->beginGroup(ColorThemesGroup);
		settings->remove(name); // remove legacy keys!
		settings->setValue(name, filename);
		settings->endGroup();
	}
}


void qtractorPaletteForm::deleteNamedPaletteConf ( const QString& name )
{
	if (m_settings) {
		m_settings->beginGroup(ColorThemesGroup);
		m_settings->remove(name);
		m_settings->endGroup();
		++m_dirtyTotal;
	}
}


bool qtractorPaletteForm::loadNamedPaletteConf (
	const QString& name, const QString& filename, QPalette& pal )
{
	QSettings conf(filename, QSettings::IniFormat);

	return loadNamedPalette(&conf, name, pal);
}


bool qtractorPaletteForm::saveNamedPaletteConf (
	const QString& name, const QString& filename, const QPalette& pal )
{
	QSettings conf(filename, QSettings::IniFormat);

	return saveNamedPalette(&conf, name, pal);
}


bool qtractorPaletteForm::loadNamedPalette (
	QSettings *settings, const QString& name, QPalette& pal )
{
	if (settings == nullptr)
		return false;

	int result = 0;

	settings->beginGroup(ColorThemesGroup);
	QStringListIterator name_iter(settings->childGroups());
	while (name_iter.hasNext() && !result) {
		const QString& name2 = name_iter.next();
		if (name2 == name) {
		#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			uint mask = pal.resolve();
		#endif
			settings->beginGroup(name + '/');
			QStringListIterator iter(settings->childKeys());
			while (iter.hasNext()) {
				const QString& key = iter.next();
				const QPalette::ColorRole cr
					= qtractorPaletteForm::colorRole(key);
				const QStringList& clist
					= settings->value(key).toStringList();
				if (clist.count() == 3) {
					pal.setColor(QPalette::Active,   cr, QColor(clist.at(0)));
					pal.setColor(QPalette::Inactive, cr, QColor(clist.at(1)));
					pal.setColor(QPalette::Disabled, cr, QColor(clist.at(2)));
				#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
					mask &= ~(1 << int(cr));
				#endif
					++result;
				}
			}
		#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
			pal.resolve(mask);
		#endif
			settings->endGroup();
		}
	}
	settings->endGroup();

	return (result > 0);
}


bool qtractorPaletteForm::saveNamedPalette (
	QSettings *settings, const QString& name, const QPalette& pal )
{
	if (settings == nullptr)
		return false;

	settings->beginGroup(ColorThemesGroup);
	settings->beginGroup(name + '/');
	for (int i = 0; g_colorRoles[i].key; ++i) {
		const QString& key
			= QString::fromLatin1(g_colorRoles[i].key);
		const QPalette::ColorRole cr
			= g_colorRoles[i].value;
		QStringList clist;
		clist.append(pal.color(QPalette::Active,   cr).name());
		clist.append(pal.color(QPalette::Inactive, cr).name());
		clist.append(pal.color(QPalette::Disabled, cr).name());
		settings->setValue(key, clist);
	}
	settings->endGroup();
	settings->endGroup();

	return true;
}


QPalette::ColorRole qtractorPaletteForm::colorRole ( const QString& name )
{
	static QHash<QString, QPalette::ColorRole> s_colorRoles;

	if (s_colorRoles.isEmpty()) {
		for (int i = 0; g_colorRoles[i].key; ++i) {
			const QString& key
				= QString::fromLatin1(g_colorRoles[i].key);
			const QPalette::ColorRole value
				= g_colorRoles[i].value;
			s_colorRoles.insert(key, value);
		}
	}

	return s_colorRoles.value(name, QPalette::NoRole);
}


bool qtractorPaletteForm::isDirty (void) const
{
	return (m_dirtyTotal > 0);
}


void qtractorPaletteForm::accept (void)
{
	setShowDetails(m_ui.detailsCheck->isChecked());

	if (m_dirtyCount > 0)
		saveButtonClicked();

	QDialog::accept();
}


void qtractorPaletteForm::reject (void)
{
	if (m_dirtyCount > 0) {
		const QString& name = paletteName();
		if (name.isEmpty()) {
			if (QMessageBox::warning(this,
				tr("Warning - %1").arg(QDialog::windowTitle()),
				tr("Some settings have been changed.\n\n"
				"Do you want to discard the changes?"),
				QMessageBox::Discard |
				QMessageBox::Cancel) == QMessageBox::Cancel)
				return;
		} else {
			switch (QMessageBox::warning(this,
				tr("Warning - %1").arg(QDialog::windowTitle()),
				tr("Some settings have been changed:\n\n"
				"\"%1\".\n\nDo you want to save the changes?")
				.arg(name),
				QMessageBox::Save |
				QMessageBox::Discard |
				QMessageBox::Cancel)) {
			case QMessageBox::Save:
				saveButtonClicked();
				// Fall thru...
			case QMessageBox::Discard:
				break;
			default: // Cancel...
				return;
			}
		}
	}

	QDialog::reject();
}


void qtractorPaletteForm::setDefaultDir ( const QString& dir )
{
	if (m_settings) {
		m_settings->beginGroup(PaletteEditorGroup);
		m_settings->setValue(DefaultDirKey, dir);
		m_settings->endGroup();
	}
}


QString qtractorPaletteForm::defaultDir (void) const
{
	QString dir;

	if (m_settings) {
		m_settings->beginGroup(PaletteEditorGroup);
		dir = m_settings->value(DefaultDirKey).toString();
		m_settings->endGroup();
	}

	return dir;
}


void qtractorPaletteForm::setShowDetails ( bool on )
{
	if (m_settings) {
		m_settings->beginGroup(PaletteEditorGroup);
		m_settings->setValue(ShowDetailsKey, on);
		m_settings->endGroup();
	}
}


bool qtractorPaletteForm::isShowDetails (void) const
{
	bool on = false;

	if (m_settings) {
		m_settings->beginGroup(PaletteEditorGroup);
		on = m_settings->value(ShowDetailsKey).toBool();
		m_settings->endGroup();
	}

	return on;
}


void qtractorPaletteForm::showEvent ( QShowEvent *event )
{
	QDialog::showEvent(event);

	detailsCheckClicked();
}


void qtractorPaletteForm::resizeEvent ( QResizeEvent *event )
{
	QDialog::resizeEvent(event);

	detailsCheckClicked();
}


//-------------------------------------------------------------------------
// qtractorPaletteForm::PaletteModel

qtractorPaletteForm::PaletteModel::PaletteModel ( QObject *parent )
	: QAbstractTableModel(parent)
{
	for (m_nrows = 0; g_colorRoles[m_nrows].key; ++m_nrows) {
		const QPalette::ColorRole value
			= g_colorRoles[m_nrows].value;
		const QString& key
			= QString::fromLatin1(g_colorRoles[m_nrows].key);
		m_roleNames.insert(value, key);
	}

	m_generate = true;
}


int qtractorPaletteForm::PaletteModel::rowCount ( const QModelIndex& ) const
{
	return m_nrows;
}


int qtractorPaletteForm::PaletteModel::columnCount ( const QModelIndex& ) const
{
	return 4;
}


QVariant qtractorPaletteForm::PaletteModel::data ( const QModelIndex& index, int role ) const
{
	if (!index.isValid())
		return QVariant();
	if (index.row() < 0 || index.row() >= m_nrows)
		return QVariant();
	if (index.column() < 0 || index.column() >= 4)
		return QVariant();

	if (index.column() == 0) {
		if (role == Qt::DisplayRole)
			return m_roleNames.value(QPalette::ColorRole(index.row()));
		if (role == Qt::EditRole) {
		#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
			const uint mask = m_palette.resolveMask();
		#else
			const uint mask = m_palette.resolve();
		#endif
			return bool(mask & (1 << index.row()));
		}
	}
	else
	if (role == Qt::BackgroundRole) {
		return m_palette.color(
			columnToGroup(index.column()),
			QPalette::ColorRole(index.row()));
	}

	return QVariant();
}


bool qtractorPaletteForm::PaletteModel::setData (
	const QModelIndex& index, const QVariant& value, int role )
{
	if (!index.isValid())
		return false;

	if (index.column() != 0 && role == Qt::BackgroundRole) {
		const QColor& color = value.value<QColor>();
		const QPalette::ColorRole cr = QPalette::ColorRole(index.row());
		const QPalette::ColorGroup cg = columnToGroup(index.column());
		m_palette.setBrush(cg, cr, color);
		QModelIndex index_begin = PaletteModel::index(cr, 0);
		QModelIndex index_end = PaletteModel::index(cr, 3);
		if (m_generate) {
			m_palette.setBrush(QPalette::Inactive, cr, color);
			switch (cr) {
				case QPalette::WindowText:
				case QPalette::Text:
				case QPalette::ButtonText:
				case QPalette::Base:
					break;
				case QPalette::Dark:
					m_palette.setBrush(QPalette::Disabled, QPalette::WindowText, color);
					m_palette.setBrush(QPalette::Disabled, QPalette::Dark, color);
					m_palette.setBrush(QPalette::Disabled, QPalette::Text, color);
					m_palette.setBrush(QPalette::Disabled, QPalette::ButtonText, color);
					index_begin = PaletteModel::index(0, 0);
					index_end = PaletteModel::index(m_nrows - 1, 3);
					break;
				case QPalette::Window:
					m_palette.setBrush(QPalette::Disabled, QPalette::Base, color);
					m_palette.setBrush(QPalette::Disabled, QPalette::Window, color);
					index_begin = PaletteModel::index(QPalette::Base, 0);
					break;
				case QPalette::Highlight:
					m_palette.setBrush(QPalette::Disabled, QPalette::Highlight, color.darker(120));
					break;
				default:
					m_palette.setBrush(QPalette::Disabled, cr, color);
					break;
			}
		}
		emit paletteChanged(m_palette);
		emit dataChanged(index_begin, index_end);
		return true;
	}

	if (index.column() == 0 && role == Qt::EditRole) {
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		uint mask = m_palette.resolveMask();
	#else
		uint mask = m_palette.resolve();
	#endif
		const bool masked = value.value<bool>();
		const int i = index.row();
		if (masked) {
			mask |= (1 << i);
		} else {
			const QPalette::ColorRole cr = QPalette::ColorRole(i);
			m_palette.setBrush(QPalette::Active, cr,
				m_parentPalette.brush(QPalette::Active, cr));
			m_palette.setBrush(QPalette::Inactive, cr,
				m_parentPalette.brush(QPalette::Inactive, cr));
			m_palette.setBrush(QPalette::Disabled, cr,
				m_parentPalette.brush(QPalette::Disabled, cr));
			mask &= ~(1 << i);
		}
	#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
		m_palette.setResolveMask(mask);
	#else
		m_palette.resolve(mask);
	#endif
		emit paletteChanged(m_palette);
		const QModelIndex& index_end = PaletteModel::index(i, 3);
		emit dataChanged(index, index_end);
		return true;
	}

	return false;
}


Qt::ItemFlags qtractorPaletteForm::PaletteModel::flags ( const QModelIndex& index ) const
{
	if (!index.isValid())
		return Qt::ItemIsEnabled;
	else
		return Qt::ItemIsEditable | Qt::ItemIsEnabled;
}


QVariant qtractorPaletteForm::PaletteModel::headerData (
	int section, Qt::Orientation orientation, int role ) const
{
	if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
		if (section == 0)
			return tr("Color Role");
		else
		if (section == groupToColumn(QPalette::Active))
			return tr("Active");
		else
		if (section == groupToColumn(QPalette::Inactive))
			return tr("Inactive");
		else
		if (section == groupToColumn(QPalette::Disabled))
			return tr("Disabled");
	}

	return QVariant();
}


const QPalette& qtractorPaletteForm::PaletteModel::palette(void) const
{
	return m_palette;
}


void qtractorPaletteForm::PaletteModel::setPalette (
	const QPalette& palette, const QPalette& parentPalette )
{
	m_palette = palette;
	m_parentPalette = parentPalette;

	const QModelIndex& index_begin = index(0, 0);
	const QModelIndex& index_end = index(m_nrows - 1, 3);
	emit dataChanged(index_begin, index_end);
}


QPalette::ColorGroup qtractorPaletteForm::PaletteModel::columnToGroup ( int index ) const
{
	if (index == 1)
		return QPalette::Active;
	else
	if (index == 2)
		return QPalette::Inactive;

	return QPalette::Disabled;
}


int qtractorPaletteForm::PaletteModel::groupToColumn ( QPalette::ColorGroup group ) const
{
	if (group == QPalette::Active)
		return 1;
	else
	if (group == QPalette::Inactive)
		return 2;

	return 3;
}


//-------------------------------------------------------------------------
// qtractorPaletteForm::ColorDelegate

QWidget *qtractorPaletteForm::ColorDelegate::createEditor ( QWidget *parent,
	const QStyleOptionViewItem&, const QModelIndex& index ) const
{
	QWidget *editor = nullptr;

	if (index.column() == 0) {
		RoleEditor *ed = new RoleEditor(parent);
		QObject::connect(ed,
			SIGNAL(changed(QWidget *)),
			SIGNAL(commitData(QWidget *)));
	//	ed->setFocusPolicy(Qt::NoFocus);
	//	ed->installEventFilter(const_cast<ColorDelegate *>(this));
		editor = ed;
	} else {
		ColorEditor *ed = new ColorEditor(parent);
		QObject::connect(ed,
			SIGNAL(changed(QWidget *)),
			SIGNAL(commitData(QWidget *)));
		ed->setFocusPolicy(Qt::NoFocus);
		ed->installEventFilter(const_cast<ColorDelegate *>(this));
		editor = ed;
	}

	return editor;
}


void qtractorPaletteForm::ColorDelegate::setEditorData (
	QWidget *editor, const QModelIndex& index ) const
{
	if (index.column() == 0) {
		const bool masked
			= index.model()->data(index, Qt::EditRole).value<bool>();
		RoleEditor *ed = static_cast<RoleEditor *>(editor);
		ed->setEdited(masked);
		const QString& colorName
			= index.model()->data(index, Qt::DisplayRole).value<QString>();
		ed->setLabel(colorName);
	} else {
		const QColor& color
			= index.model()->data(index, Qt::BackgroundRole).value<QColor>();
		ColorEditor *ed = static_cast<ColorEditor *>(editor);
		ed->setColor(color);
	}
}


void qtractorPaletteForm::ColorDelegate::setModelData ( QWidget *editor,
	QAbstractItemModel *model, const QModelIndex& index ) const
{
	if (index.column() == 0) {
		RoleEditor *ed = static_cast<RoleEditor *>(editor);
		const bool masked = ed->edited();
		model->setData(index, masked, Qt::EditRole);
	} else {
		ColorEditor *ed = static_cast<ColorEditor *>(editor);
		if (ed->changed()) {
			const QColor& color = ed->color();
			model->setData(index, color, Qt::BackgroundRole);
		}
	}
}


void qtractorPaletteForm::ColorDelegate::updateEditorGeometry ( QWidget *editor,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	QItemDelegate::updateEditorGeometry(editor, option, index);
	editor->setGeometry(editor->geometry().adjusted(0, 0, -1, -1));
}


void qtractorPaletteForm::ColorDelegate::paint ( QPainter *painter,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	QStyleOptionViewItem opt = option;

	const bool masked
		= index.model()->data(index, Qt::EditRole).value<bool>();
	if (index.column() == 0 && masked)
		opt.font.setBold(true);

	QItemDelegate::paint(painter, opt, index);

//	painter->setPen(opt.palette.midlight().color());
	painter->setPen(Qt::darkGray);
	painter->drawLine(opt.rect.right(), opt.rect.y(),
		opt.rect.right(), opt.rect.bottom());
	painter->drawLine(opt.rect.x(), opt.rect.bottom(),
		opt.rect.right(), opt.rect.bottom());
}


QSize qtractorPaletteForm::ColorDelegate::sizeHint (
	const QStyleOptionViewItem& option, const QModelIndex &index) const
{
	return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
}


//-------------------------------------------------------------------------
// qtractorPaletteForm::ColorButton

qtractorPaletteForm::ColorButton::ColorButton ( QWidget *parent )
	: QPushButton(parent), m_brush(Qt::darkGray)
{
	QPushButton::setMinimumWidth(48);

	QObject::connect(this,
		SIGNAL(clicked()),
		SLOT(chooseColor()));
}


const QBrush& qtractorPaletteForm::ColorButton::brush (void) const
{
	return m_brush;
}


void qtractorPaletteForm::ColorButton::setBrush ( const QBrush& brush )
{
	m_brush = brush;
	update();
}


void qtractorPaletteForm::ColorButton::paintEvent ( QPaintEvent *event )
{
	QPushButton::paintEvent(event);

	QStyleOptionButton opt;
	opt.initFrom(this);

	const QRect& rect
		= style()->subElementRect(QStyle::SE_PushButtonContents, &opt, this);

	QPainter paint(this);
	paint.setBrush(QBrush(m_brush.color()));
	paint.drawRect(rect.adjusted(+1, +1, -2, -2));
}


void qtractorPaletteForm::ColorButton::chooseColor (void)
{
	QWidget *pParentWidget = nullptr;
	QColorDialog::ColorDialogOptions options;
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions && pOptions->bDontUseNativeDialogs) {
		options |= QColorDialog::DontUseNativeDialog;
		pParentWidget = QWidget::window();
	}

	const QColor& color	= QColorDialog::getColor(
		m_brush.color(), pParentWidget, QString(), options);

	if (color.isValid()) {
		m_brush.setColor(color);
		emit changed();
	}
}


//-------------------------------------------------------------------------
// qtractorPaletteForm::ColorEditor

qtractorPaletteForm::ColorEditor::ColorEditor ( QWidget *parent )
	: QWidget(parent)
{
	QLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	m_button = new qtractorPaletteForm::ColorButton(this);
	layout->addWidget(m_button);
	QObject::connect(m_button,
		SIGNAL(changed()),
		SLOT(colorChanged()));
	setFocusProxy(m_button);
	m_changed = false;
}


void qtractorPaletteForm::ColorEditor::setColor ( const QColor& color )
{
	m_button->setBrush(color);
	m_changed = false;
}


QColor qtractorPaletteForm::ColorEditor::color (void) const
{
	return m_button->brush().color();
}


void qtractorPaletteForm::ColorEditor::colorChanged (void)
{
	m_changed = true;
	emit changed(this);
}


bool qtractorPaletteForm::ColorEditor::changed (void) const
{
	return m_changed;
}


//-------------------------------------------------------------------------
// qtractorPaletteForm::RoleEditor

qtractorPaletteForm::RoleEditor::RoleEditor ( QWidget *parent )
	: QWidget(parent)
{
	m_edited = false;

	QHBoxLayout *layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);

	m_label = new QLabel(this);
	layout->addWidget(m_label);
	m_label->setAutoFillBackground(true);
	m_label->setIndent(3); // HACK: it should have the same value of textMargin in QItemDelegate
	setFocusProxy(m_label);

	m_button = new QToolButton(this);
	m_button->setToolButtonStyle(Qt::ToolButtonIconOnly);
	m_button->setIcon(QIcon::fromTheme("itemReset"));
	m_button->setIconSize(QSize(8, 8));
	m_button->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
	layout->addWidget(m_button);

	QObject::connect(m_button,
		SIGNAL(clicked()),
		SLOT(resetProperty()));
}


void qtractorPaletteForm::RoleEditor::setLabel ( const QString& label )
{
	m_label->setText(label);
}


void qtractorPaletteForm::RoleEditor::setEdited ( bool on )
{
	QFont font;
	if (on)
		font.setBold(on);
	m_label->setFont(font);
	m_button->setEnabled(on);
	m_edited = on;
}


bool qtractorPaletteForm::RoleEditor::edited (void) const
{
	return m_edited;
}


void qtractorPaletteForm::RoleEditor::resetProperty (void)
{
	setEdited(false);
	emit changed(this);
}


// end of qtractorPaletteForm.cpp
