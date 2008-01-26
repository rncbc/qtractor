// qtractorPluginForm.cpp
//
/****************************************************************************
   Copyright (C) 2005-2008, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorPluginForm.h"
#include "qtractorPluginCommand.h"

#include "qtractorPlugin.h"
#include "qtractorPluginListView.h"

#include "qtractorSlider.h"

#include "qtractorOptions.h"
#include "qtractorMainForm.h"

#include <QMessageBox>
#include <QGridLayout>
#include <QValidator>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QDoubleSpinBox>

#include <math.h>

#if defined(Q_WS_X11)
#include <QApplication>
#include <QX11Info>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
typedef void (*XEventProc)(XEvent *);
#endif


// This shall hold the default preset name.
static QString g_sDefPreset;


//---------------------------------------------------------------------

#if defined(Q_WS_X11)

static bool g_bXError = false;

static int tempXErrorHandler ( Display *, XErrorEvent * )
{
	g_bXError = true;
	return 0;
}

static XEventProc getXEventProc ( Display *pDisplay, Window w )
{
	int iSize;
	unsigned long iBytes, iCount;
	unsigned char *pData;
	XEventProc eventProc = NULL;
	Atom atomType, atom = XInternAtom(pDisplay, "_XEventProc", false);

	g_bXError = false;
	XErrorHandler oldErrorHandler = XSetErrorHandler(tempXErrorHandler);
	XGetWindowProperty(pDisplay, w, atom, 0, 1, false,
		AnyPropertyType, &atomType,  &iSize, &iCount, &iBytes, &pData);
	if (g_bXError == false && iCount == 1)
		eventProc = (XEventProc) (*(int *) pData);
	XSetErrorHandler(oldErrorHandler);

	return eventProc;
}

static Window getXChildWindow ( Display *pDisplay, Window w )
{
	Window wRoot, wParent, *pwChildren;
	unsigned int iChildren = 0;

	XQueryTree(pDisplay, w, &wRoot, &wParent, &pwChildren, &iChildren);

	return (iChildren > 0 ? pwChildren[0] : NULL);
}

static unsigned int getXButton ( Qt::MouseButtons buttons )
{
	int button = 0;
	if (buttons & Qt::LeftButton)
		button = Button1;
	else
	if (buttons & Qt::MidButton)
		button = Button2;
	else
	if (buttons & Qt::RightButton)
		button = Button3;
	return button;
}

static unsigned int getXButtonState ( Qt::MouseButtons buttons )
{
	int state = 0;
	if (buttons & Qt::LeftButton)
		state |= Button1Mask;
	if (buttons & Qt::MidButton)
		state |= Button2Mask;
	if (buttons & Qt::RightButton)
		state |= Button3Mask;
	return state;
}

#endif // Q_WS_X11


//----------------------------------------------------------------------------
// qtractorPluginForm::EditorWidget -- Plugin editor wrapper widget.

class qtractorPluginForm::EditorWidget : public QWidget
{
public:

	// Constructor.
	EditorWidget(qtractorPluginForm *pPluginForm, Qt::WindowFlags wflags = 0)
		: QWidget(NULL, wflags),
#if defined(Q_WS_X11)
			m_pDisplay(NULL),
			m_wEditor(NULL),
			m_eventProc(NULL),
#endif
			m_pPluginForm(pPluginForm) {}

	// Specialized editor methods.
	void setPlugin(qtractorPlugin *pPlugin)
	{
		QWidget::setWindowTitle((pPlugin->type())->name());
		pPlugin->openEditor(this);
		QWidget::setFixedSize(pPlugin->editorSize());
#if defined(Q_WS_X11)
		m_pDisplay = QX11Info::display();
		m_wEditor  = getXChildWindow(m_pDisplay, (Window) winId());
		if (m_wEditor) {
			m_eventProc = getXEventProc(m_pDisplay, m_wEditor);
			if (m_eventProc)
				XSelectInput(m_pDisplay, m_wEditor, NoEventMask);
		}
#endif
	}

protected:

	// Visibility event handlers.
	void closeEvent(QCloseEvent *pCloseEvent)
		{ QWidget::closeEvent(pCloseEvent); m_pPluginForm->toggleEditor(false); }

#if defined(Q_WS_X11)

	// Mouse events.
	void enterEvent(QEvent *pEnterEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::enterEvent()\n");
		QWidget::enterEvent(pEnterEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = QCursor::pos();
			const QPoint& pos = QWidget::mapFromGlobal(globalPos);
			ev.xcrossing.display = m_pDisplay;
			ev.xcrossing.type    = EnterNotify;
			ev.xcrossing.window  = m_wEditor;
			ev.xcrossing.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xcrossing.time    = CurrentTime;
			ev.xcrossing.x       = pos.x();
			ev.xcrossing.y       = pos.y();
			ev.xcrossing.x_root  = globalPos.x();
			ev.xcrossing.y_root  = globalPos.y();
			ev.xcrossing.mode    = NotifyNormal;
			ev.xcrossing.detail  = NotifyAncestor;
			ev.xcrossing.state   = getXButtonState(QApplication::mouseButtons());
			sendXEvent(&ev);
		}
	}

	void mousePressEvent(QMouseEvent *pMouseEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::mousePressEvent()\n");
		QWidget::mousePressEvent(pMouseEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = pMouseEvent->globalPos();
			const QPoint& pos = pMouseEvent->pos();
			ev.xbutton.display = m_pDisplay;
			ev.xbutton.type    = ButtonPress;
			ev.xbutton.window  = m_wEditor;
			ev.xbutton.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xbutton.time    = CurrentTime;
			ev.xbutton.x       = pos.x();
			ev.xbutton.y       = pos.y();
			ev.xbutton.x_root  = globalPos.x();
			ev.xbutton.y_root  = globalPos.y();
			ev.xbutton.button  = getXButton(pMouseEvent->buttons());
			ev.xbutton.state   = getXButtonState(pMouseEvent->buttons());
			sendXEvent(&ev);
		}
	}

	void mouseMoveEvent(QMouseEvent *pMouseEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::mouseMoveEvent()\n");
		QWidget::mouseMoveEvent(pMouseEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = pMouseEvent->globalPos();
			const QPoint& pos = pMouseEvent->pos();
			ev.xmotion.display = m_pDisplay;
			ev.xmotion.type    = MotionNotify;
			ev.xmotion.window  = m_wEditor;
			ev.xmotion.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xmotion.time    = CurrentTime;
			ev.xmotion.x       = pos.x();
			ev.xmotion.y       = pos.y();
			ev.xmotion.x_root  = globalPos.x();
			ev.xmotion.y_root  = globalPos.y();
			ev.xmotion.state   = getXButtonState(pMouseEvent->buttons());
			sendXEvent(&ev);
		}
	}

	void mouseReleaseEvent(QMouseEvent *pMouseEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::mouseReleaseEvent()\n");
		QWidget::mouseReleaseEvent(pMouseEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = pMouseEvent->globalPos();
			const QPoint& pos = pMouseEvent->pos();
			ev.xbutton.display = m_pDisplay;
			ev.xbutton.type    = ButtonRelease;
			ev.xbutton.window  = m_wEditor;
			ev.xbutton.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xbutton.time    = CurrentTime;
			ev.xbutton.x       = pos.x();
			ev.xbutton.y       = pos.y();
			ev.xbutton.x_root  = globalPos.x();
			ev.xbutton.y_root  = globalPos.y();
			ev.xbutton.button  = getXButton(pMouseEvent->buttons());
			ev.xbutton.state   = getXButtonState(pMouseEvent->buttons());
			sendXEvent(&ev);
		}
	}

	void wheelEvent(QWheelEvent *pWheelEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::wheelEvent()\n");
		QWidget::wheelEvent(pWheelEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = pWheelEvent->globalPos();
			const QPoint& pos = pWheelEvent->pos();
			ev.xbutton.display = m_pDisplay;
			ev.xbutton.type    = ButtonPress;
			ev.xbutton.window  = m_wEditor;
			ev.xbutton.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xbutton.time    = CurrentTime;
			ev.xbutton.x       = pos.x();
			ev.xbutton.y       = pos.y();
			ev.xbutton.x_root  = globalPos.x();
			ev.xbutton.y_root  = globalPos.y();
			if (pWheelEvent->delta() > 0) {
				ev.xbutton.button = Button4;
				ev.xbutton.state |= Button4Mask;
			} else {
				ev.xbutton.button = Button5;
				ev.xbutton.state |= Button5Mask;
			}
			sendXEvent(&ev);
			// FIXME: delay here?
			ev.xbutton.type = ButtonRelease;
			sendXEvent(&ev);
		}
	}

	void leaveEvent(QEvent *pLeaveEvent)
	{
	//	qDebug("DEBUG> qtractorPluginWidget::leaveEvent()\n");
		QWidget::leaveEvent(pLeaveEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QPoint& globalPos = QCursor::pos();
			const QPoint& pos = QWidget::mapFromGlobal(globalPos);
			ev.xcrossing.display = m_pDisplay;
			ev.xcrossing.type    = LeaveNotify;
			ev.xcrossing.window  = m_wEditor;
			ev.xcrossing.root    = RootWindow(m_pDisplay, DefaultScreen(m_pDisplay));
			ev.xcrossing.time    = CurrentTime;
			ev.xcrossing.x       = pos.x();
			ev.xcrossing.y       = pos.y();
			ev.xcrossing.x_root  = globalPos.x();
			ev.xcrossing.y_root  = globalPos.y();
			ev.xcrossing.mode    = NotifyNormal;
			ev.xcrossing.detail  = NotifyAncestor;
    		ev.xcrossing.focus   = QWidget::hasFocus();
			ev.xcrossing.state   = getXButtonState(QApplication::mouseButtons());
			sendXEvent(&ev);
		}
	}

	// Composite paint event.
	void paintEvent(QPaintEvent *pPaintEvent)
	{
		QWidget::paintEvent(pPaintEvent);
		if (m_wEditor) {
			XEvent ev;
			::memset(&ev, 0, sizeof(ev));
			const QRect& rect = QWidget::geometry();
			ev.xexpose.type    = Expose;
			ev.xexpose.display = m_pDisplay;
			ev.xexpose.window  = m_wEditor;
			ev.xexpose.x       = rect.x();
			ev.xexpose.y       = rect.y();
			ev.xexpose.width   = rect.width();
			ev.xexpose.height  = rect.height();
			sendXEvent(&ev);
		}
	}

	// Send X11 event.
	void sendXEvent(XEvent *pEvent)
	{
		if (m_eventProc) {
			// If the plugin publish a event procedure, so it doesn't
			// have a message thread running, pass the event directly.
			(*m_eventProc)(pEvent);
		}
		else
		if (m_wEditor) {
			// If the plugin have a message thread running, then send events
			// to that window: it will be caught by the message thread!
			XSendEvent(m_pDisplay, m_wEditor, false, 0L, pEvent);
			XFlush(m_pDisplay);
		}
	}

#endif

private:

	// Instance variables...
#if defined(Q_WS_X11)
	Display   *m_pDisplay;
	Window     m_wEditor;
	XEventProc m_eventProc;
#endif

	qtractorPluginForm *m_pPluginForm;
};


//----------------------------------------------------------------------------
// qtractorPluginForm -- UI wrapper form.

// Constructor.
qtractorPluginForm::qtractorPluginForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QWidget(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pPlugin       = NULL;
	m_pEditorWidget = NULL;
	m_pGridLayout   = NULL;
	m_iDirtyCount   = 0;
	m_iUpdate       = 0;

    m_ui.PresetComboBox->setValidator(
		new QRegExpValidator(QRegExp("[\\w-]+"), m_ui.PresetComboBox));
	m_ui.PresetComboBox->setInsertPolicy(QComboBox::NoInsert);

	// Have some effective feedback when toggling on/off...
	QIcon icons;
	icons.addPixmap(
		QPixmap(":/icons/itemLedOff.png"), QIcon::Active, QIcon::Off);
	icons.addPixmap(
		QPixmap(":/icons/itemLedOn.png"), QIcon::Active, QIcon::On);
	m_ui.ActivateToolButton->setIcon(icons);

	if (g_sDefPreset.isEmpty())
		g_sDefPreset = tr("(default)");

	// UI signal/slot connections...
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(editTextChanged(const QString&)),
		SLOT(changePresetSlot(const QString&)));
	QObject::connect(m_ui.PresetComboBox,
		SIGNAL(activated(const QString &)),
		SLOT(loadPresetSlot(const QString&)));
	QObject::connect(m_ui.SavePresetToolButton,
		SIGNAL(clicked()),
		SLOT(savePresetSlot()));
	QObject::connect(m_ui.DeletePresetToolButton,
		SIGNAL(clicked()),
		SLOT(deletePresetSlot()));
	QObject::connect(m_ui.EditToolButton,
		SIGNAL(toggled(bool)),
		SLOT(editSlot(bool)));
	QObject::connect(m_ui.ActivateToolButton,
		SIGNAL(toggled(bool)),
		SLOT(activateSlot(bool)));
}


// Destructor.
qtractorPluginForm::~qtractorPluginForm (void)
{
	clear();
}


// Plugin accessors.
void qtractorPluginForm::setPlugin ( qtractorPlugin *pPlugin )
{
	clear();

	// Just in those cases where
	// we've been around before...
	if (m_pGridLayout) {
		delete m_pGridLayout;
		m_pGridLayout = NULL;
	}

	// Set the new reference...
	m_pPlugin = pPlugin;

	if (m_pPlugin == NULL)
		return;

	// Plugin might not have its own editor...
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(4);
	m_pGridLayout->setSpacing(4);
#if QT_VERSION >= 0x040300
	m_pGridLayout->setHorizontalSpacing(16);
#endif
	const QList<qtractorPluginParam *>& params = m_pPlugin->params();
	int iRows = params.count();
	int iCols = 1;
	while (iRows > 12 && iCols < 3) {
		iRows >>= 1;
		iCols++;
	}
	int iRow = 0;
	int iCol = 0;
	QListIterator<qtractorPluginParam *> iter(params);
	while (iter.hasNext()) {
		qtractorPluginParam *pParam = iter.next();
		qtractorPluginParamWidget *pParamWidget
			= new qtractorPluginParamWidget(pParam, this);
		m_paramWidgets.append(pParamWidget);
		m_pGridLayout->addWidget(pParamWidget, iRow, iCol);
		QObject::connect(pParamWidget,
			SIGNAL(valueChanged(qtractorPluginParam *, float)),
			SLOT(valueChangeSlot(qtractorPluginParam *, float)));
		if (++iRow > iRows) {
			iRow = 0;
			iCol++;
		}
	}
	layout()->addItem(m_pGridLayout);

	// Show editor button if available?
	m_ui.EditToolButton->setVisible((m_pPlugin->type())->isEditor());

	// Set plugin name as title...
	updateCaption();
	adjustSize();

	updateActivated();
	refresh();
	stabilize();
}


void qtractorPluginForm::activateForm (void)
{
	if (!isVisible())
		show();

	raise();
	activateWindow();

	if (m_pEditorWidget) {
		if (!m_pEditorWidget->isVisible())
			m_pEditorWidget->show();
		m_pEditorWidget->raise();
		m_pEditorWidget->activateWindow();
	}
}


// Editor widget methods.
void qtractorPluginForm::toggleEditor ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;
	if (!(m_pPlugin->type())->isEditor())
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Plugin supplies its own editor form...
	if (bOn && m_pEditorWidget == NULL) {
		Qt::WindowFlags wflags = Qt::Window
		#if QT_VERSION >= 0x040200
			| Qt::CustomizeWindowHint
		#endif
			| Qt::WindowTitleHint
			| Qt::WindowSystemMenuHint
			| Qt::WindowMinMaxButtonsHint
			| Qt::Tool;
		m_pEditorWidget = new EditorWidget(this, wflags);
		m_pEditorWidget->setPlugin(m_pPlugin);
		m_pEditorWidget->show();
		m_pPlugin->idleEditor();
	} 
	else if (!bOn && m_pEditorWidget) {
		m_pPlugin->closeEditor();
		m_pEditorWidget->close();
		m_pEditorWidget->deleteLater();
		m_pEditorWidget = NULL;
	}

	// Set the toggle button anyway...
	m_ui.EditToolButton->setChecked(bOn);

	m_iUpdate--;
}


// Visibility event handlers.
void qtractorPluginForm::showEvent ( QShowEvent *pShowEvent )
{
	QWidget::showEvent(pShowEvent);

	if (m_pEditorWidget)
		m_pEditorWidget->show();
}


// Plugin accessor.
qtractorPlugin *qtractorPluginForm::plugin (void) const
{
	return m_pPlugin;
}


// Plugin preset accessors.
void qtractorPluginForm::setPreset ( const QString& sPreset )
{
	if (!sPreset.isEmpty()) {
		m_iUpdate++;
		m_ui.PresetComboBox->setEditText(sPreset);
		m_iUpdate--;
	}
}

QString qtractorPluginForm::preset (void) const
{
	QString sPreset = m_ui.PresetComboBox->currentText();

	if (sPreset == g_sDefPreset || m_iDirtyCount > 0)
		sPreset.clear();

	return sPreset;
}


// Update form caption.
void qtractorPluginForm::updateCaption (void)
{
	if (m_pPlugin == NULL)
		return;

	QString sCaption = (m_pPlugin->type())->name();

	qtractorPluginList *pPluginList = m_pPlugin->list();
	if (pPluginList && !pPluginList->name().isEmpty())
		sCaption += " - " + pPluginList->name();

	setWindowTitle(sCaption);

	if (m_pEditorWidget)
		m_pEditorWidget->setWindowTitle(sCaption);
}


// Update activation state.
void qtractorPluginForm::updateActivated (void)
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;
	m_ui.ActivateToolButton->setChecked(m_pPlugin->isActivated());
	m_iUpdate--;
}


// Update port widget state.
void qtractorPluginForm::updateParam ( unsigned long iIndex )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	QListIterator<qtractorPluginParamWidget *> iter(m_paramWidgets);
	while (iter.hasNext()) {
		qtractorPluginParamWidget *pParamWidget = iter.next();
		if ((pParamWidget->param())->index() == iIndex) {
			pParamWidget->refresh();
			break;
		}
	}

	m_iUpdate--;
}


// Preset management slots...
void qtractorPluginForm::changePresetSlot ( const QString& sPreset )
{
	if (m_iUpdate > 0)
		return;

	if (!sPreset.isEmpty() && m_ui.PresetComboBox->findText(sPreset) >= 0)
		m_iDirtyCount++;

	stabilize();
}


void qtractorPluginForm::loadPresetSlot ( const QString& sPreset )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0 || sPreset.isEmpty())
		return;

	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm == NULL)
		return;

	if (sPreset == g_sDefPreset) {
		// Reset to default...
		pMainForm->commands()->exec(
			new qtractorResetPluginCommand(m_pPlugin));
	} else {
		// An existing preset is about to be loaded...
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			QSettings& settings = pOptions->settings();
			// Get the preset entry, if any...
			settings.beginGroup(m_pPlugin->presetGroup());
			QStringList vlist = settings.value(sPreset).toStringList();
			settings.endGroup();
			// Is it there?
			if (!vlist.isEmpty()) {
				pMainForm->commands()->exec(
					new qtractorPresetPluginCommand(m_pPlugin, vlist));
			}
		}
	}

	stabilize();
}


void qtractorPluginForm::savePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset = m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// The current state preset is about to be saved...
	QStringList vlist = m_pPlugin->values();
	// Is there any?
	if (!vlist.isEmpty()) {
		qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
		if (pMainForm) {
			qtractorOptions *pOptions = pMainForm->options();
			if (pOptions) {
				QSettings& settings = pOptions->settings();
				// Set the preset entry...
				settings.beginGroup(m_pPlugin->presetGroup());
				settings.setValue(sPreset, vlist);
				settings.endGroup();
				refresh();
			}
		}
	}

	stabilize();
}


void qtractorPluginForm::deletePresetSlot (void)
{
	if (m_pPlugin == NULL)
		return;

	const QString& sPreset =  m_ui.PresetComboBox->currentText();
	if (sPreset.isEmpty() || sPreset == g_sDefPreset)
		return;

	// A preset entry is about to be deleted...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			// Prompt user if he/she's sure about this...
			if (pOptions->bConfirmRemove) {
				if (QMessageBox::warning(this,
					tr("Warning") + " - " QTRACTOR_TITLE,
					tr("About to delete preset:\n\n"
					"\"%1\" (%2)\n\n"
					"Are you sure?")
					.arg(sPreset)
					.arg((m_pPlugin->type())->name()),
					tr("OK"), tr("Cancel")) > 0)
					return;
			}
			// Go ahead...
			QSettings& settings = pOptions->settings();
			settings.beginGroup(m_pPlugin->presetGroup());
			settings.remove(sPreset);
			settings.endGroup();
			refresh();
		}
	}

	stabilize();
}


// Editor slot.
void qtractorPluginForm::editSlot ( bool bOn )
{
	toggleEditor(bOn);
}


// Activation slot.
void qtractorPluginForm::activateSlot ( bool bOn )
{
	if (m_pPlugin == NULL)
		return;

	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Make it a undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorActivatePluginCommand(m_pPlugin, bOn));

	m_iUpdate--;
}


// Something has changed.
void qtractorPluginForm::valueChangeSlot (
	qtractorPluginParam *pParam, float fValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	// Make it a undoable command...
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm)
		pMainForm->commands()->exec(
			new qtractorPluginParamCommand(pParam, fValue));

	m_pPlugin->idleEditor();

	m_iUpdate--;

	m_iDirtyCount++;
	stabilize();
}


// Parameter-widget refreshner-loader.
void qtractorPluginForm::refresh (void)
{
	if (m_pPlugin == NULL)
		return;

	m_iUpdate++;

	const QString sOldPreset = m_ui.PresetComboBox->currentText();
	m_ui.PresetComboBox->clear();
	qtractorMainForm *pMainForm = qtractorMainForm::getInstance();
	if (pMainForm) {
		qtractorOptions *pOptions = pMainForm->options();
		if (pOptions) {
			pOptions->settings().beginGroup(m_pPlugin->presetGroup());
			m_ui.PresetComboBox->insertItems(0,
				pOptions->settings().childKeys());
			pOptions->settings().endGroup();
		}
	}
	m_ui.PresetComboBox->addItem(g_sDefPreset);
	m_ui.PresetComboBox->setEditText(sOldPreset);

	QListIterator<qtractorPluginParamWidget *> iter(m_paramWidgets);
	while (iter.hasNext())
		iter.next()->refresh();

	m_pPlugin->idleEditor();

	m_iDirtyCount = 0;
	m_iUpdate--;
}


// Preset control.
void qtractorPluginForm::stabilize (void)
{
	bool bExists  = false;
	bool bEnabled = (m_pPlugin != NULL);
	m_ui.ActivateToolButton->setEnabled(bEnabled);
	if (bEnabled)
		bEnabled = ((m_pPlugin->type())->controlIns() > 0);
	m_ui.PresetComboBox->setEnabled(bEnabled);

	if (bEnabled) {
		const QString& sPreset = m_ui.PresetComboBox->currentText();
		bEnabled = (!sPreset.isEmpty() && sPreset != g_sDefPreset);
	    bExists  = (m_ui.PresetComboBox->findText(sPreset) >= 0);
	}

	m_ui.SavePresetToolButton->setEnabled(
		bEnabled && (!bExists || m_iDirtyCount > 0));
	m_ui.DeletePresetToolButton->setEnabled(
		bEnabled && bExists);
}


// Clear up plugin form...
void qtractorPluginForm::clear()
{
	qDeleteAll(m_paramWidgets);
	m_paramWidgets.clear();

	if (m_pEditorWidget) {
		if (m_pPlugin)
			m_pPlugin->closeEditor();
		delete m_pEditorWidget;
		m_pEditorWidget = NULL;
	}
}


// editor widget accessor.
QWidget *qtractorPluginForm::editorWidget (void)
{
	if (m_pEditorWidget)
		return static_cast<QWidget *> (m_pEditorWidget);
	else
		return static_cast<QWidget *> (this);
}


//----------------------------------------------------------------------------
// qtractorPluginParamWidget -- Plugin port widget.
//

// Constructor.
qtractorPluginParamWidget::qtractorPluginParamWidget (
	qtractorPluginParam *pParam, QWidget *pParent )
	: QFrame(pParent), m_pParam(pParam), m_iUpdate(0)
{
	m_pGridLayout = new QGridLayout();
	m_pGridLayout->setMargin(0);
	m_pGridLayout->setSpacing(4);

	m_pLabel    = NULL;
	m_pSlider   = NULL;
	m_pSpinBox  = NULL;
	m_pCheckBox = NULL;
	m_pDisplay  = NULL;

	if (m_pParam->isToggled()) {
		m_pCheckBox = new QCheckBox(/*this*/);
		m_pCheckBox->setText(m_pParam->name());
	//	m_pCheckBox->setChecked(m_pParam->value() > 0.1f);
		m_pGridLayout->addWidget(m_pCheckBox, 0, 0);
		QObject::connect(m_pCheckBox,
			SIGNAL(toggled(bool)),
			SLOT(checkBoxToggled(bool)));
	} else if (m_pParam->isInteger()) {
		m_pGridLayout->setColumnMinimumWidth(0, 120);
		m_pLabel = new QLabel(/*this*/);
		m_pLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		m_pLabel->setText(m_pParam->name() + ':');
		m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 2);
		m_pSpinBox = new QDoubleSpinBox(/*this*/);
		m_pSpinBox->setMaximumWidth(64);
		m_pSpinBox->setDecimals(0);
		m_pSpinBox->setMinimum(m_pParam->minValue());
		m_pSpinBox->setMaximum(m_pParam->maxValue());
		m_pSpinBox->setAlignment(Qt::AlignHCenter);
	//	m_pSpinBox->setValue(int(m_pParam->value()));
		m_pGridLayout->addWidget(m_pSpinBox, 0, 2);
		QObject::connect(m_pSpinBox,
			SIGNAL(valueChanged(const QString&)),
			SLOT(spinBoxValueChanged(const QString&)));
	} else {
		m_pLabel = new QLabel(/*this*/);
		if (m_pParam->isDisplay()) {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
			m_pLabel->setFixedWidth(72);
			m_pLabel->setText(m_pParam->name());
			m_pGridLayout->addWidget(m_pLabel, 0, 0);
		} else {
			m_pLabel->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
			m_pLabel->setText(m_pParam->name() + ':');
			m_pGridLayout->addWidget(m_pLabel, 0, 0, 1, 3);
		}
		m_pSlider = new qtractorSlider(Qt::Horizontal/*, this*/);
		m_pSlider->setTickPosition(QSlider::NoTicks);
		m_pSlider->setMinimumWidth(120);
		m_pSlider->setMinimum(0);
		m_pSlider->setMaximum(10000);
		m_pSlider->setPageStep(1000);
		m_pSlider->setSingleStep(100);
		m_pSlider->setDefault(paramToSlider(m_pParam->defaultValue()));
	//	m_pSlider->setValue(paramToSlider(m_pParam->value()));
		if (m_pParam->isDisplay()) {
			m_pGridLayout->addWidget(m_pSlider, 0, 1);
			m_pDisplay = new QLabel(/*this*/);
			m_pDisplay->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			m_pDisplay->setText(m_pParam->display());
			m_pDisplay->setFixedWidth(72);
			m_pGridLayout->addWidget(m_pDisplay, 0, 2);
		} else {
			m_pGridLayout->addWidget(m_pSlider, 1, 0, 1, 2);
			int iDecs = paramDecs();
			m_pSpinBox = new QDoubleSpinBox(/*this*/);
			m_pSpinBox->setMaximumWidth(64);
			m_pSpinBox->setDecimals(iDecs);
			m_pSpinBox->setMinimum(m_pParam->minValue());
			m_pSpinBox->setMaximum(m_pParam->maxValue());
			m_pSpinBox->setSingleStep(::powf(10.0f, - float(iDecs)));
		#if QT_VERSION >= 0x040200
			m_pSpinBox->setAccelerated(true);
		#endif
		//	m_pSpinBox->setValue(m_pParam->value());
			m_pGridLayout->addWidget(m_pSpinBox, 1, 2);
			QObject::connect(m_pSpinBox,
				SIGNAL(valueChanged(const QString&)),
				SLOT(spinBoxValueChanged(const QString&)));
		}
		QObject::connect(m_pSlider,
			SIGNAL(valueChanged(int)),
			SLOT(sliderValueChanged(int)));
	}

	QFrame::setLayout(m_pGridLayout);
//	QFrame::setFrameShape(QFrame::StyledPanel);
//	QFrame::setFrameShadow(QFrame::Raised);
	QFrame::setToolTip(m_pParam->name());
}


// Refreshner-loader method.
void qtractorPluginParamWidget::refresh (void)
{
	m_iUpdate++;
	if (m_pParam->isToggled()) {
		m_pCheckBox->setChecked(m_pParam->value() > 0.001f);
	} else if (m_pParam->isInteger()) {
		m_pSpinBox->setValue(int(m_pParam->value()));
	} else {
		m_pSlider->setValue(paramToSlider(m_pParam->value()));
		if (m_pSpinBox)
			m_pSpinBox->setValue(m_pParam->value());
		if (m_pDisplay)
			m_pDisplay->setText(m_pParam->display());
	}
	m_iUpdate--;
}


// Slider conversion methods.
int qtractorPluginParamWidget::paramToSlider ( float fValue ) const
{
	int   iValue = 0;
	float fScale = m_pParam->maxValue() - m_pParam->minValue();
	if (fScale > 1E-6f)
		iValue = int((10000.0f * (fValue - m_pParam->minValue())) / fScale);

	return iValue;
}

float qtractorPluginParamWidget::sliderToParam ( int iValue ) const
{
	float fDelta = m_pParam->maxValue() - m_pParam->minValue();
	return m_pParam->minValue() + (float(iValue) * fDelta) / 10000.0f;
}


// Spin-box decimals helper.
int qtractorPluginParamWidget::paramDecs (void) const
{
	int   iDecs = 0;
	float fDecs = ::log10f(m_pParam->maxValue() - m_pParam->minValue());
	if (fDecs < 0.0f)
		iDecs = 3;
	else if (fDecs < 1.0f)
		iDecs = 2;
	else if (fDecs < 3.0f)
		iDecs = 1;

	return iDecs;

}

// Change slots.
void qtractorPluginParamWidget::checkBoxToggled ( bool bOn )
{
	float fValue = (bOn ? 1.0f : 0.0f);

//	m_pParam->setValue(fValue);
	emit valueChanged(m_pParam, fValue);
}

void qtractorPluginParamWidget::spinBoxValueChanged ( const QString& sText )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fValue = 0.0f;
	if (m_pParam->isInteger()) {
		fValue = float(sText.toInt());
	} else {
		fValue = sText.toFloat();
	}

	//	Don't let be no-changes...
	if (::fabsf(m_pParam->value() - fValue)
		> ::powf(10.0f, - float(paramDecs()))) {
		emit valueChanged(m_pParam, fValue);
		if (m_pSlider)
			m_pSlider->setValue(paramToSlider(fValue));
	}

	m_iUpdate--;
}

void qtractorPluginParamWidget::sliderValueChanged ( int iValue )
{
	if (m_iUpdate > 0)
		return;

	m_iUpdate++;

	float fValue = sliderToParam(iValue);
	//	Don't let be no-changes...
	if (::fabsf(m_pParam->value() - fValue)
		> ::powf(10.0f, - float(paramDecs()))) {
		emit valueChanged(m_pParam, fValue);
		if (m_pSpinBox)
			m_pSpinBox->setValue(fValue);
		if (m_pDisplay)
			m_pDisplay->setText(m_pParam->display());
	}

	m_iUpdate--;
}


// end of qtractorPluginForm.cpp
