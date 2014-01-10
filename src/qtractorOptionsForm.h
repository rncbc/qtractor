// qtractorOptionsForm.h
//
/****************************************************************************
   Copyright (C) 2005-2014, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorOptionsForm_h
#define __qtractorOptionsForm_h

#include "ui_qtractorOptionsForm.h"


// Forward declarations...
class qtractorOptions;


//----------------------------------------------------------------------------
// qtractorOptionsForm -- UI wrapper form.

class qtractorOptionsForm : public QDialog
{
	Q_OBJECT

public:

	// Constructor.
	qtractorOptionsForm(QWidget *pParent = 0, Qt::WindowFlags wflags = 0);
	// Destructor.
	~qtractorOptionsForm();

	void setOptions(qtractorOptions *pOptions);
	qtractorOptions *options() const;

protected slots:

	void accept();
	void reject();
	void changed();
	void chooseMetroBarFilename();
	void chooseMetroBeatFilename();
	void updateMetroNoteNames();
	void changeAudioMeterLevel(int iColor);
	void changeMidiMeterLevel(int iColor);
	void changeAudioMeterColor(const QString& sColor);
	void changeMidiMeterColor(const QString& sColor);
	void chooseAudioMeterColor();
	void chooseMidiMeterColor();
	void resetMeterColors();
	void choosePluginType(int iPluginType);
	void changePluginPath(const QString& sPluginPath);
	void choosePluginPath();
	void selectPluginPath();
	void addPluginPath();
	void removePluginPath();
	void moveUpPluginPath();
	void moveDownPluginPath();
	void chooseLv2PresetDir();
	void chooseMessagesFont();
	void chooseMessagesLogPath();
	void chooseSessionTemplatePath();
	void stabilizeForm();

protected:

	// Browse for an existing audio filename.
	QString getOpenAudioFileName(
		const QString& sTitle, const QString& sFilename);

	// Special combo-box color item helpers.
	void updateColorText(QLineEdit *pLineEdit, const QColor& color);

	// Session format ext/suffix helpers.
	int sessionFormatFromExt(const QString& sSessionExt) const;
	QString sessionExtFromFormat(int iSessionFormat) const;

private:

	// The Qt-designer UI struct...
	Ui::qtractorOptionsForm m_ui;

	// Instance variables...
	qtractorOptions *m_pOptions;
	int m_iDirtyCount;

	// Meter colors.
	enum { AudioMeterColors = 5, MidiMeterColors = 2 };
	QColor m_audioMeterColors[AudioMeterColors];
	QColor m_midiMeterColors[MidiMeterColors];

	// Plug-ins path cache.
	QStringList m_ladspaPaths;
	QStringList m_dssiPaths;
	QStringList m_lv2Paths;
	QStringList m_vstPaths;
};


#endif	// __qtractorOptionsForm_h


// end of qtractorOptionsForm.h
