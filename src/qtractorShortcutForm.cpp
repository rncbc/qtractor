// qtractorShortcutForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2025, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorShortcutForm.h"

#include "qtractorMidiControlObserverForm.h"

#include "qtractorOptions.h"

#include <QAction>
#include <QMenu>

#include <QMessageBox>
#include <QPushButton>
#include <QHeaderView>

#include <QPainter>
#include <QLineEdit>
#include <QToolButton>
#include <QKeyEvent>


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEdit

// Shortcut key to text event translation.
void qtractorShortcutTableItemEdit::keyPressEvent ( QKeyEvent *pKeyEvent )
{
	const Qt::KeyboardModifiers modifiers
		= pKeyEvent->modifiers();

	int iKey = pKeyEvent->key();

	if (modifiers == Qt::NoModifier) {
		switch (iKey) {
		case Qt::Key_Return:
			emit editingFinished();
			return;
		case Qt::Key_Escape:
			emit editingCanceled();
			return;
		}
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
	if (modifiers & Qt::MetaModifier)
		iKey |= Qt::META;

	QLineEdit::setText(QKeySequence(iKey).toString());
}


//-------------------------------------------------------------------------
// qtractorShortcutTableItemEditor

qtractorShortcutTableItemEditor::qtractorShortcutTableItemEditor (
	QWidget *pParent ) : QWidget(pParent)
{
	m_pItemEdit = new qtractorShortcutTableItemEdit(/*this*/);
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helper...
	m_pItemEdit->setClearButtonEnabled(true);
#endif
	m_pToolButton = new QToolButton(/*this*/);
	m_pToolButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
//	m_pToolButton->setIconSize(QSize(18, 18));
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	m_pToolButton->setIcon(QIcon::fromTheme("itemReset"));
#else
	m_pToolButton->setIcon(QIcon::fromTheme("itemClear"));
#endif

	QHBoxLayout *pLayout = new QHBoxLayout();
	pLayout->setSpacing(0);
	pLayout->setContentsMargins(0, 0, 0, 0);
	pLayout->addWidget(m_pItemEdit);
	pLayout->addWidget(m_pToolButton);
	QWidget::setLayout(pLayout);
	
	QWidget::setFocusPolicy(Qt::StrongFocus);
	QWidget::setFocusProxy(m_pItemEdit);

	QObject::connect(m_pItemEdit,
		SIGNAL(editingFinished()),
		SLOT(finish()));
	QObject::connect(m_pItemEdit,
		SIGNAL(editingCanceled()),
		SLOT(cancel()));
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	QObject::connect(m_pItemEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(changed(const QString&)));
#endif
	QObject::connect(m_pToolButton,
		SIGNAL(clicked()),
		SLOT(clear()));
}


// Shortcut text accessors.
void qtractorShortcutTableItemEditor::setText ( const QString& sText )
{
	m_pItemEdit->setText(sText);
}


QString qtractorShortcutTableItemEditor::text (void) const
{
	return m_pItemEdit->text();
}


// Default (initial) shortcut text accessors.
void qtractorShortcutTableItemEditor::setDefaultText ( const QString& sDefaultText )
{
	m_sDefaultText = sDefaultText;

	changed(text());
}

const QString& qtractorShortcutTableItemEditor::defaultText(void) const
{
	return m_sDefaultText;
}


// Shortcut text clear/toggler.
void qtractorShortcutTableItemEditor::clear (void)
{
	if (m_pItemEdit->text() == m_sDefaultText)
		m_pItemEdit->clear();
	else
		m_pItemEdit->setText(m_sDefaultText);

	m_pItemEdit->setFocus();
}


// Shortcut text finish notification.
void qtractorShortcutTableItemEditor::finish (void)
{
	const bool bBlockSignals = m_pItemEdit->blockSignals(true);
	emit editingFinished();
	m_index = QModelIndex();
	m_sDefaultText.clear();
	m_pItemEdit->blockSignals(bBlockSignals);
}


// Shortcut text cancel notification.
void qtractorShortcutTableItemEditor::cancel (void)
{
	const bool bBlockSignals = m_pItemEdit->blockSignals(true);
	m_index = QModelIndex();
	m_sDefaultText.clear();
	emit editingFinished();
	m_pItemEdit->blockSignals(bBlockSignals);
}


// Shortcut text change notification.
void qtractorShortcutTableItemEditor::changed ( const QString& )
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	if (m_sDefaultText.isEmpty()) {
		m_pToolButton->setVisible(false);
		m_pToolButton->setEnabled(false);
	} else {
		m_pToolButton->setVisible(true);
		m_pToolButton->setEnabled(m_sDefaultText != text());
	}
#endif
}


//-------------------------------------------------------------------------
// qtractorShortcutTableItemDelegate

qtractorShortcutTableItemDelegate::qtractorShortcutTableItemDelegate (
	qtractorShortcutForm *pShortcutForm )
	: QItemDelegate(pShortcutForm->tableWidget()),
		m_pShortcutForm(pShortcutForm)
{
}


// Overridden paint method.
void qtractorShortcutTableItemDelegate::paint ( QPainter *pPainter,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	// Special treatment for action icon+text...
	if (index.column() == 0) {
		pPainter->save();
		if (option.state & QStyle::State_Selected) {
			const QPalette& pal = option.palette;
			pPainter->fillRect(option.rect, pal.highlight().color());
			pPainter->setPen(pal.highlightedText().color());
		}
		// Draw the icon...
		QRect rect = option.rect;
		const QSize iconSize(16, 16);
		const QIcon& icon
			= index.model()->data(index, Qt::DecorationRole).value<QIcon>();
		pPainter->drawPixmap(1,
			rect.top() + ((rect.height() - iconSize.height()) >> 1),
			icon.pixmap(iconSize));
		// Draw the text...
		rect.setLeft(iconSize.width() + 2);
		pPainter->drawText(rect,
			Qt::TextShowMnemonic | Qt::AlignLeft | Qt::AlignVCenter,
			index.model()->data(index, Qt::DisplayRole).toString());
		pPainter->restore();
	} else {
		// Others do as default...
		QItemDelegate::paint(pPainter, option, index);
	}
}


QWidget *qtractorShortcutTableItemDelegate::createEditor ( QWidget *pParent,
	const QStyleOptionViewItem& /*option*/, const QModelIndex& index ) const
{
	// Keyboard shortcut.
	qtractorShortcutTableItemEditor *pItemEditor
		= new qtractorShortcutTableItemEditor(pParent);
	pItemEditor->setIndex(index);
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

	if (m_pShortcutForm->commitEditor(pItemEditor))
		emit commitData(pItemEditor);

	emit closeEditor(pItemEditor);
}


QSize qtractorShortcutTableItemDelegate::sizeHint (
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
}


//-------------------------------------------------------------------------
// qtractorShortcutForm

// Constructor.
qtractorShortcutForm::qtractorShortcutForm (
	const QList<QAction *>& actions, QWidget *pParent ) : QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	//QDialog::setWindowModality(Qt::ApplicationModal);

	m_pActionControl = nullptr;

	m_pActionControlItem = nullptr;

	m_actions = actions;

	QListIterator<QAction *> iter(m_actions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		if (pAction->objectName().isEmpty())
			continue;
		const QKeySequence& shortcut = pAction->shortcut();
		const QString& sShortcutText = shortcut.toString();
		if (!sShortcutText.isEmpty())
			m_shortcuts.insert(sShortcutText, pAction);
	}

//	m_ui.ShortcutTable->setIconSize(QSize(16, 16));
	m_ui.ShortcutTable->setItemDelegate(
		new qtractorShortcutTableItemDelegate(this));

	QHeaderView *pHeaderView = m_ui.ShortcutTable->header();
	pHeaderView->setStretchLastSection(true);
	pHeaderView->setDefaultAlignment(Qt::AlignLeft);
	pHeaderView->resizeSection(0, 180);
	pHeaderView->resizeSection(1, 320);
//	pHeaderView->hideSection(3);

	// Custom context menu...
	m_ui.ShortcutTable->setContextMenuPolicy(Qt::CustomContextMenu);

	// Restore last seen form position and extents...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->loadWidgetGeometry(this, true);
		pOptions->loadComboBoxHistory(m_ui.ShortcutSearchComboBox);
		m_ui.ShortcutSearchComboBox->setEditText(QString());
	}

#if QT_VERSION >= QT_VERSION_CHECK(5, 2, 0)
	// Some conveniency cleaner helper...
	m_ui.ShortcutSearchComboBox->lineEdit()->setClearButtonEnabled(true);
	m_ui.ShortcutSearchComboBox->lineEdit()->setPlaceholderText(
		tr("Search shortcuts"));
#endif

	QObject::connect(m_ui.ShortcutSearchComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(refresh()));

	QObject::connect(m_ui.ShortcutTable,
		SIGNAL(itemDoubleClicked(QTreeWidgetItem *, int)),
		SLOT(actionShortcutActivated(QTreeWidgetItem *, int)));

	QObject::connect(m_ui.ShortcutTable,
		SIGNAL(itemChanged(QTreeWidgetItem *, int)),
		SLOT(actionShortcutChanged(QTreeWidgetItem *, int)));

	QObject::connect(m_ui.ShortcutTable,
		SIGNAL(customContextMenuRequested(const QPoint&)),
		SLOT(actionControlMenuRequested(const QPoint&)));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));

	refresh();
}


qtractorShortcutForm::~qtractorShortcutForm (void)
{
	// Store form position and extents...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		pOptions->saveComboBoxHistory(m_ui.ShortcutSearchComboBox);
		pOptions->saveWidgetGeometry(this, true);
	}
}


// Action shortcut/control table widget accessor.
QTreeWidget *qtractorShortcutForm::tableWidget (void) const
{
	return m_ui.ShortcutTable;
}


// MIDI Controller manager accessor.
void qtractorShortcutForm::setActionControl ( qtractorActionControl *pActionControl )
{
	m_pActionControl = pActionControl;

	QHeaderView *pHeaderView = m_ui.ShortcutTable->header();
	if (m_pActionControl) {
		pHeaderView->showSection(3);
	} else {
		pHeaderView->hideSection(3);
	}

	refresh();
}

qtractorActionControl *qtractorShortcutForm::actionControl (void) const
{
	return m_pActionControl;
}


// Action shortcut/control dirty-flag accessors.
bool qtractorShortcutForm::isDirtyActionShortcuts (void) const
{
	return !m_dirty_shortcuts.isEmpty();
}

bool qtractorShortcutForm::isDirtyActionControls (void) const
{
	return !m_dirty_controls.isEmpty();;
}


// Shortcut action finder & settler.
bool qtractorShortcutForm::commitEditor (
	qtractorShortcutTableItemEditor *pItemEditor )
{
	const QModelIndex& index = pItemEditor->index();
	if (!index.isValid())
		return false;

	const QString& sShortcutText = pItemEditor->text();
	const QString& sDefaultText = pItemEditor->defaultText();

	if (sShortcutText == sDefaultText)
		return false;

	QTreeWidgetItem *pItem = nullptr;

	if (!sShortcutText.isEmpty()) {
		QAction *pAction = m_shortcuts.value(sShortcutText, nullptr);
		if (pAction)
			pItem = m_action_items.value(pAction, nullptr);
		if (pItem) {
			QMessageBox::warning(this,
				tr("Warning"),
				tr("Keyboard shortcut (%1) already assigned (%2).")
					.arg(sShortcutText)
					.arg(pItem->text(0).remove('&')),
				QMessageBox::Cancel);
			pItemEditor->clear();
			return false;
		}
	}

	pItem = m_ui.ShortcutTable->topLevelItem(index.row());
	if (pItem) {
		QAction *pAction = m_item_actions.value(pItem, nullptr);
		if (pAction)
			m_dirty_shortcuts.insert(pAction, sShortcutText);
	}

	return true;
}


void qtractorShortcutForm::actionShortcutActivated (
	QTreeWidgetItem *pItem, int iColumn )
{
	switch (iColumn) {
	case 3:
		// MIDI Controller...
		actionControlActivated();
		break;
	case 2:
	default:
		// Keyboard shortcut...
		m_ui.ShortcutTable->editItem(pItem, 2);
		break;
	}
}


void qtractorShortcutForm::actionShortcutChanged (
	QTreeWidgetItem *pItem, int iColumn )
{
	if (iColumn == 2) {
		const QString& sShortcutText
			= QKeySequence(pItem->text(2).trimmed()).toString();
		pItem->setText(2, sShortcutText);
	}

	stabilizeForm();
}


void qtractorShortcutForm::refresh (void)
{
	m_pActionControlItem = nullptr;

	m_ui.ShortcutTable->clear();

	m_item_actions.clear();
	m_action_items.clear();

	QString sSearch = m_ui.ShortcutSearchComboBox->currentText().simplified();
	const QRegularExpression rx(sSearch.replace(
		QRegularExpression("[\\s]+"), ".*"),
		QRegularExpression::CaseInsensitiveOption);

	QList<QTreeWidgetItem *> items;
	QListIterator<QAction *> iter(m_actions);
	while (iter.hasNext()) {
		QAction *pAction = iter.next();
		if (pAction->objectName().isEmpty())
			continue;
		const QString& sActionText
			= qtractorActionControl::menuActionText(pAction, pAction->text());
		if (!rx.pattern().isEmpty()
			&& !rx.match(sActionText).hasMatch()
			&& !rx.match(pAction->statusTip()).hasMatch())
			continue;
		QString sShortcutText;
		if (m_dirty_shortcuts.contains(pAction))
			sShortcutText = m_dirty_shortcuts.value(pAction);
		else
			sShortcutText = pAction->shortcut().toString();
		QString sControlText;
		if (m_dirty_controls.contains(pAction))
			sControlText = m_dirty_controls.value(pAction);
		else
			sControlText = actionControlText(pAction);
		QTreeWidgetItem *pItem = new QTreeWidgetItem();
		pItem->setIcon(0, pAction->icon());
		pItem->setText(0, sActionText);
		pItem->setText(1, pAction->statusTip());
		pItem->setText(2, sShortcutText);
		pItem->setText(3, sControlText);
		pItem->setFlags(
			Qt::ItemIsEnabled | Qt::ItemIsEditable | Qt::ItemIsSelectable);
		m_item_actions.insert(pItem, pAction);
		m_action_items.insert(pAction, pItem);
		items.append(pItem);
	}
	m_ui.ShortcutTable->addTopLevelItems(items);
	m_ui.ShortcutTable->expandAll();

	stabilizeForm();
}


void qtractorShortcutForm::accept (void)
{
	// Apply keyboard shortcuts...
	QHash<QAction *, QString>::ConstIterator iter1
		= m_dirty_shortcuts.constBegin();
	const QHash<QAction *, QString>::ConstIterator& iter1_end
		= m_dirty_shortcuts.constEnd();
	for ( ; iter1 != iter1_end; ++iter1)
		iter1.key()->setShortcut(QKeySequence(iter1.value()));

	// Free old/cloned MIDI observers...
	QHash<QAction *,MidiObserver *>::ConstIterator iter2
		= m_dirty_observers.constBegin();
	const QHash<QAction *, MidiObserver *>::ConstIterator& iter2_end
		= m_dirty_observers.constEnd();
	for ( ; iter2 != iter2_end; ++iter2) {
		MidiObserver *pMidiObserver = iter2.value();
		if (pMidiObserver)
			delete pMidiObserver;
	}

	QDialog::accept();
}


void qtractorShortcutForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (isDirtyActionShortcuts()) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"),
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
	else
	if (isDirtyActionControls()) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("MIDI Controller shortcuts have been changed.\n\n"
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

	qtractorMidiControl *pMidiControl = nullptr;
	if (bReject && m_pActionControl)
		pMidiControl = qtractorMidiControl::getInstance();
	if (pMidiControl) {
		QHash<QAction *,MidiObserver *>::ConstIterator iter
			= m_dirty_observers.constBegin();
		const QHash<QAction *, MidiObserver *>::ConstIterator& iter_end
			= m_dirty_observers.constEnd();
		for ( ; iter != iter_end; ++iter) {
			QAction *pMidiObserverAction = iter.key();
			MidiObserver *pMidiObserver = iter.value();
			MidiObserver *pNewMidiObserver
				= m_pActionControl->getMidiObserver(pMidiObserverAction);
			if (pNewMidiObserver) {
				pMidiControl->unmapMidiObserver(pNewMidiObserver);
				m_pActionControl->removeMidiObserver(pMidiObserverAction);
			}
			if (pMidiObserver) {
				pNewMidiObserver = m_pActionControl->addMidiObserver(pMidiObserverAction);
				pNewMidiObserver->setType(pMidiObserver->type());
				pNewMidiObserver->setChannel(pMidiObserver->channel());
				pNewMidiObserver->setParam(pMidiObserver->param());
				pNewMidiObserver->setLogarithmic(pMidiObserver->isLogarithmic());
				pNewMidiObserver->setFeedback(pMidiObserver->isFeedback());
				pNewMidiObserver->setInvert(pMidiObserver->isInvert());
				pNewMidiObserver->setHook(pMidiObserver->isHook());
				pNewMidiObserver->setLatch(pMidiObserver->isLatch());
				pMidiControl->mapMidiObserver(pNewMidiObserver);
				delete pMidiObserver;
			}
		}
	}

	if (bReject)
		QDialog::reject();
}


void qtractorShortcutForm::stabilizeForm (void)
{
	const bool bValid
		= (isDirtyActionShortcuts() || isDirtyActionControls());
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(bValid);
}


void qtractorShortcutForm::actionControlMenuRequested ( const QPoint& pos )
{
	if (m_pActionControl == nullptr)
		return;

	QMenu menu(this);

	menu.addAction(
		QIcon::fromTheme("itemControllers"),
		tr("&MIDI Controller..."), this,
		SLOT(actionControlActivated()));

	menu.exec(m_ui.ShortcutTable->viewport()->mapToGlobal(pos));
}


void qtractorShortcutForm::actionControlActivated (void)
{
	if (m_pActionControl == nullptr)
		return;

	m_pActionControlItem = m_ui.ShortcutTable->currentItem();
	if (m_pActionControlItem == nullptr)
		return;

	QAction *pMidiObserverAction = m_item_actions.value(m_pActionControlItem, nullptr);
	if (pMidiObserverAction == nullptr)
		return;

	qtractorMidiControlObserverForm::showInstance(pMidiObserverAction, this);

	qtractorMidiControlObserverForm *pMidiObserverForm
		= qtractorMidiControlObserverForm::getInstance();
	if (pMidiObserverForm) {
		if (!m_dirty_observers.contains(pMidiObserverAction)) {
			MidiObserver *pMidiObserver
				= m_pActionControl->getMidiObserver(pMidiObserverAction);
			if (pMidiObserver) {
				MidiObserver *pNewMidiObserver
					= new MidiObserver(pMidiObserverAction);
				pNewMidiObserver->setType(pMidiObserver->type());
				pNewMidiObserver->setChannel(pMidiObserver->channel());
				pNewMidiObserver->setParam(pMidiObserver->param());
				pNewMidiObserver->setLogarithmic(pMidiObserver->isLogarithmic());
				pNewMidiObserver->setFeedback(pMidiObserver->isFeedback());
				pNewMidiObserver->setInvert(pMidiObserver->isInvert());
				pNewMidiObserver->setHook(pMidiObserver->isHook());
				pNewMidiObserver->setLatch(pMidiObserver->isLatch());
				pMidiObserver = pNewMidiObserver;
			}
			m_dirty_observers.insert(pMidiObserverAction, pMidiObserver);
		}
		QObject::connect(pMidiObserverForm,
			SIGNAL(accepted()),
			SLOT(actionControlAccepted()));
		QObject::connect(pMidiObserverForm,
			SIGNAL(rejected()),
			SLOT(actionControlRejected()));
	}
}


void qtractorShortcutForm::actionControlAccepted (void)
{
	if (m_pActionControl == nullptr)
		return;

	if (m_pActionControlItem) {
		QAction *pMidiObserverAction
			= m_item_actions.value(m_pActionControlItem, nullptr);
		if (pMidiObserverAction) {
			const QString& sControlText = actionControlText(pMidiObserverAction);
			m_pActionControlItem->setText(3, sControlText);
			m_dirty_controls.insert(pMidiObserverAction, sControlText);
		}
		m_pActionControlItem = nullptr;
	}

	stabilizeForm();
}


void qtractorShortcutForm::actionControlRejected (void)
{
	if (m_pActionControl == nullptr)
		return;

	if (m_pActionControlItem) {
		QAction *pMidiObserverAction
			= m_item_actions.value(m_pActionControlItem, nullptr);
		if (pMidiObserverAction) {
			qtractorActionControl::MidiObserver *pMidiObserver
				= m_dirty_observers.value(pMidiObserverAction, nullptr);
			if (pMidiObserver)
				delete pMidiObserver;
			m_dirty_observers.remove(pMidiObserverAction);
		}
		m_pActionControlItem = nullptr;
	}

	stabilizeForm();
}


QString qtractorShortcutForm::actionControlText ( QAction *pAction ) const
{
	QString sActionControlText;

	if (m_pActionControl) {
		qtractorActionControl::MidiObserver *pMidiObserver
			= m_pActionControl->getMidiObserver(pAction);
		if (pMidiObserver) {
			QStringList clist;
			clist.append(qtractorMidiControl::nameFromType(pMidiObserver->type()));
			clist.append(QString::number(pMidiObserver->channel() + 1));
			clist.append(QString::number(pMidiObserver->param()));
			sActionControlText = clist.join(", ");
		}
	}

	return sActionControlText;
}


// end of qtractorShortcutForm.cpp

