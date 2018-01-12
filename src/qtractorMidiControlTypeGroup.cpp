// qtractorMidiControltypeGroup.cpp
//
/****************************************************************************
   Copyright (C) 2005-2015, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorMidiControlTypeGroup.h"

#include <QComboBox>
#include <QLineEdit>
#include <QLabel>


//----------------------------------------------------------------------------
// qtractorMidiControlTypeGroup - MIDI control type/param widget group.

// Constructor.
qtractorMidiControlTypeGroup::qtractorMidiControlTypeGroup (
	qtractorMidiEditor *pMidiEditor,
	QComboBox *pControlTypeComboBox,
	QComboBox *pControlParamComboBox,
	QLabel *pControlParamTextLabel,
	QComboBox *pControlParamLimitComboBox,
	QLabel *pControlParamLimitTextLabel) : QObject(),
		m_pMidiEditor(pMidiEditor),
		m_pControlTypeComboBox(pControlTypeComboBox),
		m_pControlParamComboBox(pControlParamComboBox),
		m_pControlParamTextLabel(pControlParamTextLabel),
		m_pControlParamLimitComboBox(pControlParamLimitComboBox),
		m_pControlParamLimitTextLabel(pControlParamLimitTextLabel),
		m_iControlParamUpdate(0),
		m_iControlParamLimitUpdate(0)
{
	const QIcon iconControlType(":/images/itemProperty.png");
	m_pControlTypeComboBox->clear();
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEON),
		int(qtractorMidiEvent::NOTEON));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NOTEOFF),
		int(qtractorMidiEvent::NOTEOFF));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::KEYPRESS),
		int(qtractorMidiEvent::KEYPRESS));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CONTROLLER),
		int(qtractorMidiEvent::CONTROLLER));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PGMCHANGE),
		int(qtractorMidiEvent::PGMCHANGE));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CHANPRESS),
		int(qtractorMidiEvent::CHANPRESS));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::PITCHBEND),
		int(qtractorMidiEvent::PITCHBEND));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::REGPARAM),
		int(qtractorMidiEvent::REGPARAM));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::NONREGPARAM),
		int(qtractorMidiEvent::NONREGPARAM));
	m_pControlTypeComboBox->addItem(iconControlType,
		qtractorMidiControl::nameFromType(qtractorMidiEvent::CONTROL14),
		int(qtractorMidiEvent::CONTROL14));

	m_pControlParamComboBox->setInsertPolicy(QComboBox::NoInsert);
	if( m_pControlParamLimitComboBox )
		m_pControlParamLimitComboBox->setInsertPolicy(QComboBox::NoInsert);

	QObject::connect(m_pControlTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(activateControlType(int)));
	QObject::connect(m_pControlParamComboBox,
		SIGNAL(activated(int)),
		SLOT(activateControlParam(int)));
	QObject::connect(m_pControlParamLimitComboBox,
		SIGNAL(activated(int)),
		SLOT(activateControlParamLimit(int)));

}


// Accessors.
void qtractorMidiControlTypeGroup::setControlType (
	qtractorMidiControl::ControlType ctype )
{
	const int iControlType = indexFromControlType(ctype);
	m_pControlTypeComboBox->setCurrentIndex(iControlType);
	activateControlType(iControlType);
}

qtractorMidiControl::ControlType qtractorMidiControlTypeGroup::controlType (void) const
{
	return controlTypeFromIndex(m_pControlTypeComboBox->currentIndex());
}

qtractorMidiControl::ControlType
qtractorMidiControlTypeGroup::controlTypeFromIndex ( int iIndex ) const
{
	if (iIndex < 0 || iIndex >= m_pControlTypeComboBox->count())
		return qtractorMidiEvent::NOTEON;
	else
		return qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iIndex).toInt());
}



void qtractorMidiControlTypeGroup::setControlParam (
	unsigned short iParam )
{
	const int iControlParam = indexFromControlParam(iParam);
	if (iControlParam >= 0) {
		m_pControlParamComboBox->setCurrentIndex(iControlParam);
		activateControlParam(iControlParam);
	} else {
		const QString& sControlParam = QString::number(iParam);
		m_pControlParamComboBox->setEditText(sControlParam);
		editControlParamFinished();
	}
}

unsigned short qtractorMidiControlTypeGroup::controlParam (void) const
{
	if (m_pControlParamComboBox->isEditable()) {
		unsigned short iParam = 0;
		const QString& sControlParam
			= m_pControlParamComboBox->currentText();
		bool bOk = false;
		iParam = sControlParam.toInt(&bOk);
		if (bOk) return iParam;
	}

	return controlParamFromIndex(m_pControlParamComboBox->currentIndex());

}

void qtractorMidiControlTypeGroup::setControlParamLimit (
	unsigned short iParamLimit )
{
	const int iControlParamLimit = indexFromControlParamLimit(iParamLimit);
	if (iControlParamLimit >= 0) {
		m_pControlParamLimitComboBox->setCurrentIndex(iControlParamLimit);
		activateControlParamLimit(iControlParamLimit);
	} else {
		const QString& sControlParamLimit = QString::number(iParamLimit);
		m_pControlParamLimitComboBox->setEditText(sControlParamLimit);
		editControlParamLimitFinished();
	}
}

unsigned short qtractorMidiControlTypeGroup::controlParamLimit (void) const
{
	if (m_pControlParamLimitComboBox->isEditable()) {
		unsigned short iParamLimit = 0;
		const QString& sControlParamLimit
			= m_pControlParamLimitComboBox->currentText();
		bool bOk = false;
		iParamLimit = sControlParamLimit.toInt(&bOk);
		if (bOk) return iParamLimit;
	}

	return controlParamLimitFromIndex(m_pControlParamLimitComboBox->currentIndex());

}


unsigned short qtractorMidiControlTypeGroup::controlParamFromIndex ( int iIndex ) const
{
	if (iIndex >= 0 && iIndex < m_pControlParamComboBox->count())
		return m_pControlParamComboBox->itemData(iIndex).toInt();
	else
		return 0;
}

unsigned short qtractorMidiControlTypeGroup::controlParamLimitFromIndex ( int iIndex ) const
{
	if (iIndex >= 0 && iIndex < m_pControlParamLimitComboBox->count())
		return m_pControlParamLimitComboBox->itemData(iIndex).toInt();
	else
		return 0;
}


// Stabilizers.
void qtractorMidiControlTypeGroup::updateControlType ( int iControlType )
{
	// Just in case...
	if (m_pMidiEditor)
		m_pMidiEditor->updateInstrumentNames();

	if (iControlType < 0)
		iControlType = m_pControlTypeComboBox->currentIndex();

	const qtractorMidiControl::ControlType ctype
		= qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iControlType).toInt());

	const bool bOldEditable
		= m_pControlParamComboBox->isEditable();
	const int iOldParam
		= m_pControlParamComboBox->currentIndex();
	const QString sOldParam
		= m_pControlParamComboBox->currentText();

	m_pControlParamComboBox->clear();

	bool bOldLimitEditable = false;
	unsigned short iOldParamLimit = USHRT_MAX;
	QString sOldParamLimit;

	if( m_pControlParamLimitComboBox != NULL ){
		bOldLimitEditable = m_pControlParamLimitComboBox->isEditable();
		iOldParamLimit = m_pControlParamLimitComboBox->currentIndex();
		sOldParamLimit = m_pControlParamLimitComboBox->currentText();
		m_pControlParamLimitComboBox->clear();
	}

	const QString sTextMask("%1 - %2");

	switch (ctype) {
	case qtractorMidiEvent::NOTEON:
	case qtractorMidiEvent::NOTEOFF:
	case qtractorMidiEvent::KEYPRESS: {
		const QIcon iconNotes(":/images/itemNotes.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		if( m_pControlParamLimitComboBox ){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(true);
			m_pControlParamLimitComboBox->setEnabled(true);
			m_pControlParamLimitComboBox->setEditable(false);
			m_pControlParamLimitComboBox->addItem(iconNotes,
				"(None)", int(USHRT_MAX));
		}
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconNotes,
				sTextMask.arg(iParam).arg(
					m_pMidiEditor ? m_pMidiEditor->noteName(iParam)
					: qtractorMidiEditor::defaultNoteName(iParam)),
				int(iParam));
			if(m_pControlParamLimitComboBox != NULL )
				m_pControlParamLimitComboBox->addItem(iconNotes,
					sTextMask.arg(iParam).arg(
					m_pMidiEditor ? m_pMidiEditor->noteName(iParam)
					: qtractorMidiEditor::defaultNoteName(iParam)),
					int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CONTROLLER: {
		const QIcon iconControllers(":/images/itemControllers.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		if( m_pControlParamLimitComboBox){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(true);
			m_pControlParamLimitComboBox->setEnabled(true);
			m_pControlParamLimitComboBox->setEditable(false);
			m_pControlParamLimitComboBox->addItem(iconControllers,
				"(None)", int(USHRT_MAX));
		}
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconControllers,
				sTextMask.arg(iParam).arg(
					m_pMidiEditor ? m_pMidiEditor->controllerName(iParam)
					: qtractorMidiEditor::defaultControllerName(iParam)),
				int(iParam));
			if( m_pControlParamLimitComboBox != NULL)
				m_pControlParamLimitComboBox->addItem(iconControllers,
					sTextMask.arg(iParam).arg(
					m_pMidiEditor ? m_pMidiEditor->controllerName(iParam)
					: qtractorMidiEditor::defaultControllerName(iParam)),
					int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::PGMCHANGE: {
		const QIcon iconPatches(":/images/itemPatches.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		if( m_pControlParamLimitComboBox ){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(true);
			m_pControlParamLimitComboBox->setEnabled(true);
			m_pControlParamLimitComboBox->setEditable(false);
			m_pControlParamLimitComboBox->addItem(iconPatches,
				"(None)", int(USHRT_MAX));
		}
		for (unsigned short iParam = 0; iParam < 128; ++iParam) {
			m_pControlParamComboBox->addItem(iconPatches,
				sTextMask.arg(iParam).arg('-'), int(iParam));
			if( m_pControlParamLimitComboBox != NULL)
				m_pControlParamLimitComboBox->addItem(iconPatches,
					sTextMask.arg(iParam).arg('-'), int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::REGPARAM: {
		const QIcon iconRpns(":/images/itemRpns.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(true);
		if( m_pControlParamLimitComboBox){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(false);
			m_pControlParamLimitComboBox->setEnabled(false);
			m_pControlParamLimitComboBox->setEditable(false);
		}
		const QMap<unsigned short, QString>& rpns
			= (m_pMidiEditor ? m_pMidiEditor->rpnNames()
			: qtractorMidiEditor::defaultRpnNames());
		QMap<unsigned short, QString>::ConstIterator rpns_iter
			= rpns.constBegin();
		const QMap<unsigned short, QString>::ConstIterator& rpns_end
			= rpns.constEnd();
		for ( ; rpns_iter != rpns_end; ++rpns_iter) {
			const unsigned short iParam = rpns_iter.key();
			m_pControlParamComboBox->addItem(iconRpns,
				sTextMask.arg(iParam).arg(rpns_iter.value()),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::NONREGPARAM: {
		const QIcon iconNrpns(":/images/itemNrpns.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(true);
		if( m_pControlParamLimitComboBox){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(false);
			m_pControlParamLimitComboBox->setEnabled(false);
			m_pControlParamLimitComboBox->setEditable(false);
		}
		const QMap<unsigned short, QString>& nrpns
			= (m_pMidiEditor ? m_pMidiEditor->nrpnNames()
			: qtractorMidiEditor::defaultNrpnNames());
		QMap<unsigned short, QString>::ConstIterator nrpns_iter
			= nrpns.constBegin();
		const QMap<unsigned short, QString>::ConstIterator& nrpns_end
			= nrpns.constEnd();
		for ( ; nrpns_iter != nrpns_end; ++nrpns_iter) {
			const unsigned short iParam = nrpns_iter.key();
			m_pControlParamComboBox->addItem(iconNrpns,
				sTextMask.arg(iParam).arg(nrpns_iter.value()),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CONTROL14: {
		const QIcon iconControllers(":/images/itemControllers.png");
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(true);
		m_pControlParamComboBox->setEnabled(true);
		m_pControlParamComboBox->setEditable(false);
		if( m_pControlParamLimitComboBox){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(false);
			m_pControlParamLimitComboBox->setEnabled(false);
			m_pControlParamLimitComboBox->setEditable(false);
		}
		for (unsigned short iParam = 1; iParam < 32; ++iParam) {
			m_pControlParamComboBox->addItem(iconControllers,
				sTextMask.arg(iParam).arg(
					m_pMidiEditor ? m_pMidiEditor->control14Name(iParam)
					: qtractorMidiEditor::defaultControl14Name(iParam)),
				int(iParam));
		}
		break;
	}
	case qtractorMidiEvent::CHANPRESS:
	case qtractorMidiEvent::PITCHBEND:
	default:
		if (m_pControlParamTextLabel)
			m_pControlParamTextLabel->setEnabled(false);
		m_pControlParamComboBox->setEnabled(false);
		m_pControlParamComboBox->setEditable(false);
		if( m_pControlParamLimitComboBox){
			if( m_pControlParamLimitTextLabel )
				m_pControlParamLimitTextLabel->setEnabled(false);
			m_pControlParamLimitComboBox->setEnabled(false);
			m_pControlParamLimitComboBox->setEditable(false);
		}
		break;
	}

	if (iOldParam >= 0 && iOldParam < m_pControlParamComboBox->count())
		m_pControlParamComboBox->setCurrentIndex(iOldParam);

	if (m_pControlParamLimitComboBox && iOldParamLimit < m_pControlParamLimitComboBox->count())
		m_pControlParamLimitComboBox->setCurrentIndex(iOldParamLimit);

	if (m_pControlParamComboBox->isEditable()) {
		QObject::connect(m_pControlParamComboBox->lineEdit(),
			SIGNAL(editingFinished()),
			SLOT(editControlParamFinished()));
		if (bOldEditable)
			m_pControlParamComboBox->setEditText(sOldParam);
	}
	if (m_pControlParamLimitComboBox && m_pControlParamLimitComboBox->isEditable()) {
		QObject::connect(m_pControlParamLimitComboBox->lineEdit(),
			SIGNAL(editingFinished()),
			SLOT(editControlParamLimitFinished()));
		if (bOldLimitEditable)
			m_pControlParamLimitComboBox->setEditText(sOldParamLimit);
	}
}


// Private slots.
void qtractorMidiControlTypeGroup::activateControlType ( int iControlType )
{
	updateControlType(iControlType);

	emit controlTypeChanged(iControlType);

	activateControlParam(m_pControlParamComboBox->currentIndex());
	if( m_pControlParamLimitComboBox )
		activateControlParamLimit(m_pControlParamLimitComboBox->currentIndex());
}


void qtractorMidiControlTypeGroup::activateControlParam ( int iControlParam )
{
	const unsigned short iParam
		= m_pControlParamComboBox->itemData(iControlParam).toInt();

	emit controlParamChanged(int(iParam));
}

void qtractorMidiControlTypeGroup::activateControlParamLimit ( int iControlParamLimit )
{
	const unsigned short iParamLimit
		= m_pControlParamLimitComboBox->itemData(iControlParamLimit).toInt();

	emit controlParamLimitChanged(int(iParamLimit));
}



void qtractorMidiControlTypeGroup::editControlParamFinished (void)
{
	if (m_iControlParamUpdate > 0)
		return;

	++m_iControlParamUpdate;

	const QString& sControlParam
		= m_pControlParamComboBox->currentText();

	bool bOk = false;
	const unsigned short iParam = sControlParam.toInt(&bOk);
	if (bOk) emit controlParamChanged(int(iParam));

	--m_iControlParamUpdate;
}

void qtractorMidiControlTypeGroup::editControlParamLimitFinished (void)
{
	if (m_iControlParamLimitUpdate > 0)
		return;

	++m_iControlParamLimitUpdate;

	const QString& sControlParamLimit
		= m_pControlParamLimitComboBox->currentText();

	bool bOk = false;
	const unsigned short iParam = sControlParamLimit.toInt(&bOk);
	if (bOk) emit controlParamLimitChanged(int(iParam));

	--m_iControlParamLimitUpdate;
}


// Find combo-box index from control type.
int qtractorMidiControlTypeGroup::indexFromControlType (
	qtractorMidiControl::ControlType ctype ) const
{
	const int iItemCount = m_pControlTypeComboBox->count();
	for (int iIndex = 0; iIndex < iItemCount; ++iIndex) {
		if (qtractorMidiControl::ControlType(
			m_pControlTypeComboBox->itemData(iIndex).toInt()) == ctype)
			return iIndex;
	}
	return (-1);
}


// Find combo-box index from control parameter number.
int qtractorMidiControlTypeGroup::indexFromControlParam (
	unsigned short iParam ) const
{
	const int iItemCount = m_pControlParamComboBox->count();
	for (int iIndex = 0; iIndex < iItemCount; ++iIndex) {
		if (m_pControlParamComboBox->itemData(iIndex).toInt() == int(iParam))
			return iIndex;
	}
	return (-1);
}

int qtractorMidiControlTypeGroup::indexFromControlParamLimit (
	unsigned short iParam ) const
{
	const int iItemCount = m_pControlParamLimitComboBox->count();
	for (int iIndex = 0; iIndex < iItemCount; ++iIndex) {
		if (m_pControlParamLimitComboBox->itemData(iIndex).toInt() == int(iParam))
			return iIndex;
	}
	return (-1);
}

// end of qtractorMidiControlTypeGroup.cpp
