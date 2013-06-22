// qtractorObserver.h
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

#ifndef __qtractorObserver_h
#define __qtractorObserver_h

#include <QString>
#include <QList>

// Forward declarations.
class qtractorSubject;
class qtractorObserver;
class qtractorCurve;


//---------------------------------------------------------------------------
// qtractorSubject - Scalar parameter value model.

class qtractorSubject
{
public:

	// Constructor.
	qtractorSubject(float fValue = 0.0f, float fDefaultValue = 0.0f);

	// Destructor.
	~qtractorSubject();

	// Direct value accessors.
	void setValue(float fValue, qtractorObserver *pSender = NULL);
	float value() const
		{ return m_fValue; }

	float prevValue() const
		{ return m_fPrevValue; }

	// Observers notification.
	void notify(qtractorObserver *pSender, bool bUpdate);

	// Observer list accessors.
	void attach(qtractorObserver *pObserver)
		{ m_observers.append(pObserver); }
	void detach(qtractorObserver *pObserver)
		{ m_observers.removeAll(pObserver); }

	const QList<qtractorObserver *>& observers() const
		{ return m_observers; }

	// Queue status accessors.
	void setQueued(bool bQueued)
		{ m_bQueued = bQueued; }
	bool isQueued() const
		{ return m_bQueued; }

	// Direct address accessor.
	float *data() { return &m_fValue; }

	// Parameter name accessors.
	void setName(const QString& sName)
		{ m_sName = sName.trimmed(); }
	const QString& name() const
		{ return m_sName; }

	// Value limits accessors.
	void setMaxValue(float fMaxValue)
		{ m_fMaxValue = fMaxValue; }
	float maxValue() const
		{ return m_fMaxValue; }

	void setMinValue(float fMinValue)
		{ m_fMinValue = fMinValue; }
	float minValue() const
		{ return m_fMinValue; }

	// Default value accessor.
	void setDefaultValue(float fDefaultValue)
		{ m_fDefaultValue = fDefaultValue; }
	float defaultValue() const
		{ return m_fDefaultValue; }

	void resetValue(qtractorObserver *pSender = NULL)
		{ setValue(m_fDefaultValue, pSender); }

	// Toggled mode accessors.
	void setToggled(bool bToggled)
		{ m_bToggled = bToggled; }
	bool isToggled() const
		{ return m_bToggled; }

	// Filter value within legal bounds.
	float safeValue (float fValue) const
	{
		if (m_bToggled) {
			const float fThreshold = 0.5f * (m_fMinValue + m_fMaxValue);
			fValue = (fValue > fThreshold ?  m_fMaxValue : m_fMinValue);
		}
		else 
		if (fValue > m_fMaxValue)
			fValue = m_fMaxValue;
		else
		if (fValue < m_fMinValue)
			fValue = m_fMinValue;

		return fValue;
	}

	// Normalized scale converters.
	float valueFromScale ( float fScale ) const
		{ return m_fMinValue + fScale * (m_fMaxValue - m_fMinValue); }
	float scaleFromValue ( float fValue ) const
		{ return (fValue - m_fMinValue) / (m_fMaxValue - m_fMinValue); }

	// Automation curve association.
	void setCurve(qtractorCurve *pCurve)
		{ m_pCurve = pCurve; }
	qtractorCurve *curve() const
		{ return m_pCurve; }

	// Queue flush (singleton) -- notify all pending observers.
	static void flushQueue(bool bUpdate);
	
	// Queue reset (clear).
	static void resetQueue();

private:

	// Instance variables.
	float   m_fValue;
	bool	m_bQueued;

	float   m_fPrevValue;

	// Human readable name/label.
	QString m_sName;

	// Value limits.
	float   m_fMinValue;
	float   m_fMaxValue;

	// Default value.
	float   m_fDefaultValue;

	// Toggled value mode (max or min).
	bool    m_bToggled;

	// Automation curve association.
	qtractorCurve *m_pCurve;

	// List of observers (obviously)
	QList<qtractorObserver *> m_observers;
};


//---------------------------------------------------------------------------
// qtractorObserver - Scalar parameter value control/view.

class qtractorObserver
{
public:

	// Constructor.
	qtractorObserver(qtractorSubject *pSubject = NULL) : m_pSubject(pSubject)
		{ if (m_pSubject) m_pSubject->attach(this); }

	// Virtual destructor.
	virtual ~qtractorObserver()
		{ if (m_pSubject) m_pSubject->detach(this); }

	// Subject value accessor.
	void setSubject(qtractorSubject *pSubject)
	{
		if (m_pSubject /* && pSubject*/)
			m_pSubject->detach(this);

		m_pSubject = pSubject;

		if (m_pSubject)
			m_pSubject->attach(this);
	}

	qtractorSubject *subject() const
		{ return m_pSubject; }

	// Indirect value accessors.
	void setValue(float fValue)
		{ if (m_pSubject) m_pSubject->setValue(fValue, this); }
	float value() const
		{ return (m_pSubject ? m_pSubject->value() : 0.0f); }

	float prevValue() const
		{ return (m_pSubject ? m_pSubject->prevValue() : 0.0f); }

	// Value limits accessors.
	float maxValue() const
		{ return (m_pSubject ? m_pSubject->maxValue() : 1.0f); }
	float minValue() const
		{ return (m_pSubject ? m_pSubject->minValue() : 0.0f); }

	// Default value accessor.
	void setDefaultValue(float fDefaultValue)
		{ if (m_pSubject) m_pSubject->setDefaultValue(fDefaultValue); }
	float defaultValue() const
		{ return (m_pSubject ? m_pSubject->defaultValue() : 0.0f); }

	void resetValue()
		{ if (m_pSubject) m_pSubject->resetValue(this); }

	// Queue status accessors.
	bool isQueued() const
		{ return (m_pSubject ? m_pSubject->isQueued() : false); }

	// Toggled mode accessors.
	bool isToggled() const
		{ return (m_pSubject ? m_pSubject->isToggled() : false); }

	// Filter value within legal bounds.
	float safeValue(float fValue) const
		{ return (m_pSubject ? m_pSubject->safeValue(fValue) : 0.0f); }

	// Normalized scale converters.
	float valueFromScale ( float fScale ) const
		{ return (m_pSubject ? m_pSubject->valueFromScale(fScale) : 0.0f); }
	float scaleFromValue ( float fValue ) const
		{ return (m_pSubject ? m_pSubject->scaleFromValue(fValue) : 0.0f); }

	// Automation curve association.
	void setCurve(qtractorCurve *pCurve)
		{ if (m_pSubject) m_pSubject->setCurve(pCurve); }
	qtractorCurve *curve() const
		{ return (m_pSubject ? m_pSubject->curve() : NULL); }

	// Pure virtual view updater.
	virtual void update(bool bUpdate) = 0;

private:

	// Instance variables.
	qtractorSubject *m_pSubject;
};


#endif  // __qtractorObserver_h


// end of qtractorObserver.h
