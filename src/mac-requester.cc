/* Capacity Request module for MFTDMA-DAMA */

#include "mac-requester.h"
#include "mac-allocator.h"


static class RequesterCombinerClass : public TclClass
{
	public:
		RequesterCombinerClass() : TclClass("Requester/Combiner") {}
		TclObject* create(int, const char*const*)
		{
			return (new RequesterCombiner());
		}
} class_requester_combiner;


/* Initialize Requester and Allocator*/
TdmaRequester::TdmaRequester():  qMon_(0)
{
	reqerr_ = NULL;
	bind("request_",&request_);
	bind("req_period_",&delay_);
	bind("alpha_", &alpha_);

	prev_in = 0;
	bod_log = NULL;
	buffer_cur = 0;
	input_cur = 0;

	rate_req = 0;
}

void TdmaRequester::expire(Event* e)
{
	this->dorequest();
	resched(delay_);

}

void TdmaRequester::init()
{
	slot_num_ = mac_->slot_num_;
}


int TdmaRequester::command(int argc, const char*const* argv)
{

	if (argc == 2) 
	{
		if (strcmp(argv[1], "start") == 0) 
		{
			resched(0.0);	
			return TCL_OK;
		} 
		if (strcmp(argv[1], "stop") == 0) 
		{
			force_cancel();
			return TCL_OK;
		}
	} else if (argc == 3)
	{
		if (strcmp(argv[1], "attach") == 0)
		{
			mac_ = (MacTdmaDama*) TclObject::lookup(argv[2]);
			if(mac_ == 0)
				return TCL_ERROR;
			init();
			return TCL_OK;
		}
                if (strcmp(argv[1], "eventtrace") == 0)
                {
                        bod_log = (EventTrace *)TclObject::lookup(argv[2]);
                        if (!bod_log)
                                return TCL_ERROR;
                        return (TCL_OK);
                }
		if (strcmp(argv[1], "setifq") == 0)
		{
			ifq_ = (Queue *)TclObject::lookup(argv[2]);
			if (!ifq_)
				return TCL_ERROR;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "setmon") == 0)
		{
			qMon_ = (QueueMonitor *)TclObject::lookup(argv[2]);
			if (!qMon_)
				return TCL_ERROR;
			return (TCL_OK);
		}
		if (strcmp(argv[1], "reqerr") == 0)
		{
			reqerr_ = (ErrorModel *)TclObject::lookup(argv[2]);
			if (!reqerr_)
				return TCL_ERROR;
			return (TCL_OK);
		}
	}
	return TclObject::command(argc,argv);
}


RequesterCombiner::RequesterCombiner():   
 TdmaRequester(), last(0), tin(0), total_vol(0)
{
	bind("max_rbdc_",&max_rbdc_);
	bind("min_rbdc_",&min_rbdc_);
	bind("max_vbdc_",&max_vbdc_);

	bind("rbdc_", &rbdc_);
	bind("vbdc_", &vbdc_);
	bind("avbdc_", &avbdc_);
	bind("win_", &win_);

	bzero(&vec_vol, MAX_TIME_WIN*sizeof(int));
}


int RequesterCombiner::command(int argc, const char*const* argv)
{
	return TdmaRequester::command(argc, argv);
}


/* Compute the rate in kbps to generate the CR */
int TdmaRequester::rate()
{
	double rate_kbps;

	/* input rate estimation in Kbit_per_sec */
	input_cur = (int)qMon_->barrivals() - prev_in;
	rate_kbps = input_cur*0.008/delay_;
	rate_req = alpha_ * rate_req + (1-alpha_) * rate_kbps;
	prev_in = (int)qMon_->barrivals();
	return (int)lround(rate_req);
}

int TdmaRequester::volume()
{
	/* This function is deprecated */
	buffer_cur = (int)qMon_->barrivals() - (int)qMon_->bdepartures() - (int)qMon_->bdrops();
	return buffer_cur; 
}


void RequesterCombiner::dorequest()
{
Packet* p;
double c_rate = 0.0;
struct hdr_tdmadama *dh; 
struct hdr_mac *mh;

	if ( !(rbdc_ || vbdc_))
		return;

	/* override previous request */
	if (rbdc_)
		c_rate = rate();

	/* Cap the maximum rate */
	if ( c_rate > max_rbdc_ )
                c_rate = max_rbdc_;

	if ( c_rate < min_rbdc_ && rbdc_ ) 
		c_rate = min_rbdc_;


	if (avbdc_ || vbdc_) 
		buffer_cur = ifq_->byteLength();

	if (vbdc_) {
		/* VBDC requests keep             */
		/* previous requests into account */
		last = (last + 1) % win_;
		total_vol -= vec_vol[last];
		if ( buffer_cur > total_vol + input_cur ) {
			vec_vol[last] = buffer_cur - total_vol
				- input_cur;
			total_vol += vec_vol[last];
		} else
			vec_vol[last] = 0;
		buffer_cur = vec_vol[last];
	}

	if ( buffer_cur > max_vbdc_ )
		buffer_cur = max_vbdc_;

	p = Packet::alloc();
	mh = HDR_MAC(p);
	mh->ftype() = MF_CONTROL;
	dh = HDR_TDMADAMA(p);
	dh->type() = CR;
	dh->slot_num() = slot_num_;
	dh->rate() = (int)c_rate;
	dh->volume() = buffer_cur;

	/* log CR transmission */
	if (bod_log) {
		sprintf(bod_log->buffer(), "%-3d %7.04lf CR %5d %5d",
			slot_num_, NOW, dh->rate(), dh->volume());
		bod_log->dump();
	}
	mac_->recv(p, NULL);
}



void RequesterCombiner::init()
{
	TdmaRequester::init();
}


