#ifndef PVSTORAGE_H
#define PVSTORAGE_H

#include <vector>
#include <map>
#include <set>
#include <limits>

#include <epicsMutex.h>
#include <epicsTypes.h>
#include <epicsEvent.h>
#include <pv/thread.h>
#include <pv/sharedPtr.h>

//#include "pvCollector.h"

template <typename T>
class pvStorage : public pvCollector
{
    typedef std::map< epicsUInt64, T > events_t;
	friend class pvStorageDouble;
public:		// Public member functions

	pvStorage( const std::string & pvName, epics::pvData::ScalarType type )
		:	pvCollector( pvName )
		,	m_pvName( pvName )
		,	m_Type(	type )
	{
	}

    void saveValue( epicsUInt64 tsKey, T value )
	{
		//epicsUInt32		sec		= static_cast<epicsUInt32>( tsKey >> 32 );
		//epicsUInt32		nsec	= static_cast<epicsUInt32>( tsKey );
		//std::cout << "pvStorage::saveValue Saving " << value << " at [ " << sec << ", " << nsec << " ]" << std::endl;

		try
		{
			epicsGuard<epicsMutex>	guard( m_mutex );
			while ( m_events.size() >= getMaxEvents() )
			{
				m_events.erase( m_events.begin() );
			}
			if ( m_events.size() == 0 )
				(void) m_events.insert( std::make_pair( tsKey, value ) );
			else
			{
				typename events_t::iterator	it = m_events.end();
				(void) m_events.insert( --it, std::make_pair( tsKey, value ) );
			}
			// epicsUInt32		sec		= static_cast<epicsUInt32>( tsKey >> 32 );
			// epicsUInt32		nsec	= static_cast<epicsUInt32>( tsKey );
			// std::cout << "pvStorageDouble::saveValue Saving " << value << " at [ " << sec << ", " << nsec << " ]" << std::endl;
		}
		catch( std::exception & err )
		{
			std::cerr << "pvStorage::saveValue exception caught: " << err.what() << std::endl;
		}
	}

	size_t getNumSavedValues( )
	{
		epicsGuard<epicsMutex>	guard( m_mutex );
		return m_events.size();
	}

    void writeValues( const std::string & testDirPath )
	{
		if ( getNumSavedValues() == 0 )
			return;

		std::string     saveFilePath( testDirPath );
		saveFilePath += "/";
		saveFilePath += m_pvName;
		//saveFilePath += ".pvCollect";
		// std::cout << "Creating test dir: " << testDirPath << std::endl;
		mkdir( testDirPath.c_str(), ACCESSPERMS );

		std::cout << "Writing " << getNumSavedValues() << " values to test file: " << saveFilePath << std::endl;
		std::ofstream   fout( saveFilePath.c_str() );
		writeValues( fout );
	}

    void writeValues( std::ostream & fout )
	{
		epicsGuard<epicsMutex>	guard( m_mutex );
		fout << "[" << std::endl;
		for ( typename events_t::iterator it = m_events.begin(); it != m_events.end(); ++it )
		{
			epicsUInt64		key		= it->first;
			epicsUInt32		sec		= key >> 32;
			epicsUInt32		nsec	= key;
			fout	<<	std::fixed << std::setw(17)
					<< "    [	[ "	<< sec << ", " << nsec << "], " << it->second << " ]," << std::endl;
		}
		fout << "]" << std::endl;
	}

public:		// Public class functions
public:		// Public member variables
	events_t 					m_events;
private:	// Private member variables
	std::string					m_pvName;
	epics::pvData::ScalarType	m_Type;
    epicsMutex					m_mutex;
};

class pvStorageDouble : public pvStorage<double>
{
    typedef std::map< epicsUInt64, double > events_t;
public:		// Public member functions
	pvStorageDouble( const std::string & pvName, epics::pvData::ScalarType type )
		:	pvStorage<double>( pvName, type )
	{
	}

    void saveValue( epicsUInt64 tsKey, double value )
	{
		try
		{
			epicsGuard<epicsMutex>	guard( m_mutex );
			while ( m_events.size() >= getMaxEvents() )
			{
				m_events.erase( m_events.begin() );
			}
			if ( m_events.size() == 0 )
				(void) m_events.insert( std::make_pair( tsKey, value ) );
			else
			{
				events_t::iterator	it = m_events.end();
				(void) m_events.insert( --it, std::make_pair( tsKey, value ) );
			}
			// epicsUInt32		sec		= static_cast<epicsUInt32>( tsKey >> 32 );
			// epicsUInt32		nsec	= static_cast<epicsUInt32>( tsKey );
			// std::cout << "pvStorageDouble::saveValue Saving " << value << " at [ " << sec << ", " << nsec << " ]" << std::endl;
		}
		catch( std::exception & err )
		{
			std::cerr << "pvStorage::saveValue exception caught: " << err.what() << std::endl;
		}
	}

public:		// Public class functions
public:		// Public member variables
private:	// Private member variables
	//events_t 					m_events;
    //epicsMutex					m_mutex;
};

#endif // PVSTORAGE_H
