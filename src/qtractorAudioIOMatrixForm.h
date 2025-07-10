// qtractorAudioIOMatrixForm.h
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

#ifndef __qtractorAudioIOMatrixForm_h
#define __qtractorAudioIOMatrixForm_h

#include <QDialog>

#include <QTableWidget>


// forward decls.
class QButtonGroup;


//-------------------------------------------------------------------------
// qtractorAudioIOMatrixForm

namespace Ui { class qtractorAudioIOMatrixForm; }


class qtractorAudioIOMatrixForm: public QDialog
{
	Q_OBJECT

public:

	qtractorAudioIOMatrixForm(QWidget *parent = nullptr);

	virtual ~qtractorAudioIOMatrixForm();

	void setChannels(int nins, int nouts);
	int inputChannels() const;
	int outputChannels() const;

	void setMatrix(const QList<int>& matrix);
	const QList<int>& matrix() const;

	void refresh();

	class TableWidget;
	class TableCell;
	class RadioButton;

	const QList<QButtonGroup *>& groups() const;

protected slots:

	void inputChannelsChanged(int index);
	void outputChannelsChanged(int index);

	void accept();
	void reject();

private:

	Ui::qtractorAudioIOMatrixForm *p_ui;
	Ui::qtractorAudioIOMatrixForm& m_ui;

	QList<QButtonGroup *> m_groups;

	QList<int> m_matrix;
};


//-------------------------------------------------------------------------
// qtractorAudioIOMatrixForm::TableWidget

class qtractorAudioIOMatrixForm::TableWidget : public QTableWidget
{
public:

	TableWidget(QWidget *parent = nullptr);

	void setForm(qtractorAudioIOMatrixForm *form);
	qtractorAudioIOMatrixForm *form() const;

	bool isDirty() const;

protected:

	void mousePressEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

	void toggleCell(int row, int col);

private:

	qtractorAudioIOMatrixForm *m_form;

	int m_dirty;
};


#endif // __qtractorAudioIOMatrixForm_h

// end of qtractorAudioIOMatrixForm.h
