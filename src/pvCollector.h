#ifndef PVCOLLECTOR_H
#define PVCOLLECTOR_H

#include <vector>
#include <map>
#include <set>
#include <limits>

#include <epicsTypes.h>
#include <epicsEvent.h>
#include <pv/thread.h>
#include <pv/pvIntrospect.h>
#include <pv/sharedPtr.h>


class pvCollector
{
public:		// Public member functions
    explicit pvCollector( const std::string pvName )
		:	m_pvName(	pvName	)
	{
		REFTRACE_INCREMENT(c_num_instances);
	}

    virtual ~pvCollector()
	{
		REFTRACE_DECREMENT(c_num_instances);
		close();
	}

	void close( );

	/// saveValue called to save a new value to the collector
	template<typename T>
    void saveValue( epicsUInt64 tsKey, T value );

	template<typename T>
    void saveValues( std::list< std::pair<epicsUInt64,T> > newValues );

    void writeValues( const std::string & testDirPath );
    void writeValues( std::ostream & fout );

	virtual size_t getNumSavedValues( )
	{
		return 0;
	}

#if 0
	/// getEvents
	int getEvents( std::map< epicsUInt64, T > & events, epicsUInt64 from = 0, epicsUInt64 to = std::numeric_limits<epicsUInt64>::max() ) const;
#endif

public:	// Public class functions
    static size_t	getMaxEvents();
    static void		setMaxEvents( size_t maxEvents );
    static size_t	getNumInstances();
	static void addPVCollector( const std::string & pvName, pvCollector * pPVCollector );
	static	pvCollector		*	createPVCollector( const std::string & pvName, epics::pvData::ScalarType type );
	static	pvCollector		*	getPVCollector( const std::string & pvName, epics::pvData::ScalarType type );
    static void		allCollectorsWriteValues( const std::string & testDirPath );

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
