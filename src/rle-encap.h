#ifndef ns_rlelink_h
#define ns_rlelink_h

#include "satlink.h"

#define HDR_RLE(p)     (hdr_ll_rle::access(p))

#define TRAILER      1      // Seqno
#define FRAG_HDR     2      // Fragment Hdr = Base Fragment + Label
#define EXT_HDR      2      // [C/Total len/LT/T] + Label
#define ALPDU_HDR    0      // ALDPUD HDR = Type + Label

// RLE context info
struct hdr_ll_rle
{
	/* This fields are metainfo to build fragments */

	unsigned addr_;
	unsigned total_length;
	unsigned frag_id;
	unsigned start_end;
	unsigned stream_id;
	unsigned seqno;
	unsigned psize;

	double sendtime;			 // time the packet is sent
	static int offset_;

	inline int& offset() { return offset_; }
	static hdr_ll_rle* access(const Packet* p)
	{
		return (hdr_ll_rle*) p->access(offset_);
	}


};

class RleLL : public SatLL {
public:
        //SatLL() : LL(), arpcache_(-1), arpcachedst_(-1) {}
        RleLL();
        virtual void recv(Packet* p, Handler* h);

protected:
	int sent_down_;
	int sent_up_;
	int alpdu_hdr_;
	int command(int argc, const char*const* argv);
	void init_packet(Packet* p);
	Queue* qMon_;
};

#endif
