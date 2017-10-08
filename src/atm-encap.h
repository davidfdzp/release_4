/* ATM/AAL5 multiprotcol encapsulator (RFC 2684) for MFTDMA-DAMA
 * This code considers the VC encapsulation (Sec 3) rather than LLC */

#include "satlink.h"
#include <queue-monitor.h>

#define ATM_TRL_LEN     8
#define ATM_PRIO_NO     8
#define HDR_ATM(p)     (hdr_ll_atm::access(p))

// #define DEBUG

/* ATM header */
struct hdr_ll_atm
{

	int pt_;					 // end of packet
	int addr_;					 // VPI/VCI
	int length_;				 // size of packet
	double sendtime_;			 // time the packet is sent
	static int offset_;

	inline int& offset() { return offset_; }
	static hdr_ll_atm* access(const Packet* p)
	{
		return (hdr_ll_atm*) p->access(offset_);
	}
	inline int& pt() { return pt_; }
	inline int& addr() { return addr_; }
	inline int& length() { return length_; }
	inline double& sendtime() { return sendtime_; }

};


class AtmLL: public SatLL
{
	public:

		AtmLL();
		virtual void recv(Packet* p, Handler* h);
		virtual void sendUp(Packet* p);
		virtual void sendDown(Packet* p);

		virtual void reset();
		int command(int argc, const char*const* argv);

		void send_frags(Packet*);
		void insert_buffer(Packet*);

		int fragsize_;
		void dump_buffer();
		void dump(Packet*);

	protected:

		Queue* qMon_;
		static int cid_;
		int id_;

		int sent_up_;
		int sent_down_;

		Packet* buffer_;

};
