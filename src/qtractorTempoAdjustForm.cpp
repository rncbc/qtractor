// qtractorTempoAdjustForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2026, rncbc aka Rui Nuno Capela. All rights reserved.

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

#include "qtractorTempoAdjustForm.h"

#include "qtractorAbout.h"
#include "qtractorSession.h"

#include "qtractorAudioClip.h"
#include "qtractorAudioEngine.h"

#include "qtractorMainForm.h"
#include "qtractorTracks.h"

#include <QMessageBox>
#include <QLineEdit>
#include <QPainter>
#include <QPaintEvent>
#include <QProgressBar>
#include <QElapsedTimer>

#include <cmath>


#ifdef CONFIG_LIBAUBIO
#include <aubio/aubio.h>
#endif

#ifdef CONFIG_MINIBPM
#include "minibpm/src/MiniBpm.cpp"
#endif

// Audio clip beat-detection callback.
struct audioClipTempoDetectData
{	// Ctor.
	audioClipTempoDetectData(unsigned short iChannels,
		unsigned iSampleRate, unsigned int iBlockSize = 1024)
		: count(0), offset(0), channels(iChannels), nstep(iBlockSize >> 3)
	{
	#ifdef CONFIG_LIBAUBIO
		aubio = new_aubio_tempo("default", iBlockSize, nstep, iSampleRate);
		ibuf = new_fvec(nstep);
		obuf = new_fvec(1);
	#endif
	#ifdef CONFIG_MINIBPM
		minibpm = new breakfastquay::MiniBPM * [iChannels];
		for (unsigned short i = 0; i < channels; ++i)
			minibpm[i] = new breakfastquay::MiniBPM(iSampleRate);
	#endif
	}
	// Dtor.
	~audioClipTempoDetectData()
	{
	#ifdef CONFIG_LIBAUBIO
		beats.clear();
		del_fvec(obuf);
		del_fvec(ibuf);
		del_aubio_tempo(aubio);
	#endif
	#ifdef CONFIG_MINIBPM
		for (unsigned short i = 0; i < channels; ++i)
			delete minibpm[i];
		delete [] minibpm;
	#endif
	}
	// Members.
	unsigned int count;
	unsigned long offset;
	unsigned short channels;
	unsigned int nstep;
#ifdef CONFIG_LIBAUBIO
	aubio_tempo_t *aubio;
	fvec_t *ibuf;
	fvec_t *obuf;
	QList<unsigned long> beats;
#endif
#ifdef CONFIG_MINIBPM
	breakfastquay::MiniBPM **minibpm;
#endif
};


static void audioClipTempoDetect (
	float **ppFrames, unsigned int iFrames, void *pvArg )
{
	audioClipTempoDetectData *pData
		= static_cast<audioClipTempoDetectData *> (pvArg);

#ifdef CONFIG_LIBAUBIO
	unsigned int i = 0;
	while (i < iFrames) {
		unsigned int j = 0;
		for (; j < pData->nstep && i < iFrames; ++j, ++i) {
			float fSum = 0.0f;
			for (unsigned short n = 0; n < pData->channels; ++n)
				fSum += ppFrames[n][i];
			fvec_set_sample(pData->ibuf, fSum / float(pData->channels), j);
		}
		for (; j < pData->nstep; ++j)
			fvec_set_sample(pData->ibuf, 0.0f, j);
		aubio_tempo_do(pData->aubio, pData->ibuf, pData->obuf);
		const bool is_beat = bool(fvec_get_sample(pData->obuf, 0));
		if (is_beat) {
			unsigned long iOffset = aubio_tempo_get_last(pData->aubio);
			if (iOffset >= pData->offset)
				iOffset -= pData->offset;
			pData->beats.append(iOffset);
		}
	}
#endif

#ifdef CONFIG_MINIBPM
	for (unsigned short i = 0; i < pData->channels; ++i) {
		pData->minibpm[i]->process(ppFrames[i], iFrames);
	}
#endif

	if (++(pData->count) > 100) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			QProgressBar *pProgressBar = pMainForm->progressBar();
			pProgressBar->setValue(pProgressBar->value() + iFrames);
		}
		qtractorSession::stabilize();
		pData->count = 0;
	}
}


//----------------------------------------------------------------------------
// qtractorTempoAdjustForm::ClipWidget -- Dang simple graphical widget

class qtractorTempoAdjustForm::ClipWidget : public QFrame
{
public:

	// Constructor.
	ClipWidget(qtractorTempoAdjustForm *pForm)
		: QFrame(pForm), m_pForm(pForm)
	{
		QFrame::setSizePolicy(
			QSizePolicy::Expanding,
			QSizePolicy::Expanding);

		QFrame::setFrameShape(QFrame::Panel);
		QFrame::setFrameShadow(QFrame::Sunken);

		qtractorClip *pClip = m_pForm->clip();
		if (pClip) {
			qtractorTrack *pTrack = pClip->track();
			if (pTrack) {
				QPalette pal;
				pal.setColor(QPalette::WindowText, pTrack->foreground());
				pal.setColor(QPalette::Window, pTrack->background());
				QFrame::setPalette(pal);
			}
		}
	}

	// Accessors.
	void setBeats( const QList<unsigned long>& beats )
		{ m_beats = beats; QFrame::update(); }
	const QList<unsigned long>& beats() const
		{ return m_beats; }

	void clearBeats()
		{ m_beats.clear(); QFrame::update(); }

	// Refresh method.
	void refresh()
		{ updatePixmap(); QFrame::update(); }

protected:

	// Backing-store method.
	void updatePixmap()
	{
		qtractorClip *pClip = m_pForm->clip();
		if (pClip == nullptr)
			return;

		qtractorTrack *pTrack = pClip->track();
		if (pTrack == nullptr)
			return;

		qtractorTimeScale *pTimeScale = m_pForm->timeScale();
		if (pTimeScale == nullptr)
			return;

		const int w
			= pTimeScale->pixelFromFrame(m_pForm->rangeEnd())
			- pTimeScale->pixelFromFrame(m_pForm->rangeStart());
		const int h = QFrame::height();

		m_pixmap = QPixmap(w, h);
		m_pixmap.fill(QFrame::palette().base().color());

		// Render the actual clip region...
		QPainter painter(&m_pixmap);
		QColor bg = pTrack->background();
		const QPen pen(bg.darker());
		bg.setAlpha(192); // translucency...
	#ifdef CONFIG_GRADIENT
		QLinearGradient grad(0, 0, 0, h);
		grad.setColorAt(0.4, bg);
		grad.setColorAt(1.0, bg.darker(130));
		const QBrush brush(grad);
	#else
		const QBrush brush(bg);
	#endif
		painter.setPen(pen);
		painter.setBrush(brush);

		const unsigned long iClipOffset
			= m_pForm->rangeStart() - pClip->clipStart();
		const QRect rectClip(0, 0, w, h);
		painter.drawRect(rectClip);
		pClip->draw(&painter, rectClip, iClipOffset);
	}

	// Paint method...
	void paintEvent(QPaintEvent *pPaintEvent)
	{
		QPainter painter(this);

		// Render the scaled pixmap region...
		const qreal w = qreal(QFrame::width());
		const qreal h = qreal(QFrame::height());
		const qreal dw = qreal(m_pixmap.width()) / w;
		const qreal dh = qreal(m_pixmap.height()) / h;
		const QRectF& rect = QRectF(pPaintEvent->rect());
		painter.drawPixmap(rect, m_pixmap, QRectF(
			rect.x() * dw, rect.y() * dh,
			rect.width() * dw, rect.height() * dh));

		// Render beat lines...
		qtractorTimeScale *pTimeScale = m_pForm->timeScale();
		if (pTimeScale == nullptr)
			return;

		const unsigned long iRangeStart = m_pForm->rangeStart();
		const int x0 = pTimeScale->pixelFromFrame(iRangeStart);
		qreal x = 0;

		const float fTempo = pTimeScale->tempo();
		if (fTempo > 0.1f) {
			const float fBeatLength
				= 60.0f * float(pTimeScale->sampleRate()) / fTempo;
			if (fBeatLength > 0.0f) {
				unsigned short iBeat = 0;
				while (x < w) {
					const unsigned long iOffset
						= ::lrintf(float(++iBeat) * fBeatLength);
					const int x1
						= pTimeScale->pixelFromFrame(iRangeStart + iOffset);
					x = qreal(x1 - x0) / dw;
					painter.drawLine(QPointF(x, 0), QPointF(x, h));
				}
			}
		}

		if (!m_beats.isEmpty()) {
			qtractorClip *pClip = m_pForm->clip();
			if (pClip) {
				QColor color = Qt::red;
				color.setAlpha(120);
				painter.setPen(QPen(color, 3));
				foreach (unsigned long iOffset, m_beats) {
					const int x1
						= pTimeScale->pixelFromFrame(iRangeStart + iOffset);
					x = qreal(x1 - x0) / dw;
					painter.drawLine(QPointF(x, 0), QPointF(x, h));
				}
			}
		}
	}

	// Resize method...
	void resizeEvent(QResizeEvent *)
		{ updatePixmap(); }

private:

	// Instance variables.
	qtractorTempoAdjustForm *m_pForm;

	// Local double-buffering pixmap.
	QPixmap m_pixmap;

	// Extracted beat offsets.
	QList<unsigned long> m_beats;
};


//----------------------------------------------------------------------------
// qtractorTempoAdjustForm -- UI wrapper form.

// Constructor.
qtractorTempoAdjustForm::qtractorTempoAdjustForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Initialize local time scale.
	m_pTimeScale = new qtractorTimeScale();
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession)
		m_pTimeScale->copy(*pSession->timeScale());

	m_pClip = nullptr;
	m_pAudioClip = nullptr;

	m_iRangeStart = 0;
	m_iRangeEnd   = 0;

	m_pClipWidget = nullptr;

	m_pTempoTap = new QElapsedTimer();
	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	m_ui.RangeStartSpinBox->setTimeScale(m_pTimeScale);
	m_ui.RangeEndSpinBox->setTimeScale(m_pTimeScale);

	// FIXME: Set an absolute max. 30 seconds...
	m_ui.RangeEndSpinBox->setMaximum(30 * m_pTimeScale->sampleRate());

	m_ui.TempoSpinBox->setTempo(m_pTimeScale->tempo(), false);
	m_ui.TempoSpinBox->setBeatsPerBar(m_pTimeScale->beatsPerBar(), false);
	m_ui.TempoSpinBox->setBeatDivisor(m_pTimeScale->beatDivisor(), true);
	m_ui.TempoResetPushButton->setEnabled(false);

	// Set proper time scales display format...
	m_ui.FormatComboBox->setCurrentIndex(int(m_pTimeScale->displayFormat()));

	// Initialize dirty control state (nope).
	m_iDirtySetup = 0;
	m_iDirtyCount = 0;

	// Try to set minimal window positioning.
	adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.TempoSpinBox,
		SIGNAL(valueChanged(float, unsigned short, unsigned short)),
		SLOT(tempoChanged()));
	QObject::connect(m_ui.TempoTapPushButton,
		SIGNAL(clicked()),
		SLOT(tempoTap()));
	QObject::connect(m_ui.TempoDetectPushButton,
		SIGNAL(clicked()),
		SLOT(tempoDetect()));
	QObject::connect(m_ui.TempoResetPushButton,
		SIGNAL(clicked()),
		SLOT(tempoReset()));

	QObject::connect(m_ui.RangeStartSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(rangeStartChanged(unsigned long)));
	QObject::connect(m_ui.RangeStartSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.RangeEndSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(rangeEndChanged(unsigned long)));
	QObject::connect(m_ui.RangeEndSpinBox,
		SIGNAL(displayFormatChanged(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.RangeBeatsSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(rangeBeatsChanged(int)));
	QObject::connect(m_ui.FormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.AdjustPushButton,
		SIGNAL(clicked()),
		SLOT(tempoAdjust()));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorTempoAdjustForm::~qtractorTempoAdjustForm (void)
{
	setClip(nullptr);

	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
	if (m_pTempoTap)
		delete m_pTempoTap;
}


// Clip accessors.
void qtractorTempoAdjustForm::setClip ( qtractorClip *pClip )
{
	m_pClip = pClip;

	if (m_pClip) {
		const unsigned long	iClipStart = m_pClip->clipStart();
		const unsigned long	iClipEnd = iClipStart + m_pClip->clipLength();
		m_ui.RangeStartSpinBox->setMinimum(iClipStart);
		m_ui.RangeStartSpinBox->setMaximum(iClipEnd);
		m_ui.RangeEndSpinBox->setMinimum(iClipStart);
		m_ui.RangeEndSpinBox->setMaximum(iClipEnd);
		if (m_pClip->isClipSelected()) {
			m_iRangeStart = m_pClip->clipSelectStart();
			m_iRangeEnd   = m_pClip->clipSelectEnd();
		} else {
			m_iRangeStart = iClipStart;
			m_iRangeEnd   = iClipEnd;
		}
		m_ui.RangeStartSpinBox->setValue(m_iRangeStart, false);
		m_ui.RangeEndSpinBox->setValue(m_iRangeEnd, false);
	}

	if (m_pClip && m_pClip->track()
		&& (m_pClip->track())->trackType() == qtractorTrack::Audio)
		m_pAudioClip = static_cast<qtractorAudioClip *> (m_pClip);
	else
		m_pAudioClip = nullptr;

	if (m_pClipWidget) {
		delete m_pClipWidget;
		m_pClipWidget = nullptr;
	}

	if (m_pClip == nullptr)
		return;

	m_pClipWidget = new ClipWidget(this);
	m_pClipWidget->setMinimumHeight(160);
	m_ui.MainBoxSplitter->insertWidget(0, m_pClipWidget);
	m_ui.MainBoxSplitter->setStretchFactor(0, 2);

	if (m_pAudioClip) {
		m_ui.TempoDetectPushButton->setEnabled(true);
	} else {
		m_ui.TempoDetectPushButton->setEnabled(false);
		m_ui.TempoDetectPushButton->hide();
	}

	updateRangeLength(m_iRangeEnd - m_iRangeStart);
	updateRangeSelect();

	m_iDirtyCount = 0;
	stabilizeForm();
}


qtractorClip *qtractorTempoAdjustForm::clip (void) const
{
	return m_pClip;
}

qtractorAudioClip *qtractorTempoAdjustForm::audioClip (void) const
{
	return m_pAudioClip;
}


// Range accessors.
void qtractorTempoAdjustForm::setRangeStart ( unsigned long iRangeStart )
{
	++m_iDirtySetup;

	const unsigned long iMinRangeStart
		= m_ui.RangeStartSpinBox->minimum();
	const unsigned long iMaxRangeStart
		= m_ui.RangeStartSpinBox->maximum();
	if (iRangeStart < iMinRangeStart)
		iRangeStart = iMinRangeStart;
	else
	if (iRangeStart > iMaxRangeStart)
		iRangeStart = iMaxRangeStart;
	m_ui.RangeStartSpinBox->setValue(iRangeStart, false);
	updateRangeStart(iRangeStart);

	--m_iDirtySetup;
}

unsigned long qtractorTempoAdjustForm::rangeStart (void) const
{
	return m_ui.RangeStartSpinBox->value();
}


void qtractorTempoAdjustForm::setRangeEnd ( unsigned long iRangeEnd )
{
	++m_iDirtySetup;

	const unsigned long iMinRangeEnd
		= m_ui.RangeEndSpinBox->minimum();
	const unsigned long iMaxRangeEnd
		= m_ui.RangeEndSpinBox->maximum();
	if (iRangeEnd < iMinRangeEnd)
		iRangeEnd = iMinRangeEnd;
	else
	if (iRangeEnd > iMaxRangeEnd)
		iRangeEnd = iMaxRangeEnd;
	m_ui.RangeEndSpinBox->setValue(iRangeEnd, false);
	updateRangeEnd(iRangeEnd);

	--m_iDirtySetup;
}

unsigned long qtractorTempoAdjustForm::rangeEnd (void) const
{
	return m_ui.RangeEndSpinBox->value();
}


void qtractorTempoAdjustForm::setRangeBeats (
	unsigned short iRangeBeats, bool bRangeLength )
{
	++m_iDirtySetup;

	const unsigned short iMinRangeBeats
		= m_ui.RangeBeatsSpinBox->minimum();
	const unsigned short iMaxRangeBeats
		= m_ui.RangeBeatsSpinBox->maximum();
	if (iRangeBeats < iMinRangeBeats)
		iRangeBeats = iMinRangeBeats;
	else
	if (iRangeBeats > iMaxRangeBeats)
		iRangeBeats = iMaxRangeBeats;
	m_ui.RangeBeatsSpinBox->setValue(iRangeBeats);
	updateRangeBeats(iRangeBeats, bRangeLength);

	--m_iDirtySetup;
}

unsigned short qtractorTempoAdjustForm::rangeBeats (void) const
{
	return m_ui.RangeBeatsSpinBox->value();
}


// Accepted results accessors.
float qtractorTempoAdjustForm::tempo (void) const
{
	return m_ui.TempoSpinBox->tempo();
}

unsigned short qtractorTempoAdjustForm::beatsPerBar (void) const
{
	return m_ui.TempoSpinBox->beatsPerBar();
}

unsigned short qtractorTempoAdjustForm::beatDivisor (void) const
{
	return m_ui.TempoSpinBox->beatDivisor();
}


// Time-scale accessor.
qtractorTimeScale *qtractorTempoAdjustForm::timeScale (void) const
{
	return m_pTimeScale;
}


// Tempo signature has changed.
void qtractorTempoAdjustForm::tempoChanged (void)
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoChanged()");
#endif

	m_iTempoTap = 0;
	m_fTempoTap = 0.0f;

	const float fTempo = m_ui.TempoSpinBox->tempo();
	if (fTempo > 0.1f) {
		const float fBeatLength
			= 60.0f * float(m_pTimeScale->sampleRate()) / fTempo;
		if (fBeatLength > 0.0f) {
			const unsigned long iRangeStart
				= m_ui.RangeStartSpinBox->value();
			const unsigned long iRangeEnd
				= m_ui.RangeEndSpinBox->value();
			const float fRangeLength
				= float(iRangeEnd - iRangeStart);
			setRangeBeats(::lrintf(fRangeLength / fBeatLength), true);
		}
	}

	changed();
}


// Audio clip beat-detector method .
void qtractorTempoAdjustForm::tempoDetect (void)
{
	if (m_pAudioClip == nullptr)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoDetect()");
#endif

	qtractorTrack *pTrack = m_pAudioClip->track();
	if (pTrack == nullptr)
		return;

	qtractorAudioBus *pAudioBus
		= static_cast<qtractorAudioBus *> (pTrack->outputBus());
	if (pAudioBus == nullptr)
		return;

	QApplication::setOverrideCursor(QCursor(Qt::WaitCursor));
	m_ui.TempoDetectPushButton->setEnabled(false);

	const unsigned short iChannels = pAudioBus->channels();
	const unsigned int iSampleRate = m_pTimeScale->sampleRate();

	const unsigned long iRangeStart = m_ui.RangeStartSpinBox->value();
	const unsigned long iRangeEnd   = m_ui.RangeEndSpinBox->value();

	const unsigned long iOffset = iRangeStart - m_pAudioClip->clipStart();
	const unsigned long iLength = iRangeEnd - iRangeStart;

	QProgressBar *pProgressBar = nullptr;
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pProgressBar = pMainForm->progressBar();
	if (pProgressBar) {
		pProgressBar->setRange(0, iLength / 100);
		pProgressBar->reset();
		pProgressBar->show();
	}

	audioClipTempoDetectData data(iChannels, iSampleRate);

#ifdef CONFIG_LIBAUBIO

	float fTempoDetect = 0.0f;

	for (int n = 0; n < 5; ++n) { // 5 times at least!...
		m_pAudioClip->clipExport(audioClipTempoDetect, &data, iOffset, iLength);
		fTempoDetect = float(aubio_tempo_get_confidence(data.aubio));
		if (fTempoDetect > 0.1f)
			break;
	//	data.beats.clear();
		data.offset += iLength;
		if (pProgressBar)
			pProgressBar->setMaximum(data.offset / 100);
	}

	if (fTempoDetect > 0.1f) {
		const float fTempo
			= aubio_tempo_get_bpm(data.aubio);
		m_ui.TempoSpinBox->setTempo(::rintf(fTempo), true);
	}

	if (m_pClipWidget)
		m_pClipWidget->setBeats(data.beats);

#endif	// CONFIG_LIBAUBIO

#ifdef CONFIG_MINIBPM

	unsigned short i;

	const unsigned short iBeatsPerBar
		= m_ui.TempoSpinBox->beatsPerBar();
	for (i = 0; i < iChannels; ++i) {
		data.minibpm[i]->setBeatsPerBar(iBeatsPerBar);
	}

	m_pAudioClip->clipExport(audioClipTempoDetect, &data, iOffset, iLength);

	float fTempoSum = 0.0f;
	for (i = 0; i < iChannels; ++i) {
		fTempoSum += data.minibpm[i]->estimateTempo();
	}

	const float fTempo = fTempoSum / float(iChannels);
	if (fTempo > 0.1f) {
		m_ui.TempoSpinBox->setTempo(::rintf(fTempo), true);
		if (m_pClipWidget) {
			QList<unsigned long> beats;
			const unsigned long iBeatLength
				= ::lrintf(60.0f * float(iSampleRate) / fTempo);
			for (unsigned long n = iBeatLength; n < iLength; n += iBeatLength)
				beats.append(n);
			m_pClipWidget->setBeats(beats);
		}
	}

#endif	// CONFIG_MINIBPM

	if (pProgressBar)
		pProgressBar->hide();

	m_ui.TempoDetectPushButton->setEnabled(true);
	QApplication::restoreOverrideCursor();
}


// Reset to nominal tempo/time-signature.
void qtractorTempoAdjustForm::tempoReset (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoReset()");
#endif

	m_ui.TempoSpinBox->setTempo(m_pTimeScale->tempo(), false);
	m_ui.TempoSpinBox->setBeatsPerBar(m_pTimeScale->beatsPerBar(), false);
	m_ui.TempoSpinBox->setBeatDivisor(m_pTimeScale->beatDivisor(), false);
	m_ui.TempoResetPushButton->setEnabled(false);

	m_ui.RangeStartSpinBox->setValue(m_iRangeStart, false);
	m_ui.RangeEndSpinBox->setValue(m_iRangeEnd, false);

	updateRangeLength(m_iRangeEnd - m_iRangeStart);
	updateRangeSelect();

	m_iDirtyCount = 0;
	stabilizeForm();
}


// Adjust as instructed.
void qtractorTempoAdjustForm::tempoAdjust (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoAdjust()");
#endif

	const unsigned short iRangeBeats = m_ui.RangeBeatsSpinBox->value();
	if (iRangeBeats < 1)
		return;

	const unsigned long iRangeStart
		= m_ui.RangeStartSpinBox->value();
	const unsigned long iRangeEnd
		= m_ui.RangeEndSpinBox->value();

	const float fBeatLength
		= float(iRangeEnd - iRangeStart) / float(iRangeBeats);
	const float fTempo
		= 60.0f * float(m_pTimeScale->sampleRate()) / fBeatLength;

	m_ui.TempoSpinBox->setTempo(::rintf(fTempo), true);

//	tempoChanged();
}


// Tempo tap click.
void qtractorTempoAdjustForm::tempoTap (void)
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::tempoTap()");
#endif

	const int iTimeTap = m_pTempoTap->restart();

	if (iTimeTap < 200 || iTimeTap > 2000) { // Magic!
		m_iTempoTap = 0;
		m_fTempoTap = 0.0f;
		return;
	}

	const float fTempoTap = ::rintf(60000.0f / float(iTimeTap));
#if 0
	m_fTempoTap += fTempoTap;
	if (++m_iTempoTap > 2) {
		m_fTempoTap /= float(m_iTempoTap);
		m_ui.TempoSpinBox->setTempo(::rintf(m_fTempoTap), false);
		m_iTempoTap	= 1; // Median-like averaging...
	}
#else
	if (m_fTempoTap  > 0.0f) {
		m_fTempoTap *= 0.5f;
		m_fTempoTap += 0.5f * fTempoTap;
	} else {
		m_fTempoTap  = fTempoTap;
	}
	if (++m_iTempoTap > 2) {
		m_ui.TempoSpinBox->setTempo(::rintf(m_fTempoTap), false);
		m_iTempoTap	= 1; // Median-like averaging...
		m_fTempoTap = fTempoTap;
	}
#endif

//	tempoChanged();
}


// Adjust delta-value spin-boxes to new anchor frame.
void qtractorTempoAdjustForm::rangeStartChanged ( unsigned long iRangeStart )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::rangeStartChanged(%lu)", iRangeStart);
#endif

	updateRangeStart(iRangeStart);
	updateRangeSelect();
	changed();
}


void qtractorTempoAdjustForm::rangeEndChanged ( unsigned long iRangeEnd )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::rangeEndChanged(%lu)", iRangeEnd);
#endif

	updateRangeEnd(iRangeEnd);
	updateRangeSelect();
	changed();
}


void qtractorTempoAdjustForm::rangeBeatsChanged ( int iRangeBeats )
{
	if (m_iDirtySetup > 0)
		return;

#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::rangeBeatsChanged(%d)", iRangeBeats);
#endif

	updateRangeBeats(iRangeBeats, true);
	changed();
}


// Display format has changed.
void qtractorTempoAdjustForm::formatChanged ( int iDisplayFormat )
{
#ifdef CONFIG_DEBUG
	qDebug("qtractorTempoAdjustForm::formatChanged()");
#endif

	const bool bBlockSignals = m_ui.FormatComboBox->blockSignals(true);
	m_ui.FormatComboBox->setCurrentIndex(iDisplayFormat);

	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.RangeStartSpinBox->setDisplayFormat(displayFormat);
	m_ui.RangeEndSpinBox->setDisplayFormat(displayFormat);

	if (m_pTimeScale)
		m_pTimeScale->setDisplayFormat(displayFormat);

	m_ui.FormatComboBox->blockSignals(bBlockSignals);

	stabilizeForm();
}


// Dirty up settings.
void qtractorTempoAdjustForm::changed (void)
{
	if (m_iDirtySetup > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qtractorTempoAdjustForm::accept (void)
{
	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorTempoAdjustForm::reject (void)
{
	bool bReject = true;

	// Check if there's any pending changes...
	if (m_iDirtyCount > 0) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Discard | QMessageBox::Cancel;
		if (m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->isEnabled())
			buttons |= QMessageBox::Apply;
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Apply:
			accept();
			return;
		case QMessageBox::Discard:
			break;
		default: // Cancel.
			bReject = false;
		}
	}

	if (bReject)
		QDialog::reject();
}


// Adjust current range start/end...
void qtractorTempoAdjustForm::updateRangeStart ( unsigned long iRangeStart )
{
	updateRangeLength(m_ui.RangeEndSpinBox->value() - iRangeStart);
}

void qtractorTempoAdjustForm::updateRangeEnd ( unsigned long iRangeEnd )
{
	updateRangeLength(iRangeEnd - m_ui.RangeStartSpinBox->value());
}


// Adjust current range beat count from length...
void qtractorTempoAdjustForm::updateRangeLength ( unsigned long iRangeLength )
{
	const int iMaxRangeBeats // Follows from max. tempo = 300bpm.
		= int(60.0f * float(iRangeLength) / float(m_pTimeScale->sampleRate()));
	m_ui.RangeBeatsSpinBox->setMaximum(iMaxRangeBeats);

	const float fTempo = m_ui.TempoSpinBox->tempo();
	if (fTempo > 0.0f) {
		const float fBeatLength
			= 60.0f * float(m_pTimeScale->sampleRate()) / fTempo;
		if (fBeatLength > 0.0f)
			setRangeBeats(::lrintf(float(iRangeLength) / fBeatLength), false);
	}
}


// Repaint the graphics...
void qtractorTempoAdjustForm::updateRangeBeats (
	unsigned short iRangeBeats, bool bRangeLength )
{
	if (bRangeLength) {
		const float fTempo = m_ui.TempoSpinBox->tempo();
		if (fTempo > 0.0f) {
			const float fBeatLength
				= 60.0f * float(m_pTimeScale->sampleRate()) / fTempo;
			if (fBeatLength > 0.0f) {
				const unsigned long iRangeStart
					= m_ui.RangeStartSpinBox->value();
				const unsigned long iRangeLength
					= ::lrintf(float(iRangeBeats) * fBeatLength);
				setRangeEnd(iRangeStart + iRangeLength);
				updateRangeSelect();
			}
		}
		m_ui.TempoResetPushButton->setEnabled(true);
	}

	if (m_pClipWidget)
		m_pClipWidget->clearBeats();
}


// Adjust current selection edit/tail.
void qtractorTempoAdjustForm::updateRangeSelect (void)
{
	const unsigned long iRangeStart = m_ui.RangeStartSpinBox->value();
	const unsigned long iRangeEnd   = m_ui.RangeEndSpinBox->value();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		pSession->setEditHead(iRangeStart);
		pSession->setEditTail(iRangeEnd);
	}

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorTracks *pTracks = pMainForm->tracks();
		if (pTracks && m_pClip)  {
			pTracks->selectClipRange(m_pClip);
			pMainForm->selectNotifySlot(nullptr);
		}
	}

	if (m_pClipWidget)
		m_pClipWidget->refresh();
}


// Stabilize current form state.
void qtractorTempoAdjustForm::stabilizeForm (void)
{
	const unsigned long iRangeStart  = m_ui.RangeStartSpinBox->value();
	const unsigned long iRangeEnd    = m_ui.RangeEndSpinBox->value();
	const unsigned short iRangeBeats = m_ui.RangeBeatsSpinBox->value();
	const bool bValid = (iRangeEnd > iRangeStart && iRangeBeats > 0);
	m_ui.AdjustPushButton->setEnabled(bValid);
	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(
		bValid && m_iDirtyCount > 0);
}


//----------------------------------------------------------------------
// class qtractorTimeScaleClipTempoAdjustCommand - implementation.
//

// Constructor.
qtractorTempoAdjustCommand::qtractorTempoAdjustCommand (
	qtractorTempoAdjustForm *pForm )
	: qtractorCommand(QObject::tr("clip tempo"))
{
	// Save current clip range...
	m_pClip = pForm->clip();

	m_iRangeStart = pForm->rangeStart();
	m_iRangeEnd   = pForm->rangeEnd();

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Avoid automatic time stretching option for audio clips...
		const bool bAutoTimeStretch
			= pSession->isAutoTimeStretch();
		pSession->setAutoTimeStretch(false);
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		qtractorTimeScale::Cursor& cursor = pTimeScale->cursor();
		qtractorTimeScale::Node *pNode = cursor.seekFrame(m_pClip->clipStart());
		m_pTimeScaleNodeCommand = new qtractorTimeScaleUpdateNodeCommand(
			pTimeScale, pNode->frame, pForm->tempo(), 2,
			pForm->beatsPerBar(), pForm->beatDivisor());
		pSession->setAutoTimeStretch(bAutoTimeStretch);
	} else {
		m_pTimeScaleNodeCommand = nullptr;
	}

	setFlags(m_pTimeScaleNodeCommand->flags());
}


// Destructor.
qtractorTempoAdjustCommand::~qtractorTempoAdjustCommand (void)
{
	if (m_pTimeScaleNodeCommand)
		delete m_pTimeScaleNodeCommand;
}


// Tempo-adjust command methods.
bool qtractorTempoAdjustCommand::redo (void)
{
	if (m_pTimeScaleNodeCommand == nullptr)
		return false;

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return false;

	// Save clip range...
	unsigned long iClipStartTime = m_pClip->clipStartTime();
	unsigned long iClipStart = pSession->frameFromTick(iClipStartTime);
	const unsigned long iRangeStart = m_iRangeStart - iClipStart;
	const unsigned long iRangeEnd   = m_iRangeEnd - iClipStart;

	// Do it!,,,
	const bool bRedo = m_pTimeScaleNodeCommand->redo();

	// Restore clip range...
	iClipStart = pSession->frameFromTick(iClipStartTime);
	m_iRangeStart = iClipStart + iRangeStart;
	m_iRangeEnd   = iClipStart + iRangeEnd;

	pSession->setEditHead(m_iRangeStart);
	pSession->setEditTail(m_iRangeEnd);

	return bRedo;
}

bool qtractorTempoAdjustCommand::undo (void) { return redo(); }


// end of qtractorTempoAdjustForm.cpp
