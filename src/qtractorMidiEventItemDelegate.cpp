// qtractorMidiEventItemDelegate.cpp
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiEventItemDelegate.h"
#include "qtractorMidiEventListModel.h"
#include "qtractorMidiEvent.h"

#include "qtractorMidiEditor.h"
#include "qtractorMidiEditCommand.h"

#include "qtractorSpinBox.h"

#include <QApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QKeyEvent>


//----------------------------------------------------------------------------
// qtractorMidiEventItemDelegate -- Custom (tree) list item delegate.

// Constructor.
qtractorMidiEventItemDelegate::qtractorMidiEventItemDelegate (
	QObject *pParent ) : QItemDelegate(pParent)
{
}


// Destructor.
qtractorMidiEventItemDelegate::~qtractorMidiEventItemDelegate (void)
{
}


// Keyboard event hook.
bool qtractorMidiEventItemDelegate::eventFilter ( QObject *pObject, QEvent *pEvent )
{
    QWidget *pEditor = qobject_cast<QWidget*> (pObject);

	if (pEditor) {

		switch (pEvent->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease:
		{
			QKeyEvent *pKeyEvent = static_cast<QKeyEvent *> (pEvent);
			if ((pKeyEvent->modifiers() & Qt::ControlModifier) == 0 &&
				(pKeyEvent->key() == Qt::Key_Up || pKeyEvent->key() == Qt::Key_Down)) {
				pEvent->ignore();
				return true;
			}
			if (pKeyEvent->key() == Qt::Key_Enter ||
				pKeyEvent->key() == Qt::Key_Return) {
				emit commitData(pEditor);
				if (pEditor)
					pEditor->close();
				return true;
			}
			break;
		}

		case QEvent::FocusOut:
		{
			if (!pEditor->isActiveWindow()
				|| (QApplication::focusWidget() != pEditor)) {
				QWidget *pWidget = QApplication::focusWidget();
				while (pWidget) {
					if (pWidget == pEditor)
						return false;
					pWidget = pWidget->parentWidget();
				}
				emit commitData(pEditor);
			}
			return false;
		}

		default:
			break;
		}
	}

    return QItemDelegate::eventFilter(pObject, pEvent);
}


// QItemDelegate Interface...

void qtractorMidiEventItemDelegate::paint ( QPainter *pPainter,
	const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    QItemDelegate::paint(pPainter, option, index);
}


QSize qtractorMidiEventItemDelegate::sizeHint (
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
    return QItemDelegate::sizeHint(option, index) + QSize(4, 4);
}


QWidget *qtractorMidiEventItemDelegate::createEditor ( QWidget *pParent,
	const QStyleOptionViewItem& /*option*/, const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (index.model());

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return NULL;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return NULL;

	QWidget *pEditor = NULL;

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= new qtractorTimeSpinBox(pParent);
		pTimeSpinBox->setTimeScale(pMidiEditor->timeScale());
		pEditor = pTimeSpinBox;
		break;
	}

	case 2: // Name.
	{
		if (pEvent->type() == qtractorMidiEvent::NOTEON ||
			pEvent->type() == qtractorMidiEvent::KEYPRESS) {
			QComboBox *pComboBox = new QComboBox(pParent);
			for (unsigned char note = 0; note < 128; ++note)
				pComboBox->addItem(pMidiEditor->noteName(note));
			pEditor = pComboBox;
		}
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = new QSpinBox(pParent);
		if (pEvent->type() == qtractorMidiEvent::PITCHBEND) {
			pSpinBox->setMinimum(-8192);
			pSpinBox->setMaximum(+8192);
		} else {
			pSpinBox->setMinimum(0);
			pSpinBox->setMaximum(127);
		}
		pEditor = pSpinBox;
		break;
	}

	case 4: // Duration/Data.
	{
		if (pEvent->type() == qtractorMidiEvent::NOTEON) {
			qtractorTimeSpinBox *pTimeSpinBox
				= new qtractorTimeSpinBox(pParent);
			pTimeSpinBox->setTimeScale(
				(pListModel->editor())->timeScale());
			pTimeSpinBox->setDeltaValue(true);
			pEditor = pTimeSpinBox;
		}
		break;
	}

	default:
		break;
	}

	if (pEditor) {
		pEditor->installEventFilter(
			const_cast<qtractorMidiEventItemDelegate *> (this));
	}

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::createEditor(%p, %d, %d) = %p",
		pParent, index.row(), index.column(), pEditor);
#endif

	return pEditor;
}


void qtractorMidiEventItemDelegate::setEditorData ( QWidget *pEditor,
	const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (index.model());

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::setEditorData(%p, %p, %d, %d)",
		pEditor, pListModel, index.row(), index.column());
#endif

	qtractorTimeScale *pTimeScale = pMidiEditor->timeScale();

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			pTimeSpinBox->setValue(pTimeScale->frameFromTick(
				pMidiEditor->timeOffset() + pEvent->time()));
		}
		break;
	}

	case 2: // Name.
	{
		QComboBox *pComboBox = qobject_cast<QComboBox *> (pEditor);
		if (pComboBox)
			pComboBox->setCurrentIndex(int(pEvent->note()));
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = qobject_cast<QSpinBox *> (pEditor);
		if (pSpinBox) {
			if (pEvent->type() == qtractorMidiEvent::PITCHBEND)
				pSpinBox->setValue(pEvent->pitchBend());
			else
				pSpinBox->setValue(pEvent->value());
		}
		break;
	}

	case 4: // Duration/Data.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			pTimeSpinBox->setValue(
				pTimeScale->frameFromTick(pEvent->duration()));
		}
		break;
	}

	default:
		break;
	}
}


void qtractorMidiEventItemDelegate::setModelData ( QWidget *pEditor,
	QAbstractItemModel *pModel,	const QModelIndex& index ) const
{
	const qtractorMidiEventListModel *pListModel
		= static_cast<const qtractorMidiEventListModel *> (pModel);

	qtractorMidiEvent *pEvent = pListModel->eventOfIndex(index);
	if (pEvent == NULL)
		return;

	qtractorMidiEditor *pMidiEditor = pListModel->editor();
	if (pMidiEditor == NULL)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorMidiEventItemDelegate::setModelData(%p, %p, %d, %d)",
		pEditor, pListModel, index.row(), index.column());
#endif

	qtractorTimeScale *pTimeScale = pMidiEditor->timeScale();

	qtractorMidiEditCommand *pEditCommand
		= new qtractorMidiEditCommand(pMidiEditor->midiClip(),
			tr("edit %1").arg(pListModel->headerData(
				index.column(), Qt::Horizontal, Qt::DisplayRole)
					.toString().toLower()));

	switch (index.column()) {
	case 0: // Time.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			unsigned long iTime
				= pTimeScale->tickFromFrame(pTimeSpinBox->valueFromText());
			if (iTime > pMidiEditor->timeOffset())
				iTime -= pMidiEditor->timeOffset();
			else
				iTime = 0;
			unsigned long iDuration = 0;
			if (pEvent->type() == qtractorMidiEvent::NOTEON)
				iDuration = pEvent->duration();
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		break;
	}

	case 2: // Name.
	{
		QComboBox *pComboBox = qobject_cast<QComboBox *> (pEditor);
		if (pComboBox) {
			int iNote = pComboBox->currentIndex();
			unsigned long iTime = pEvent->time();
			pEditCommand->moveEvent(pEvent, iNote, iTime);			
		}
		break;
	}

	case 3: // Value.
	{
		QSpinBox *pSpinBox = qobject_cast<QSpinBox *> (pEditor);
		if (pSpinBox) {
			int iValue = pSpinBox->value();
			pEditCommand->resizeEventValue(pEvent, iValue);
		}
		break;
	}

	case 4: // Duration/Data.
	{
		qtractorTimeSpinBox *pTimeSpinBox
			= qobject_cast<qtractorTimeSpinBox *> (pEditor);
		if (pTimeSpinBox) {
			unsigned long iTime = pEvent->time();
			unsigned long iDuration
				= pTimeScale->tickFromFrame(pTimeSpinBox->value());
			pEditCommand->resizeEventTime(pEvent, iTime, iDuration);
		}
		break;
	}

	default:
		break;
	}

	// Do it.
	pMidiEditor->commands()->exec(pEditCommand);
}


void qtractorMidiEventItemDelegate::updateEditorGeometry ( QWidget *pEditor,
	const QStyleOptionViewItem& option, const QModelIndex& index ) const
{
	QItemDelegate::updateEditorGeometry(pEditor, option, index);
}


// end of qtractorMidiEventItemDelegate.h
