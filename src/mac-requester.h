/* Capacity requests for DVB-RCS */

#ifndef ns_mac_requester_h
#define ns_mac_requester_h

#define MAX_TIME_WIN        512

#include "mac-tdmadama.h"

/* Make a request to allocator for each Tdma node */
class TdmaRequester: public TimerHandler, public TclObject 
{

	public:
		TdmaRequester();
		virtual void expire(Event*);

		/* MAC rate and buffer estimates */
		int rate();
		int volume();

		virtual void dorequest() {};

		int command(int argc, const char*const* argv);

		// initialize dynamic variables
		void init();

		/* pointer to Queue */
		Queue* ifq_;
		QueueMonitor* qMon_;

		/* QS variables */
		double totQSreq_;
		double delay_;

	protected:
		MacTdmaDama* mac_;
		int slot_num_;
		struct sac_field* sac_; 
		int prev_in;
		int buffer_cur;
		int input_cur;

		double rate_req;
		double alpha_;

		/* the request to submit */
		double request_;

		ErrorModel* reqerr_;
		EventTrace* bod_log;

};


class RequesterCombiner: public TdmaRequester
{

	public:
		RequesterCombiner();
		int command(int argc, const char*const* argv);

		virtual void dorequest();

	private:

		int rbdc_;
		int vbdc_;
		int avbdc_;
		int time_win_;

		double max_rbdc_;
		double min_rbdc_;
		double max_vbdc_;

		int prev_in;
		int win_;
		int last;
		double tin;

		int total_vol;
		int vec_vol[MAX_TIME_WIN];

		/* new functions */
		struct sac* dulm_IE;
		void init();

};


#endif	/* ns_mac_requester_h */
