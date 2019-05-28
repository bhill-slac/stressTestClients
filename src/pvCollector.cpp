#include <list>
#include <algorithm>

#include <epicsMath.h>
#include <errlog.h>
#include <pv/reftrack.h>

#include "pvCollector.h"

#include <epicsExport.h>

namespace pvd = epics::pvData;

// timeout to flush partial events
static double maxEventAge = 2.5;

int collectorDebug;

size_t pvCollector<double>::c_num_instances	= 0;
size_t pvCollector<double>::c_max_events	= 360000;	// 1 hour at 100hz

void	pvCollector::setMaxEvents( size_t maxEvents )
{
	c_max_events = maxEvents;
}

pvCollector::PVCollector( const std::string pvName, unsigned int prio)
    :	m_pvName(	pvName	)
    ,	m_events()
{
    REFTRACE_INCREMENT(num_instances);

    m_events.resize(max_events);
}

pvCollector::~PVCollector()
{
    REFTRACE_DECREMENT(c_num_instances);
    close();
}


void pvCollector::close()
{
    {
        Guard G(m_mutex);
        run = false;
    }
    wakeup.signal();
    processor.exitWait();
}

template <typename T>
void pvCollector::saveValue( epicsUInt64 tsKey, T value )
{
	try
	{
		Guard	guard( m_mutex );
		while ( m_events<T>.size() >= c_maxEvents )
		{
			m_events<T>.erase( m_events<T>.begin() );
		}
		(void) m_events<T>.insert( m_events<T>.end(), std::make_pair( tsKey, value ) );
	}
	catch( std::exception & err )
	{
		std::cerr << "pvCollector::saveValue exception caught: " << err.what() << std::endl;
	}
}

template <typename T>
void PVColector::writeValues( std::ostream & output )
{
	Guard	guard( m_mutex );

	output << "[" << std::endl;
	for ( std::map< epicsUInt64, T >::iterator it = m_events.begin(); it != m_events.end(); ++it )
	{
		epicsUInt64		key		= it->first;
		epicsUInt32		sec		= key >> 32;
		epicsUInt32		nsec	= key;
		output	<<	std::fixed << std::setw(17)
				<< "    [	[ "	<< sec << ", " << nsec << "], " << it->second << " ]," << std::endl;
	}
	fout << "]" << std::endl;
}

extern "C" {
epicsExportAddress(double, maxEventRate);
epicsExportAddress(double, maxEventAge);
epicsExportAddress(int, pvCollectorDebug);
}
