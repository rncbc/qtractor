// qtractorShortcutForm.h
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

#ifndef __qtractorShortcutForm_h
#define __qtractorShortcutForm_h

#include "ui_qtractorShortcutForm.h"


#include <QHash>
#include <QItemDelegate>
#include <QLineEdit>


class QAction;
class QToolButton;


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEdit

class qtractorShortcutTableItemEdit : public QLineEdit
{
	Q_OBJECT

public:

	// Constructor.
	qtractorShortcutTableItemEdit(QWidget *pParent = NULL)
		: QLineEdit(pParent) {}

signals:

	// Custom cancel signal.
	void editingCanceled();

protected:

	// Shortcut key to text event translation.
	void keyPressEvent(QKeyEvent *pKeyEvent);
};


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEditor

class qtractorShortcutTableItemEditor : public QWidget
{
	Q_OBJECT

public:

	// Constructor.
	qtractorShortcutTableItemEditor(QWidget *pParent = NULL);

	// Shortcut text accessors.
	void setText(const QString& sText);
	QString text() const;

	// Index model row accessors.
	void setIndex(const QModelIndex& index)
		{ m_index = index; }
	const QModelIndex& index() const
		{ return m_index; }

	// Default (initial) shortcut text accessors.
	void setDefaultText(const QString& sDefaultText)
		{ m_sDefaultText = sDefaultText; }
	const QString& defaultText() const
		{ return m_sDefaultText; }

signals:

	void editingFinished();
	void editingCanceled();

public slots:

	void clear();

protected slots:

	void finish();
	void cancel();

private:

	// Instance variables.
	qtractorShortcutTableItemEdit *m_pItemEdit;
	QToolButton *m_pToolButton;
	QString m_sDefaultText;
	QModelIndex m_index;
};


//-------------------------------------------------------------------------
// qtractorShortcutTableItemDelegate
class qtractorShortcutForm;

class qtractorShortcutTableItemDelegate : public QItemDelegate
{
	Q_OBJECT

public:

	// Constructor.
	qtractorShortcutTableItemDelegate(qtractorShortcutForm *pShortcutForm);

protected:

	void paint(QPainter *pPainter,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

	QWidget *createEditor(QWidget *pParent, 
		const QStyleOptionViewItem& option,
		const QModelIndex & index) const;

	void setEditorData(QWidget *pEditor,
		const QModelIndex &index) const;
	void setModelData(QWidget *pEditor,
		QAbstractItemModel *pModel,
		const QModelIndex& index) const;

protected slots:

	void commitEditor();

private:

	qtractorShortcutForm *m_pShortcutForm;
};


//-------------------------------------------------------------------------
// qtractorShortcutForm

class qtractorShortcutForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorShortcutForm(const QList<QAction *>& actions, QWidget *pParent = NULL);
	
	// Destructor.
	~qtractorShortcutForm();

	// Shortcut table widget accessor.
	QTableWidget *tableWidget() const;

	// Shortcut action finder & settler.
	bool commitEditor(qtractorShortcutTableItemEditor *pItemEditor);

protected slots:

	void actionActivated(QTableWidgetItem *);
	void actionChanged(QTableWidgetItem *);

	void accept();
	void reject();

private:

	// The Qt-designer UI struct...
	Ui::qtractorShortcutForm m_ui;

	QHash<QAction *, int> m_actions;
	QHash<QString, int> m_shortcuts;

	int m_iDirtyCount;
};


#endif	// __qtractorShortcutForm_h

// end of qtractorShortcutForm.h
