#ifndef PVSTORAGE_H
#define PVSTORAGE_H

#include <vector>
#include <map>
#include <set>
#include <limits>

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
		epicsUInt32		sec		= static_cast<epicsUInt32>( tsKey >> 32 );
		epicsUInt32		nsec	= static_cast<epicsUInt32>( tsKey );
		std::cout << "pvStorage::saveValue Saving " << value << " at [ " << sec << ", " << nsec << " ]" << std::endl;
		static_cast<T *>(this)->saveValue( tsKey, value );
	}

	size_t getNumSavedValues( ) const
	{
		return m_events.size();
	}

public:		// Public class functions
public:		// Public member variables
	events_t 					m_events;
private:	// Private member variables
	std::string					m_pvName;
	epics::pvData::ScalarType	m_Type;
};

class pvStorageDouble : public pvStorage<double>
{
    typedef std::map< epicsUInt64, double > events_t;
public:		// Public member functions
	pvStorageDouble( const std::string & pvName, epics::pvData::ScalarType type )
		:	pvStorage( pvName, type )
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
    epicsMutex					m_mutex;
};

#endif // PVSTORAGE_H
