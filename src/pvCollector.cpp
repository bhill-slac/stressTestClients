#include <iomanip>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sys/stat.h>
#include <list>
#include <map>
#include <algorithm>

#include <errno.h>
#include <errlog.h>
#include <pv/reftrack.h>

#include "pvCollector.h"
#include "pvStorage.h"

#include <epicsExport.h>
#include <epicsGuard.h>
#include <epicsMath.h>

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
	{
		// printf( "getPVCollector Channel %s:	Found pvCollector\n", pvName.c_str() );
		pCollector	= it->second;
	}

	if ( pCollector == NULL )
	{
		pCollector = createPVCollector( pvName, type );
	}
	return pCollector;
}

pvCollector * pvCollector::createPVCollector( const std::string & pvName, pvd::ScalarType type )
{
	printf( "createPVCollector %s: type %s\n", pvName.c_str(), pvd::ScalarTypeFunc::name(type) );
	epicsGuard<epicsMutex> G(c_mutex);
	
	pvCollector	*	pCollector	= NULL;
	if ( pCollector == NULL )
	{
		switch ( type )
		{
		default:
			printf( "createPVCollector %s: type %s not supported yet!\n", pvName.c_str(), pvd::ScalarTypeFunc::name(type) );
			break;
		case pvd::pvByte:
			pCollector = new pvStorage<epicsInt8>( pvName, type );
			break;
		case pvd::pvUByte:
			pCollector = new pvStorage<epicsUInt8>( pvName, type );
			break;
		case pvd::pvShort:
			pCollector = new pvStorage<epicsInt16>( pvName, type );
			break;
		case pvd::pvUShort:
			pCollector = new pvStorage<epicsUInt16>( pvName, type );
			break;
		case pvd::pvInt:
			pCollector = new pvStorage<epicsInt32>( pvName, type );
			break;
		case pvd::pvUInt:
			pCollector = new pvStorage<epicsUInt32>( pvName, type );
			break;
		case pvd::pvLong:
			pCollector = new pvStorage<epicsInt64>( pvName, type );
			break;
		case pvd::pvULong:
			pCollector = new pvStorage<epicsUInt64>( pvName, type );
			break;
		case pvd::pvDouble:
			pCollector = new pvStorage<double>( pvName, type );
			break;
		case pvd::pvString:
			pCollector = new pvStorage<std::string>( pvName, type );
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


void pvCollector::writeValues( const std::string & testDirPath )
{
	if ( getNumSavedValues() == 0 )
		return;

	std::string     saveFilePath( testDirPath );
	saveFilePath += "/";
	saveFilePath += m_pvName;
	// saveFilePath += ".pvCollect";
	// std::cout << "Creating test dir: " << testDirPath << std::endl;
	int status = mkdir( testDirPath.c_str(), ACCESSPERMS );
	if ( status != 0 && errno != EEXIST )
	{
		std::cerr << "pvCollector::writeValues error " << errno << " creating test dir: " << testDirPath << std::endl;
		std::cerr << strerror(errno) << std::endl;
	}

	std::cout << "Writing " << getNumSavedValues() << " values to test file: " << saveFilePath << std::endl;
	std::ofstream   fout( saveFilePath.c_str() );
	writeValues( fout );
#if 0
	fout << "[" << std::endl;
	for ( std::deque<t_TsReal>::iterator it = m_ValueQueue.begin(); it != m_ValueQueue.end(); ++it )
	{
		fout	<<	std::fixed << std::setw(17)
				<< "    [	[ "	<< it->ts.secPastEpoch << ", " << it->ts.nsec << "], " << it->val << " ]," << std::endl;
	}
	fout << "]" << std::endl;
#endif
}


void pvCollector::writeValues( std::ostream & fout )
{
	fout << "[" << std::endl;
	fout << "]" << std::endl;
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
