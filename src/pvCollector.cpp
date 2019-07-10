#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <algorithm>

#include <epicsMath.h>
#include <errlog.h>
#include <pv/reftrack.h>

#include "pvCollector.h"
#include "pvStorage.h"

#include <epicsExport.h>
#include <epicsGuard.h>

namespace pvd = epics::pvData;

// timeout to flush partial events
static double maxEventAge = 2.5;

int collectorDebug;

#if 1
size_t		pvCollector::c_num_instances	= 0;
size_t		pvCollector::c_max_events		= 360000;	// 1 hour at 100hz
epicsMutex	pvCollector::c_mutex;
std::map< std::string, pvCollector * >	pvCollector::c_instances;
#else
template<> size_t		pvCollector<double>::c_num_instances	= 0;
template<> size_t		pvCollector<double>::c_max_events		= 360000;	// 1 hour at 100hz
template<> epicsMutex	pvCollector<double>::c_mutex;
template<> std::map< std::string, pvCollector<double> * >	pvCollector<double>::c_instances;
#endif

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
void	pvCollector::addPVCollector( const std::string & pvName, pvCollector * pPVCollector )
{
	epicsGuard<epicsMutex> G(c_mutex);
	c_instances[pvName] = pPVCollector;
}

#if 1
pvCollector * pvCollector::getPVCollector( const std::string & pvName, pvd::ScalarType type )
#else
template<> pvCollector<double> * pvCollector<double>::getPVCollector( const std::string & pvNam, pvd::ScalarType typee )
#endif
{
	epicsGuard<epicsMutex> G(c_mutex);
#if 1
	pvCollector	*	pCollector	= NULL;
	std::map< std::string, pvCollector * >::iterator	it;
#else
	pvCollector<double>	*	pCollector	= NULL;
	std::map< std::string, pvCollector<double> * >::iterator	it;
#endif
	it = c_instances.find( pvName );
	if ( it != c_instances.end() )
		pCollector	= it->second;

	if ( pCollector == NULL )
	{
		pCollector = createPVCollector( pvName, type );
	}
	return pCollector;
}

pvCollector * pvCollector::createPVCollector( const std::string & pvName, pvd::ScalarType type )
{
	printf( "createPVCollector %s: type %d\n", pvName.c_str(), type );
	epicsGuard<epicsMutex> G(c_mutex);
	
	pvCollector	*	pCollector	= NULL;
	if ( pCollector == NULL )
	{
		switch ( type )
		{
		default:
			break;
		case pvd::pvDouble:
			pCollector = new pvStorageDouble( pvName, type );
			break;
		}
	}
	if ( pCollector )
		addPVCollector( pvName, pCollector );
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
        //run = false;
    }
    //wakeup.signal();
    //processor.exitWait();
}


#if 0
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
#endif


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
