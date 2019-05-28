#ifndef PVCOLLECTOR_H
#define PVCOLLECTOR_H

#include <vector>
#include <map>
#include <set>

#include <epicsTypes.h>
#include <epicsEvent.h>
#include <pv/thread.h>
#include <pv/sharedPtr.h>

#if 0
template class PVStorage<typename T>
{
    typedef std::map< epicsUInt64, T > events_t;

public:	// Public class functions

private:	// Private member variables
    epicsMutex			m_mutex;
    events_t			m_events;
}
#endif

class pvCollector
{
    explicit pvCollector( const std::string pvName, unsigned int prio );
    ~pvCollector();

	/// saveValue called to save a new value to the collector
	template <typename T>
    void saveValue( epicsUInt64 tsKey, T value );

	template <typename T>
    void writeValues( std::ostream & output );

	/// getEvents
    template <typename T>
	int getEvents( std::map< epicsUInt64, T > & events, epicsUInt64 from = 0, epicsUInt64 to = std::numeric_limits<epicsUInt64>::max() ) const;

public:	// Public class functions
    static size_t	getMaxEvents();
    static void		setMaxEvents( size_t maxEvents );
    static size_t	getNumInstances();
	static	pvCollector	*	getPVCollector( const std::string & pvName );

private:	// Private member variables
    epicsMutex						m_mutex;
    std::map< epicsUInt64, double >	m_events;

private:	// Private class variables
	static size_t		c_num_instances;
	static size_t		c_max_events;		// Note: Can be changed dynamically
    static epicsMutex	c_mutex;
    static std::map< std::string, pvCollector * >	c_instances;

    EPICS_NOT_COPYABLE(pvCollector)
};
#endif // PVCOLLECTOR_H
