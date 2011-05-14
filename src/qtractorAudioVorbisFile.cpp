// qtractorAudioVorbisFile.cpp
//
/****************************************************************************
   Copyright (C) 2005-2011, rncbc aka Rui Nuno Capela. All rights reserved.

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
#include "qtractorAudioVorbisFile.h"

#ifdef CONFIG_LIBVORBIS
// libvorbis encoder API.
#include <vorbis/vorbisenc.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <time.h>


//----------------------------------------------------------------------
// class qtractorAudioVorbisFile -- Buffered audio file implementation.
//

// Constructor.
qtractorAudioVorbisFile::qtractorAudioVorbisFile ( unsigned short iChannels,
	unsigned int iSampleRate, unsigned int iBufferSize )
{
	// Initialize state variables.
	m_iMode  = qtractorAudioVorbisFile::None;
	m_pFile  = NULL;

	// Estimate channel and sample-rate (for encoder purposes only).
	m_iChannels   = iChannels;
	m_iSampleRate = iSampleRate;
	m_iFrames     = 0;

#ifdef CONFIG_LIBVORBIS
	m_ovinfo = NULL;
	m_ovsect = 0;
#endif	// CONFIG_LIBVORBIS

	// Estimate buffer size.
	m_iBufferSize = (4096 << 1);
	// Adjust size the next nearest power-of-two.
	while (m_iBufferSize < iBufferSize)
		m_iBufferSize <<= 1;
}

// Destructor.
qtractorAudioVorbisFile::~qtractorAudioVorbisFile (void)
{
	close();
}


// Open method.
bool qtractorAudioVorbisFile::open ( const QString& sFilename, int iMode )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioVorbisFile::open(\"%s\", %d)",
		sFilename.toUtf8().constData(), iMode);
#endif
	close();

	// As said, need a minimum of specification for write mode.
	if (iMode == Write && (m_iChannels == 0 || m_iSampleRate == 0))
		return false;

	// Whether for Read or Write...
	const char *pszMode;
	switch (iMode) {
	case Read:
		pszMode = "rb";
		break;
	case Write:
		pszMode = "wb";
		break;
	default:
		return false;
	}

	// Now open it.
	QByteArray aFilename = sFilename.toUtf8();
	m_pFile = ::fopen(aFilename.constData(), pszMode);
	if (m_pFile == NULL)
		return false;

#ifdef CONFIG_LIBVORBIS

	// Now's time for the vorbis stuff...
	switch (iMode) {

		case Read:
		{
			// Open the Ogg Vorbis file for decoding...
			if (::ov_open(m_pFile, &m_ovfile, NULL, 0) < 0) {
				close();
				return false;
			}
			// Grab the vorbis file info...
			m_ovinfo = ::ov_info(&m_ovfile, -1);
			m_ovsect = 0;
			break;
		}
	
		case Write:
		{
			// Init the Ogg Vorbis structs for encoding...
			m_ovinfo = new vorbis_info;
			vorbis_info_init(m_ovinfo);
			// Using a VBR quality mode:
			// Quality=0.1 (lowest quality, smallest file)
			// Quality=1.0 (highest quality, largest file).
			int iQuality = qtractorAudioFileFactory::defaultQuality();
			if (vorbis_encode_init_vbr(m_ovinfo, 
					(long) m_iChannels,
					(long) m_iSampleRate,
					0.1f * float(iQuality)) != 0) {
				close();
				return false;
			}
			// Add a comments
			vorbis_comment_init(&m_ovcomment);
			vorbis_comment_add_tag(&m_ovcomment,
				(char *) "ENCODER", (char *) "qtractorAudioVorbisFile");
			// Set up the analysis state and auxiliary encoding storage.
			vorbis_analysis_init(&m_ovdsp, m_ovinfo);
			vorbis_block_init(&m_ovdsp, &m_ovblock);
			// Set up our packet->stream encoder;
			// pick a random serial number; that way we can more
			// likely build chained streams just by concatenation...
			::srand(::time(NULL));
			ogg_stream_init(&m_ovstate, ::rand());
			// Vorbis streams begin with three headers; the initial header
			// (with most of the codec setup parameters) which is mandated
			// by the ogg bitstream spec; the second header holds any comment
			// fields; the third header holds the bitstream codebook.
			// We merely need to make the headers, then pass them to
			// libvorbis one at a time; libvorbis handles the additional
			// ogg bitstream constraints.
			ogg_packet ovhcodec;
			ogg_packet ovhcomment;
			ogg_packet ovhcodebook;
			vorbis_analysis_headerout(&m_ovdsp, &m_ovcomment,
				&ovhcodec, &ovhcomment, &ovhcodebook);
			// Automatically placed in its own page...
			ogg_stream_packetin(&m_ovstate, &ovhcodec);
			ogg_stream_packetin(&m_ovstate, &ovhcomment);
			ogg_stream_packetin(&m_ovstate, &ovhcodebook);
			// This ensures the actual audio data will
			// start on a new page, as per spec...
			ogg_page ovpage;
			while (ogg_stream_flush(&m_ovstate, &ovpage)) {
				fwrite(ovpage.header, 1, ovpage.header_len, m_pFile);
				fwrite(ovpage.body,   1, ovpage.body_len,   m_pFile);
			}
			// Done on write/encoder overhead.
			break;
		}
	}

	// Set open mode (deterministically).
	m_iMode = iMode;

	return true;

#else	// CONFIG_LIBVORBIS

	return false;

#endif
}


// Read method.
int qtractorAudioVorbisFile::read ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioVorbisFile::read(%p, %d)", ppFrames, iFrames);
#endif

	int nread = 0;

#ifdef CONFIG_LIBVORBIS
	float **ppBuffer;
	nread = ::ov_read_float(&m_ovfile, &ppBuffer, iFrames, &m_ovsect);
	if (nread < 0)	// HACK: Just in case things get go thru...
		nread = ::ov_read_float(&m_ovfile, &ppBuffer, iFrames, &m_ovsect);
	if (nread > 0) {
		unsigned short i;
		for (i = 0; i < (unsigned short) m_ovinfo->channels; ++i) {
			::memcpy(ppFrames[i], ppBuffer[i], nread * sizeof(float));
		}
	}
#endif	// CONFIG_LIBVORBIS

	return nread;
}


// Write method.
int qtractorAudioVorbisFile::write ( float **ppFrames, unsigned int iFrames )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioVorbisFile::write(%p, %d)", ppFrames, iFrames);
#endif

	int nwrite = 0;

#ifdef CONFIG_LIBVORBIS

	// Expose the buffer to submit data...
	float **ppBuffer = vorbis_analysis_buffer(&m_ovdsp, iFrames);

	// Uninterleaved samples...
	for (unsigned short i = 0; i < m_iChannels; ++i)
		::memcpy(ppBuffer[i], ppFrames[i], iFrames * sizeof(float));

	// Tell the library how much we actually submitted...
	vorbis_analysis_wrote(&m_ovdsp, iFrames);

	// Flush this block...
	flush();

	// We've wrote it all, hopefully...
	m_iFrames += iFrames;
	nwrite = iFrames;

#endif	// CONFIG_LIBVORBIS

	return nwrite;
}


// Flush encoder buffers...
void qtractorAudioVorbisFile::flush ( bool fEos )
{
#ifdef CONFIG_LIBVORBIS

	// End of file: this can be done implicitly in the mainline,
	// but it's easier to see here in non-clever fashion;
	// tell the library we're at end of stream so that it can handle
	// the last frame and mark end of stream in the output properly...
	if (fEos)
		vorbis_analysis_wrote(&m_ovdsp, 0);

	// Vorbis does some data preanalysis, then divides up blocks
	// for more involved (potentially parallel) processing.
	// Get a single block for encoding now...

	ogg_packet ovpacket;	// raw packet of data for decode.
	ogg_page   ovpage;		// ogg bitstream page; vorbis packets are inside.

	while (vorbis_analysis_blockout(&m_ovdsp, &m_ovblock) == 1) {
		// Analysis, assume we want to use bitrate management...
		vorbis_analysis(&m_ovblock, NULL);
		vorbis_bitrate_addblock(&m_ovblock);
		// Probably we should do this also as left-over processing...
		while (vorbis_bitrate_flushpacket(&m_ovdsp, &ovpacket)) {
			// Weld the packet into the bitstream...
			ogg_stream_packetin(&m_ovstate, &ovpacket);
			// Write out pages (if any)...
			while (ogg_stream_pageout(&m_ovstate, &ovpage)) {
				fwrite(ovpage.header, 1, ovpage.header_len, m_pFile);
				fwrite(ovpage.body,   1, ovpage.body_len,   m_pFile);
				if (ogg_page_eos(&ovpage))
					break;
			}
		}
	}

#endif	// CONFIG_LIBVORBIS
}


// Seek method.
bool qtractorAudioVorbisFile::seek ( unsigned long iOffset )
{
#ifdef DEBUG_0
	qDebug("qtractorAudioVorbisFile::seek(%d)", iOffset);
#endif

#ifdef CONFIG_LIBVORBIS
	return (::ov_pcm_seek(&m_ovfile, iOffset) == 0);
#else
	return false;
#endif
}


// Close method.
void qtractorAudioVorbisFile::close (void)
{
#ifdef DEBUG_0
	qDebug("qtractorAudioVorbisFile::close()");
#endif

	if (m_pFile) {
#ifdef CONFIG_LIBVORBIS	
		// Reinitialize libvorbis stuff...
		switch (m_iMode) {
		case Read:
			::ov_clear(&m_ovfile);
		//	::fclose(m_pFile); -- already closed on ov_clear?
			break;
		case Write:
			flush(true);
			ogg_stream_clear(&m_ovstate);
			vorbis_block_clear(&m_ovblock);
			vorbis_dsp_clear(&m_ovdsp);
			vorbis_comment_clear(&m_ovcomment);
			vorbis_info_clear(m_ovinfo);
			delete m_ovinfo;
			::fclose(m_pFile);
			break;
		}
#else
		::fclose(m_pFile);
#endif
		m_pFile = NULL;
	}

	// Reset all other state relevant variables.
#ifdef CONFIG_LIBVORBIS
	m_ovsect = 0;
	::memset(&m_ovfile,    0, sizeof(m_ovfile));
	::memset(&m_ovstate,   0, sizeof(m_ovstate));
	::memset(&m_ovblock,   0, sizeof(m_ovblock));
	::memset(&m_ovdsp,     0, sizeof(m_ovdsp));
	::memset(&m_ovcomment, 0, sizeof(m_ovcomment));
	m_ovinfo = NULL;
#endif

	m_iMode = qtractorAudioVorbisFile::None;
	m_iFrames = 0;
}


// Open mode accessor.
int qtractorAudioVorbisFile::mode (void) const
{
	return m_iMode;
}


// Open channel(s) accessor.
unsigned short qtractorAudioVorbisFile::channels (void) const
{
#ifdef CONFIG_LIBVORBIS
	return (m_ovinfo ? m_ovinfo->channels : 0);
#else
	return 0;
#endif
}


// Estimated number of frames specialty (aprox. 8secs).
unsigned long qtractorAudioVorbisFile::frames (void) const
{
#ifdef CONFIG_LIBVORBIS
	if (m_iMode == Read)	
		return ::ov_pcm_total((OggVorbis_File *) &m_ovfile, -1);
#endif
	return m_iFrames;
}


// Sample rate specialty.
unsigned int qtractorAudioVorbisFile::sampleRate (void) const
{
#ifdef CONFIG_LIBVORBIS
	return (m_ovinfo ? m_ovinfo->rate : 0);
#else
	return 0;
#endif
}


// end of qtractorAudioVorbisFile.cpp
