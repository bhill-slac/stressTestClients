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
#if 0
    void saveValue( epicsUInt64 tsKey, T value );
    void saveValues( std::list<std::pair<epicsUInt64,T>> newValues );
#endif

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
	int getEvents( std::map< epicsUInt64, T > & events, epicsUInt64 from = 0, epicsUInt64 to = std::numeric_limits<epicsUInt64>::max() ) const;

public:	// Public class functions
    static size_t	getMaxEvents();
    static void		setMaxEvents( size_t maxEvents );
    static size_t	getNumInstances();
	static void addPVCollector( const std::string & pvName, pvCollector & collector );
	static	pvCollector		*	findPVCollector( const std::string & pvName );

private:	// Private member variables
    epicsMutex						m_mutex;
	std::string						m_pvName;

private:	// Private class variables
	static size_t		c_num_instances;
	static size_t		c_max_events;		// Note: Can be changed dynamically
    static epicsMutex	c_mutex;
    static std::map< std::string, pvCollector * >	c_instances;

//private:	// Private class variables
    EPICS_NOT_COPYABLE(pvCollector)
};
#endif // PVCOLLECTOR_H
