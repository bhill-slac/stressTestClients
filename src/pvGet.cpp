/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
#include <iomanip>
#include <iostream>
#include <vector>
#include <set>
#include <deque>
#include <queue>
#include <string>
#include <istream>
#include <fstream>
#include <sstream>

#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>

#include <epicsStdlib.h>
#include <epicsGetopt.h>
#include <epicsExit.h>
#include <epicsGuard.h>
#include <epicsTime.h>
#include <alarm.h>

#include <pv/pvData.h>
#include <pv/logger.h>
#include <pv/lock.h>
#include <pv/event.h>
#include <pv/monitor.h>
#include <pv/thread.h>
#include <pv/reftrack.h>
#include <pv/timeStamp.h>

#include <pv/caProvider.h>
#include <pv/logger.h>
#include <pva/client.h>

#include "pvCollector.h"
#include "pvStorage.h"

#define USE_SIGNAL
#ifndef EXECNAME
#define EXECNAME "pvGet"
#endif

#define PV_GET_MAJOR_VERSION        0
#define PV_GET_MINOR_VERSION        1
#define PV_GET_MAINTENANCE_VERSION  0
#define PV_GET_DEVELOPMENT_FLAG     1

namespace pvd = epics::pvData;

namespace {

// From pvAccessCPP/pvtoolsSrc/pvutils.cpp
double timeout = 5.0;
bool debugFlag = false;
pvd::PVStructure::Formatter::format_t outmode = pvd::PVStructure::Formatter::NT;
// outmode = pvd::PVStructure::Formatter::Raw;
// outmode = pvd::PVStructure::Formatter::NT;
// outmode = pvd::PVStructure::Formatter::JSON;
int verbosity   = 0;
std::string request("");
std::string defaultProvider("pva");

typedef struct _tsReal
{
    epicsTimeStamp  ts;
    double          val;
    _tsReal( ) : ts(), val() { val = NAN; };
    _tsReal( const epicsTimeStamp & newTs, double newVal ) : ts(newTs), val(newVal){ };
}   t_TsReal;

// This could go to it's own cpp file and header
// Borrowed from pvAccessCPP/pvtoolsSrc/pvutils.h
struct Tracker
{
    static epicsMutex           doneLock;
    static epicsEvent           doneEvt;
    typedef std::set<Tracker*>  inprog_t;
    static inprog_t             inprog;
    static bool                 abort;
    std::string			   	 	m_Name;

    Tracker()
    {
        epicsGuard<epicsMutex> G(doneLock);
        inprog.insert(this);
    }
    ~Tracker()
    {
        done();
    }
    void done()
    {
        {
            epicsGuard<epicsMutex> G(doneLock);
            inprog.erase(this);
        }
        doneEvt.signal();
    }

    void restartTracker()
    {
        epicsGuard<epicsMutex> G(doneLock);
        inprog.insert(this);
    }
	virtual void restart( pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest ) = 0;

    virtual void writeValues( const std::string & testDirPath ) = 0;

	/// getName( const std::string & name )
	const std::string & getName( ) const
	{
		return m_Name;
	}

	/// setName( const std::string & name )
	void setName( const std::string & name )
	{
		m_Name = name;
	}

    static void prepare();
    EPICS_NOT_COPYABLE(Tracker)
};

epicsMutex Tracker::doneLock;
epicsEvent Tracker::doneEvt;
Tracker::inprog_t Tracker::inprog;
bool Tracker::abort = false;

#ifdef USE_SIGNAL
static
void alldone(int num)
{
    (void)num;
    Tracker::abort = true;
    Tracker::doneEvt.signal();
}
#endif

void Tracker::prepare()
{
#ifdef USE_SIGNAL
        signal(SIGINT, alldone);
        signal(SIGTERM, alldone);
        signal(SIGQUIT, alldone);
#endif
}

size_t pvnamewidth;

int haderror;

void usage (void)
{
    fprintf( stdout, "\nUsage: " EXECNAME " [options] <PV:Name>...\n"
            "\n"
            "options:\n" \
            "  -h: Help: Print this message\n" \
            "  -V: Print version and exit\n" \
            "  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n" \
            "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n" \
            "  -p <provider>:     Set default provider name, default is '%s'\n" \
            "  -M <raw|nt|json>:  Output mode.  default is 'nt'\n" \
            "  -q:                Quiet mode, print only error messages\n" \
            "  -d:                Enable debug output\n"
            "  -f <input file>:   Read pvName list from file, one line per pvName.\n"
            "  -D <dirpath>:      Directory path where captured values are saved to <dirpath>/<pvname>.\n"
            "  -S:                Show each PV as it's acquired, same output options as pvmonitor.\n"
            "  -R <delay>:        Repeat w/ delay.  Not applicable for monitor mode.\n"
            "  -C:                Capture each PV and save to a test file.\n"
            " Output details:\n"
            "  -v:                Show entire structure (implies Raw mode)\n" \
            "  -vv:               Get in Raw mode.     Highlight  valid fields, show all fields.\n"
            "\n"
            "example: " EXECNAME " double01\n\n"
//          , request.c_str(), timeout, defaultProvider.c_str()
            , "value", 5.0, "pva" );
}

// This could go to it's own cpp file and header
// Borrowed from pvmonitor.cpp
struct Worker
{
    virtual ~Worker() {}
    virtual void process(const pvac::MonitorEvent& event) =0;
};

// This could go to it's own cpp file and header
// Borrowed from pvmonitor.cpp
// simple work queue with thread.
// moves monitor queue handling off of PVA thread(s)
struct WorkQueue : public epicsThreadRunable
{
    typedef std::tr1::shared_ptr<Worker>    value_type;
    typedef std::tr1::weak_ptr<Worker>      weak_type;
    // work queue holds only weak_ptr
    // so jobs must be kept alive seperately
    typedef std::deque<std::pair<weak_type, pvac::MonitorEvent> > queue_t;
    epicsEvent      event;
    epicsMutex      mutex;
    queue_t         queue;
    bool            running;
    pvd::Thread     worker;

    WorkQueue()
        :running(true)
        ,worker(pvd::Thread::Config()
                .name("pvaEventHandler")
                .autostart(true)
                .run(this))
    {}
    ~WorkQueue() {close();}

    void close()
    {
        {
            epicsGuard<epicsMutex> G(mutex);
            running = false;
        }
        event.signal();
        worker.exitWait();
    }

    void push(const weak_type& cb, const pvac::MonitorEvent& evt)
    {
        bool wake;
        {
            epicsGuard<epicsMutex> G(mutex);
            if(!running) return; // silently refuse to queue during/after close()
            wake = queue.empty();
            queue.push_back(std::make_pair(cb, evt));
        }
        if(wake)
            event.signal();
    }

    virtual void run() OVERRIDE FINAL
    {
        epicsGuard<epicsMutex> G(mutex);

        while(running) {
            if(queue.empty()) {
                epicsGuardRelease<epicsMutex> U(G);
                event.wait();
            } else {
                queue_t::value_type ent(queue.front());
                value_type cb(ent.first.lock());
                queue.pop_front();
                if(!cb) continue;

                try {
                    epicsGuardRelease<epicsMutex> U(G);
                    cb->process(ent.second);
                }catch(std::exception& e){
                    std::cout << "Error in pvaEventHandler WorkQueue: " << e.what() << "\n";
                }
            }
        }
    }
};

// From pvAccessCPP/pvtoolsSrc/pvget.cpp
struct Getter : public pvac::ClientChannel::GetCallback,
#ifdef GETTER_BLOCK
				public Worker,
#endif
				public Tracker,
				public std::tr1::enable_shared_from_this<Getter>
{
    POINTER_DEFINITIONS(Getter);

    WorkQueue   			&   monwork;
    pvac::Operation 			op;
	bool            			fCapture;
	bool            			fShow;
	double						m_Repeat;
    size_t                 	 	m_QueueSizeMax;
    std::deque<t_TsReal>   	 	m_ValueQueue;
    epicsMutex      			m_QueueLock;
	pvStorage<double>		*	m_pvCollector;
	//pvac::ClientChannel			m_clientChannel;

    Getter(WorkQueue& monwork, pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest, bool fCapture, bool fShow, double repeat )
		:monwork(monwork)
		,op()
        ,fCapture(fCapture)
        ,fShow(fShow)
		,m_Repeat( repeat )
        ,m_QueueSizeMax( 262144 )
		,m_ValueQueue()
    {
		setName( channel.name() );
#ifdef GETTER_BLOCK
		monwork.push( shared_from_this(), pvRequest );
#else
        op = channel.get(this, pvRequest);
#endif
    }
    virtual ~Getter()
 	{
        try {
		std::cout << "~Getter: Cancel monitor of " << getName() << std::endl; 
        op.cancel();
        }
        catch(std::exception& e){
            std::cout << "Error in ~Getter: " << e.what() << "\n";
        }
    }
	void restart( pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest )
	{
		op.cancel();
        op = channel.get(this, pvRequest);
		setName( channel.name() );
		restartTracker();
	}

#ifdef GETTER_BLOCK
    /// process is called for each item on the WorkQueue
    virtual void process( pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest ) OVERRIDE FINAL
    {
    try {
        unsigned n;
        // running on our worker thread
        switch(evt.event)
        {
        case pvac::MonitorEvent::Fail:
            std::cerr << std::setw(pvnamewidth) << std::left << mon.name() << " Error " << evt.message << "\n";
            haderror = 1;
            done();
            break;
        case pvac::MonitorEvent::Cancel:
            std::cerr << std::setw(pvnamewidth) << std::left << mon.name() << " Cancel " << evt.message << "\n";
            break;
        case pvac::MonitorEvent::Disconnect:
            std::cout << std::setw(pvnamewidth) << std::left << mon.name() << " <Disconnect>\n";
            valid.clear();
            break;
        case pvac::MonitorEvent::Data:
            for(n=0; n<2 && mon.poll(); n++)
            {
                valid |= mon.changed;

				if ( fCapture )
				{
					// Capture the new value
					capture( evt );
				}

                if ( fShow )
                {
                    pvd::PVStructure::Formatter fmt(mon.root->stream()
                                                    .format(outmode));

                    if(verbosity>=3)
                        fmt.highlight(mon.changed); // show all
                    else if(verbosity>=2)
                        fmt.highlight(mon.changed).show(valid);
                    else
                        fmt.show(mon.changed); // highlight none

                    std::cout << std::setw(pvnamewidth) << std::left << mon.name() << ' ' << fmt;
                }
            }
            if(n==2)
            {
                // too many updates, re-queue to balance with others
            	std::cerr << std::setw(pvnamewidth) << std::left << mon.name() << " re-queue " << evt.message << "\n";
                monwork.push(shared_from_this(), evt);
            }
            else if(n==0)
            {
                LOG(epics::pvAccess::logLevelDebug, "%s Spurious Data event on channel", mon.name().c_str());
            }
            else
            {
                if(mon.complete())
                    done();
            }
            break;
        }
        std::cout.flush();
    }
        catch(std::exception& e)
        {
            std::cout << "Error in capture handler : " << e.what() << "\n";
        }
    }
#endif

    /// capture is called for each pvAccess MonitorEvent::Data
    virtual void capture(const pvac::GetEvent& evt) OVERRIDE FINAL
    {
        std::tr1::shared_ptr<const pvd::PVStructure>    pvStruct = evt.value;
        assert( pvStruct != 0 );
		std::tr1::shared_ptr<const pvd::PVScalar> pPVScalar;
		pPVScalar = pvStruct->getSubField<pvd::PVScalar>("value");
		if ( pPVScalar )
		{
			pvd::FieldConstPtr	pField	= pPVScalar->getField();
			pvd::ScalarConstPtr	pScalar = pPVScalar->getScalar();
			if ( pScalar )
			{
				pvCollector	*	pCollector = pvCollector::getPVCollector( op.name(), pScalar->getScalarType() );
				m_pvCollector = dynamic_cast<pvStorage<double> *>( pCollector );
			}
		}
		else
		{
			//pvd::FieldConstPtr	pField	= pPVScalar->getField();
			//printf( "Channel %s:	FieldType=%d\n", op.name().c_str(), pField->getType() );
			// printf( "Channel %s:	NTScalar value: FieldType=%d, ScalarType=%d\n", op.name().c_str(), pField->getType(), pScalar->getScalarType() );
		}

        // std::cout << *valfld
        // const PVFieldPtrArray & pvFields = pvStruct->getPVFields();
        // pvStruct->getSubField<pvd::PVDouble>("value")
        // cout << ScalarTypeFunc::name(pPVScalarValue->typeCode);
        // cout << ScalarTypeFunc::name(PVT::typeCode);
        // template<> const ScalarType PVDouble::typeCode = pvDouble;
        try
        {
            std::tr1::shared_ptr<const pvd::PVInt>  pStatus		= pvStruct->getSubField<pvd::PVInt>("alarm.status");
            std::tr1::shared_ptr<const pvd::PVInt>  pSeverity	= pvStruct->getSubField<pvd::PVInt>("alarm.severity");
            // Don't capture values w/ timeout alarm.status
            if ( pStatus != NULL && pStatus->get() == TIMEOUT_ALARM )
                return;
            // Don't capture values w/ INVALID alarm.severity
            if ( pSeverity != NULL && pSeverity->get() == INVALID_ALARM )
                return;
            std::tr1::shared_ptr<const pvd::PVDouble>   pValue  = pvStruct->getSubField<pvd::PVDouble>("value");
            double          value           = NAN;
            if ( pValue )
            {
                value = pValue->getAs<double>();
            }

            epicsUInt32     secPastEpoch    = 1;
            epicsUInt32     nsec            = 2;
            std::tr1::shared_ptr<const pvd::PVScalar>   pScalarSec  = pvStruct->getSubField<pvd::PVScalar>("timeStamp.secondsPastEpoch");
            if ( pScalarSec )
            {
                secPastEpoch    = pScalarSec->getAs<pvd::uint32>();
            }
            std::tr1::shared_ptr<const pvd::PVScalar>   pScalarNSec = pvStruct->getSubField<pvd::PVScalar>("timeStamp.nanoseconds");
            if ( pScalarNSec )
            {
                nsec    = pScalarNSec->getAs<pvd::uint32>();
            }
            epicsTimeStamp  timeStamp;
            timeStamp.secPastEpoch = secPastEpoch;
            timeStamp.nsec = nsec;
			epicsUInt64		tsKey = secPastEpoch;
			tsKey <<= 32;
			tsKey += nsec;
			if( m_pvCollector )
			{
				// printf( "Getter::capture %s: saveValue %f\n", op.name().c_str(), value );
				m_pvCollector->saveValue( tsKey, value );
			}

			std::tr1::shared_ptr<const pvd::PVScalar>   pscChannelCacheSize;
			pscChannelCacheSize  = pvStruct->getSubField<pvd::PVScalar>( "ccacheSize.value" );
			if ( pscChannelCacheSize )
			{
				std::string		nameChannelCacheSize = op.name() + ".ccacheSize.value";
				pvd::ScalarConstPtr	pScalar = pscChannelCacheSize->getScalar();
				pvCollector	*	pCollector = pvCollector::getPVCollector( nameChannelCacheSize, pScalar->getScalarType() );
				pvStorage<epicsUInt64>	*	pStorage = dynamic_cast<pvStorage<epicsUInt64> *>( pCollector );
                epicsUInt64		sChannelCache    = pscChannelCacheSize->getAs<pvd::uint64>();
				if ( pStorage )
				{
					std::cout << "saveValue " << nameChannelCacheSize << " " << sChannelCache << " at [ " << secPastEpoch << ", " << nsec << " ]" << std::endl;
					pStorage->saveValue( tsKey, sChannelCache );
					// m_pvCollectors.push( pStorage );
				}
			}

            t_TsReal    tsPrior;
            assert( isnan(tsPrior.val) );

            //if ( pStatus == NULL || pStatus->get() != NO_ALARM )
            //  return;

            {   // Keep guard while accessing m_ValueQueue
            epicsGuard<epicsMutex> G(m_QueueLock);
            if ( !m_ValueQueue.empty() )
                tsPrior = m_ValueQueue.back();
            t_TsReal    tsValue( timeStamp, value );
            if ( ! isnan(tsValue.val) )
            {
                if( m_ValueQueue.size() >= m_QueueSizeMax )
                    m_ValueQueue.pop_front();
                m_ValueQueue.push_back( tsValue );
            }
            }
        }
        catch(std::runtime_error& e)
        {
            std::cout << "Bad Field Type in capture handler : " << e.what() << "\n";
        }
        catch(std::exception& e)
        {
            std::cout << "Error in capture handler : " << e.what() << "\n";
        }
    }

    /// Save the timestamped values on the queue to a file
    void writeValues( const std::string & testDirPath )
    {
		if ( m_pvCollector )
		{
			printf( "PVCollector %s: Saved %zu values.\n", m_Name.c_str(), m_pvCollector->getNumSavedValues() );
			m_pvCollector->writeValues( testDirPath );
			return;
		}
        if ( m_ValueQueue.size() == 0 )
            return;

        std::string     saveFilePath( testDirPath );
        saveFilePath += "/";
        saveFilePath += m_Name;
        // std::cout << "Creating test dir: " << testDirPath << std::endl;
        mkdir( testDirPath.c_str(), ACCESSPERMS );

        std::cout << "Writing " << m_ValueQueue.size() << " values to test file: " << saveFilePath << std::endl;
        std::ofstream   fout( saveFilePath.c_str() );
        fout << "[" << std::endl;
        for ( std::deque<t_TsReal>::iterator it = m_ValueQueue.begin(); it != m_ValueQueue.end(); ++it )
        {
			fout	<<	std::fixed << std::setw(17)
            		<< "    [	[ "	<< it->ts.secPastEpoch << ", " << it->ts.nsec << "], " << it->val << " ]," << std::endl;
        }
        fout << "]" << std::endl;
    }

    virtual void getDone(const pvac::GetEvent& event) OVERRIDE FINAL
    {
        switch(event.event) {
        case pvac::GetEvent::Fail:
			std::cout<<std::setw(pvnamewidth)<<std::left<<op.name()<<' ';
            std::cerr<<"Error "<<event.message<<"\n";
            haderror = 1;
            break;
        case pvac::GetEvent::Cancel:
            break;
        case pvac::GetEvent::Success: {
			if ( fCapture )
			{
				// Capture the new value
				capture( event );
			}

			if ( fShow ) {
			std::cout<<std::setw(pvnamewidth)<<std::left<<op.name()<<' ';
            pvd::PVStructure::Formatter fmt(event.value->stream()
                                            .format(outmode));

            if(verbosity>=2)
                fmt.highlight(*event.valid); // show all, highlight valid
            else
                fmt.show(*event.valid); // only show valid, highlight none

            std::cout<<fmt;
			}
        }
            break;
        }
        std::cout.flush();
        done();
    }

};

} // namespace

#ifndef MAIN
#  define MAIN main
#endif

int MAIN (int argc, char *argv[])
{
	double temp;
    try {
        int opt;                    /* getopt() current option */
#ifdef PVCAPTURE
        bool capture    = true;
#else
        bool capture    = false;
#endif
        bool monitor    = false;
        bool fShow      = false;
        double repeat   = -1;
        std::string         pvFilename("");
        std::vector<std::string>    pvList;

        epics::RefMonitor refmon;
        std::string     testDirPath( "/tmp/pvGetTest1" );

        // ================ Parse Arguments

        while ((opt = getopt(argc, argv, ":hvVCSD:M:r:R:w:tp:qdcF:f:ni")) != -1) {
            switch (opt) {
            case 'h':               /* Print usage */
                usage();
                return 0;
            case 'v':
                verbosity++;
                break;
            case 'V':               /* Print version */
            {
                epics::pvAccess::Version version(EXECNAME, "cpp",
                                    PV_GET_MAJOR_VERSION,
                                    PV_GET_MINOR_VERSION,
                                    PV_GET_MAINTENANCE_VERSION,
                                    PV_GET_DEVELOPMENT_FLAG);
                fprintf(stdout, "%s\n", version.getVersionString().c_str());
                return 0;
            }
            case 'S':
                fShow = true;
                break;
            case 'D':
                testDirPath = optarg;
                break;
            case 'C':
                capture = true;
                break;
            case 'M':
                if(strcmp(optarg, "raw")==0) {
                    outmode = pvd::PVStructure::Formatter::Raw;
                } else if(strcmp(optarg, "nt")==0) {
                    outmode = pvd::PVStructure::Formatter::NT;
                } else if(strcmp(optarg, "json")==0) {
                    outmode = pvd::PVStructure::Formatter::JSON;
                } else {
                    fprintf(stderr, "Unknown output mode '%s'\n", optarg);
                    outmode = pvd::PVStructure::Formatter::Raw;
                }
                break;
            case 'w':               /* Set PVA timeout value */
                if((epicsScanDouble(optarg, &temp)) != 1)
                {
                    fprintf(stderr, "'%s' is not a valid timeout value "
                                    "- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
                } else {
                    timeout = temp;
                }
                break;
            case 'R':
                /* Set repeat delay */
                if((epicsScanDouble(optarg, &temp)) != 1)
                {
                    fprintf(stderr, "'%s' is not a valid repeat delay value "
                                    "- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
                } else {
                    repeat = temp;
                }
				break;
            case 'r':
                request = optarg;
                break;
            case 't':               /* Terse mode */
            case 'i':               /* T-types format mode */
            case 'F':               /* Store this for output formatting */
            case 'n':
            case 'q':               /* Quiet mode */
                // deprecate
                break;
            case 'f':               /* Use input stream as input */
                pvFilename = optarg;
                break;
            case 'p':               /* Set default provider */
                defaultProvider = optarg;
                break;
            case 'd':               /* Debug log level */
                debugFlag = true;
                break;
            case 'c':               /* Clean-up and report used instance count */
                break;
            case '?':
                fprintf(stderr,
                        "Unrecognized option: '-%c'. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            case ':':
                fprintf(stderr,
                        "Option '-%c' requires an argument. ('" EXECNAME " -h' for help.)\n",
                        optopt);
                return 1;
            default :
                usage();
                return 1;
            }
        }

        if(monitor)
		{
            timeout = -1;
            repeat  = -1;
		}

        if(verbosity>0 && outmode==pvd::PVStructure::Formatter::NT)
            outmode = pvd::PVStructure::Formatter::Raw;

        pvd::PVStructure::shared_pointer pvRequest;
        try {
            pvRequest = pvd::createRequest(request);
        } catch(std::exception& e){
            fprintf(stderr, "failed to parse request string: %s\n", e.what());
            return 1;
        }

        if ( pvFilename.size() > 0 )
        {
            try
            {
                std::ifstream   fin( pvFilename.c_str() );
                std::string     line;
                if ( !fin.is_open() )
                    std::cout << "Unable to open " << pvFilename << std::endl;
                else
                {
                    while ( getline( fin, line ) )
                    {
                        pvnamewidth = std::max(pvnamewidth, line.size());
                        pvList.push_back( line );
                    }
                    fin.close();
                }
            }
            catch( std::exception & e )
            {
                std::cerr << "Error: " << e.what() << "\n";
            }
        }

        for(int i = optind; i < argc; i++)
        {
            pvnamewidth = std::max(pvnamewidth, strlen(argv[i]));
            pvList.push_back( argv[i] );
        }

		if ( pvList.size() == 0 )
		{
			usage();
			return 1;
		}

        // Everything up to here is just related to handling cmd line arguments
        // Configure logging
        SET_LOG_LEVEL(debugFlag ? epics::pvAccess::logLevelDebug : epics::pvAccess::logLevelError);

        epics::pvAccess::ca::CAClientFactory::start();

		std::vector<std::tr1::shared_ptr<Tracker> > tracked;
		pvac::ClientProvider provider(defaultProvider);

		epics::auto_ptr<WorkQueue> Q;
		if(monitor)
			Q.reset(new WorkQueue);

		for ( std::vector<std::string>::const_iterator it = pvList.begin(); it != pvList.end(); ++it )
		{
			pvac::ClientChannel chan( provider.connect(*it) );

			if(monitor) {
				printf( "Monitor mode no longer supported!\n" );
			}

			// Get
			std::tr1::shared_ptr<Getter> get( new Getter( *Q, chan, pvRequest, capture, fShow, repeat) );
			tracked.push_back(get);
		}

		Tracker::prepare(); // install signal handler

		do  {   // Create and run PVA clients for pvNames in argv[argc]
			{

            // ========================== Wait for operations to complete, or timeout

            if(debugFlag)
                std::cerr << "Waiting...\n";

            {
                epicsGuard<epicsMutex> G(Tracker::doneLock);
                while(Tracker::inprog.size() && !Tracker::abort)
                {
                    epicsGuardRelease<epicsMutex> U(G);
                    if(timeout<=0)
                    {
                        Tracker::doneEvt.wait();
                    }
                    else if(!Tracker::doneEvt.wait(timeout))
                    {
                        haderror = 1;
                        std::cerr << "Trackers timed out after " << timeout*2 << std::endl;
                        break;
                    }
                }
            }
			}

    	if ( Tracker::abort )
			break;

		if ( repeat >= 0 )
		{
			epicsThreadSleep( repeat );

			for ( std::vector<std::tr1::shared_ptr<Tracker> >::iterator it = tracked.begin(); it != tracked.end(); ++it )
			{
				pvac::ClientChannel chan( provider.connect((*it)->getName()) );
				(*it)->restart( chan, pvRequest );
			}
		}
	}   while ( repeat >= 0 && !Tracker::abort );

	for ( std::vector<std::tr1::shared_ptr<Tracker> >::iterator it = tracked.begin(); it != tracked.end(); ++it )
	{
		(*it)->writeValues( testDirPath );
	}

	if(refmon.running())
	{
		refmon.stop();
		// show final counts
		refmon.current();
	}

    } catch(std::exception& e) {
        std::cerr << EXECNAME << "Error: " << e.what() << "\n";
        return 1;
    }

	return 0;
}
