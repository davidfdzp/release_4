/* University of Aberdeen - 2009 */

#include <sys/param.h>
#include <algorithm>
#include "trafgen.h"
#include "ranvar.h"
#include "agent.h"
#include "config.h"
#include "packet.h"
#include "ip.h"
#include "rtp.h"


#define MAX_D_SAMP    1000

struct hdr_voice
{
	int id_;
	double tx_time;
	int seqno;
};

/* Voice Model */

class Voice : public TrafficGenerator
{
	public:
		Voice();
		virtual double next_interval(int&);
		virtual void timeout();
		int command(int argc, const char*const* argv);
		virtual void process_data(int size, AppData* data);
		void rscore();

		void reset();

		double rscore_;
		double mos_;
		double loss_ratio_;
		double loss_playout_;

	protected:
		void init();
		double ontime_;			 /* average length of burst (sec) */
		double offtime_;		 /* average length of idle time (sec) */
		double interval_;		 /* packet inter-arrival time during burst (sec) */
		unsigned int rem_;		 /* number of packets left in current burst */
		int det_;

		double delay_;
		int drop_prob_;

		ExponentialRandomVariable burstlen_;
		ExponentialRandomVariable idletime_;

		int seqno;

	private:

		int sent_;
		int rx_tot_;

		double _delay;

		int init_seqno;

		static int voice_cnt;
		int id_;

		double dd_buf[MAX_D_SAMP];

};

int Voice::voice_cnt = 0;

static class VoiceClass : public TclClass
{
	public:
		VoiceClass() : TclClass("Application/Traffic/Voice") {}
		TclObject* create(int, const char*const*)
		{
			return (new Voice());
		}
} class_voicetx;

int Voice::command(int argc, const char*const* argv)
{

	if(argc==2)
	{
		if (strcmp(argv[1], "reset") == 0) 
		{
			reset();
			return (TCL_OK);
		} else 
		if (strcmp(argv[1], "update_score") == 0) 
		{
			rscore();
			return (TCL_OK);
		} 
	} else
	if(argc==3)
	{
		if (strcmp(argv[1], "use-rng") == 0)
		{
			burstlen_.seed((char *)argv[2]);
			idletime_.seed((char *)argv[2]);
			return (TCL_OK);
		}
	}
	
	return TrafficGenerator::command(argc,argv);

}


Voice::Voice() : burstlen_(0.0), idletime_(0.0), seqno(0),
sent_(0), rx_tot_(0)
{
	bind_time("burst_time_", &ontime_);
	bind_time("idle_time_", idletime_.avgp());
	bind_bw("interval_", &interval_);
	bind("packetSize_", &size_);
	bind("deterministic_",&det_);
	bind("delay_",&delay_);
	bind("cur_delay_",&_delay);

	bind("rscore_", &rscore_);
	bind("mos_", &mos_);
	bind("loss_ratio_", &loss_ratio_);
	bind("loss_playout_", &loss_playout_);

	bind("sent_",&sent_);

	id_ = voice_cnt++;

}


void Voice::reset()
{

	_delay = 0;

	sent_ = 0;
	rx_tot_ = 0;

}


void Voice::init()
{
	/* compute inter-packet interval during bursts based on
	 * packet size and burst rate.  then compute average number
	 * of packets in a burst. */

	burstlen_.setavg(ontime_/interval_);
	rem_ = 0;
	if (agent_)
		agent_->set_pkttype(PT_EXP);

	reset();

}


double Voice::next_interval(int& size)
{

	double t = interval_;

	if (rem_ == 0)
	{
		/* compute number of packets in next burst */
		if ( det_ )
			rem_ = int(ontime_/interval_) + 1;
		else
			rem_ = int(burstlen_.value()) + 1;


		/* start of an idle period, compute idle time */
		if ( det_ )
			t += *idletime_.avgp();
		else
			t += idletime_.value();

	}
	rem_--;

	size = size_;
	return(t);
}


void Voice::timeout()
{
	PacketData* pd;
	struct hdr_voice hv;

	if (! running_)
		return;

	sent_++;

	hv.seqno = seqno++;
	hv.tx_time = NOW;
	hv.id_ = id_;

	pd = new PacketData(sizeof(hdr_voice));
	memcpy(pd->data(), &hv, sizeof(hdr_voice));
	agent_->sendmsg(size_, pd);

	// printf("%.3lf SNT %d %d\n", NOW, id_, hv.seqno);

	/* figure out when to send the next one */
	nextPkttime_ = next_interval(size_);

	/* schedule it */
	if (nextPkttime_ > 0)
		timer_.resched(nextPkttime_);

}


void Voice::process_data(int size, AppData* data)
{
	struct hdr_voice* hvp;
	PacketData* pd;
	int tx_est;

	pd = (PacketData*)data;
	hvp = (hdr_voice*)(pd->data());

	// printf("%.3lf RCV %d %d %d\n", NOW, id_, hvp->id_, hvp->seqno);

	_delay = (NOW - hvp->tx_time)*1000;
	dd_buf[rx_tot_ % MAX_D_SAMP] = _delay; 	
	rx_tot_++;


	if (rx_tot_ == 1) 
		init_seqno = hvp->seqno;

	/* number of packet sent estimate */
	tx_est = hvp->seqno - init_seqno + 1; 

	loss_ratio_ = 1.0 - double(rx_tot_)/tx_est;


}


/* R-score for G729 */
void Voice::rscore()
{
	double Ie, Id;
	int i,ss;
	double score;
	double e;

	ss = MIN(MAX_D_SAMP,rx_tot_);

	sort(dd_buf, dd_buf+ss);

	rscore_ = 0;
	for(i=0; i<ss; i++) {
		e = double(ss-1-i)/ss + loss_ratio_;

		Ie = 10.0 + 47.82 * log(1.0 + 18.0 * e);
		Id = dd_buf[i] * 0.024;
		if ( dd_buf[i] > 177.3 ) 
			Id += 0.11*( dd_buf[i] - 177.3);

		score = 94.2 - (Ie + Id);
		if (score > rscore_) {
			delay_ = dd_buf[i];
			rscore_ = score;
			loss_playout_ = e;
		}

	}

	mos_ = 1.0 + 0.035*rscore_ 
		+ 7e-6*rscore_*(rscore_-60.0)*(100.0-rscore_);

}
