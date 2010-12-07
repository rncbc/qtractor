// qtractorObserver.h
//
/****************************************************************************
   Copyright (C) 2010, rncbc aka Rui Nuno Capela. All rights reserved.

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


//---------------------------------------------------------------------------
// qtractorSubject - Scalar parameter value model.

class qtractorSubject
{
public:

	// Constructor.
	qtractorSubject(float fValue = 0.0f);

	// Destructor.
	~qtractorSubject();

	// Direct value accessors.
	void setValue(float fValue, qtractorObserver *pSender = NULL);
	float value() const;

	float prevValue() const;

	// Busy flag predicate.
	bool isBusy() const;

	// Observers notification
	void notify(qtractorObserver *pSender = NULL);

	// Observer list accessors.
	void attach(qtractorObserver *pObserver);
	void detach(qtractorObserver *pObserver);

	const QList<qtractorObserver *>& observers() const;

	// Queue status accessors.
	void setQueued(bool bQueued);
	bool isQueued() const;

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

	// Queue flush (singleton) -- notify all pending observers.
	static void flushQueue();
	
	// Queue reset (clear).
	static void resetQueue();

private:

	// Instance variables.
	float   m_fValue;
	bool	m_bBusy;
	bool	m_bQueued;

	float   m_fPrevValue;

	// Human readable name/label.
	QString m_sName;

	// Value limits.
	float   m_fMinValue;
	float   m_fMaxValue;

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

	// Busy flag predicate.
	bool isBusy() const
		{ return (m_pSubject ? m_pSubject->isBusy() : true); }

	// Pure virtual view updater.
	virtual void update() = 0;

private:

	// Instance variables.
	qtractorSubject *m_pSubject;
};


#endif  // __qtractorObserver_h


// end of qtractorObserver.h
