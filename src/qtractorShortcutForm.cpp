// qtractorShortcutForm.cpp
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

#include "qtractorShortcutForm.h"

#include "qtractorOptions.h"

#include <QAction>
#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>

#include <QPainter>
#include <QLineEdit>
#include <QToolButton>
#include <QKeyEvent>


//-------------------------------------------------------------------------
// qtractorShortcutTableItem

class qtractorShortcutTableItem : public QTableWidgetItem
{
public:

	// Constructors.
	qtractorShortcutTableItem(const QIcon& icon, const QString& sText)
		: QTableWidgetItem(icon, sText)
		{ setFlags(flags() & ~Qt::ItemIsEditable); }
	qtractorShortcutTableItem(const QString& sText)
		: QTableWidgetItem(sText)
		{ setFlags(flags() & ~Qt::ItemIsEditable); }
	qtractorShortcutTableItem(const QKeySequence& shortcut)
		: QTableWidgetItem(QString(shortcut)) {}
};


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEdit

class qtractorShortcutTableItemEdit : public QLineEdit
{
public:

	// Constructor.
	qtractorShortcutTableItemEdit(QWidget *pParent = NULL)
		: QLineEdit(pParent) {}

protected:

	// Shortcut key to text event translation.
	void keyPressEvent(QKeyEvent *pKeyEvent)
	{
		int iKey = pKeyEvent->key();
		Qt::KeyboardModifiers modifiers = pKeyEvent->modifiers();

		if (iKey == Qt::Key_Return && modifiers == Qt::NoModifier) {
			emit editingFinished();
			return;
		}
	
		if (iKey >= Qt::Key_Shift && iKey < Qt::Key_F1) {
			QLineEdit::keyPressEvent(pKeyEvent);
			return;
		}

		if (modifiers & Qt::ShiftModifier)
			iKey |= Qt::SHIFT;
		if (modifiers & Qt::ControlModifier)
			iKey |= Qt::CTRL;
		if (modifiers & Qt::AltModifier)
			iKey |= Qt::ALT;

		QLineEdit::setText(QString(QKeySequence(iKey)));
	}
};


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEditor

qtractorShortcutTableItemEditor::qtractorShortcutTableItemEditor (
	QWidget *pParent ) : QWidget(pParent)
{
	m_pLineEdit = new qtractorShortcutTableItemEdit(/*this*/);

	m_pToolButton = new QToolButton(/*this*/);
	m_pToolButton->setFixedWidth(18);
	m_pToolButton->setText("X");

	QHBoxLayout *pLayout = new QHBoxLayout();
	pLayout->setSpacing(0);
	pLayout->setMargin(0);
	pLayout->addWidget(m_pLineEdit);
	pLayout->addWidget(m_pToolButton);
	QWidget::setLayout(pLayout);
	
	QWidget::setFocusPolicy(Qt::StrongFocus);
	QWidget::setFocusProxy(m_pLineEdit);

	QObject::connect(m_pLineEdit,
		SIGNAL(editingFinished()),
		SLOT(finish()));
	QObject::connect(m_pToolButton,
		SIGNAL(clicked()),
		SLOT(clear()));
}


// Shortcut text accessors.
void qtractorShortcutTableItemEditor::setText ( const QString& sText )
{
	m_pLineEdit->setText(sText);
}


QString qtractorShortcutTableItemEditor::text (void) const
{
	return m_pLineEdit->text();
}


// Shortcut text clear/toggler.
void qtractorShortcutTableItemEditor::clear (void)
{
	if (m_pLineEdit->text() == m_sDefaultText)
		m_pLineEdit->clear();
	else
		m_pLineEdit->setText(m_sDefaultText);

	m_pLineEdit->setFocus();
}


// Shortcut text finish notification.
void qtractorShortcutTableItemEditor::finish (void)
{
	emit editingFinished();
}


//-------------------------------------------------------------------------
// qtractorShortcutTableItemDelegate

qtractorShortcutTableItemDelegate::qtractorShortcutTableItemDelegate (
	QTableWidget *pTableWidget )
	: QItemDelegate(pTableWidget), m_pTableWidget(pTableWidget)
{
}


// Overridden paint method.
void qtractorShortcutTableItemDelegate::paint ( QPainter *pPainter,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	// Special treatment for action icon+text...
	if (index.column() == 0) {
		QTableWidgetItem *pItem = m_pTableWidget->item(index.row(), 0);
		pPainter->save();
		if (option.state & QStyle::State_Selected) {
			const QPalette& pal = option.palette;
			pPainter->fillRect(option.rect, pal.highlight().color());
			pPainter->setPen(pal.highlightedText().color());
		}
		// Draw the icon...
		QRect rect = option.rect;
		const QSize& iconSize = m_pTableWidget->iconSize();
		pPainter->drawPixmap(1,
			rect.top() + ((rect.height() - iconSize.height()) >> 1),
			pItem->icon().pixmap(iconSize));
		// Draw the text...
		rect.setLeft(iconSize.width() + 2);
		pPainter->drawText(rect,
			Qt::TextShowMnemonic | Qt::AlignLeft | Qt::AlignVCenter,
			pItem->text());
		pPainter->restore();
	} else {
		// Others do as default...
		QItemDelegate::paint(pPainter, option, index);
	}
}


QWidget *qtractorShortcutTableItemDelegate::createEditor ( QWidget *pParent,
	const QStyleOptionViewItem& /*option*/, const QModelIndex& index ) const
{
	qtractorShortcutTableItemEditor *pItemEditor
		= new qtractorShortcutTableItemEditor(pParent);
	pItemEditor->setDefaultText(
		index.model()->data(index, Qt::DisplayRole).toString());
	QObject::connect(pItemEditor,
		SIGNAL(editingFinished()),
		SLOT(commitEditor()));
	return pItemEditor;
}


void qtractorShortcutTableItemDelegate::setEditorData ( QWidget *pEditor,
	const QModelIndex& index ) const
{
	qtractorShortcutTableItemEditor *pItemEditor
		= qobject_cast<qtractorShortcutTableItemEditor *> (pEditor);
	pItemEditor->setText(
		index.model()->data(index, Qt::DisplayRole).toString());
}


void qtractorShortcutTableItemDelegate::setModelData ( QWidget *pEditor,
	QAbstractItemModel *pModel, const QModelIndex& index ) const
{
	qtractorShortcutTableItemEditor *pItemEditor
		= qobject_cast<qtractorShortcutTableItemEditor *> (pEditor);
	pModel->setData(index, pItemEditor->text());
}


void qtractorShortcutTableItemDelegate::commitEditor (void)
{
	qtractorShortcutTableItemEditor *pItemEditor
		= qobject_cast<qtractorShortcutTableItemEditor *> (sender());
	emit commitData(pItemEditor);
	emit closeEditor(pItemEditor);
}


//-------------------------------------------------------------------------
// qtractorShortcutForm

qtractorShortcutForm::qtractorShortcutForm ( QList<QAction *> actions,
	QWidget *pParent ) : QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::ApplicationModal);

	m_iDirtyCount = 0;

//	m_ui.qtractorShortcutTable = new QTableWidget(0, 3, this);
	m_ui.ShortcutTable->setIconSize(QSize(16, 16));
	m_ui.ShortcutTable->setItemDelegate(
		new qtractorShortcutTableItemDelegate(m_ui.ShortcutTable));
//	m_ui.ShortcutTable->setSelectionMode(QAbstractItemView::SingleSelection);
//	m_ui.ShortcutTable->setSelectionBehavior(QAbstractItemView::SelectRows);
//	m_ui.ShortcutTable->setAlternatingRowColors(true);
//	m_ui.ShortcutTable->setSortingEnabled(true);

//	m_ui.ShortcutTable->setHorizontalHeaderLabels(
//		QStringList() << tr("Item") << tr("Description") << tr("Shortcut"));
	m_ui.ShortcutTable->horizontalHeader()->setStretchLastSection(true);
	m_ui.ShortcutTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
	m_ui.ShortcutTable->horizontalHeader()->resizeSection(0, 120);
	m_ui.ShortcutTable->horizontalHeader()->resizeSection(1, 260);

	int iRowHeight = m_ui.ShortcutTable->fontMetrics().height() + 4;
	m_ui.ShortcutTable->verticalHeader()->setDefaultSectionSize(iRowHeight);
	m_ui.ShortcutTable->verticalHeader()->hide();

	int iRow = 0;
	QListIterator<QAction *> iter(actions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		if (pAction->objectName().isEmpty())
			continue;
		m_ui.ShortcutTable->insertRow(iRow);
		m_ui.ShortcutTable->setItem(iRow, 0,
			new qtractorShortcutTableItem(pAction->icon(), pAction->text()));
		m_ui.ShortcutTable->setItem(iRow, 1,
			new qtractorShortcutTableItem(pAction->statusTip()));
		m_ui.ShortcutTable->setItem(iRow, 2,
			new qtractorShortcutTableItem(pAction->shortcut()));
		m_actions.append(pAction);
		++iRow;
	}

	// Restore last seen form position and extents...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->loadWidgetGeometry(this, true);
	
	QObject::connect(m_ui.ShortcutTable,
		SIGNAL(itemActivated(QTableWidgetItem *)),
		SLOT(actionActivated(QTableWidgetItem *)));

	QObject::connect(m_ui.ShortcutTable,
		SIGNAL(itemChanged(QTableWidgetItem *)),
		SLOT(actionChanged(QTableWidgetItem *)));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


qtractorShortcutForm::~qtractorShortcutForm (void)
{
	// Store form position and extents...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions)
		pOptions->saveWidgetGeometry(this, true);
}


void qtractorShortcutForm::actionActivated ( QTableWidgetItem *pItem )
{
	m_ui.ShortcutTable->editItem(m_ui.ShortcutTable->item(pItem->row(), 2));
}


void qtractorShortcutForm::actionChanged ( QTableWidgetItem *pItem )
{
	pItem->setText(QString(QKeySequence(pItem->text().trimmed())));
	++m_iDirtyCount;
}


void qtractorShortcutForm::accept (void)
{
	if (m_iDirtyCount > 0) {
		for (int iRow = 0; iRow < m_actions.count(); ++iRow) {
			QAction *pAction = m_actions[iRow];
			pAction->setShortcut(
				QKeySequence(m_ui.ShortcutTable->item(iRow, 2)->text()));
		}
	}

	QDialog::accept();
}


void qtractorShortcutForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"), // + " - " QTRACTOR_TITLE,
			tr("Keyboard shortcuts have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			accept();
			return;
		case QMessageBox::Discard:
			break;
		default:    // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// end of qtractorShortcutForm.cpp
