// qtractorTempoCurveCommand.cpp
//
/****************************************************************************
   Copyright (C) 2018, spog aka Samo Pogačnik. All rights reserved.

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

#include "qtractorSession.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"

#include "qtractorTimeScaleCommand.h"
#include "qtractorTempoCurveCommand.h"

//----------------------------------------------------------------------
// class qtractorTempoCurveBaseCommand - declaration.
//

// Constructor.
qtractorTempoCurveBaseCommand::qtractorTempoCurveBaseCommand ( const QString& sName )
	: qtractorCommand(sName)
{
}

// Virtual command methods.
bool qtractorTempoCurveBaseCommand::redo (void)
{
	return execute(true);
}

bool qtractorTempoCurveBaseCommand::undo (void)
{
	return execute(false);
}


// Virtual command methods.
bool qtractorTempoCurveBaseCommand::execute ( bool /*bRedo*/ )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession && !pSession->isPlaying())
		pSession->process_curve(pSession->playHead());

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks) {
			pTracks->clearSelect();
			pTracks->updateTrackList(pTracks->currentTrack());
		}
	}

	return true;
}


//----------------------------------------------------------------------
// class qtractorTempoCurveCommand - declaration.
//

// Constructor.
qtractorTempoCurveCommand::qtractorTempoCurveCommand (
	const QString& sName, qtractorTempoCurve *pTempoCurve )
	: qtractorTempoCurveBaseCommand(sName), m_pTempoCurve(pTempoCurve)
{
}


//----------------------------------------------------------------------
// class qtractorTempoCurveProcessCommand - declaration.
//

// Constructor.
qtractorTempoCurveProcessCommand::qtractorTempoCurveProcessCommand (
	qtractorTempoCurve *pTempoCurve, bool bProcess )
	: qtractorTempoCurveCommand(QObject::tr("automation play"), pTempoCurve),
		m_bProcess(bProcess)
{
}


// Virtual command methods.
bool qtractorTempoCurveProcessCommand::execute ( bool bRedo )
{
	if (m_pTempoCurve == NULL)
		return false;

	const bool bProcess = m_pTempoCurve->isProcess();
	m_pTempoCurve->setProcess(m_bProcess);
	m_bProcess = bProcess;

	return qtractorTempoCurveBaseCommand::execute(bRedo);
}


//----------------------------------------------------------------------
// class qtractorTempoCurveColorCommand - declaration.
//

// Constructor.
qtractorTempoCurveColorCommand::qtractorTempoCurveColorCommand (
	qtractorTempoCurve *pTempoCurve, const QColor& color )
	: qtractorTempoCurveCommand(QObject::tr("automation color"), pTempoCurve),
		m_color(color)
{
}


// Virtual command methods.
bool qtractorTempoCurveColorCommand::execute ( bool /*bRedo*/ )
{
	if (m_pTempoCurve == NULL)
		return false;

	const QColor color = m_pTempoCurve->color();
	m_pTempoCurve->setColor(m_color);
	m_color = color;

	return true;
}


//----------------------------------------------------------------------
// class qtractorTempoCurveEditCommand - declaration.
//

// Constructors.
qtractorTempoCurveEditCommand::qtractorTempoCurveEditCommand (
	const QString& sName, qtractorTempoCurve *pTempoCurve )
	: qtractorTempoCurveCommand(sName, pTempoCurve),
		m_edits(pTempoCurve)
{
}

qtractorTempoCurveEditCommand::qtractorTempoCurveEditCommand ( qtractorTempoCurve *pTempoCurve )
	: qtractorTempoCurveCommand(QObject::tr("automation edit"), pTempoCurve),
		m_edits(pTempoCurve)
{
}


// Destructor.
qtractorTempoCurveEditCommand::~qtractorTempoCurveEditCommand (void)
{
	m_edits.clear();
}


// Primitive command methods.
void qtractorTempoCurveEditCommand::addNode ( qtractorTimeScale::Node *pNode )
{
	m_edits.addNode(pNode);
}


void qtractorTempoCurveEditCommand::moveNode ( qtractorTimeScale::Node *pNode,
	float fTempo, unsigned short iBeatsPerBar, unsigned short iBeatDivisor, bool bAttached )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoCurveEditCommand::%s@%d: (%g, %u, %u, %d)", __func__, __LINE__,
		fTempo, iBeatsPerBar, iBeatDivisor, bAttached);
#endif
#if 0
	m_edits.moveNode(pNode, fTempo, iBeatsPerBar, iBeatDivisor);
#endif

	qtractorSession *pSession = qtractorSession::getInstance();
	qtractorTimeScale *pTimeScale = NULL;
	if (pSession)
		pTimeScale = pSession->timeScale();

	if (((pNode->attached != bAttached) || pNode->allowChange()) && (pNode->getTs() == pTimeScale)) {
		// Now, express the change as a undoable command...
		pSession->execute(new qtractorTimeScaleUpdateNodeCommand(
			pTimeScale, pNode->frame, fTempo, 2, iBeatsPerBar, iBeatDivisor, bAttached));
	}
}


void qtractorTempoCurveEditCommand::removeNode ( qtractorTimeScale::Node *pNode )
{
	m_edits.removeNode(pNode);
}


// Composite predicate.
bool qtractorTempoCurveEditCommand::isEmpty (void) const
{
	return m_edits.isEmpty();
}


// Common executive method.
bool qtractorTempoCurveEditCommand::execute ( bool bRedo )
{
	if (!m_edits.execute(bRedo))
		return false;

	return qtractorTempoCurveBaseCommand::execute(bRedo);
}


//----------------------------------------------------------------------
// class qtractorTempoCurveClearCommand - declaration.
//

// Constructor.
qtractorTempoCurveClearCommand::qtractorTempoCurveClearCommand ( qtractorTempoCurve *pTempoCurve )
	: qtractorTempoCurveEditCommand(QObject::tr("automation clear"), pTempoCurve)
{
	qtractorTimeScale::Node *pNode = m_pTempoCurve->timeScale()->nodes().first();
	while (pNode) {
		qtractorTempoCurveEditCommand::removeNode(pNode);
		pNode = pNode->next();
	}
}


// end of qtractorTempoCurveCommand.cpp
