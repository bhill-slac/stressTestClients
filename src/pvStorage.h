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

#include "pvCollector.h"

template <typename T>
class pvStorage : public pvCollector
{
public:		// Public member functions

    void saveValue( epicsUInt64 tsKey, T value );
	{
		try
		{
			epicsGuard<epicsMutex>	guard( m_mutex );
			while ( m_events.size() >= c_max_events )
			{
				m_events.erase( m_events.begin() );
			}
			(void) m_events.insert( m_events.crbegin(), std::make_pair( tsKey, value ) );
		}
		catch( std::exception & err )
		{
			std::cerr << "pvStorage::saveValue exception caught: " << err.what() << std::endl;
		}
	}

public:		// Public class functions

public:		// Public member variables

private:	// Private member variables
    typedef std::map< epicsUInt64, T > events_t;
//    epicsMutex			m_mutex;
}

#endif // PVSTORAGE_H
