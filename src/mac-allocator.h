#ifndef ns_mac_allocator_h
#define ns_mac_allocator_h

#include "mac-tdmadama.h"

#define MAX_RUL      256
#define MAX_FRM      256
#define MSK_LEN      512

#define SLOT_MODE      0
#define CONT_MODE      1
#define FIRST_FIT      0
#define BEST_FIT       1

class TdmaAllocator;

/* implements the timer that copy next allocation */
class PollAllocator: public TimerHandler
{
	public:
		PollAllocator(TdmaAllocator* a): allocator_(a) {}
		void expire(Event *e);

	protected:
		TdmaAllocator* allocator_;
};

/* Allocator classes */
class TdmaAllocator: public Connector 
{
	friend class AllocationDelay;

	public:
		TdmaAllocator();
		int command(int argc, const char*const* argv)
		{
			return Connector::command(argc,argv);
		};

		void virtual allocation() {};
		int active_node_;
		int bytes_superframe_;
		double frame_duration_;
		double slot_time_;
		int hub_;				 // allocator logical addr

		void virtual init();
		virtual void rcv_req(Packet*) {};


	protected:
		MacTdmaDama* mac_;

		/* request vector */
		double requestv_[MAX_TERM];
		void printrequest_nxt();

		/* allocation delay */
		double delay_;
		double jitter_;

		PollAllocator poll_;
		

		BaseTrace* bt;
};



class Terminal;

class Frame
{
	public:
		Frame() : marg(2) {};

		void reset();

		int carriers_;			 // number of carrier/frame
		int timeslots_;			 // number of timeslots/frame
		int burstsize_;			 // number of transport_cell/burst
		int payload_;			 // size of transport cell

		int marg;

		int slots;
		int fi;

		void print();
		void draw(BaseTrace*, int, int, int, int);

		int frame[MAX_CARR][MAX_SLOTS];
};

class Terminal
{
	public:
		Terminal();
		unsigned char* mask() {return _mask;}

		struct
		{
			Frame* fp;
			int rule;
		} rules[MAX_RUL];
		int rulenum;

		void addrule(int, const char*const*);

		int testmask(int, int);
		void setmask(int, int);
		void reset(int);
		int id_;

	private:
		unsigned char _mask[MSK_LEN];

};

class AllocatorMFTDMA: public TdmaAllocator
{

	public:

		AllocatorMFTDMA();

		int command(int argc, const char*const* argv);

		/* main allocation routine */
		virtual void allocation();
		virtual void init();

		void trace(char *, int);
		void printreq();
		void print_bits_buf(unsigned int x, char* buf);
		static char* print_bits(unsigned char*, int);

		void savereq();

		int mode_;				 //  SLOT_MODE, CONT_MODE
		int layout_;			 //  FIRST_FIT, BEST_FIT

		// ==1 means "do not carry to the next round"
		int forget_debit_;
		int max_req_buf_;
		int hwin_;
		int wwin_;

		int masksize;
		int sl_threshold_;

	protected:

		double firstfit(Frame*, Terminal*);
		double bestfit(Frame*, Terminal*);
		void init_request_queues(unsigned*);

		virtual void rcv_req(Packet*);

		void reset_allocator();

		void allocator_report();
		void btp_gen();
		void assign_timeslot(Packet*);

		int bs;
		int ns;
		int feedback_;
		double utilization_;
		double tolerance_;
		int utility_;
		double load_;
		double testing_;

		int* deficit;		 // deficit for allocator
		int slot_index;
		int cur_term;

		int a[MAX_TERM];			 /* allocations */

		void reset();

	private:
		int slot_count_;
		int freeslots;
		int left_to_check;

		Frame sf[MAX_FRM];

		unsigned cnst_rate[MAX_TERM];
		unsigned reqv_rate[MAX_TERM];
		unsigned reqv_volume[MAX_TERM];

		int nf;

		Terminal tr[MAX_TERM];

		int page;

		static char wrk[1024];
};
#endif							 /*  mac_allocator_h  */
