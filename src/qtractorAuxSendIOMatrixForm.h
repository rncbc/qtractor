// qtractorAuxSendIOMatrixForm.h
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

#ifndef __qtractorAuxSendIOMatrixForm_h
#define __qtractorAuxSendIOMatrixForm_h

#include <QDialog>

#include <QTableWidget>


// forward decls.
class QButtonGroup;


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm

namespace Ui { class qtractorAuxSendIOMatrixForm; }


class qtractorAuxSendIOMatrixForm: public QDialog
{
	Q_OBJECT

public:

	qtractorAuxSendIOMatrixForm(QWidget *parent = nullptr);

	virtual ~qtractorAuxSendIOMatrixForm();

	void setChannels(int nins, int nouts);
	int inputChannels() const;
	int outputChannels() const;

	void setMatrix(const QList<int>& matrix);
	const QList<int>& matrix() const;

	void refresh();

	class TableWidget;
	class RadioButton;

	const QList<QButtonGroup *>& groups() const;

protected slots:

	void inputChannelsChanged(int index);
	void outputChannelsChanged(int index);

	void accept();
	void reject();

private:

	Ui::qtractorAuxSendIOMatrixForm *p_ui;
	Ui::qtractorAuxSendIOMatrixForm& m_ui;

	QList<QButtonGroup *> m_groups;

	QList<int> m_matrix;
};


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm::TableWidget

class qtractorAuxSendIOMatrixForm::TableWidget : public QTableWidget
{
public:

	TableWidget(QWidget *parent = nullptr);

	void setForm(qtractorAuxSendIOMatrixForm *form);
	qtractorAuxSendIOMatrixForm *form() const;

protected:

	void mousePressEvent(QMouseEvent *event) override;
	void keyPressEvent(QKeyEvent *event) override;

	void toggleCell(int row, int col);

private:

	qtractorAuxSendIOMatrixForm *m_form;
};


#endif // __qtractorAuxSendIOMatrixForm_h

// end of qtractorAuxSendIOMatrixForm.h
