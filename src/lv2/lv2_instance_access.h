/* lv2_extension_data.h - C header file for the LV2 Instance Access extension.
 * Copyright (C) 2008-2009 David Robillard <http://drobilla.net>
 *
 * This header is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This header is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this header; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 01222-1307 USA
 */

#ifndef LV2_INSTANCE_ACCESS_H
#define LV2_INSTANCE_ACCESS_H

#define LV2_INSTANCE_ACCESS_URI "http://lv2plug.in/ns/ext/instance-access"


/** @file
 * C header for the LV2 Instance Access extension
 * <http://lv2plug.in/ns/ext/instance-access>.
 *
 * This extension defines a method for (e.g.) plugin UIs to get a direct
 * handle to an LV2 plugin instance (LV2_Handle), if possible.
 *
 * To support this feature the host must pass an LV2_Feature struct to the
 * UI instantiate method with URI "http://lv2plug.in/ns/ext/instance-access"
 * and data pointed directly to the LV2_Handle of the plugin instance.
 */


#endif /* LV2_INSTANCE_ACCESS_H */

