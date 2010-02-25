/************************************************************************
 *
 * Save/Restore extension for LV2
 *
 * Copyright (C) 2007-2008 Lars Luthman <lars.luthman@gmail.com>
 * 
 * This header is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This header is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301
 * USA.
 *
 ***********************************************************************/

#ifndef LV2_SAVERESTORE
#define LV2_SAVERESTORE

#include <lv2.h>

#define LV2_SAVERESTORE_URI "http://ll-plugins.nongnu.org/lv2/ext/saverestore"

#ifdef __cplusplus
extern "C" {
#endif
  
  /** @file
      
      This extension defines a way to save and restore the internal state
      of an LV2 plugin instance. The internal state does not include input
      port values, which are visible to the host, but rather things that
      the host has no way of knowing, such as loaded sample buffers that have
      been modified using GUI commands, or internal variables that have been
      updated and modified as a reaction to the input streams. A plugin that
      implements this extension must list 
      <http://ll-plugins.nongnu.org/lv2/ext/saverestore> as an optional
      or required Feature in its RDF data, and it must have an 
      extension_data() function pointer in its descriptor that returns a 
      pointer to a LV2SR_Descriptor, defined below, when called with the same
      URI. The data pointer for the host feature passed to the plugin's 
      instantiate() function should be NULL.
      
      The function pointer members in the LV2SR_Descriptor struct belong to the
      Instantiation class, and they must not be NULL.
      
      For the host, the typical use of this extension would go like this:
      
      * To save a plugin instance that has not been saved previously:
        - Create a directory /home/foo/mysession/myplugininstance
	- LV2SR_File* files;
	  lv2srdesc->save(myplugin_handle,
	                  "/home/foo/mysession/myplugininstance",
			  &files);
	- for each element in 'files' that has the 'must_copy' flag set, copy
	  that file into /home/foo/mysession/myplugininstance if isn't already
	  there and change the 'path' member to reflect the new location
	- save the names and paths for all elements in 'files' somewhere as
          part of the session
    
      * To restore a plugin instance that has been saved:
	- instantiate a new instance of that plugin
	- load the file paths and names from your session, allocate an array of
	  LV2SR_File pointers, allocate the LV2SR_File objects and fill in the
	  'name' and 'path' members
	- lv2srdesc->restore(myplugin_handle, files);
      
      * To save a plugin instance that already has been saved earlier:
        - LV2SR_File* files;
	  lv2srdesc->save(myplugin_handle,
	                  "/home/foo/mysession/myplugininstance",
			  &files);
	- erase all files in /home/foo/mysession/myplugininstance that aren't
	  listed in the new 'files' array
	- for each element in 'files' that has the 'must_copy' flag set, copy
	  that file into /home/foo/mysession/myplugininstance if isn't already
	  there and change the 'path' member to reflect the new location
	- save the names and paths for all elements in 'files' somewhere as
          part of the session
	  
      This extension allows for two "levels" of saving a plugin's state - 
      the host can either just copy the files with the 'must_copy' flag set 
      into its session directory and rely on the other files staying at the
      same paths in the filesystem (a "shallow" save) or it can copy every
      mentioned file into its session directory (a "deep" save). The latter
      may be useful when moving sessions between machines.
      
      Most simple plugins whose behaviour is completely defined by the current
      values in its control input ports should not need to implement this
      extension.
      
  */
  

  /** A simple struct that represents a file used by a plugin instance. */
  typedef struct _LV2SR_File {
    
    /** A '\0'-terminated string that the plugin uses to identify the file (it 
	can't use the file path since the host may move the file). The host 
	should copy this verbatim and not try to interpret it in any way. */
    char* name;
    
    /** The absolute path to the file. */
    char* path;
    
    /** This is set to 1 by the plugin in the save() callback if this file must
	be copied by the host into persistent storage, 0 otherwise. Its value
	is not significant in the restore() callback. */
    uint32_t must_copy;
    
  } LV2SR_File;
  
  
  /** A pointer to an object of this type is returned by extension_data() when
      called with the URI for this host feature. */
  typedef struct _LV2SR_Descriptor {
    
    /** This function tells the plugin instance to save its current state.
	
        The files parameter should be a pointer to a valid LV2SR_File**. After 
	the call to save() has returned, files MUST point to the start of 
	a NULL-terminated array of LV2SR_File pointers allocated by 
	the plugin, that the host should store as part of the plugin's saved
	state. The path members of the elements of this	array MUST all be 
	valid filesystem paths. If the member must_copy of an element of this
	array is nonzero, the host MUST copy the file at the associated path
	into some sort of persistant storage as a part of the plugin's state. 
	If must_copy is zero, the host is free to choose whether to copy the 
	file, or rely on the file staying in the same place until the plugin 
	state is restored again. This array as well as all the name and path
	members of its elements should be deallocated by the host.
	
	The parameter directory MUST be a path to an existing directory. If 
        the plugin creates any new must_copy files as a part of its save 
	process it SHOULD create the new files in that directory. This is to
	help the host avoid unnecessary copying of files.

	A plugin that creates new files as a part of its save process must NOT
	overwrite or delete existing files in that directory, even if it knows
	that it created them itself for an earlier save. The host may want to
	keep them around for reverting back to an earlier state.
	
	This function should return NULL if the plugin has successfully saved
	its state and a dynamically allocated error message if not (e.g. not 
	enough room on the disk). Any returned error message should be 
	deallocated by the host using free(). If NULL is returned the host 
	should not try to access or deallocate the array pointed to by the 
	files parameter, or it's elements.
	
	This function pointer must not be set to NULL.
    */
    char* (*save)(LV2_Handle       handle,
		  const char*      directory,
		  LV2SR_File***    files);
    
    /** This function tells the plugin instance to restore to a saved state.
	
        The files parameter must be a pointer to a NULL-terminated array of 
	LV2SR_File pointers, which should be identical to an array produced by
	an instance of the same plugin in an earlier call to save(), with the
	exception that the file paths may be changed to reflect the new 
	locations of the files (the session may have been moved to some other 
	part of the filesystem, the host may have reorganised its file 
	hierarchy etc).
	
	This function should return NULL if it successfully restores a state
	from the given parameters and an error message otherwise. Any returned
	error message should be deallocated by the host. 

	This function pointer must not be set to NULL.
    */
    char* (*restore)(LV2_Handle         handle,
		     const LV2SR_File** files);

  } LV2SR_Descriptor;


#ifdef __cplusplus
}
#endif


#endif
