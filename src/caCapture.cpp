/*
 * Copyright information and license terms for this software can be
 * found in the file LICENSE that is included with the distribution
 */
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

#define USE_SIGNAL
#ifndef EXECNAME
#define EXECNAME "caCapture"
#endif

#define CA_CAPTURE_MAJOR_VERSION			0
#define CA_CAPTURE_MINOR_VERSION			1
#define CA_CAPTURE_MAINTENANCE_VERSION		0
#define CA_CAPTURE_DEVELOPMENT_FLAG		1

namespace pvd = epics::pvData;

namespace EXECNAME {

// From pvAccessCPP/pvtoolsSrc/pvutils.cpp
double timeout = 5.0;
bool debugFlag = false;
int	verbosity	= 0;
std::string defaultProvider("ca");

// This could go to it's own cpp file and header
// Borrowed from pvAccessCPP/pvtoolsSrc/pvutils.h
struct Tracker
{
    static epicsMutex			doneLock;
    static epicsEvent			doneEvt;
    typedef std::set<Tracker*>	inprog_t;
    static inprog_t				inprog;
    static bool					abort;

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
    "options:\n"
    "  -h:                Help: Print this message\n"
    "  -V:                Print version and exit\n"
    "Channel Access options:\n"
    "  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n"
    "  -m <msk>: Specify CA event mask to use.  <msk> is any combination of\n"
    "            'v' (value), 'a' (alarm), 'l' (log/archive), 'p' (property).\n"
    "            Default event mask is 'va'\n"
    "  -p <pri>: CA priority (0-%u, default 0=lowest)\n"
    "Timestamps:\n"
    "  Default:  Print absolute timestamps (as reported by CA server)\n"
    "  -t <key>: Specify timestamp source(s) and type, with <key> containing\n"
    "            's' = CA server (remote) timestamps\n"
    "            'c' = CA client (local) timestamps (shown in '()'s)\n"
    "            'n' = no timestamps\n"
    "            'r' = relative timestamps (time elapsed since start of program)\n"
    "            'i' = incremental timestamps (time elapsed since last update)\n"
    "            'I' = incremental timestamps (time since last update, by channel)\n"
    "            'r', 'i' or 'I' require 's' or 'c' to select the time source\n"
    "  -P:       Show fiducial pulse ID with timestamps\n"
    "Enum format:\n"
    "  -n:       Print DBF_ENUM values as number (default is enum string)\n"
    "Array values: Print number of elements, then list of values\n"
    "  Default:  Request and print all elements (dynamic arrays supported)\n"
    "  -# <num>: Request and print up to <num> elements\n"
    "  -S:       Print arrays of char as a string (long string)\n"
    "Floating point format:\n"
    "  Default:  Use %%g format\n"
    "  -e <num>: Use %%e format, with a precision of <num> digits\n"
    "  -f <num>: Use %%f format, with a precision of <num> digits\n"
    "  -g <num>: Use %%g format, with a precision of <num> digits\n"
    "  -s:       Get value as string (honors server-side precision)\n"
    "  -lx:      Round to long integer and print as hex number\n"
    "  -lo:      Round to long integer and print as octal number\n"
    "  -lb:      Round to long integer and print as binary number\n"
    "Integer number format:\n"
    "  Default:  Print as decimal number\n"
    "  -0x:      Print as hex number\n"
    "  -0o:      Print as octal number\n"
    "  -0b:      Print as binary number\n"
    "Alternate output field separator:\n"
    "  -F <ofs>: Use <ofs> to separate fields in output\n"
    "\n"
    "  -q:                Quiet mode, print only error messages\n"
    "  -d:                Enable debug output\n"
    "  -f <input file>:   Read pvName list from file, one line per pvName.\n"
    "  -S:                Show each PV as it's captured, same output options as pvmonitor.\n"
    "  -vv:     Get in Raw mode.     Highlight  valid fields, show all fields.\n"
    "\n"
    "Example: " EXECNAME " -f8 MY:PV1 MY:PV2\n\n"
    "  (doubles are printed as %%f with precision of 8)\n\n"
             , DEFAULT_TIMEOUT, CA_PRIORITY_MAX);
}



/*+**************************************************************************
 *
 * Function:	event_handler
 *
 * Description:	CA event_handler for request type callback
 * 		Prints the event data
 *
 * Arg(s) In:	args  -  event handler args (see CA manual)
 *
 **************************************************************************-*/

static void event_handler (evargs args)
{
    pv* pv = args.usr;

    pv->status = args.status;
    if (args.status == ECA_NORMAL)
    {
        pv->dbrType = args.type;
        pv->nElems = args.count;
        pv->value = (void *) args.dbr;    /* casting away const */

        print_time_val_sts(pv, reqElems);
        fflush(stdout);

        pv->value = NULL;
    }
}


/*+**************************************************************************
 *
 * Function:	connection_handler
 *
 * Description:	CA connection_handler 
 *
 * Arg(s) In:	args  -  connection_handler_args (see CA manual)
 *
 **************************************************************************-*/

static void connection_handler ( struct connection_handler_args args )
{
    pv *ppv = ( pv * ) ca_puser ( args.chid );
    if ( args.op == CA_OP_CONN_UP ) {
        nConn++;
        if (!ppv->onceConnected) {
            ppv->onceConnected = 1;
                                /* Set up pv structure */
                                /* ------------------- */

                                /* Get natural type and array count */
            ppv->dbfType = ca_field_type(ppv->chid);
            ppv->dbrType = dbf_type_to_DBR_TIME(ppv->dbfType); /* Use native type */
            if (dbr_type_is_ENUM(ppv->dbrType))                /* Enums honour -n option */
            {
                if (enumAsNr) ppv->dbrType = DBR_TIME_INT;
                else          ppv->dbrType = DBR_TIME_STRING;
            }
            else if (floatAsString &&
                     (dbr_type_is_FLOAT(ppv->dbrType) || dbr_type_is_DOUBLE(ppv->dbrType)))
            {
                ppv->dbrType = DBR_TIME_STRING;
            }
                                /* Set request count */
            ppv->nElems   = ca_element_count(ppv->chid);
            ppv->reqElems = reqElems > ppv->nElems ? ppv->nElems : reqElems;

                                /* Issue CA request */
                                /* ---------------- */
            /* install monitor once with first connect */
            ppv->status = ca_create_subscription(ppv->dbrType,
                                                ppv->reqElems,
                                                ppv->chid,
                                                eventMask,
                                                event_handler,
                                                (void*)ppv,
                                                NULL);
        }
    }
    else if ( args.op == CA_OP_CONN_DOWN ) {
        nConn--;
        ppv->status = ECA_DISCONN;
        print_time_val_sts(ppv, reqElems);
    }
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
	typedef std::tr1::shared_ptr<Worker>	value_type;
	typedef std::tr1::weak_ptr<Worker>		weak_type;
	// work queue holds only weak_ptr
	// so jobs must be kept alive seperately
	typedef std::deque<std::pair<weak_type, pvac::MonitorEvent> > queue_t;
	epicsEvent		event;
	epicsMutex		mutex;
	queue_t			queue;
	bool			running;
	pvd::Thread		worker;

	WorkQueue()
		:running(true)
		,worker(pvd::Thread::Config()
				.name("pvCapture handler")
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
					std::cout << "Error in monitor handler : " << e.what() << "\n";
				}
			}
		}
	}
};


// This could go to it's own cpp file and header
// Borrowed from pvmonitor.cpp
struct MonTracker : public pvac::ClientChannel::MonitorCallback,
					public Worker,
					public Tracker,
					public std::tr1::enable_shared_from_this<MonTracker>
{
	POINTER_DEFINITIONS(MonTracker);

	MonTracker(WorkQueue& monwork, pvac::ClientChannel& channel, const pvd::PVStructurePtr& pvRequest, const char * testDirPath, bool fShow)
		:monwork(monwork)
		,m_QueueSizeMax( 262144	)
		,valid()
		,fShow(fShow)
		,m_testDirPath(testDirPath)
		,mon(channel.monitor(this, pvRequest)	)
	{}
	virtual ~MonTracker()
	{
		mon.cancel();
	}

    epicsMutex		queueLock;
	WorkQueue	&	monwork;
	
	typedef	struct _tsReal
	{
		epicsTimeStamp	ts;
		double			val;
		_tsReal( ) : ts(), val() { val = NAN; };
		_tsReal( const epicsTimeStamp & newTs, double newVal ) : ts(newTs), val(newVal){ };
	}	t_TsReal;
	size_t					m_QueueSizeMax;
	std::deque<t_TsReal>	m_ValueQueue;

	pvd::BitSet valid; // only access for process()
	bool	fShow;
	std::string		m_testDirPath;

	pvac::Monitor mon; // must be last data member

	/// monitorEvent is called for each new pvAccess event for specified request on this client channel
	/// The ClientChannel defines: virtual void monitorEvent() = 0;
	virtual void monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL
	{
	// TODO: Getting this error msg if I create multiple MonTracker objects for the same PV name
	// Unhandled exception following exception in ClientChannel::MonitorCallback::monitorEvent(): epicsMutex::invalidMutex()
	// or
	// Unhandled exception in ClientChannel::MonitorCallback::monitorEvent(): tr1::bad_weak_ptr
	// Unhandled exception in ClientChannel::MonitorCallback::monitorEvent(): epicsMutex::invalidMutex()
	// Unhandled exception in ClientChannel::MonitorCallback::monitorEvent(): epicsMutex::invalidMutex()
		// shared_from_this() will fail as Cancel is delivered in our dtor.
		if(evt.event==pvac::MonitorEvent::Cancel) return;

		// running on internal provider worker thread
		// minimize work here.
		monwork.push(shared_from_this(), evt);
	}

	/// Save the timestamped values on the queue to a file
	void saveValues( )
	{
		if ( m_ValueQueue.size() == 0 )
			return;

		std::string		saveFilePath( m_testDirPath );
		saveFilePath += "/";
		saveFilePath += mon.name();
		// std::cout << "Creating test dir: " << m_testDirPath << std::endl;
		mkdir( m_testDirPath.c_str(), ACCESSPERMS );

		std::cout << "Writing " << m_ValueQueue.size() << " values to test file: " << saveFilePath << std::endl;
		std::ofstream	fout( saveFilePath.c_str() );
		fout << "[" << std::endl;
		for ( std::deque<t_TsReal>::iterator it = m_ValueQueue.begin(); it != m_ValueQueue.end(); ++it )
		{
			fout << "    [ [ " << it->ts.secPastEpoch << ", " << it->ts.nsec << "], " << it->val << " ]," << std::endl;
		}
		fout << "]" << std::endl;
	}

	/// capture is called for each pvAccess MonitorEvent::Data on the WorkQueue
	virtual void capture(const pvac::MonitorEvent& evt) OVERRIDE FINAL
	{
		assert( evt.event == pvac::MonitorEvent::Data );
		std::tr1::shared_ptr<const pvd::PVStructure>	pvStruct = mon.root;
		assert( pvStruct != 0 );

		try
		{
			std::tr1::shared_ptr<const pvd::PVInt>	pStatus = pvStruct->getSubField<pvd::PVInt>("alarm.status");
			std::tr1::shared_ptr<const pvd::PVInt>	pSeverity = pvStruct->getSubField<pvd::PVInt>("alarm.severity");
			// Only capture values w/ alarm.status NO_ALARM
			if ( pStatus == NULL || pStatus->get() != NO_ALARM )
				return;
			if ( pSeverity == NULL || pSeverity->get() != 0 )
				return;
			std::tr1::shared_ptr<const pvd::PVDouble>	pValue	= pvStruct->getSubField<pvd::PVDouble>("value");
			double			value			= NAN;
			if ( pValue )
			{
				value = pValue->getAs<double>();
			}

			epicsUInt32		secPastEpoch	= 1;
			epicsUInt32		nsec			= 2;
			std::tr1::shared_ptr<const pvd::PVScalar>	pScalarSec	= pvStruct->getSubFieldT<pvd::PVScalar>("timeStamp.secondsPastEpoch");
			if ( pScalarSec )
			{
				secPastEpoch	= pScalarSec->getAs<pvd::uint32>();
			}
			std::tr1::shared_ptr<const pvd::PVScalar>	pScalarNSec	= pvStruct->getSubFieldT<pvd::PVScalar>("timeStamp.nanoseconds");
			if ( pScalarNSec )
			{
				nsec	= pScalarNSec->getAs<pvd::uint32>();
			}
			epicsTimeStamp	timeStamp;
			timeStamp.secPastEpoch = secPastEpoch;
			timeStamp.nsec = nsec;
			//pvd::TimeStamp	timeStamp( secPastEpoch, nsec );
			t_TsReal	tsValue( timeStamp, value );
			t_TsReal	tsPrior;
			assert( isnan(tsPrior.val) );

			{	// Keep guard while accessing m_ValueQueue
			epicsGuard<epicsMutex> G(queueLock);
			if ( !m_ValueQueue.empty() )
				tsPrior = m_ValueQueue.back();
			if ( ! isnan(tsValue.val) )
			{
				if( m_ValueQueue.size() >= m_QueueSizeMax )
					m_ValueQueue.pop_front();
				m_ValueQueue.push_back( tsValue );
			}
			}

			// Check for missed counter update
			if ( ! isnan(tsValue.val) && tsValue.val != 0 && ! isnan(tsPrior.val) )
			{
				if ( tsPrior.val + 1.0 != tsValue.val )
				{
					if ( debugFlag )
					{
						std::cout	<< "tsPrior:"
							<< " val="	<< tsPrior.val	
							<< ", ts=[" << tsPrior.ts.secPastEpoch << ", " << tsPrior.ts.nsec << "]"
							<< ", SEVR=" << *pSeverity
							<< ", STAT=" << *pStatus << "\n";
						std::cout	<< "tsValue:"
							<< " val="	<< tsValue.val	
							<< ", ts=[" << tsValue.ts.secPastEpoch << ", " << tsValue.ts.nsec << "]"
							<< ", SEVR=" << *pSeverity
							<< ", STAT=" << *pStatus << "\n";
					}
					long int	nMissed = lround( tsValue.val - tsPrior.val - 1 );
					LOG( epics::pvAccess::logLevelError, "%s: Missed %ld, prior %ld, cur %ld", mon.name().c_str(),
						nMissed, static_cast<long int>(tsPrior.val), static_cast<long int>(tsValue.val) );
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

	/// process is called for each pvAccess event on the WorkQueue
	virtual void process(const pvac::MonitorEvent& evt) OVERRIDE FINAL
	{
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
			break;
		case pvac::MonitorEvent::Disconnect:
			std::cout << std::setw(pvnamewidth) << std::left << mon.name() << " <Disconnect>\n";
			valid.clear();
			break;
		case pvac::MonitorEvent::Data:
			for(n=0; n<2 && mon.poll(); n++)
			{
				valid |= mon.changed;

				// Capture the new value
				capture( evt );
				if ( fShow )
				{
					// pvd::PVStructure::Formatter fmt(mon.root->stream().format(outmode));
					// std::cout << std::setw(pvnamewidth) << std::left << mon.name() << ' ' << fmt;
				}
			}
			if(n==2)
			{
				// too many updates, re-queue to balance with others
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
};

} // namespace



/*+**************************************************************************
 *
 * Function:	main
 *
 * Description:	main()
 * 		Evaluate command line options, set up CA, connect the
 * 		channels, collect and print the data as requested
 *
 * Arg(s) In:	[options] <pv-name> ...
 *
 * Arg(s) Out:	none
 *
 * Return(s):	Standard return code (0=success, 1=error)
 *
 **************************************************************************-*/

int main (int argc, char *argv[])
{
    try
    {
    int opt;                    /* getopt() current option */
    int digits = 0;             /* getopt() no. of float digits */
    bool monitor    = true;
    bool fShow      = false;
    std::string         pvFilename("");
    std::vector<std::string>    pvList;

    std::string     testDirPath( "/tmp/caCaptureTest1" );

    // ================ Parse Arguments

    while ((opt = getopt(argc, argv, ":hvVSRD:M:r:w:tmp:qdcF:f:ni")) != -1) {
        switch (opt) {
        case 'h':               /* Print usage */
            usage();
            return 0;
        case 'V':               /* Print version */
            printf( "\nEPICS Version %s, CA Protocol version %s\n", EPICS_VERSION_STRING, ca_version() );
            epics::pvAccess::Version version(EXECNAME, "cpp",
                                CA_CAPTURE_MAJOR_VERSION,
                                CA_CAPTURE_MINOR_VERSION,
                                CA_CAPTURE_MAINTENANCE_VERSION,
                                CA_CAPTURE_DEVELOPMENT_FLAG);
            fprintf(stdout, "%s\n", version.getVersionString().c_str());
            return 0;
        case 'n':               /* Print ENUM as index numbers */
            enumAsNr=1;
            break;
        case 'P':               /* Show Fiducial Pulse ID from timestamps */
            tsShowPulseId = 1;
            break;
        case 't':               /* Select timestamp source(s) and type */
            tsSrcServer = 0;
            tsSrcClient = 0;
            {
                int i = 0;
                char c;
                while ((c = optarg[i++]))
                    switch (c) {
                    case 's': tsSrcServer = 1; break;
                    case 'c': tsSrcClient = 1; break;
                    case 'n': break;
                    case 'r': tsType = relative; break;
                    case 'i': tsType = incremental; break;
                    case 'I': tsType = incrementalByChan; break;
                    default :
                        fprintf(stderr, "Invalid argument '%c' "
                                "for option '-t' - ignored.\n", c);
                    }
            }
            break;
        case 'w':               /* Set CA timeout value */
            double temp;
            if(epicsScanDouble(optarg, &temp) != 1)
            {
                fprintf(stderr, "'%s' is not a valid timeout value "
                                "- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
            } else {
                timeout = temp;
            }
            break;
        case '#':               /* Array count */
            if (sscanf(optarg,"%ld", &reqElems) != 1)
            {
                fprintf(stderr, "'%s' is not a valid array element count "
                        "- ignored. ('camonitor -h' for help.)\n", optarg);
                reqElems = 0;
            }
            break;
        case 'p':               /* CA priority */
            if (sscanf(optarg,"%u", &caPriority) != 1)
            {
                fprintf(stderr, "'%s' is not a valid CA priority "
                        "- ignored. ('camonitor -h' for help.)\n", optarg);
                caPriority = DEFAULT_CA_PRIORITY;
            }
            if (caPriority > CA_PRIORITY_MAX) caPriority = CA_PRIORITY_MAX;
            break;
        case 'm':               /* Select CA event mask */
            eventMask = 0;
            {
                int i = 0;
                char c, err = 0;
                while ((c = optarg[i++]) && !err)
                    switch (c) {
                    case 'v': eventMask |= DBE_VALUE; break;
                    case 'a': eventMask |= DBE_ALARM; break;
                    case 'l': eventMask |= DBE_LOG; break;
                    case 'p': eventMask |= DBE_PROPERTY; break;
                        default :
                            fprintf(stderr, "Invalid argument '%s' "
                                    "for option '-m' - ignored.\n", optarg);
                            eventMask = DBE_VALUE | DBE_ALARM;
                            err = 1;
                    }
            }
            break;
        case 's':               /* Select string dbr for floating type data */
            floatAsString = 1;
            break;
        case 'S':               /* Treat char array as (long) string */
            charArrAsStr = 1;
            break;
        case 'e':               /* Select %e/%f/%g format, using <arg> digits */
        case 'f':
        case 'g':
            if (sscanf(optarg, "%d", &digits) != 1)
                fprintf(stderr, 
                        "Invalid precision argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            else
            {
                if (digits>=0 && digits<=VALID_DOUBLE_DIGITS)
                    sprintf(dblFormatStr, "%%-.%d%c", digits, opt);
                else
                    fprintf(stderr, "Precision %d for option '-%c' "
                            "out of range - ignored.\n", digits, opt);
            }
            break;
        case 'l':               /* Convert to long and use integer format */
        case '0':               /* Select integer format */
            switch ((char) *optarg) {
            case 'x': outType = hex; break;    /* x print Hex */
            case 'b': outType = bin; break;    /* b print Binary */
            case 'o': outType = oct; break;    /* o print Octal */
            default :
                outType = dec;
                fprintf(stderr, "Invalid argument '%s' "
                        "for option '-%c' - ignored.\n", optarg, opt);
            }
            if (outType != dec) {
              if (opt == '0') outTypeI = outType;
              else            outTypeF = outType;
            }
            break;
        case 'F':               /* Store this for output and tool_lib formatting */
            fieldSeparator = (char) *optarg;
            break;
        case 'S':
            fShow = true;
            break;
        case 'D':
            testDirPath = optarg;
            break;
        case 'f':               /* Use input stream as input */
            pvFilename = optarg;
            break;
        case 'd':               /* Debug log level */
            debugFlag = true;
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
        timeout = -1;

	// Add custom CA exception handler
	// (void) ca_add_exceptionEvent( pfunc, arg )


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

    // Everything up to here is just related to handling cmd line arguments
    {   // Create and run PVA clients for pvNames in argv[argc]
    // Configure logging
    SET_LOG_LEVEL(debugFlag ? epics::pvAccess::logLevelDebug : epics::pvAccess::logLevelError);

    epics::pvAccess::ca::CAClientFactory::start();

    {
        pvac::ClientProvider provider(defaultProvider);

        std::vector<std::tr1::shared_ptr<MonTracker> > tracked;

        epics::auto_ptr<WorkQueue> Q;
        Q.reset(new WorkQueue);

        for ( std::vector<std::string>::const_iterator it = pvList.begin(); it != pvList.end(); ++it )
        {
            pvac::ClientChannel chan( provider.connect(*it) );

            // std::tr1::shared_ptr<MonTracker> mon(new MonTracker(*Q, chan, pvRequest, testDirPath.c_str(), fShow));
            // tracked.push_back(mon);
        }

        // ========================== Wait for operations to complete, or timeout

        Tracker::prepare(); // install signal handler

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
                    std::cerr << "Timeout" << std::endl;
                    break;
                }
            }
        }

        std::cout << std::endl;
        for ( std::vector<std::tr1::shared_ptr<MonTracker> >::iterator it = tracked.begin(); it != tracked.end(); ++it )
        {
            (*it)->saveValues();
        }

    }
    // ========================== All done now

    if(debugFlag)
        std::cerr << "Done\n";

    return haderror ? 1 : 0;
    }
    }
    catch(std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
