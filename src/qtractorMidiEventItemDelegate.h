// qtractorMidiEventItemDelegate.h
//
/****************************************************************************
   Copyright (C) 2005-2010, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorMidiEventItemDelegate_h
#define __qtractorMidiEventItemDelegate_h

#include <QItemDelegate>


//----------------------------------------------------------------------------
// qtractorMidiEventItemDelegate -- Custom (tree) list item delegate.

class qtractorMidiEventItemDelegate : public QItemDelegate
{
    Q_OBJECT

public:

	// Constructor.
    qtractorMidiEventItemDelegate(QObject *pParent = NULL);

	// Destructor.
    ~qtractorMidiEventItemDelegate();

	// Keyboard event hook.
    bool eventFilter(QObject *pObject, QEvent *pEvent);

	// QItemDelegate Interface...

    void paint(QPainter *pPainter,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

    QSize sizeHint(
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

    QWidget *createEditor(QWidget *pParent,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;

    void setEditorData(QWidget *pEditor,
		const QModelIndex& index) const;

    void setModelData(QWidget *pEditor,
		QAbstractItemModel *pModel,
		const QModelIndex& index) const;

    void updateEditorGeometry(QWidget *pEditor,
		const QStyleOptionViewItem& option,
		const QModelIndex& index) const;
};


#endif  // __qtractorMidiEventItemDelegate_h


// end of qtractorMidiEventItemDelegate.h
