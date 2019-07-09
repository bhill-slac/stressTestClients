#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <algorithm>

#include <epicsMath.h>
#include <errlog.h>
#include <pv/reftrack.h>

#include "pvCollector.h"

#include <epicsExport.h>
#include <epicsGuard.h>

namespace pvd = epics::pvData;

// timeout to flush partial events
static double maxEventAge = 2.5;

int collectorDebug;

template<> size_t		pvCollector<double>::c_num_instances	= 0;
template<> size_t		pvCollector<double>::c_max_events		= 360000;	// 1 hour at 100hz
template<> epicsMutex	pvCollector<double>::c_mutex;
template<> std::map< std::string, pvCollector<double> * >	pvCollector<double>::c_instances;

size_t	pvCollector::getMaxEvents()
{
	return c_max_events;
}
void	pvCollector::setMaxEvents( size_t maxEvents )
{
	c_max_events = maxEvents;
}
size_t	pvCollector::getNumInstances()
{
	return c_num_instances;
}
void	pvCollector::addPVCollector( const std::string & pvName, pvCollector & collector )
{
	epicsGuard<epicsMutex> G(c_mutex);
	c_instances[pvName] = &collector;
}

template<> pvCollector<double> * pvCollector<double>::findPVCollector( const std::string & pvName )
{
	epicsGuard<epicsMutex> G(c_mutex);
	pvCollector<double>	*	pCollector	= NULL;
	std::map< std::string, pvCollector<double> * >::iterator	it;
	it = c_instances.find( pvName );
	if ( it != c_instances.end() )
		pCollector	= it->second;
	return pCollector;
}

//
// pvCollector member functions
//

/// pvCollector::close()
void pvCollector::close()
{
    {
        epicsGuard<epicsMutex> G(m_mutex);
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
		epicsGuard<epicsMutex>	guard( m_mutex );
		while ( m_events.size() >= c_max_events )
		{
			m_events.erase( m_events.begin() );
		}
		(void) m_events.insert( m_events.end(), std::make_pair( tsKey, value ) );
	}
	catch( std::exception & err )
	{
		std::cerr << "pvCollector::saveValue exception caught: " << err.what() << std::endl;
	}
}


#if 0
template <typename T>
void pvCollector::writeValues( std::ostream & output )
{
	epicsGuard<epicsMutex>	guard( m_mutex );
	std::map< epicsUInt64, double >::iterator it2 = m_events.begin();

	output << "[" << std::endl;
	for ( std::map< epicsUInt64, double >::iterator it = m_events.begin(); it != m_events.end(); ++it )
	{
		epicsUInt64		key		= it->first;
		epicsUInt32		sec		= key >> 32;
		epicsUInt32		nsec	= key;
		output	<<	std::fixed << std::setw(17)
				<< "    [	[ "	<< sec << ", " << nsec << "], " << it->second << " ]," << std::endl;
	}
	output << "]" << std::endl;
}
#endif

extern "C" {
//epicsExportAddress(double, maxEventRate);
epicsExportAddress(double, maxEventAge);
//epicsExportAddress(int, pvCollectorDebug);
}
