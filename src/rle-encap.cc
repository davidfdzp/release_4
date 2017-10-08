/* Contains source code for: RLE encapsulator */

#include "rle-encap.h"
//#include "sattrace.h"
//#include "satposition.h"
//#include "satgeometry.h"
//#include "satnode.h"
//#include "satroute.h"
//#include "errmodel.h"
//#include "sat-hdlc.h"


static class RleSatLLClass : public TclClass {
public:
	RleSatLLClass() : TclClass("LL/Rle") { }
	TclObject* create(int, const char*const*) {
		return (new RleLL());
	}
} sat_class_rle;


static class RleHeaderClass : public PacketHeaderClass
{
	public:
		RleHeaderClass(): PacketHeaderClass("PacketHeader/Rle",
			sizeof(hdr_ll_rle))
		{
			bind_offset(&hdr_ll_rle::offset_);
		}
} class_hdr_ll_rle;

int hdr_ll_rle::offset_;

RleLL::RleLL() : SatLL() {
	bind("sent_up_", &sent_up_);
	bind("sent_down_", &sent_down_);
	bind("alpdu_hdr_", &alpdu_hdr_);
}

void RleLL::recv(Packet* p, Handler* /*h*/)
{
	hdr_cmn *ch = HDR_CMN(p);
	
	/*
	 * Sanity Check
	 */
	assert(initialized());
	
	// If direction = UP, then pass it up the stack
	// Otherwise, set direction to DOWN and pass it down the stack
	if(ch->direction() == hdr_cmn::UP) {
		sent_up_++;
		uptarget_ ? sendUp(p) : drop(p);
		return;
	}

	init_packet(p);
	sent_down_++;
	ch->direction() = hdr_cmn::DOWN;
	sendDown(p);

}

void RleLL::init_packet(Packet* p)
{
	
	hdr_cmn *ch = HDR_CMN(p);
	hdr_ip *iph = hdr_ip::access(p);
	hdr_ll_rle *rh = hdr_ll_rle::access(p);

	rh->total_length = ch->size_ + alpdu_hdr_;     // The default ALPDU_HDR is zero
	rh->stream_id = iph->flowid();
	if (iph->prio() > 7) {
		fprintf(stderr, "RleLL::init_packet(): only 8 priorities supported\n");
		abort();
	}

	rh->frag_id = iph->prio();
	rh->start_end = 0x0003;
	rh->seqno = 0;
	
	rh->psize = rh->total_length;
	rh->stream_id = iph->flowid();

}

int RleLL::command(int argc, const char*const* argv)
{

	if (argc == 3) {
		if (strcmp(argv[1], "setnode") == 0) {
			satnode_ = (SatNode*) TclObject::lookup(argv[2]);
			return (TCL_OK);
		} else 
                if (strcmp(argv[1], "setmon") == 0)
                {
                        qMon_ = (Queue*)TclObject::lookup(argv[2]);
                        if (!qMon_)
                                return TCL_ERROR;
                        return (TCL_OK);
                }
	}
	return SatLL::command(argc, argv);
}



