// qtractorMidiToolsForm.cpp
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

#include "qtractorMidiToolsForm.h"

#include "qtractorAbout.h"
#include "qtractorMidiEditor.h"
#include "qtractorMidiClip.h"

#include "qtractorMidiEditCommand.h"

#include "qtractorTimeScaleCommand.h"

#include "qtractorOptions.h"
#include "qtractorSession.h"

#include <QPainter>
#include <QPainterPath>
#include <QMessageBox>
#include <QPushButton>

#include <ctime>
#include <cmath>


// This shall hold the default preset name.
static QString g_sDefPreset;


class TimeshiftCurve : public QWidget
{
public:

	// Constructor.
	TimeshiftCurve(QWidget *pParent = nullptr) : QWidget(pParent), m_p(0.0f) {}

	// Accessors.
	void setTimeshift(float p) { m_p = p; update(); }

	// Characteristic method.
	static float timeshift(float t, float p)
	{
	#if 0//TIMESHIFT_LOGSCALE
		if (p > 0.0f)
			t = ::sqrtf(t * ::powf(1.0f - (10.0f * ::logf(t) / p), 0.1f * p));
		else
		if (p < 0.0f)
			t = ::sqrtf(1.0f - ((1.0f - t) * ::powf(1.0f + (::logf(1.0f - t) / p), -p)));
	#else
		if (p > 0.0f)
			t = 1.0f - ::powf(1.0f - t, 1.0f / (1.0f - 0.01f * (p + 1e-9f)));
		else
		if (p < 0.0f)
			t = 1.0f - ::powf(1.0f - t, 1.0f + 0.01f * p);
	#endif
		return t;
	}

protected:

	// Paint event method.
	void paintEvent(QPaintEvent */*pPaintEvent*/)
	{
		QPainter painter(this);

		painter.setRenderHint(QPainter::Antialiasing);

		const int w = QWidget::width();
		const int h = QWidget::height();

		const int x0 = w >> 1;
		const int y0 = h >> 1;

		QPen pen(Qt::gray);
		painter.setPen(pen);
		painter.drawLine(x0, 0, x0, h);
		painter.drawLine(0, y0, w, y0);

		QPainterPath path;
		path.moveTo(0, h);
		for (int x = 4; x < w; x += 4) {
			const float t = float(x) / float(w);
			path.lineTo(x, h - int(timeshift(t, m_p) * float(h)));
		}
		path.lineTo(w, 0);

		pen.setColor(Qt::red);
		pen.setWidth(2);
		painter.setPen(pen);
		painter.drawPath(path);
	}

private:

	// Instance variables
	float m_p;
};


//----------------------------------------------------------------------------
// qtractorMidiToolsForm -- UI wrapper form.

// Constructor.
qtractorMidiToolsForm::qtractorMidiToolsForm ( QWidget *pParent )
	: QDialog(pParent)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	// Window modality (let plugin/tool windows rave around).
	QDialog::setWindowModality(Qt::WindowModal);

	m_pTimeScale = nullptr;

	m_iDirtyCount = 0;
	m_iUpdate = 0;

	// Special timeshift characteristic curve display...
	m_pTimeshiftCurve = new TimeshiftCurve();

	QVBoxLayout *pFrameLayout = new QVBoxLayout();
	pFrameLayout->setContentsMargins(1, 1, 1, 1);
	pFrameLayout->addWidget(m_pTimeshiftCurve);
	m_ui.TimeshiftFrame->setLayout(pFrameLayout);

	m_ui.PresetNameComboBox->setValidator(
		new QRegularExpressionValidator(
			QRegularExpression("[\\w-]+"), m_ui.PresetNameComboBox));
	m_ui.PresetNameComboBox->setInsertPolicy(QComboBox::NoInsert);

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");

	// Set some time spin-box specialties...
	m_ui.TransposeTimeSpinBox->setDeltaValue(true);
	m_ui.ResizeDurationSpinBox->setDeltaValue(true);

	// Reinitialize random seed.
	::srand(::time(nullptr));

	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession) {
		// Copy from global time-scale instance...
		m_pTimeScale = new qtractorTimeScale(*pSession->timeScale());
		m_ui.TransposeTimeSpinBox->setTimeScale(m_pTimeScale);
		m_ui.ResizeDurationSpinBox->setTimeScale(m_pTimeScale);
		// Fill-up snap-per-beat items...
		const QIcon snapIcon(":/images/itemBeat.png");
		const QSize snapIconSize(8, 16);
		const QStringList& snapItems = qtractorTimeScale::snapItems();
		QStringListIterator snapIter(snapItems);
		m_ui.QuantizeTimeComboBox->clear();
		m_ui.QuantizeTimeComboBox->setIconSize(snapIconSize);
	//	snapIter.toFront();
		snapIter.next();
		while (snapIter.hasNext())
			m_ui.QuantizeTimeComboBox->addItem(snapIcon, snapIter.next());
	//	m_ui.QuantizeTimeComboBox->insertItems(0, snapItems);
		m_ui.QuantizeDurationComboBox->clear();
		m_ui.QuantizeDurationComboBox->setIconSize(snapIconSize);
		snapIter.toFront();
		snapIter.next();
		while (snapIter.hasNext())
			m_ui.QuantizeDurationComboBox->addItem(snapIcon, snapIter.next());
	//	m_ui.QuantizeDurationComboBox->insertItems(0, snapItems);
		m_ui.QuantizeSwingComboBox->clear();
		m_ui.QuantizeSwingComboBox->setIconSize(snapIconSize);
		snapIter.toFront();
		snapIter.next();
		while (snapIter.hasNext())
			m_ui.QuantizeSwingComboBox->addItem(snapIcon, snapIter.next());
	//	m_ui.QuantizeSwingComboBox->insertItems(0, snapItems);
		m_ui.ResizeLegatoLengthComboBox->clear();
		m_ui.ResizeLegatoLengthComboBox->setIconSize(snapIconSize);
		snapIter.toFront();
		snapIter.next();
		while (snapIter.hasNext())
			m_ui.ResizeLegatoLengthComboBox->addItem(snapIcon, snapIter.next());
	//	m_ui.ResizeLegatoLengthComboBox->insertItems(0, snapItems);
		m_ui.ResizeSplitLengthComboBox->clear();
		m_ui.ResizeSplitLengthComboBox->setIconSize(snapIconSize);
		snapIter.toFront();
		snapIter.next();
		while (snapIter.hasNext())
			m_ui.ResizeSplitLengthComboBox->addItem(snapIcon, snapIter.next());
	//	m_ui.ResizeSplitLengthComboBox->insertItems(0, snapItems);
		// Default quantization value...
		unsigned short iSnapPerBeat = m_pTimeScale->snapPerBeat();
		if (iSnapPerBeat > 0)
			--iSnapPerBeat;
		const int iSnapIndex = qtractorTimeScale::indexFromSnap(iSnapPerBeat);
		m_ui.QuantizeTimeComboBox->setCurrentIndex(iSnapIndex);
		m_ui.QuantizeDurationComboBox->setCurrentIndex(iSnapIndex);
		m_ui.QuantizeSwingComboBox->setCurrentIndex(0);
		m_ui.QuantizeSwingTypeComboBox->setCurrentIndex(0);
		m_ui.ResizeLegatoLengthComboBox->setCurrentIndex(iSnapIndex);
		m_ui.ResizeSplitLengthComboBox->setCurrentIndex(iSnapIndex);
		// Initial tempo-ramp range...
		if (pSession->editHead() < pSession->editTail()) {
			qtractorTimeScale::Cursor cursor(m_pTimeScale);
			qtractorTimeScale::Node *pNode = cursor.seekFrame(pSession->editHead());
			const float T1 = pNode->tempo;
			const unsigned short iBeatsPerBar1
				= pNode->beatsPerBar;
			const unsigned short iBeatDivisor1
				= pNode->beatDivisor;
			qtractorTimeScale::Node *pPrev = pNode->prev();
			const float T0 = pPrev ? pPrev->tempo : T1;
			const unsigned short iBeatsPerBar0
				= pPrev ? pPrev->beatsPerBar : iBeatsPerBar1;
			const unsigned short iBeatDivisor0
				= pPrev ? pPrev->beatDivisor : iBeatDivisor1;
			m_ui.TemporampFromSpinBox->setTempo(T0);
			m_ui.TemporampFromSpinBox->setBeatsPerBar(iBeatsPerBar0);
			m_ui.TemporampFromSpinBox->setBeatDivisor(iBeatDivisor0);
			m_ui.TemporampToSpinBox->setTempo(T1);
			m_ui.TemporampToSpinBox->setBeatsPerBar(iBeatsPerBar1);
			m_ui.TemporampToSpinBox->setBeatDivisor(iBeatDivisor1);
		} else {
			m_ui.Timeshift->setEnabled(false);
			m_ui.Temporamp->setEnabled(false);
		}
	}

	// Scale-quantize stuff...
	m_ui.QuantizeScaleKeyComboBox->clear();
	m_ui.QuantizeScaleKeyComboBox->insertItems(0,
		qtractorMidiEditor::scaleKeyNames());
	m_ui.QuantizeScaleKeyComboBox->setCurrentIndex(0);
	m_ui.QuantizeScaleComboBox->clear();
	m_ui.QuantizeScaleComboBox->insertItems(0,
		qtractorMidiEditor::scaleTypeNames());
	m_ui.QuantizeScaleComboBox->setCurrentIndex(0);

	// Choose BBT to be default format here.
	formatChanged(qtractorTimeScale::BBT);

	// Load initial preset names;
	loadPreset(g_sDefPreset);
	timeshiftSpinBoxChanged(m_ui.TimeshiftSpinBox->value());
	refreshPresets();

	// Try to restore old window positioning.
	// adjustSize();

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(presetChanged(const QString&)));
	QObject::connect(m_ui.PresetNameComboBox,
		SIGNAL(activated(int)),
		SLOT(presetActivated(int)));
	QObject::connect(m_ui.PresetSaveToolButton,
		SIGNAL(clicked()),
		SLOT(presetSave()));
	QObject::connect(m_ui.PresetDeleteToolButton,
		SIGNAL(clicked()),
		SLOT(presetDelete()));

	QObject::connect(m_ui.QuantizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.QuantizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeTimeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeTimeSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeDurationComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeDurationSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeSwingCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeSwingComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeSwingSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeSwingTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeSwingSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeScaleCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeScaleKeyComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.QuantizeScaleComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));

	QObject::connect(m_ui.TransposeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TransposeNoteCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeNoteSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeTimeSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.TransposeFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.TransposeReverseCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));

	QObject::connect(m_ui.NormalizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.NormalizePercentCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizePercentSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.NormalizeValueSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));

	QObject::connect(m_ui.RandomizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RandomizeNoteCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeNoteSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeTimeSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeDurationSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RandomizeValueSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));

	QObject::connect(m_ui.ResizeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.ResizeDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeDurationSpinBox,
		SIGNAL(valueChanged(unsigned long)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeDurationFormatComboBox,
		SIGNAL(activated(int)),
		SLOT(formatChanged(int)));
	QObject::connect(m_ui.ResizeValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeValueSpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeValue2ComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeValue2SpinBox,
		SIGNAL(valueChanged(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeLegatoCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeLegatoTypeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeLegatoLengthComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeLegatoModeComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeJoinCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeSplitCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeSplitLengthComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));
	QObject::connect(m_ui.ResizeSplitOffsetComboBox,
		SIGNAL(activated(int)),
		SLOT(changed()));

	QObject::connect(m_ui.RescaleCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.RescaleTimeCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleTimeSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleDurationSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleValueCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleValueSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(changed()));
	QObject::connect(m_ui.RescaleInvertCheckBox,
		 SIGNAL(toggled(bool)),
		 SLOT(changed()));

	QObject::connect(m_ui.TimeshiftCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TimeshiftSpinBox,
		SIGNAL(valueChanged(double)),
		SLOT(timeshiftSpinBoxChanged(double)));
	QObject::connect(m_ui.TimeshiftSlider,
		SIGNAL(valueChanged(int)),
		SLOT(timeshiftSliderChanged(int)));
	QObject::connect(m_ui.TimeshiftDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));

	QObject::connect(m_ui.TemporampCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.TemporampFromSpinBox,
		SIGNAL(valueChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.TemporampToSpinBox,
		SIGNAL(valueChanged(const QString&)),
		SLOT(changed()));
	QObject::connect(m_ui.TemporampDurationCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(changed()));

	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtonBox,
		SIGNAL(rejected()),
		SLOT(reject()));
}


// Destructor.
qtractorMidiToolsForm::~qtractorMidiToolsForm (void)
{
	qDeleteAll(m_timeScaleNodeCommands);
	m_timeScaleNodeCommands.clear();

	// Don't forget to get rid of local time-scale instance...
	if (m_pTimeScale)
		delete m_pTimeScale;
}


// Set (initial) tool page.
void qtractorMidiToolsForm::setToolIndex ( int iToolIndex )
{
	// Set the proper tool page.
	m_ui.ToolTabWidget->setCurrentIndex(iToolIndex);
	switch (iToolIndex) {
	case qtractorMidiEditor::Quantize:
		m_ui.QuantizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Transpose:
		m_ui.TransposeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Normalize:
		m_ui.NormalizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Randomize:
		m_ui.RandomizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Resize:
		m_ui.ResizeCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Rescale:
		m_ui.RescaleCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Timeshift:
		m_ui.TimeshiftCheckBox->setChecked(true);
		break;
	case qtractorMidiEditor::Temporamp:
		m_ui.TemporampCheckBox->setChecked(true);
		break;
	default:
		break;
	}

	// Done.
	stabilizeForm();
}


// Retrieve the current export type, if the case arises.
int qtractorMidiToolsForm::toolIndex (void) const
{
	return m_ui.ToolTabWidget->currentIndex();
}


// Preset management methods...
void qtractorMidiToolsForm::loadPreset ( const QString& sPreset )
{
	// An existing preset is about to be loaded...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QVariant> vlist;
		QSettings& settings = pOptions->settings();
		// Get the preset entry...
		settings.beginGroup("/MidiTools");
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.beginGroup('/' + sPreset);
		// Quantize tool...
		vlist = settings.value("/Quantize").toList();
		if (vlist.count() > 4) {
		//	m_ui.QuantizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.QuantizeTimeCheckBox->setChecked(vlist[1].toBool());
			m_ui.QuantizeTimeComboBox->setCurrentIndex(vlist[2].toInt());
			m_ui.QuantizeDurationCheckBox->setChecked(vlist[3].toBool());
			m_ui.QuantizeDurationComboBox->setCurrentIndex(vlist[4].toInt());
		}
		// Swing-quantize tool...
		if (vlist.count() > 8) {
			m_ui.QuantizeSwingCheckBox->setChecked(vlist[5].toBool());
			m_ui.QuantizeSwingComboBox->setCurrentIndex(vlist[6].toInt());
			m_ui.QuantizeSwingSpinBox->setValue(vlist[7].toDouble());
			m_ui.QuantizeSwingTypeComboBox->setCurrentIndex(vlist[8].toInt());
		}
		// Percent-quantize tool...
		if (vlist.count() > 10) {
			m_ui.QuantizeTimeSpinBox->setValue(vlist[9].toDouble());
			m_ui.QuantizeDurationSpinBox->setValue(vlist[10].toDouble());
		}
		// Scale-quantize tool...
		if (vlist.count() > 13) {
			m_ui.QuantizeScaleCheckBox->setChecked(vlist[11].toBool());
			m_ui.QuantizeScaleKeyComboBox->setCurrentIndex(vlist[12].toInt());
			m_ui.QuantizeScaleComboBox->setCurrentIndex(vlist[13].toInt());
		}
		// Transpose tool...
		vlist = settings.value("/Transpose").toList();
		if (vlist.count() > 4) {
		//	m_ui.TransposeCheckBox->setChecked(vlist[0].toBool());
			m_ui.TransposeNoteCheckBox->setChecked(vlist[1].toBool());
			m_ui.TransposeNoteSpinBox->setValue(vlist[2].toInt());
			m_ui.TransposeTimeCheckBox->setChecked(vlist[3].toBool());
			m_ui.TransposeTimeSpinBox->setValue(vlist[4].toUInt());
		}
		// Transpose/reverse tool...
		if (vlist.count() > 5)
			m_ui.TransposeReverseCheckBox->setChecked(vlist[5].toBool());
		// Normalize tool...
		vlist = settings.value("/Normalize").toList();
		if (vlist.count() > 4) {
		//	m_ui.NormalizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.NormalizePercentCheckBox->setChecked(vlist[1].toBool());
			m_ui.NormalizePercentSpinBox->setValue(vlist[2].toDouble());
			m_ui.NormalizeValueCheckBox->setChecked(vlist[3].toBool());
			m_ui.NormalizeValueSpinBox->setValue(vlist[4].toInt());
		}
		// Randomize tool...
		vlist = settings.value("/Randomize").toList();
		if (vlist.count() > 8) {
		//	m_ui.RandomizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.RandomizeNoteCheckBox->setChecked(vlist[1].toBool());
			m_ui.RandomizeNoteSpinBox->setValue(vlist[2].toDouble());
			m_ui.RandomizeTimeCheckBox->setChecked(vlist[3].toBool());
			m_ui.RandomizeTimeSpinBox->setValue(vlist[4].toDouble());
			m_ui.RandomizeDurationCheckBox->setChecked(vlist[5].toBool());
			m_ui.RandomizeDurationSpinBox->setValue(vlist[6].toDouble());
			m_ui.RandomizeValueCheckBox->setChecked(vlist[7].toBool());
			m_ui.RandomizeValueSpinBox->setValue(vlist[8].toDouble());
		}
		// Resize tool...
		vlist = settings.value("/Resize").toList();
		if (vlist.count() > 4) {
		//	m_ui.ResizeCheckBox->setChecked(vlist[0].toBool());
			m_ui.ResizeValueCheckBox->setChecked(vlist[1].toBool());
			m_ui.ResizeValueSpinBox->setValue(vlist[2].toInt());
			m_ui.ResizeDurationCheckBox->setChecked(vlist[3].toBool());
			m_ui.ResizeDurationSpinBox->setValue(vlist[4].toUInt());
		}
		// Resize value mode tool...
		if (vlist.count() > 6) {
			m_ui.ResizeValue2ComboBox->setCurrentIndex(vlist[5].toInt());
			m_ui.ResizeValue2SpinBox->setValue(vlist[6].toInt());
		}
		// Resize legato mode tool...
		if (vlist.count() > 10) {
			m_ui.ResizeLegatoCheckBox->setChecked(vlist[7].toBool());
			m_ui.ResizeLegatoTypeComboBox->setCurrentIndex(vlist[8].toInt());
			m_ui.ResizeLegatoLengthComboBox->setCurrentIndex(vlist[9].toInt());
			m_ui.ResizeLegatoModeComboBox->setCurrentIndex(vlist[10].toInt());
		}
		// Resize join/split tool...
		if (vlist.count() > 14) {
			m_ui.ResizeJoinCheckBox->setChecked(vlist[11].toBool());
			m_ui.ResizeSplitCheckBox->setChecked(vlist[12].toBool());
			m_ui.ResizeSplitLengthComboBox->setCurrentIndex(vlist[13].toInt());
			m_ui.ResizeSplitOffsetComboBox->setCurrentIndex(vlist[14].toInt());
		}
		// Rescale tool...
		vlist = settings.value("/Rescale").toList();
		if (vlist.count() > 6) {
		//	m_ui.RescaleCheckBox->setChecked(vlist[0].toBool());
			m_ui.RescaleTimeCheckBox->setChecked(vlist[1].toBool());
			m_ui.RescaleTimeSpinBox->setValue(vlist[2].toDouble());
			m_ui.RescaleDurationCheckBox->setChecked(vlist[3].toBool());
			m_ui.RescaleDurationSpinBox->setValue(vlist[4].toDouble());
			m_ui.RescaleValueCheckBox->setChecked(vlist[5].toBool());
			m_ui.RescaleValueSpinBox->setValue(vlist[6].toDouble());
		}
		// Rescale/invert tool...
		if (vlist.count() > 7)
			m_ui.RescaleInvertCheckBox->setChecked(vlist[7].toBool());
		// Timeshift tool...
		vlist = settings.value("/Timeshift").toList();
		if (vlist.count() > 2) {
		//	m_ui.TimeshiftCheckBox->setChecked(vlist[0].toBool());
			m_ui.TimeshiftSpinBox->setValue(vlist[1].toDouble());
			m_ui.TimeshiftDurationCheckBox->setChecked(vlist[2].toBool());
		}
		// Temporamp tool...
		vlist = settings.value("/Temporamp").toList();
		if (vlist.count() > 3) {
		//	m_ui.TemporampCheckBox->setChecked(vlist[0].toBool());
		//	m_ui.TemporampFromSpinBox->setTempo(vlist[1].toDouble());
			m_ui.TemporampToSpinBox->setTempo(float(vlist[2].toDouble()));
			m_ui.TemporampDurationCheckBox->setChecked(float(vlist[3].toBool()));
		}
		// All loaded.
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.endGroup();
		settings.endGroup();
	}
}


void qtractorMidiToolsForm::savePreset ( const QString& sPreset )
{
	// The current state preset is about to be saved...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QList<QVariant> vlist;
		QSettings& settings = pOptions->settings();
		// Set preset entry...
		settings.beginGroup("/MidiTools");
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.beginGroup('/' + sPreset);
		// Quantize tool...
		vlist.clear();
		vlist.append(m_ui.QuantizeCheckBox->isChecked());
		vlist.append(m_ui.QuantizeTimeCheckBox->isChecked());
		vlist.append(m_ui.QuantizeTimeComboBox->currentIndex());
		vlist.append(m_ui.QuantizeDurationCheckBox->isChecked());
		vlist.append(m_ui.QuantizeDurationComboBox->currentIndex());
		vlist.append(m_ui.QuantizeSwingCheckBox->isChecked());
		vlist.append(m_ui.QuantizeSwingComboBox->currentIndex());
		vlist.append(m_ui.QuantizeSwingSpinBox->value());
		vlist.append(m_ui.QuantizeSwingTypeComboBox->currentIndex());
		vlist.append(m_ui.QuantizeTimeSpinBox->value());
		vlist.append(m_ui.QuantizeDurationSpinBox->value());
		vlist.append(m_ui.QuantizeScaleCheckBox->isChecked());
		vlist.append(m_ui.QuantizeScaleKeyComboBox->currentIndex());
		vlist.append(m_ui.QuantizeScaleComboBox->currentIndex());
		settings.setValue("/Quantize", vlist);
		// Transpose tool...
		vlist.clear();
		vlist.append(m_ui.TransposeCheckBox->isChecked());
		vlist.append(m_ui.TransposeNoteCheckBox->isChecked());
		vlist.append(m_ui.TransposeNoteSpinBox->value());
		vlist.append(m_ui.TransposeTimeCheckBox->isChecked());
		vlist.append((unsigned int) m_ui.TransposeTimeSpinBox->value());
		vlist.append(m_ui.TransposeReverseCheckBox->isChecked());
		settings.setValue("/Transpose", vlist);
		// Normalize tool...
		vlist.clear();
		vlist.append(m_ui.NormalizeCheckBox->isChecked());
		vlist.append(m_ui.NormalizePercentCheckBox->isChecked());
		vlist.append(m_ui.NormalizePercentSpinBox->value());
		vlist.append(m_ui.NormalizeValueCheckBox->isChecked());
		vlist.append(m_ui.NormalizeValueSpinBox->value());
		settings.setValue("/Normalize", vlist);
		// Randomize tool...
		vlist.clear();
		vlist.append(m_ui.RandomizeCheckBox->isChecked());
		vlist.append(m_ui.RandomizeNoteCheckBox->isChecked());
		vlist.append(m_ui.RandomizeNoteSpinBox->value());
		vlist.append(m_ui.RandomizeTimeCheckBox->isChecked());
		vlist.append(m_ui.RandomizeTimeSpinBox->value());
		vlist.append(m_ui.RandomizeDurationCheckBox->isChecked());
		vlist.append(m_ui.RandomizeDurationSpinBox->value());
		vlist.append(m_ui.RandomizeValueCheckBox->isChecked());
		vlist.append(m_ui.RandomizeValueSpinBox->value());
		settings.setValue("/Randomize", vlist);
		// Resize tool...
		vlist.clear();
		vlist.append(m_ui.ResizeCheckBox->isChecked());
		vlist.append(m_ui.ResizeValueCheckBox->isChecked());
		vlist.append(m_ui.ResizeValueSpinBox->value());
		vlist.append(m_ui.ResizeDurationCheckBox->isChecked());
		vlist.append((unsigned int) m_ui.ResizeDurationSpinBox->value());
		vlist.append(m_ui.ResizeValue2ComboBox->currentIndex());
		vlist.append(m_ui.ResizeValue2SpinBox->value());
		vlist.append(m_ui.ResizeLegatoCheckBox->isChecked());
		vlist.append(m_ui.ResizeLegatoTypeComboBox->currentIndex());
		vlist.append(m_ui.ResizeLegatoLengthComboBox->currentIndex());
		vlist.append(m_ui.ResizeLegatoModeComboBox->currentIndex());
		vlist.append(m_ui.ResizeJoinCheckBox->isChecked());
		vlist.append(m_ui.ResizeSplitCheckBox->isChecked());
		vlist.append(m_ui.ResizeSplitLengthComboBox->currentIndex());
		vlist.append(m_ui.ResizeSplitOffsetComboBox->currentIndex());
		settings.setValue("/Resize", vlist);
		// Rescale tool...
		vlist.clear();
		vlist.append(m_ui.RescaleCheckBox->isChecked());
		vlist.append(m_ui.RescaleTimeCheckBox->isChecked());
		vlist.append(m_ui.RescaleTimeSpinBox->value());
		vlist.append(m_ui.RescaleDurationCheckBox->isChecked());
		vlist.append(m_ui.RescaleDurationSpinBox->value());
		vlist.append(m_ui.RescaleValueCheckBox->isChecked());
		vlist.append(m_ui.RescaleValueSpinBox->value());
		vlist.append(m_ui.RescaleInvertCheckBox->isChecked());
		settings.setValue("/Rescale", vlist);
		// Timeshift tool...
		vlist.clear();
		vlist.append(m_ui.TimeshiftCheckBox->isChecked());
		vlist.append(m_ui.TimeshiftSpinBox->value());
		vlist.append(m_ui.TimeshiftDurationCheckBox->isChecked());
		settings.setValue("/Timeshift", vlist);
		// Temporamp tool...
		vlist.clear();
		vlist.append(m_ui.TemporampCheckBox->isChecked());
		vlist.append(m_ui.TemporampFromSpinBox->tempo());
		vlist.append(m_ui.TemporampToSpinBox->tempo());
		vlist.append(m_ui.TemporampDurationCheckBox->isChecked());
		settings.setValue("/Temporamp", vlist);
		// All saved.
		if (!sPreset.isEmpty() && sPreset != g_sDefPreset)
			settings.endGroup();
		settings.endGroup();
	}
}


// Preset list loader.
void qtractorMidiToolsForm::refreshPresets (void)
{
	++m_iUpdate;

	const QString sOldPreset = m_ui.PresetNameComboBox->currentText();
	m_ui.PresetNameComboBox->clear();

	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/MidiTools");
		m_ui.PresetNameComboBox->insertItems(0, settings.childGroups());
		m_ui.PresetNameComboBox->model()->sort(0);
		settings.endGroup();
	}
	m_ui.PresetNameComboBox->addItem(g_sDefPreset);
	m_ui.PresetNameComboBox->setEditText(sOldPreset);

	m_iDirtyCount = 0;
	--m_iUpdate;

	stabilizeForm();
}


// Preset management slots...
void qtractorMidiToolsForm::presetChanged ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty()
		&& m_ui.PresetNameComboBox->findText(sPreset) >= 0)
		++m_iDirtyCount;

	stabilizeForm();
}


void qtractorMidiToolsForm::presetActivated ( int iPreset )
{
	++m_iUpdate;

	loadPreset(m_ui.PresetNameComboBox->itemText(iPreset));

	m_iDirtyCount = 0;
	--m_iUpdate;

	stabilizeForm();
}


void qtractorMidiToolsForm::presetSave (void)
{
	savePreset(m_ui.PresetNameComboBox->currentText());

	refreshPresets();
}


void qtractorMidiToolsForm::presetDelete (void)
{
	if (m_iUpdate > 0)
		return;

	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// A preset entry is about to be deleted...
	qtractorOptions *pOptions = qtractorOptions::getInstance();
	if (pOptions) {
		// Prompt user if he/she's sure about this...
		if (pOptions->bConfirmRemove) {
			if (QMessageBox::warning(this,
				tr("Warning"),
				tr("About to delete preset:\n\n"
				"\"%1\"\n\n"
				"Are you sure?")
				.arg(sPreset),
				QMessageBox::Ok | QMessageBox::Cancel)
				== QMessageBox::Cancel)
				return;
		}
		// Go ahead...
		QSettings& settings = pOptions->settings();
		settings.beginGroup("/MidiTools");
		settings.remove(sPreset);
		settings.endGroup();
		refreshPresets();
	}
}


// Create edit command based on given selection.
qtractorMidiEditCommand *qtractorMidiToolsForm::midiEditCommand (
	qtractorMidiClip *pMidiClip, qtractorMidiEditSelect *pSelect,
	unsigned long iTimeOffset, unsigned long iTimeStart, unsigned long iTimeEnd )
{
	qtractorSession *pSession = qtractorSession::getInstance();
	if (pSession == nullptr)
		return nullptr;

	// Create command, it will be handed over...
	qtractorMidiEditCommand *pMidiEditCommand
		= new qtractorMidiEditCommand(pMidiClip, tr("none"));

	// Set composite command title.
	QStringList tools;
	if (m_ui.QuantizeCheckBox->isChecked())
		tools.append(tr("quantize"));
	if (m_ui.TransposeCheckBox->isChecked())
		tools.append(tr("transpose"));
	if (m_ui.NormalizeCheckBox->isChecked())
		tools.append(tr("normalize"));
	if (m_ui.RandomizeCheckBox->isChecked())
		tools.append(tr("randomize"));
	if (m_ui.ResizeCheckBox->isChecked())
		tools.append(tr("resize"));
	if (m_ui.RescaleCheckBox->isChecked())
		tools.append(tr("rescale"));
	if (m_ui.TimeshiftCheckBox->isChecked())
		tools.append(tr("timeshift"));
	if (m_ui.TemporampCheckBox->isChecked())
		tools.append(tr("temporamp"));
	pMidiEditCommand->setName(tools.join(", "));

	QList<qtractorMidiEvent *> items = pSelect->items().keys();

	if (m_ui.ResizeCheckBox->isChecked()
		&& m_ui.ResizeLegatoCheckBox->isChecked()) {
		// Sort events in reverse event time...
		struct ReverseEventTime {
			bool operator() (qtractorMidiEvent *ev1, qtractorMidiEvent *ev2) const
				{ return (ev1->time() > ev2->time()); }
		};
		std::sort(items.begin(), items.end(), ReverseEventTime());
	}

	QList<qtractorMidiEvent *>::ConstIterator iter = items.constBegin();
	const QList<qtractorMidiEvent *>::ConstIterator& iter_end = items.constEnd();

	// Seed time range with a value from the list of selected events.
	long iMinTime = iTimeOffset;
	long iMaxTime = iTimeOffset;

	if (pSelect->anchorEvent())
		iMinTime = iMaxTime = pSelect->anchorEvent()->time() + iTimeOffset;

	long iMinTime2 = iMinTime;
	long iMaxTime2 = iMaxTime;

	if (iTimeStart < iTimeEnd) {
		iMinTime += long(iTimeStart);
		iMaxTime += long(iTimeEnd);
	}

	// First scan pass for the normalize and resize value ramp tools:
	// find maximum and minimum times and values from the selection...
	int iMaxValue = 0;
	int iMinValue = 0;
	if (m_ui.NormalizeCheckBox->isChecked()
		|| (m_ui.TransposeCheckBox->isChecked() &&
			m_ui.TransposeReverseCheckBox->isChecked())
		|| (m_ui.ResizeCheckBox->isChecked() &&
			m_ui.ResizeValueCheckBox->isChecked() &&
			m_ui.ResizeValue2ComboBox->currentIndex() > 0)) {
		// Make it through one time...
		for (int i = 0 ; iter != iter_end; ++i, ++iter) {
			qtractorMidiEvent *pEvent = *iter;
			const long iTime = pEvent->time() + iTimeOffset;
			const long iTime2 = iTime + pEvent->duration();
			if (iMinTime  > iTime)
				iMinTime  = iTime;
			if (iMaxTime  < iTime)
				iMaxTime  = iTime;
			if (iMinTime2 > iTime || i == 0)
				iMinTime2 = iTime;
			if (iMaxTime2 < iTime2)
				iMaxTime2 = iTime2;
			const bool bPitchBend
				= (pEvent->type() == qtractorMidiEvent::PITCHBEND);
			const int iValue
				= (bPitchBend ? pEvent->pitchBend() : pEvent->value());
			if (iMinValue > iValue || i == 0)
				iMinValue = iValue;
			if (iMaxValue < iValue)
				iMaxValue = iValue;
		}
		// Get it back to front...
		iter = items.constBegin();
	}

	// Go for the main pass...
	qtractorTimeScale::Cursor cursor(m_pTimeScale);

	// Resize/Legato: to track last event...
	qtractorMidiEvent *pLastEvent = nullptr;

	// Resize/Legato/Join: to track note first and last events...
	QHash<int, qtractorMidiEvent *> notes1;
	QHash<int, qtractorMidiEvent *> notes2;

	for ( ; iter != iter_end; ++iter) {
		qtractorMidiEvent *pEvent = *iter;
		int iNote = int(pEvent->note());
		long iTime = pEvent->time() + iTimeOffset;
		long iDuration = long(pEvent->duration());
		const bool bPitchBend = (
			pEvent->type() == qtractorMidiEvent::PITCHBEND);
		const bool b14bit = (
			pEvent->type() == qtractorMidiEvent::CONTROL14 ||
			pEvent->type() == qtractorMidiEvent::REGPARAM  ||
			pEvent->type() == qtractorMidiEvent::NONREGPARAM);
		int iValue = (bPitchBend ? pEvent->pitchBend() : pEvent->value());
		qtractorTimeScale::Node *pNode = cursor.seekTick(iTime);
		// Quantize tool...
		if (m_ui.QuantizeCheckBox->isChecked()) {
			// Swing quantize...
			if (m_ui.QuantizeSwingCheckBox->isChecked()) {
				const unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.QuantizeSwingComboBox->currentIndex() + 1);
				const unsigned long q = pNode->ticksPerBeat / p;
				if (q > 0) {
					const unsigned long t0 = q * (iTime / q);
					float d0 = 0.0f;
					if ((iTime / q) % 2)
						d0 = float(long(t0 + q) - long(iTime));
					else
						d0 = float(long(iTime) - long(t0));
					float ds = 0.01f * float(m_ui.QuantizeSwingSpinBox->value());
					ds = ds * d0;
					const int n = m_ui.QuantizeSwingTypeComboBox->currentIndex();
					for (int i = 0; i < n; ++i) // 0=Linear; 1=Quadratic; 2=Cubic.
						ds = (ds * d0) / float(q);
					iTime += long(ds);
					if (iTime < long(iTimeOffset))
						iTime = long(iTimeOffset);
				}
			}
			// Time quantize...
			if (m_ui.QuantizeTimeCheckBox->isChecked()) {
				const unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.QuantizeTimeComboBox->currentIndex() + 1);
				const unsigned long q = pNode->ticksPerBeat / p;
				iTime = q * ((iTime + (q >> 1)) / q);
				// Time percent quantize...
				const float delta = 0.01f
					* (100.0f - float(m_ui.QuantizeTimeSpinBox->value()))
					* float(long(pEvent->time() + iTimeOffset) - iTime);
				iTime += long(delta);
				if (iTime < long(iTimeOffset))
					iTime = long(iTimeOffset);
			}
			// Duration quantize...
			if (m_ui.QuantizeDurationCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				const unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.QuantizeDurationComboBox->currentIndex() + 1);
				const unsigned long q = pNode->ticksPerBeat / p;
				iDuration = q * ((iDuration + q - 1) / q);
				// Duration percent quantize...
				const float delta = 0.01f
					* (100.0f - float(m_ui.QuantizeDurationSpinBox->value()))
					* float(long(pEvent->duration()) - iDuration);
				iDuration += long(delta);
				if (iDuration < 0)
					iDuration = 0;
			}
			// Scale quantize...
			if (m_ui.QuantizeScaleCheckBox->isChecked()) {
				iNote = qtractorMidiEditor::snapToScale(iNote,
					m_ui.QuantizeScaleKeyComboBox->currentIndex(),
					m_ui.QuantizeScaleComboBox->currentIndex());
			}
		}
		// Transpose tool...
		if (m_ui.TransposeCheckBox->isChecked()) {
			if (m_ui.TransposeNoteCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				iNote += m_ui.TransposeNoteSpinBox->value();
				if (iNote < 0)
					iNote = 0;
				else
				if (iNote > 127)
					iNote = 127;
			}
			if (m_ui.TransposeTimeCheckBox->isChecked()) {
				iTime = pNode->tickFromFrame(pNode->frameFromTick(iTime)
					+ m_ui.TransposeTimeSpinBox->value());
				if (iTime < long(iTimeOffset))
					iTime = long(iTimeOffset);
			}
			if (m_ui.TransposeReverseCheckBox->isChecked()) {
				iTime = iMinTime2 + iMaxTime2 - iTime - iDuration;
				if (iTime < long(iTimeOffset))
					iTime = long(iTimeOffset);
			}
		}
		// Normalize tool...
		if (m_ui.NormalizeCheckBox->isChecked()) {
			float p, q = float(iMaxValue);
			if (m_ui.NormalizeValueCheckBox->isChecked())
				p = float(m_ui.NormalizeValueSpinBox->value());
			else
			if (b14bit)
				p = 16384.0f;
			else
			if (bPitchBend)
				p = 8192.0f;
			else
				p = 128.0f;
			if (m_ui.NormalizePercentCheckBox->isChecked()) {
				p *= float(m_ui.NormalizePercentSpinBox->value());
				q *= 100.0f;
			}
			if (q > 0.0f) {
				iValue = int((p * float(iValue)) / q);
				if (bPitchBend) {
					if (iValue > +8191)
						iValue = +8191;
					else
					if (iValue < -8191)
						iValue = -8191;
				} else {
					if (iValue > 16383 && b14bit)
						iValue = 16383;
					else
					if (iValue > 127)
						iValue = 127;
					else
					if (iValue < 0)
						iValue = 0;
				}
			}
		}
		// Randomize tool...
		if (m_ui.RandomizeCheckBox->isChecked()) {
			float p; int q;
			if (m_ui.RandomizeNoteCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				p = 0.01f * float(m_ui.RandomizeNoteSpinBox->value());
				q = 127;
				if (p > 0.0f) {
					iNote += int(p * float(q - (::rand() % (q << 1))));
					if (iNote > 127)
						iNote = 127;
					else
					if (iNote < 0)
						iNote = 0;
				}
			}
			if (m_ui.RandomizeTimeCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RandomizeTimeSpinBox->value());
				q = pNode->ticksPerBeat;
				if (p > 0.0f) {
					iTime += long(p * float(q - (::rand() % (q << 1))));
					if (iTime < long(iTimeOffset))
						iTime = long(iTimeOffset);
				}
			}
			if (m_ui.RandomizeDurationCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RandomizeDurationSpinBox->value());
				q = pNode->ticksPerBeat;
				if (p > 0.0f) {
					iDuration += long(p * float(q - (::rand() % (q << 1))));
					if (iDuration < 0)
						iDuration = 0;
				}
			}
			if (m_ui.RandomizeValueCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RandomizeValueSpinBox->value());
				q = (bPitchBend ? 8192 : 128);
				if (p > 0.0f) {
					iValue += int(p * float(q - (::rand() % (q << 1))));
					if (bPitchBend) {
						if (iValue > +8191)
							iValue = +8191;
						else
						if (iValue < -8191)
							iValue = -8191;
					} else {
						if (iValue > 16383 && b14bit)
							iValue = 16383;
						else
						if (iValue > 127 && !b14bit)
							iValue = 127;
						else
						if (iValue < 0)
							iValue = 0;
					}
				}
			}
		}
		// Resize tool...
		if (m_ui.ResizeCheckBox->isChecked()) {
			if (m_ui.ResizeDurationCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				const float T0 // @ iFrame == 0
					= m_pTimeScale->nodes().first()->tempo;
				const float T1
					= pNode->tempo;
				const unsigned long iFrames
					= qtractorTimeScale::uroundf(
						T0 * float(m_ui.ResizeDurationSpinBox->value()) / T1);
				iDuration = pNode->tickFromFrame(
					pNode->frameFromTick(iTime) + iFrames) - iTime;
			}
			if (m_ui.ResizeValueCheckBox->isChecked()) {
				const int p = (bPitchBend && iValue < 0 ? -1 : 1); // sign
				iValue = p * m_ui.ResizeValueSpinBox->value();
				if (bPitchBend) iValue <<= 6; // *128
				if (m_ui.ResizeValue2ComboBox->currentIndex() > 0) {
					int iValue2 = p * m_ui.ResizeValue2SpinBox->value();
					if (bPitchBend) iValue2 <<= 6; // *128
					const int iDeltaValue = iValue2 - iValue;
					const long iDeltaTime = iMaxTime - iMinTime;
					if (iDeltaTime > 0)
						iValue += iDeltaValue * (iTime - iMinTime) / iDeltaTime;
				}
			}
			if (m_ui.ResizeLegatoCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				if (pLastEvent) {
					long d2 = long(float(pLastEvent->time() - pEvent->time()));
					const int i	= m_ui.ResizeLegatoTypeComboBox->currentIndex();
					if (i > 0) {
						const unsigned short p = qtractorTimeScale::snapFromIndex(
							m_ui.ResizeLegatoLengthComboBox->currentIndex() + 1);
						const unsigned long q = pNode->ticksPerBeat / p;
						d2 += (i == 1 ? -long(q) : +long(q));
					}
					if (m_ui.ResizeLegatoModeComboBox->currentIndex() > 0) {
						if (iDuration < d2 && d2 > 0)
							iDuration = d2;
					}
					else if (d2 > 0)
						iDuration = d2;
				}
				pLastEvent = pEvent;
			}
			if (m_ui.ResizeJoinCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				qtractorMidiEvent *pEvent1 = notes1.value(iNote, nullptr);
				qtractorMidiEvent *pEvent2 = notes2.value(iNote, nullptr);
				if (pEvent1 && pEvent2) {
					if (pEvent->time() < pEvent1->time()) {
						if (pEvent1 != pEvent2)
							pMidiEditCommand->removeEvent(pEvent1);
						notes1.insert(iNote, pEvent);
					}
					else
					if (pEvent->time() > pEvent2->time()) {
						if (pEvent2 != pEvent1)
							pMidiEditCommand->removeEvent(pEvent2);
						notes2.insert(iNote, pEvent);
					} else {
						pMidiEditCommand->removeEvent(pEvent);
					}
				} else {
					notes1.insert(iNote, pEvent);
					notes2.insert(iNote, pEvent);
				}
			}
			if (m_ui.ResizeSplitCheckBox->isChecked()
				&& pEvent->type() == qtractorMidiEvent::NOTEON) {
				const unsigned short p = qtractorTimeScale::snapFromIndex(
					m_ui.ResizeSplitLengthComboBox->currentIndex() + 1);
				const unsigned long q = pNode->ticksPerBeat / p;
				if (q < iDuration) {
					unsigned long t0 = iTime;
					if (m_ui.ResizeSplitOffsetComboBox->currentIndex() > 0)
						t0 = q * ((t0 + (q >> 1)) / q);
					if (t0 < iTime)
						t0 += q;
					const unsigned long d0 = (t0 > iTime ? t0 - iTime : q);
					for (unsigned long d = d0; d < iDuration; d += q) {
						qtractorMidiEvent *pSplitEvent
							= new qtractorMidiEvent(
								iTime - iTimeOffset + d,
								pEvent->type(), iNote, iValue,
								iDuration < (d + q) ? iDuration - d : q);
						pMidiEditCommand->insertEvent(pSplitEvent);
					}
					iDuration = d0;
				}
			}
		}
		// Rescale tool...
		if (m_ui.RescaleCheckBox->isChecked()) {
			float p;
			if (m_ui.RescaleTimeCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RescaleTimeSpinBox->value());
				iTime = iMinTime + long(p * float(iTime - iMinTime));
				if (iTime < long(iTimeOffset))
					iTime = long(iTimeOffset);
			}
			if (m_ui.RescaleDurationCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RescaleDurationSpinBox->value());
				iDuration = long(p * float(iDuration));
				if (iDuration < 0)
					iDuration = 0;
			}
			if (m_ui.RescaleValueCheckBox->isChecked()) {
				p = 0.01f * float(m_ui.RescaleValueSpinBox->value());
				if (m_ui.RescaleInvertCheckBox->isChecked()) {
					if (bPitchBend)
						iValue = -iValue;
					else
					if (b14bit)
						iValue = 16384 - iValue;
					else
						iValue = 128 - iValue;
				}
				iValue = int(p * float(iValue));
				if (bPitchBend) {
					if (iValue > +8191)
						iValue = +8191;
					else
					if (iValue < -8191)
						iValue = -8191;
				} else {
					if (iValue > 16383 && b14bit)
						iValue = 16383;
					else
					if (iValue > 127 && !b14bit)
						iValue = 127;
					else
					if (iValue < 0)
						iValue = 0;
				}
			}
		}
		// Timeshift tool...
		if (m_ui.TimeshiftCheckBox->isChecked()) {
			const unsigned long iEditHeadTime
				= pSession->tickFromFrame(pSession->editHead());
			const unsigned long iEditTailTime
				= pSession->tickFromFrame(pSession->editTail());
			const float d = float(iEditTailTime - iEditHeadTime);
			const float p = float(m_ui.TimeshiftSpinBox->value());
			if ((p < -1e-6f || p > 1e-6f) && (d > 0.0f)) {
				const float t = float(iTime - iEditHeadTime);
				float t1 = t / d;
				if (t1 > 0.0f && t1 < 1.0f)
					t1 = TimeshiftCurve::timeshift(t1, p);
				iTime = t1 * d + float(iEditHeadTime);
				if (m_ui.TimeshiftDurationCheckBox->isChecked()) {
					float t2 = (t + float(iDuration)) / d;
					if (t2 > 0.0f && t2 < 1.0f)
						t2 = TimeshiftCurve::timeshift(t2, p);
					iDuration = t2 * d + float(iEditHeadTime) - iTime;
				}
			}
		}
		// Temporamp tool...
		if (m_ui.TemporampCheckBox->isChecked()) {
			const unsigned long iEditHeadTime
				= pSession->tickFromFrame(pSession->editHead());
			const unsigned long iEditTailTime
				= pSession->tickFromFrame(pSession->editTail());
			const float d = float(iEditTailTime - iEditHeadTime);
			const float T0 = m_ui.TemporampFromSpinBox->tempo();
			const float T1 = m_ui.TemporampToSpinBox->tempo();
			float t2 = float(iTime - iEditHeadTime) / d;
			const float s2
				= (T1 - T0) / (T0 > T1 ? T0 : T1)
				* (1.0f - t2 * t2);
			if (m_ui.TemporampDurationCheckBox->isChecked()) {
				const float d2 = s2 * float(iDuration) / d;
				iDuration += qtractorTimeScale::uroundf(d2 * d);
			}
			t2 += s2 * t2 * ::expf(- t2 * M_PI);
			iTime = iEditHeadTime + qtractorTimeScale::uroundf(t2 * d);
		}
		// Make it to the event...
		pMidiEditCommand->updateEvent(pEvent,
			iNote, iTime - iTimeOffset, iDuration, iValue);
	}

	// Resize/Join: to track note first and last events...
	if (m_ui.ResizeCheckBox->isChecked()
		&& m_ui.ResizeJoinCheckBox->isChecked()) {
		QHash<int, qtractorMidiEvent *>::ConstIterator iter1 = notes1.constBegin();
		const QHash<int, qtractorMidiEvent *>::ConstIterator& iter1_end = notes1.constEnd();
		for ( ; iter1 != iter1_end; ++iter1) {
			const int iNote = iter1.key();
			qtractorMidiEvent *pEvent1 = iter1.value();
			qtractorMidiEvent *pEvent2 = notes2.value(iNote, nullptr);
			if (pEvent2 && pEvent2 != pEvent1) {
				const unsigned long iTime = pEvent1->time();
				const unsigned long iDuration
					= pEvent2->time() + pEvent2->duration()	- iTime;
				pMidiEditCommand->resizeEventTime(pEvent1, iTime, iDuration);
				pMidiEditCommand->removeEvent(pEvent2);
			}
		}
	}

	// HACK: Add time-scale node for tempo ramp target,
	// iif not the same to current edit-head's tempo.
	// FIXME: conditional check-box at the UI level?
	if (m_ui.TemporampCheckBox->isChecked()) {
		qtractorTimeScale *pTimeScale = pSession->timeScale();
		qtractorTimeScale::Cursor cursor(pTimeScale);
		const unsigned long iEditHead = pSession->editHead();
		qtractorTimeScale::Node *pNode = cursor.seekFrame(iEditHead);
		const float T1 = m_ui.TemporampToSpinBox->tempo();
		if (qAbs(T1 - pNode->tempo) > 0.05f) {
			const unsigned short iBeatsPerBar1
				= m_ui.TemporampToSpinBox->beatsPerBar();
			const unsigned short iBeatDivisor1
				= m_ui.TemporampToSpinBox->beatDivisor();
			m_timeScaleNodeCommands.append(
				new qtractorTimeScaleAddNodeCommand(
					pTimeScale, iEditHead, T1, pNode->beatType,
					iBeatsPerBar1, iBeatDivisor1));
		}
	}

	// Done.
	return pMidiEditCommand;
}


// Common change slot.
void qtractorMidiToolsForm::changed (void)
{
	if (m_iUpdate > 0)
		return;

	++m_iDirtyCount;
	stabilizeForm();
}


// Accept settings (OK button slot).
void qtractorMidiToolsForm::accept (void)
{
	// Save as default preset...
	savePreset(g_sDefPreset);

	// Just go with dialog acceptance.
	QDialog::accept();
}


// Reject settings (Cancel button slot).
void qtractorMidiToolsForm::reject (void)
{
	// Bail out...
	QDialog::reject();
}


// Display format has changed.
void qtractorMidiToolsForm::formatChanged ( int iDisplayFormat )
{
	const qtractorTimeScale::DisplayFormat displayFormat
		= qtractorTimeScale::DisplayFormat(iDisplayFormat);

	m_ui.TransposeFormatComboBox->setCurrentIndex(iDisplayFormat);
	m_ui.ResizeDurationFormatComboBox->setCurrentIndex(iDisplayFormat);

	if (m_pTimeScale) {
		// Set from local time-scale instance...
		//m_pTimeScale->setDisplayFormat(displayFormat);
		m_ui.TransposeTimeSpinBox->setDisplayFormat(displayFormat);
		m_ui.ResizeDurationSpinBox->setDisplayFormat(displayFormat);
	}

	stabilizeForm();
}


// Stabilize current form state.
void qtractorMidiToolsForm::stabilizeForm (void)
{
	int  iEnabled = 0;
	bool bEnabled;
	bool bEnabled2;

	// Preset status...
	const QString& sPreset = m_ui.PresetNameComboBox->currentText();
	const bool bExists = (m_ui.PresetNameComboBox->findText(sPreset) >= 0);
	bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	m_ui.PresetSaveToolButton->setEnabled(bEnabled
		&& (!bExists || m_iDirtyCount > 0));
	m_ui.PresetDeleteToolButton->setEnabled(bEnabled && bExists);

	// Quantize tool...

	bEnabled = m_ui.QuantizeCheckBox->isChecked();

	m_ui.QuantizeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeTimeCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.QuantizeTimeComboBox->setEnabled(bEnabled2);
	m_ui.QuantizeTimeSpinBox->setEnabled(bEnabled2);

	m_ui.QuantizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeDurationCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.QuantizeDurationComboBox->setEnabled(bEnabled2);
	m_ui.QuantizeDurationSpinBox->setEnabled(bEnabled2);

//	if (bEnabled)
//		bEnabled = m_ui.QuantizeTimeCheckBox->isChecked();
	m_ui.QuantizeSwingCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeSwingCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.QuantizeSwingComboBox->setEnabled(bEnabled2);
	m_ui.QuantizeSwingSpinBox->setEnabled(bEnabled2);
	m_ui.QuantizeSwingTypeComboBox->setEnabled(bEnabled2);

	m_ui.QuantizeScaleCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.QuantizeScaleCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.QuantizeScaleKeyComboBox->setEnabled(bEnabled2);
	m_ui.QuantizeScaleComboBox->setEnabled(bEnabled2);

	// Transpose tool...

	bEnabled = m_ui.TransposeCheckBox->isChecked();

	m_ui.TransposeNoteCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.TransposeNoteCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.TransposeNoteSpinBox->setEnabled(bEnabled2);

	m_ui.TransposeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.TransposeTimeCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.TransposeTimeSpinBox->setEnabled(bEnabled2);
	m_ui.TransposeFormatComboBox->setEnabled(bEnabled2);

	m_ui.TransposeReverseCheckBox->setEnabled(bEnabled2);
	bEnabled2 = bEnabled2 && m_ui.TransposeReverseCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;

	// Normalize tool...

	bEnabled = m_ui.NormalizeCheckBox->isChecked();

	m_ui.NormalizePercentCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizePercentCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.NormalizePercentSpinBox->setEnabled(bEnabled2);

	m_ui.NormalizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.NormalizeValueCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.NormalizeValueSpinBox->setEnabled(bEnabled2);

	// Randomize tool...

	bEnabled = m_ui.RandomizeCheckBox->isChecked();

	m_ui.RandomizeNoteCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeNoteCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RandomizeNoteSpinBox->setEnabled(bEnabled2);

	m_ui.RandomizeTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeTimeCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RandomizeTimeSpinBox->setEnabled(bEnabled2);

	m_ui.RandomizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeDurationCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RandomizeDurationSpinBox->setEnabled(bEnabled2);

	m_ui.RandomizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RandomizeValueCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RandomizeValueSpinBox->setEnabled(bEnabled2);

	// Resize tool...

	bEnabled = m_ui.ResizeCheckBox->isChecked();

	m_ui.ResizeDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeDurationCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.ResizeDurationSpinBox->setEnabled(bEnabled2);
	m_ui.ResizeDurationFormatComboBox->setEnabled(bEnabled2);

	m_ui.ResizeValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeValueCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.ResizeValueSpinBox->setEnabled(bEnabled2);
	m_ui.ResizeValue2ComboBox->setEnabled(bEnabled2);
	if (bEnabled2)
		bEnabled2 = (m_ui.ResizeValue2ComboBox->currentIndex() > 0);
	m_ui.ResizeValue2SpinBox->setEnabled(bEnabled2);

	m_ui.ResizeLegatoCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeLegatoCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.ResizeLegatoTypeComboBox->setEnabled(bEnabled2);
	m_ui.ResizeLegatoLengthComboBox->setEnabled(bEnabled2
		&& m_ui.ResizeLegatoTypeComboBox->currentIndex() > 0);
	m_ui.ResizeLegatoModeComboBox->setEnabled(bEnabled2);

	m_ui.ResizeJoinCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.ResizeJoinCheckBox->isChecked();
	if (bEnabled2) {
		const bool bBlockSplit
			= m_ui.ResizeSplitCheckBox->blockSignals(true);
		m_ui.ResizeSplitCheckBox->setEnabled(false);
		m_ui.ResizeSplitCheckBox->setChecked(false);
		m_ui.ResizeSplitCheckBox->blockSignals(bBlockSplit);
		bEnabled2 = false;
		++iEnabled;
	} else {
		m_ui.ResizeSplitCheckBox->setEnabled(bEnabled);
		bEnabled2 = bEnabled && m_ui.ResizeSplitCheckBox->isChecked();
		if (bEnabled2) {
			const bool bBlockJoin
				= m_ui.ResizeJoinCheckBox->blockSignals(true);
			m_ui.ResizeJoinCheckBox->setEnabled(false);
			m_ui.ResizeJoinCheckBox->setChecked(false);
			m_ui.ResizeJoinCheckBox->blockSignals(bBlockJoin);
			++iEnabled;
		}
	}
	m_ui.ResizeSplitLengthComboBox->setEnabled(bEnabled2);
	m_ui.ResizeSplitOffsetComboBox->setEnabled(bEnabled2);

	// Rescale tool...

	bEnabled = m_ui.RescaleCheckBox->isChecked();

	m_ui.RescaleTimeCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RescaleTimeCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RescaleTimeSpinBox->setEnabled(bEnabled2);

	m_ui.RescaleDurationCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RescaleDurationCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RescaleDurationSpinBox->setEnabled(bEnabled2);

	m_ui.RescaleValueCheckBox->setEnabled(bEnabled);
	bEnabled2 = bEnabled && m_ui.RescaleValueCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;
	m_ui.RescaleValueSpinBox->setEnabled(bEnabled2);

	m_ui.RescaleInvertCheckBox->setEnabled(bEnabled2);
	bEnabled2 = bEnabled2 && m_ui.RescaleInvertCheckBox->isChecked();
	if (bEnabled2)
		++iEnabled;

	// Timeshift tool...

	bEnabled = m_ui.TimeshiftCheckBox->isChecked();
	if (bEnabled)
		++iEnabled;
	m_ui.TimeshiftLabel->setEnabled(bEnabled);
	m_ui.TimeshiftSpinBox->setEnabled(bEnabled);
	m_ui.TimeshiftSlider->setEnabled(bEnabled);
	m_ui.TimeshiftText->setEnabled(bEnabled);
	m_ui.TimeshiftDurationCheckBox->setEnabled(bEnabled);
	m_pTimeshiftCurve->setVisible(bEnabled);

	// Temporamp tool...

	bEnabled = m_ui.TemporampCheckBox->isChecked();
	if (bEnabled)
		++iEnabled;
	m_ui.TemporampFromLabel->setEnabled(bEnabled);
	m_ui.TemporampFromSpinBox->setEnabled(false);
	m_ui.TemporampToLabel->setEnabled(bEnabled);
	m_ui.TemporampToSpinBox->setEnabled(bEnabled);
	m_ui.TemporampText->setEnabled(bEnabled);
	m_ui.TemporampDurationCheckBox->setEnabled(bEnabled);
	if (bEnabled
		&& qAbs(
			m_ui.TemporampFromSpinBox->tempo() -
			m_ui.TemporampToSpinBox->tempo()) < 0.01f)
		iEnabled = 0;

	m_ui.DialogButtonBox->button(QDialogButtonBox::Ok)->setEnabled(iEnabled > 0);
}


// Timeshift characteristic stuff.
void qtractorMidiToolsForm::timeshiftSpinBoxChanged ( double p )
{
	if (m_iUpdate > 0)
		return;

	++m_iUpdate;
#if 0//TIMESHIFT_LOGSCALE
	int i = 0;
	if (p > +0.001)
		i = + int(2000.0f * ::log10f(1000.0f * float(+ p)));
	else
	if (p < -0.001)
		i = - int(2000.0f * ::log10f(1000.0f * float(- p)));
#else
	const int i = int(100.0f * float(p));
#endif
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiToolsForm::timeshiftSpinBoxChanged(%g) i=%d", float(p), i);
#endif
	m_ui.TimeshiftSlider->setValue(i);
	m_pTimeshiftCurve->setTimeshift(float(p));
	--m_iUpdate;

	changed();
}

void qtractorMidiToolsForm::timeshiftSliderChanged ( int i )
{
	if (m_iUpdate > 0)
		return;

	++m_iUpdate;
#if 0//TIMESHIFT_LOGSCALE
	float p = 0.0f;
	if (i > 0)
		p = + 0.001f * ::powf(10.0f, (0.0005f * float(+ i)));
	else
	if (i < 0)
		p = - 0.001f * ::powf(10.0f, (0.0005f * float(- i)));
#else
	const float p = 0.01f * float(i);
#endif
#ifdef CONFIG_DEBUG_0
	qDebug("qtractorMidiToolsForm::timeshiftSliderChanged(%d) p=%g", i, p);
#endif
	m_ui.TimeshiftSpinBox->setValue(double(p));
	m_pTimeshiftCurve->setTimeshift(p);
	--m_iUpdate;

	changed();
}


// Special tempo ramp tool helper...
qtractorTimeScaleNodeCommand *qtractorMidiToolsForm::timeScaleNodeCommand (void)
{
	if (m_timeScaleNodeCommands.isEmpty())
		return nullptr;

	qtractorTimeScaleNodeCommand *pTimeScaleNodeCommand
		= m_timeScaleNodeCommands.at(0);
	m_timeScaleNodeCommands.removeAt(0);

	return pTimeScaleNodeCommand;
}


// end of qtractorMidiToolsForm.cpp
