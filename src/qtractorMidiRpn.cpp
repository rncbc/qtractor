// qtractorMidiRpn.cpp
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

#include "qtractorMidiRpn.h"

#include <QHash>


#define RPN_MSB   0x65
#define RPN_LSB   0x64

#define NRPN_MSB  0x63
#define NRPN_LSB  0x62

#define DATA_MSB  0x06
#define DATA_LSB  0x26


#define CC14_MSB_MIN    0x00
#define CC14_MSB_MAX    0x20

#define CC14_LSB_MIN    CC14_MSB_MAX
#define CC14_LSB_MAX   (CC14_MSB_MAX << 1)


//---------------------------------------------------------------------
// xrpn_data14 - decl.
//
class xrpn_data14
{
public:

	xrpn_data14()
		: m_msb(0), m_lsb(0) {}

	xrpn_data14 ( const xrpn_data14& data )
		: m_msb(data.m_msb), m_lsb(data.m_lsb) {}

	void clear() { m_msb = m_lsb = 0; }

	bool is_msb () const
		{ return (m_msb & 0x80); }
	void set_msb ( unsigned char msb )
		{ m_msb = (msb & 0x7f) | 0x80; }
	unsigned msb() const
		{ return (m_msb & 0x7f); }

	bool is_lsb () const
		{ return (m_lsb & 0x80); }
	void set_lsb ( unsigned char lsb )
		{ m_lsb = (lsb & 0x7f) | 0x80; }
	unsigned lsb() const
		{ return (m_lsb & 0x7f); }

	unsigned short data() const
	{
		unsigned short val = 0;

		if (is_lsb()) {
			val += (m_lsb & 0x7f);
			if (is_msb())
				val += (m_msb & 0x7f) << 7;
		}
		else
		if (is_msb())
			val += (m_msb & 0x7f);

		return val;
	}

	bool is_any() const
		{ return is_msb() || is_lsb(); }
	bool is_7bit() const
		{ return is_any() && !is_14bit(); }
	bool is_14bit() const
		{ return is_msb() && is_lsb(); }

private:

	unsigned char m_msb;
	unsigned char m_lsb;
};


//---------------------------------------------------------------------
// xrpn_item - decl.
//
class xrpn_item
{
public:

	xrpn_item() : m_time(0), m_port(0), m_status(0) {}

	xrpn_item ( const xrpn_item& item ) : m_time(item.m_time),
		m_port(item.m_port), m_status(item.m_status),
		m_param(item.m_param), m_value(item.m_value) {}

	void clear()
	{
		m_time = 0;
		m_port = 0;
		m_status = 0;
		m_param.clear();
		m_value.clear();
	}

	void set_time(unsigned long time)
		{ m_time = time; }
	unsigned long time() const
		{ return m_time; }

	void set_port(int port)
		{ m_port = port; }
	int port() const
		{ return m_port; }

	void set_status(unsigned char status)
		{ m_status = status; }
	unsigned char status() const
		{ return m_status; }

	qtractorMidiRpn::Type type() const
		{ return qtractorMidiRpn::Type(m_status & 0xf0); }
	unsigned short channel() const
		{ return (m_status & 0x0f); }

	bool is_param_msb() const
		{ return m_param.is_msb(); }
	bool is_param_lsb() const
		{ return m_param.is_lsb(); }

	void set_param_msb(unsigned char msb)
		{ m_param.set_msb(msb); }
	void set_param_lsb(unsigned char lsb)
		{ m_param.set_lsb(lsb); }

	unsigned char param_msb() const
		{ return m_param.msb(); }
	unsigned char param_lsb() const
		{ return m_param.lsb(); }

	unsigned short param() const
		{ return m_param.data(); }

	bool is_value_msb() const
		{ return m_value.is_msb(); }
	bool is_value_lsb() const
		{ return m_value.is_lsb(); }

	void set_value_msb(unsigned char msb)
		{ m_value.set_msb(msb); }
	void set_value_lsb(unsigned char lsb)
		{ m_value.set_lsb(lsb); }

	unsigned char value_msb() const
		{ return m_value.msb(); }
	unsigned char value_lsb() const
		{ return m_value.lsb(); }

	unsigned short value() const
		{ return m_value.data(); }

	bool is_any() const
		{ return m_param.is_any() || m_value.is_any(); }
	bool is_7bit() const
		{ return m_param.is_any() && m_value.is_7bit(); }
	bool is_14bit() const
		{ return m_param.is_any() && m_value.is_14bit(); }

private:

	unsigned long m_time;
	int           m_port;
	unsigned char m_status;
	xrpn_data14   m_param;
	xrpn_data14   m_value;
};

typedef QHash<unsigned int, xrpn_item> xrpn_cache;


//---------------------------------------------------------------------
// xrpn_queue - decl.
//
class xrpn_queue
{
public:

	xrpn_queue ( unsigned int size = 0 )
		: m_size(0), m_mask(0), m_read(0), m_write(0), m_events(0)
		{ resize(size); }

	~xrpn_queue () { if (m_events) delete m_events; }
	
	void resize ( unsigned int size )
	{
		unsigned int new_size = 4; // must be a power-of-2...
		while (new_size < size)
			new_size <<= 1;
		if (new_size > m_size) {
			const unsigned int old_size = m_size;
			qtractorMidiRpn::Event *new_events
				= new qtractorMidiRpn::Event [new_size];
			qtractorMidiRpn::Event *old_events = m_events;
			if (old_events) {
				if (m_write > m_read) {
					::memcpy(new_events + m_read, old_events + m_read,
						(m_write - m_read) * sizeof(qtractorMidiRpn::Event));
				}
				else
				if (m_write < m_read) {
					::memcpy(new_events + m_read, old_events + m_read,
						(old_size - m_read) * sizeof(qtractorMidiRpn::Event));
					if (m_write > 0) {
						::memcpy(new_events + old_size, old_events,
							m_write * sizeof(qtractorMidiRpn::Event));
					}
					m_write += old_size;
				}
			}
			m_size = new_size;
			m_mask = new_size - 1;
			m_events = new_events;
			if (old_events)
				delete old_events;
		}
	}

	void clear() { m_read = m_write = 0; }
 
	bool push ( unsigned long time, int port,
		unsigned char status, unsigned short param, unsigned short value )
	{
		qtractorMidiRpn::Event event;

		event.time   = time;
		event.port   = port;
		event.status = status;
		event.param  = param;
		event.value  = value;

		return push(event);
	}

	bool push ( const qtractorMidiRpn::Event& event )
	{
		if (count() >= m_mask)
			resize(m_size + 4);
		const unsigned int w = (m_write + 1) & m_mask;
		if (w == m_read)
			return false;
		m_events[m_write] = event;
		m_write = w;
		return true;
	}

	bool pop ( qtractorMidiRpn::Event& event )
	{
		const unsigned int r = m_read;
		if (r == m_write)
			return false;
		event = m_events[r]; 
		m_read = (r + 1) & m_mask;
		return true;
	}

	bool is_pending () const
		{ return (m_read != m_write); }

 	unsigned int count() const
	{
		if (m_write < m_read)
			return (m_write + m_size - m_read) & m_mask;
		else
			return (m_write - m_read);
	}

private:

	unsigned int m_size;
	unsigned int m_mask;
	unsigned int m_read;
	unsigned int m_write;

	qtractorMidiRpn::Event *m_events;
};


//---------------------------------------------------------------------
// qtractorMidiRpn::Impl - decl.
//
class qtractorMidiRpn::Impl
{
public:

	Impl() : m_count(0) {}

	bool is_pending () const
		{ return m_queue.is_pending(); }

	bool dequeue ( qtractorMidiRpn::Event& event )
		{ return m_queue.pop(event); }

	void flush()
	{
		if (m_count > 0) {
			xrpn_cache::Iterator iter = m_cache.begin();
			const xrpn_cache::Iterator& iter_end = m_cache.end();
			for ( ; iter != iter_end; ++iter)
				enqueue(iter.value());
			m_cache.clear();
		//	m_count = 0;
		}
	}

	bool process ( const qtractorMidiRpn::Event& event )
	{
		const unsigned short channel = (event.status & 0x0f);

		if (event.param == RPN_MSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_msb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::RPN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::RPN | channel);
				++m_count;
			}
			item.set_param_msb(event.value);
			return true;
		}
		else
		if (event.param == RPN_LSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_lsb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::RPN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::RPN | channel);
				++m_count;
			}
			item.set_param_lsb(event.value);
			return true;
		}
		else
		if (event.param == NRPN_MSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_msb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::NRPN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::NRPN | channel);
				++m_count;
			}
			item.set_param_msb(event.value);
			return true;
		}
		else
		if (event.param == NRPN_LSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_lsb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::NRPN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::NRPN | channel);
				++m_count;
			}
			item.set_param_lsb(event.value);
			return true;
		}
		else
		if (event.param == DATA_MSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.type() == qtractorMidiRpn::None)
				return false;
			if (item.is_value_msb()
				|| (item.type() != qtractorMidiRpn::RPN
				 && item.type() != qtractorMidiRpn::NRPN)) {
				enqueue(item);
				return false;
			}
			item.set_value_msb(event.value);
			if (item.is_14bit())
				enqueue(item);
			return true;
		}
		else
		if (event.param == DATA_LSB) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.type() == qtractorMidiRpn::None)
				return false;
			if (item.is_value_lsb()
				|| (item.type() != qtractorMidiRpn::RPN
				 && item.type() != qtractorMidiRpn::NRPN)) {
				enqueue(item);
				return false;
			}
			item.set_value_lsb(event.value);
			if (item.is_14bit())
				enqueue(item);
			return true;
		}
		else
		if (event.param > CC14_MSB_MIN && event.param < CC14_MSB_MAX) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_msb() || item.is_value_msb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::CC14)
				|| (item.type() == qtractorMidiRpn::CC14
					&& item.param_lsb() != event.param + CC14_LSB_MIN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::CC14 | channel);
				++m_count;
			}
			item.set_param_msb(event.param);
			item.set_value_msb(event.value);
			if (item.is_14bit())
				enqueue(item);
			return true;
		}
		else
		if (event.param > CC14_LSB_MIN && event.param < CC14_LSB_MAX) {
			xrpn_item& item = get_item(event.port, channel);
			if (item.is_param_lsb() || item.is_value_lsb()
				|| (item.is_any() && item.type() != qtractorMidiRpn::CC14)
				|| (item.type() == qtractorMidiRpn::CC14
					&& item.param_msb() != event.param - CC14_LSB_MIN))
				enqueue(item);
			if (item.type() == qtractorMidiRpn::None) {
				item.set_time(event.time);
				item.set_status(qtractorMidiRpn::CC14 | channel);
				++m_count;
			}
			item.set_param_lsb(event.param);
			item.set_value_lsb(event.value);
			if (item.is_14bit())
				enqueue(item);
			return true;
		}

		return false;
	}

protected:

	xrpn_item& get_item ( int port, unsigned short channel )
	{
		xrpn_item& item = m_cache[(port << 4) | channel];
		item.set_port(port);
		return item;
	}

	void enqueue ( xrpn_item& item )
	{
		if (item.type() == qtractorMidiRpn::None)
			return;

		const unsigned long time = item.time();
		const int port = item.port();

		if (item.type() == qtractorMidiRpn::CC14) {
			if (item.is_14bit()) {
				m_queue.push(time, port, item.status(),
					item.param_msb(), item.value());
			} else  {
				const unsigned char status = qtractorMidiRpn::CC | item.channel();
				if (item.is_value_msb())
					m_queue.push(time, port, status,
						item.param_msb(), item.value_msb());
				if (item.is_value_lsb())
					m_queue.push(time, port, status,
						item.param_lsb(), item.value_lsb());
			}
		}
		else
		if (item.is_14bit()) {
			m_queue.push(time, port, item.status(), item.param(), item.value());
		} else {
			const unsigned char status = qtractorMidiRpn::CC | item.channel();
			if (item.type() == qtractorMidiRpn::RPN) {
				if (item.is_param_msb())
					m_queue.push(time, port, status, RPN_MSB, item.param_msb());
				if (item.is_param_lsb())
					m_queue.push(time, port, status, RPN_LSB, item.param_lsb());
			}
			else
			if (item.type() == qtractorMidiRpn::NRPN) {
				if (item.is_param_msb())
					m_queue.push(time, port, status, NRPN_MSB, item.param_msb());
				if (item.is_param_lsb())
					m_queue.push(time, port, status, NRPN_LSB, item.param_lsb());
			}
			if (item.is_value_msb())
				m_queue.push(time, port, status, DATA_MSB, item.value_msb());
			if (item.is_value_lsb())
				m_queue.push(time, port, status, DATA_LSB, item.value_lsb());
		}

		item.clear();
		--m_count;
	}

private:

	unsigned int m_count;

	xrpn_cache m_cache;
	xrpn_queue m_queue;
};


//---------------------------------------------------------------------
// qtractorMidiRpn - impl.
//

qtractorMidiRpn::qtractorMidiRpn (void)
{
	m_pImpl = new qtractorMidiRpn::Impl();
}


qtractorMidiRpn::~qtractorMidiRpn (void)
{
	delete m_pImpl;
}


bool qtractorMidiRpn::isPending (void) const
{
	return m_pImpl->is_pending();
}


bool qtractorMidiRpn::process ( const qtractorMidiRpn::Event& event )
{
	return m_pImpl->process(event);
}


bool qtractorMidiRpn::dequeue ( qtractorMidiRpn::Event& event )
{
	return m_pImpl->dequeue(event);
}


void qtractorMidiRpn::flush (void)
{
	m_pImpl->flush();
}


// end of qtractorMidiRpn.cpp
