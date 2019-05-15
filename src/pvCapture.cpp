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
#include <epicsTypes.h>

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

//#include "pvutils.h"

#define USE_SIGNAL
#ifndef EXECNAME
#define EXECNAME "pvCapture"
#endif

#define PVA_CAPTURE_MAJOR_VERSION			0
#define PVA_CAPTURE_MINOR_VERSION			1
#define PVA_CAPTURE_MAINTENANCE_VERSION		0
#define PVA_CAPTURE_DEVELOPMENT_FLAG		1

namespace {

// From pvAccessCPP/pvtoolsSrc/pvutils.cpp
double timeout = 5.0;
bool debugFlag = false;
epics::pvData::PVStructure::Formatter::format_t outmode = epics::pvData::PVStructure::Formatter::NT;
// outmode = epics::pvData::PVStructure::Formatter::Raw;
// outmode = epics::pvData::PVStructure::Formatter::NT;
// outmode = epics::pvData::PVStructure::Formatter::JSON;
int	verbosity	= 0;
std::string request("");
std::string defaultProvider("pva");

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
	fprintf( stderr, "\nUsage: " EXECNAME " [options] <PV:Name>...\n"
			"\n"
			"options:\n" \
			"  -h: Help: Print this message\n" \
			"  -V: Print version and exit\n" \
			"  -r <pv request>:   Request, specifies what fields to return and options, default is '%s'\n" \
			"  -w <sec>:          Wait time, specifies timeout, default is %f second(s)\n" \
			"  -p <provider>:     Set default provider name, default is '%s'\n" \
			"  -M <raw|nt|json>:  Output mode.  default is 'nt'\n" \
			"  -v:                Show entire structure (implies Raw mode)\n" \
			"  -q:                Quiet mode, print only error messages\n" \
			"  -d:                Enable debug output\n"
			"  -f <input file>:   Read pvName list from file, one line per pvName.\n"
			"  -S:                Show each PV as it's captured, same output options as pvmonitor.\n"
			" deprecated options:\n"
			"  -t, -i, -n, -F: ignored\n"
			" Output details:\n"
			"  -m -v:   Monitor in Raw mode. Print only fields marked as changed.\n"
			"  -m -vv:  Monitor in Raw mode. Highlight  fields marked as changed, show all valid fields.\n"
			"  -m -vvv: Monitor in Raw mode. Highlight  fields marked as changed, show all fields.\n"
			"  -vv:     Get in Raw mode.     Highlight  valid fields, show all fields.\n"
			"\n"
			"example: " EXECNAME " double01\n\n"
//			, request.c_str(), timeout, defaultProvider.c_str()
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
	typedef std::tr1::shared_ptr<Worker>	value_type;
	typedef std::tr1::weak_ptr<Worker>		weak_type;
	// work queue holds only weak_ptr
	// so jobs must be kept alive seperately
	typedef std::deque<std::pair<weak_type, pvac::MonitorEvent> > queue_t;
	epicsEvent		event;
	epicsMutex		mutex;
	queue_t			queue;
	bool			running;
	epics::pvData::Thread		worker;

	WorkQueue()
		:running(true)
		,worker(epics::pvData::Thread::Config()
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

	MonTracker(WorkQueue& monwork, pvac::ClientChannel& channel, const epics::pvData::PVStructurePtr& pvRequest, const char * testDirPath, bool fShow)
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
		_tsReal( const epicsTimeStamp & newTs, double newVal ) : ts(newTs), val(newVal){ };
	}	t_TsReal;
	size_t					m_QueueSizeMax;
	std::deque<t_TsReal>	m_ValueQueue;

	epics::pvData::BitSet valid; // only access for process()
	bool	fShow;
	std::string		m_testDirPath;

	pvac::Monitor mon; // must be last data member

	/// monitorEvent is called for each new pvAccess event for specified request on this client channel
	/// The ClientChannel defines: virtual void monitorEvent() = 0;
	virtual void monitorEvent(const pvac::MonitorEvent& evt) OVERRIDE FINAL
	{
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
		//for ( epics::pvAccess::MonitorElement::Ref	it(mon); it; ++it )
		//	epics::pvAccess::MonitorElement	&	element(*it);
		//epics::pvAccess::Monitor::shared_pointer	pmon(&mon.root);
		//epics::pvAccess::MonitorElement::Ref		element(pmon);
		std::tr1::shared_ptr<const epics::pvData::PVStructure>	pvStruct = mon.root;
		//if ( element )
		if ( pvStruct )
		{
			// mon.name() should be pvName
			// To see text representation
			// epics::pvData::PVStructure::Formatter	fmt( mon.root->stream().format(outmode) );
			// fmt.show(mon.changed); // highlight none
			// std::cout << fmt << endl;
			// or to fetch just the value field (see pvDataCPP pvData.h)
			// epics::pvData::PVField::const_shared_pointer valfld(pvStruct->getSubField("value"));
			// if(!valfld)
			//     valfld = pvStruct.value;
			// std::cout << *valfld
			// const PVFieldPtrArray & pvFields = pvStruct->getPVFields();
			// pvStruct->getSubField<epics::pvData::PVDouble>("value")
			// pvStruct->getSubField<epics::pvData::PVInt>("value")
			// pvStruct->getSubFieldT<epics::pvData::PVULong>("value")
			// pvStruct->getStructure()
			// cout << ScalarTypeFunc::name(pPVScalarValue->typeCode);
			// cout << ScalarTypeFunc::name(PVT::typeCode);
			// template<> const ScalarType PVDouble::typeCode = pvDouble;
			try
			{
				std::tr1::shared_ptr<const epics::pvData::PVDouble>	pValue	= pvStruct->getSubField<epics::pvData::PVDouble>("value");
				double			value			= FP_NAN;
				if ( pValue )
					value = pValue->getAs<double>();
				epicsUInt32		secPastEpoch	= 0;
				epicsUInt32		nsec			= 0;

				std::tr1::shared_ptr<const epics::pvData::TimeStamp>	pTs		= pvStruct->getSubField<epics::pvData::TimeStamp>("timeStamp");
				if ( pTs )
				{
					secPastEpoch	= pTs->getSecondsPastEpoch();
					nsec			= pTs->getNanoseconds();
				}
				epicsTimeStamp	timeStamp;
				timeStamp.secPastEpoch = secPastEpoch;
				timeStamp.nsec = nsec;
				//epics::pvData::TimeStamp	timeStamp( secPastEpoch, nsec );
				t_TsReal	tsValue( timeStamp, value );

				epicsGuard<epicsMutex> G(queueLock);
				if ( m_ValueQueue.size() >= m_QueueSizeMax )
				{
					m_ValueQueue.pop_front();
				}
				// m_ValueQueue.emplace_back( tsValue );
				m_ValueQueue.push_back( tsValue );
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
					epics::pvData::PVStructure::Formatter fmt(mon.root->stream()
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

#ifndef MAIN
#  define MAIN main
#endif

int MAIN (int argc, char *argv[])
{
	try {
		int opt;					/* getopt() current option */
		bool monitor	= true;
		bool fShow		= false;

		epics::RefMonitor refmon;
		std::string		testDirPath( "/tmp/pvCaptureTest1" );

		// ================ Parse Arguments

		while ((opt = getopt(argc, argv, ":hvVSRD:M:r:w:tmp:qdcF:f:ni")) != -1) {
			switch (opt) {
			case 'h':				/* Print usage */
				usage();
				return 0;
			case 'v':
				verbosity++;
				break;
			case 'V':				/* Print version */
			{
				epics::pvAccess::Version version(EXECNAME, "cpp",
									PVA_CAPTURE_MAJOR_VERSION,
									PVA_CAPTURE_MINOR_VERSION,
									PVA_CAPTURE_MAINTENANCE_VERSION,
									PVA_CAPTURE_DEVELOPMENT_FLAG);
				fprintf(stdout, "%s\n", version.getVersionString().c_str());
				return 0;
			}
			case 'S':
				fShow = true;
				break;
			case 'R':
				refmon.start(5.0);
				break;
			case 'D':
				testDirPath = optarg;
				break;
			case 'M':
				if(strcmp(optarg, "raw")==0) {
					outmode = epics::pvData::PVStructure::Formatter::Raw;
				} else if(strcmp(optarg, "nt")==0) {
					outmode = epics::pvData::PVStructure::Formatter::NT;
				} else if(strcmp(optarg, "json")==0) {
					outmode = epics::pvData::PVStructure::Formatter::JSON;
				} else {
					fprintf(stderr, "Unknown output mode '%s'\n", optarg);
					outmode = epics::pvData::PVStructure::Formatter::Raw;
				}
				break;
			case 'w':				/* Set PVA timeout value */
			{
				double temp;
				if((epicsScanDouble(optarg, &temp)) != 1)
				{
					fprintf(stderr, "'%s' is not a valid timeout value "
									"- ignored. ('" EXECNAME " -h' for help.)\n", optarg);
				} else {
					timeout = temp;
				}
			}
				break;
			case 'r':				/* Set PVA timeout value */
				request = optarg;
				break;
			case 't':				/* Terse mode */
			case 'i':				/* T-types format mode */
			case 'F':				/* Store this for output formatting */
			case 'n':
			case 'q':				/* Quiet mode */
				// deprecate
				break;
			case 'f':				/* Use input stream as input */
				fprintf(stderr, "Unsupported option -f\n");
				return 1;
			case 'm':				/* Monitor mode */
				monitor = true;
				break;
			case 'p':				/* Set default provider */
				defaultProvider = optarg;
				break;
			case 'd':				/* Debug log level */
				debugFlag = true;
				break;
			case 'c':				/* Clean-up and report used instance count */
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

		if(verbosity>0 && outmode==epics::pvData::PVStructure::Formatter::NT)
			outmode = epics::pvData::PVStructure::Formatter::Raw;

		epics::pvData::PVStructure::shared_pointer pvRequest;
		try {
			pvRequest = epics::pvData::createRequest(request);
		} catch(std::exception& e){
			fprintf(stderr, "failed to parse request string: %s\n", e.what());
			return 1;
		}

		for(int i = optind; i < argc; i++)
		{
			pvnamewidth = std::max(pvnamewidth, strlen(argv[i]));
		}

	// Everything up to here is just related to handling cmd line arguments
	{	// Create and run PVA clients for pvNames in argv[argc]
		// Configure logging
		SET_LOG_LEVEL(debugFlag ? epics::pvAccess::logLevelDebug : epics::pvAccess::logLevelError);

		epics::pvAccess::ca::CAClientFactory::start();

		{
			pvac::ClientProvider provider(defaultProvider);

			std::vector<std::tr1::shared_ptr<MonTracker> > tracked;

			epics::auto_ptr<WorkQueue> Q;
			Q.reset(new WorkQueue);

			for(int i = optind; i < argc; i++)
			{
				pvac::ClientChannel chan(provider.connect(argv[i]));

				std::tr1::shared_ptr<MonTracker> mon(new MonTracker(*Q, chan, pvRequest, testDirPath.c_str(), fShow));

				tracked.push_back(mon);
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
			if(refmon.running())
			{
				refmon.stop();
				// show final counts
				refmon.current();
			}
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
	} catch(std::exception& e)
	{
		std::cerr << "Error: " << e.what() << "\n";
		return 1;
	}
}