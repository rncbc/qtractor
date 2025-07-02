// qtractorAuxSendIOMatrixForm.cpp
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

#include "qtractorAuxSendIOMatrixForm.h"

#include "ui_qtractorAuxSendIOMatrixForm.h"


#include <QHeaderView>
#include <QButtonGroup>
#include <QRadioButton>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QMouseEvent>


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm::RadioButton

class qtractorAuxSendIOMatrixForm::RadioButton : public QRadioButton
{
public:

	RadioButton(QWidget *parent = nullptr)
		: QRadioButton(parent) {}

protected:

	bool hitButton(const QPoint&) const
		{ return false; }
};


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm::TableCell

class qtractorAuxSendIOMatrixForm::TableCell : public QWidget
{
public:

	TableCell(QWidget *parent = nullptr)
		: QWidget(parent)
	{
		m_radio = new RadioButton(this);

		QHBoxLayout *layout = new QHBoxLayout();
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addStretch(1);
		layout->addWidget(m_radio);
		layout->addStretch(1);
		QWidget::setLayout(layout);
	}

	RadioButton *radio() const
		{ return m_radio; }

private:

	RadioButton *m_radio;
};


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm

qtractorAuxSendIOMatrixForm::qtractorAuxSendIOMatrixForm ( QWidget *parent )
	: QDialog(parent), p_ui(new Ui::qtractorAuxSendIOMatrixForm), m_ui(*p_ui)
{
	m_ui.setupUi(this);

	m_ui.TableWidget->setForm(this);

	QObject::connect(m_ui.DialogButtons,
		SIGNAL(accepted()),
		SLOT(accept()));
	QObject::connect(m_ui.DialogButtons,
		SIGNAL(rejected()),
		SLOT(reject()));

	QDialog::adjustSize();
}


qtractorAuxSendIOMatrixForm::~qtractorAuxSendIOMatrixForm (void)
{
	delete p_ui;

	qDeleteAll(m_groups);
}


void qtractorAuxSendIOMatrixForm::setChannels ( int nins, int nouts )
{
#ifdef CONFIG_DEBUG
	qDebug("%s(%d, %d)", __func__, nins, nouts);
#endif
	m_ui.TableWidget->setRowCount(nins);
	m_ui.TableWidget->setColumnCount(nouts);
}


int qtractorAuxSendIOMatrixForm::inputChannels (void) const
{
	return m_ui.TableWidget->rowCount();
}


int qtractorAuxSendIOMatrixForm::outputChannels (void) const
{
	return m_ui.TableWidget->columnCount();
}


void qtractorAuxSendIOMatrixForm::setMatrix ( const QList<int>& matrix )
{
	m_matrix = matrix;
}


const QList<int>& qtractorAuxSendIOMatrixForm::matrix (void) const
{
	return m_matrix;
}



void qtractorAuxSendIOMatrixForm::inputChannelsChanged ( int index )
{
	setChannels(index + 1, outputChannels());
}


void qtractorAuxSendIOMatrixForm::outputChannelsChanged ( int index )
{
	setChannels(inputChannels(), index + 1);
}


const QList<QButtonGroup *>& qtractorAuxSendIOMatrixForm::groups (void) const
{
	return m_groups;
}


void qtractorAuxSendIOMatrixForm::refresh (void)
{
	m_ui.TableWidget->clear();

	qDeleteAll(m_groups);
	m_groups.clear();

	const int nouts = outputChannels();
	QStringList cols;
	for (int col = 0; col < nouts; ++col)
		cols.append(QString::number(col + 1));
	m_ui.TableWidget->setHorizontalHeaderLabels(cols);

	const int nins = inputChannels();
	QStringList rows;
	for (int row = 0; row < nins; ++row)
		rows.append(QString::number(row + 1));
	m_ui.TableWidget->setVerticalHeaderLabels(rows);

	for (int row = 0; row < nins; ++row) {
		int out = -1;
		if (row < m_matrix.size())
			out = m_matrix.at(row);
		else
		if (row < nouts)
			out = row;
		QButtonGroup *group = new QButtonGroup(this);
		group->setExclusive(true);
		for (int col = 0; col < nouts; ++col) {
			TableCell *cell = new TableCell();
			m_ui.TableWidget->setCellWidget(row, col, cell);
			group->addButton(cell->radio(), col);
			if (out == col)
				cell->radio()->setChecked(true);
		}
		m_groups.append(group);
	}
}


void qtractorAuxSendIOMatrixForm::accept (void)
{
	m_matrix.clear();

	const int nins = inputChannels();
	for (int row = 0; row < nins; ++row) {
		int out = -1;
		if (row < m_groups.size())
			out = m_groups.at(row)->checkedId();
		m_matrix.append(out);
	}

	QDialog::accept();
}


void qtractorAuxSendIOMatrixForm::reject (void)
{
	// Check if there's any pending changes...
	if (m_ui.TableWidget->isDirty()) {
		QMessageBox::StandardButtons buttons
			= QMessageBox::Apply
			| QMessageBox::Discard
			| QMessageBox::Cancel;
		switch (QMessageBox::warning(this,
			tr("Warning"),
			tr("Some settings have been changed.\n\n"
			"Do you want to apply the changes?"),
			buttons)) {
		case QMessageBox::Discard:
			break;
		case QMessageBox::Apply:
			accept();
			// Fall-thru...
		default:
			// Cancel.
			return;
		}
	}

	QDialog::reject();
}


//-------------------------------------------------------------------------
// qtractorAuxSendIOMatrixForm::TableWidget

qtractorAuxSendIOMatrixForm::TableWidget::TableWidget ( QWidget *parent )
	: QTableWidget(2, 2, parent), m_form(nullptr), m_dirty(0)
{
	QTableWidget::setSelectionMode(QTableWidget::NoSelection);

	QHeaderView *hheader = QTableWidget::horizontalHeader();
	if (hheader) {
		hheader->setSectionResizeMode(QHeaderView::Stretch);
		hheader->setMinimumSectionSize(24);
	}

	QHeaderView *vheader = QTableWidget::verticalHeader();
	if (vheader) {
		vheader->setSectionResizeMode(QHeaderView::Stretch);
		vheader->setMinimumSectionSize(24);
	}
}


void qtractorAuxSendIOMatrixForm::TableWidget::setForm ( qtractorAuxSendIOMatrixForm *form )
{
	m_form = form;
}


qtractorAuxSendIOMatrixForm *qtractorAuxSendIOMatrixForm::TableWidget::form (void) const
{
	return m_form;
}


bool qtractorAuxSendIOMatrixForm::TableWidget::isDirty (void) const
{
	return (m_dirty > 0);
}


void qtractorAuxSendIOMatrixForm::TableWidget::toggleCell ( int row, int col )
{
	const QList<QButtonGroup *>& groups = m_form->groups();
	if (row >= 0 && row < groups.size()) {
		QButtonGroup *group = groups.at(row);
		if (group) {
			QRadioButton *radio
				= qobject_cast<QRadioButton *> (group->button(col));
			if (radio) {
				const bool on = radio->isChecked();
				if (on) group->setExclusive(false);
				radio->setChecked(!on);
				if (on) group->setExclusive(true);
				++m_dirty;
			}
		}
	}
}


void qtractorAuxSendIOMatrixForm::TableWidget::keyPressEvent ( QKeyEvent *event )
{
	QTableWidget::keyPressEvent(event);

	if (event->key() == Qt::Key_Space) {
		const int row = QTableWidget::currentRow();
		const int col = QTableWidget::currentColumn();
		toggleCell(row, col);
	}
}



void qtractorAuxSendIOMatrixForm::TableWidget::mousePressEvent ( QMouseEvent *event )
{
	QTableWidget::mousePressEvent(event);

	if (m_form == nullptr)
		return;

	const QPoint& pos = event->pos();
	const int row = QTableWidget::rowAt(pos.y());
	const int col = QTableWidget::columnAt(pos.x());
	toggleCell(row, col);
}


// end of qtractorAuxSendIOMatrixForm.cpp
