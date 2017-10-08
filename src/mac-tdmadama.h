/* MAC basic functions */

#ifndef ns_mac_tdmadama_h
#define ns_mac_tdmadama_h


#include "marshall.h"
#include "queue-monitor.h"

#include <delay.h>
#include <connector.h>
#include <packet.h>
#include <random.h>
#include <arp.h>
#include <ll.h>
#include <mac.h>
#include <hdr_qs.h>
#include <errmodel.h>

#define GET_ETHER_TYPE(x)    GET2BYTE((x))
#define SET_ETHER_TYPE(x,y)  {u_int16_t t = (y); STORE2BYTE(x,&t);}


/* ======================================================================
   Superframe Formats
   ====================================================================== */

// These number need to be consistent with new-frame 
#define MAX_TERM                  1100
#define MAX_CARR                   20
#define MAX_SLOTS                 256
#define MAX_BURST               16000
#define MAC_TDMA_SLOT_NUM          32

#define EPS                 0.0000001

#define NOTHING_TO_SEND            -2
#define FIRST_ROUND                -1

// Turn radio on /off
#define ON                          1
#define OFF                         0

// Indicate that the slot is not allocated
#define SLOT_UNALLOC               -2
#define SLOT_NOREQ                  0
#define REQ_LOST                   -1

/* MAC header format */
#define MPE_HDR_LEN    16
#define ULE_HDR_LEN    4

#define HDR_TDMADAMA(p)      (hdr_tdmadama::access(p))

enum message_type { 
	CR,
	TBTP
};

struct burst_des {

	int assignment_id;
	double offset;
	int size;


};


struct hdr_tdmadama {

	message_type type_;
	int slot_num_;
	int rate_;
	int volume_;
	static int offset_;

	double time_;
	int size_;
	int cellno_;

        inline static int& offset() { return offset_; }
	inline static hdr_tdmadama* access(const Packet*p){
		return (struct hdr_tdmadama*) p->access(offset_);
	}

	message_type& type() { return type_; }
	int& slot_num() { return slot_num_; }
	int& rate() { return rate_; }
	int& volume() { return volume_; }
	double& time() { return time_; }

};

class FramePDU : public AppData {
public:
	FramePDU(AppDataType, int);
	virtual ~FramePDU();
	virtual int size() const { return size_; };
	virtual AppData* copy();

	Packet** pks_;

private:
	int size_;

};


class MacTdmaDama;

class SlotTimer : public Handler
{
	public:
		SlotTimer(MacTdmaDama* m, double s = 0) : mac(m)
		{
			busy_ = paused_ = 0; stime = rtime = 0.0; slottime_ = s;
		}

		virtual void handle(Event *e);

		virtual void start(Packet *p, double time);
		virtual void stop(Packet *p);
		virtual void pause(void) { assert(0); }
		virtual void resume(void) { assert(0); }

		inline int busy(void) { return busy_; }
		inline int paused(void) { return paused_; }
		inline double slottime(void) { return slottime_; }
		inline double expire(void)
		{
			return ((stime + rtime) - Scheduler::instance().clock());
		}

	protected:
		MacTdmaDama     *mac;
		int     busy_;
		int     paused_;
		Event       intr;
		double      stime;		 // start time
		double      rtime;		 // remaining time
		double      slottime_;
};

class TdmaAllocator;

/* TDMA Mac layer. */
class MacTdmaDama : public Mac
{

	public:
		MacTdmaDama();
		void virtual recv(Packet *p, Handler *h);

		/* Timer handler */
		void virtual slotHandler(Event *e);

		/* Allocator pointer to dispatch CRs */
		TdmaAllocator* allocator_;

		/* Data structure for tdma scheduling. */
		static int active_node_; // Number of terminals to be rescheduled

		/* TDMA scheduling state. */
		// The max num of slot within one frame.
		int slot_num_;			 // The slot number it's allocated.
		int seqno_;

		int slot_packet_len_;

		/* manage packet error models */
		ErrorModel* em_;

		int command(int argc, const char*const* argv);

	protected:
		Queue* qMon_;
		void recv_control(Packet*);
		void btp_decoding(Packet*);
		int used_slots_;
		int total_slots_;

	private:

		// dump only allocation
		int dump_alloc_;

		/* Packet Transmission Functions.*/
		void virtual sendUp(Packet* p);
		void virtual sendDown();
		NsObject* logtarget_;

		void mac_log(Packet *p)
		{
			logtarget_->recv(p, (Handler*) 0);
		}

		SlotTimer mhSlot_;
		EventTrace* bod_log;
		void watch_alloc();

};
#endif							 /* __mac_tdmadama_h__ */
