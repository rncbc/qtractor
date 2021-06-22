// qtractorLv2Gtk2Plugin.cpp
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

#include "qtractorAbout.h"

#include "qtractorLv2Gtk2Plugin.h"


#ifdef CONFIG_LV2
#ifdef CONFIG_LV2_UI
#ifdef CONFIG_LV2_UI_GTK2

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include <gtk/gtk.h>

#ifdef CONFIG_LV2_UI_GTKMM2
#include <gtkmm/main.h>
#endif

#pragma GCC diagnostic pop


namespace qtractorLv2Gtk2Plugin {

static unsigned int g_lv2_ui_gtk2_init = 0;

#ifdef CONFIG_LV2_UI_GTKMM2
static Gtk::Main *g_lv2_ui_gtkmm2_main = nullptr;
#endif

// Module entry point.
void init_main (void)
{
	if (++g_lv2_ui_gtk2_init == 1) {
		::gtk_init(nullptr, nullptr);
	#ifdef CONFIG_LV2_UI_GTKMM2
		if (g_lv2_ui_gtkmm2_main == nullptr)
			g_lv2_ui_gtkmm2_main = new Gtk::Main(nullptr, nullptr);
	#endif
	}
}


// Module exit point.
void exit_main (void)
{
	if (--g_lv2_ui_gtk2_init == 0) {
	#ifdef CONFIG_LV2_UI_GTKMM2
		if (g_lv2_ui_gtkmm2_main) {
			delete g_lv2_ui_gtkmm2_main;
			g_lv2_ui_gtkmm2_main = nullptr;
		}
	#endif
	}
}

}	// namespace qtractorLv2Gtk2Plugin

#endif	// CONFIG_LV2_UI_GTK2
#endif	// CONFIG_LV2_UI
#endif	// CONFIG_LV2

// end of qtractorLv2Gtk2Plugin.cpp
