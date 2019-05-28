#ifndef PVCOLLECTOR_H
#define PVCOLLECTOR_H

#include <vector>
#include <map>
#include <set>
#include <limits>

#include <epicsTypes.h>
#include <epicsEvent.h>
#include <pv/thread.h>
#include <pv/sharedPtr.h>

#if 0
class PVStorage
{

public:		// Public class functions

public:		// Public member variables
    typedef std::map< epicsUInt64, T > events_t;

private:	// Private member variables
//    epicsMutex			m_mutex;
}
#endif

template <typename T>
class pvCollector
{
    explicit pvCollector( const std::string pvName, unsigned int prio )
		:	m_pvName(	pvName	)
		,	m_events()
	{
		REFTRACE_INCREMENT(c_num_instances);

		//m_events.resize(c_max_events);
	}

    ~pvCollector()
	{
		REFTRACE_DECREMENT(c_num_instances);
		close();
	}

	void close( )
	{
	}

	/// saveValue called to save a new value to the collector
    void saveValue( epicsUInt64 tsKey, T value )
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

	//template <typename T>
    void writeValues( std::ostream & output )
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

	/// getEvents
    //template <typename T>
	int getEvents( std::map< epicsUInt64, T > & events, epicsUInt64 from = 0, epicsUInt64 to = std::numeric_limits<epicsUInt64>::max() ) const;

public:	// Public class functions
    static size_t	getMaxEvents()
	{
		return c_max_events;
	}
    static void		setMaxEvents( size_t maxEvents )
	{
		c_max_events = maxEvents;
	}
    static size_t	getNumInstances()
	{
		return c_num_instances;
	}
	static void addPVCollector( const std::string & pvName, pvCollector & collector )
	{
        epicsGuard<epicsMutex> G(c_mutex);
		c_instances[pvName] = &collector;
	}

private:	// Private member variables
    epicsMutex						m_mutex;
	std::string						m_pvName;
    std::map< epicsUInt64, T >		m_events;

private:	// Private class variables
	static size_t		c_num_instances;
	static size_t		c_max_events;		// Note: Can be changed dynamically
    static epicsMutex	c_mutex;
    static std::map< std::string, pvCollector<T> * >	c_instances;

public:
	static	pvCollector<T>	*	getPVCollector( const std::string & pvName );
//private:	// Private class variables
    EPICS_NOT_COPYABLE(pvCollector)
};
#endif // PVCOLLECTOR_H
