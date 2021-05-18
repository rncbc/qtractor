// qtractorLv2Gtk2Plugin.h
//
/****************************************************************************
   Copyright (C) 2005-2021, rncbc aka Rui Nuno Capela. All rights reserved.

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

#ifndef __qtractorLv2Gtk2Plugin_h
#define __qtractorLv2Gtk2Plugin_h

#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_UI_GTK2


namespace qtractorLv2Gtk2Plugin {

	// Module entry/exit points.
	void init_main();
	void exit_main();

}	// namespace qtractorLv2Gtk2Plugin


#endif	// CONFIG_LV2_UI_GTK2
#endif	// CONFIG_LV2_UI
#endif	// CONFIG_LV2

#endif  // __qtractorLv2Gtk2Plugin_h

// end of qtractorLv2Gtk2Plugin.h
