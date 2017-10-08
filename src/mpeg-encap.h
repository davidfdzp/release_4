/* ATM/AAL5 encapsulator for MFTDMA-DAMA */

#ifndef ns_mpeg_encap_h
#define ns_mpeg_encap_h

#include "satlink.h"
#include <queue-monitor.h>

#define ULE_HDR_AND_CRC      8
#define ULE_HDR              4
#define MAX_FRAGS           10
#define N_QUEUES           100
#define HDR_MPEG(p)     (hdr_ll_mpeg::access((p)))

// #define DEBUG

class MpegLL;

/* TS packet header */
struct hdr_ll_mpeg
{

	int pid_;				 // source address
	int addr_;				 // destination address 
	int length_;				 // size of packet

	int pusi_;
	int dvb_space_;

	double sendtime_;			 // time the packet is sent
	static int offset_;
	int queue_;
	int status_;

	inline int& offset() { return offset_; }
	static hdr_ll_mpeg* access(const Packet* p)
	{
		return (hdr_ll_mpeg*) p->access(offset_);
	}
	
	inline int& addr() { return addr_; }
	inline int& length() { return length_; }
	inline double& sendtime() { return sendtime_; }

};


class MpegTimer: public Handler
{
	public:
		MpegTimer(MpegLL*dp) { dvbp = dp; };
		void cancel(Event* p) {
			Scheduler::instance().cancel(p);
		}

		void start(Event*p, double delay) {
			Scheduler::instance().schedule(
				this, 
				p,
				delay);
		}

	protected:	
	 virtual void handle(Event * e);
	 MpegLL* dvbp;

};

class DvbPayload: public AppData
{

	public:

		DvbPayload() : AppData(PACKET_DATA), index_(0) {}
		~DvbPayload();
		void insert_frag(Packet*,int);
		AppData* copy();
		int index() { return index_;};

		Packet* frag[MAX_FRAGS];
		int frag_size[MAX_FRAGS];

	private:
		int index_;

};

class MpegLL: public SatLL
{
	public:

		MpegLL();
		virtual void recv(Packet* p, Handler* h);
		virtual void sendUp(Packet* p);
		virtual void sendDown(Packet* p);

		virtual void reset();
		int command(int argc, const char*const* argv);

		void send_frags(Packet*);
		void insert_buffer(Packet*);
		void handle_timer(Packet*);
		void new_dvb(int, Packet*); 

		int limit_;
		void dump(Packet*);

	protected:

		int get_queue(Packet**, int);
		Queue* qMon_;

		MpegTimer* tx_timer;

		Packet* tx_buffer[N_QUEUES];
		Packet* rx_buffer[N_QUEUES];
		int rx_len_;

		DvbPayload* payload;
		void setpusi(Packet*);
		
		static int cid_;
		int id_;

		int sent_up_;
		int sent_down_;

		Packet* buffer_;

		double pack_thresh;

};

#endif

