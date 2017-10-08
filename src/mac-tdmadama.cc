/* MAC block for MFTDMA-DAMA - University of Aberdeen 2009 */

#include "delay.h"
#include "connector.h"
#include "packet.h"
#include "random.h"

#define DEBUG_MAC 1

//#include <debug.h>

#include "mac-requester.h"
#include "mac-allocator.h"

#include "arp.h"
#include "ll.h"
#include "mac.h"
#include "mac-tdmadama.h"
#include "wireless-phy.h"
#include "delay.h"
#include "connector.h"
#include "packet.h"
#include "random.h"
#include "arp.h"
#include "ll.h"
#include "mac.h"


int MacTdmaDama::active_node_ = 0;

static class TdmaDamaHeaderClass : public PacketHeaderClass
{
	public:
		TdmaDamaHeaderClass(): PacketHeaderClass("PacketHeader/TdmaDama",
			sizeof(hdr_tdmadama))
		{
			bind_offset(&hdr_tdmadama::offset_);
		}
} class_hdr_tdmadama;

int hdr_tdmadama::offset_;


static class MacTdmaDamaClass : public TclClass
{
	public:
		MacTdmaDamaClass() : TclClass("Mac/TdmaDama") { }
		TclObject* create(int, const char*const*)
		{
			return (new MacTdmaDama());
		}
} class_mac_tdma;


/* Timers */
void SlotTimer::start(Packet *p, double time)
{
	Scheduler &s = Scheduler::instance();
	assert(busy_ == 0);

	busy_ = 1;
	paused_ = 0;
	stime = s.clock();
	rtime = time;
	assert(rtime >= 0.0);

	s.schedule(this, p, rtime);
}


void SlotTimer::stop(Packet *p)
{
	Scheduler &s = Scheduler::instance();
	assert(busy_);

	if(paused_ == 0)
		s.cancel((Event *)p);

	// Should free the packet p.
	Packet::free(p);

	busy_ = 0;
	paused_ = 0;
	stime = 0.0;
	rtime = 0.0;
}


/* Slot timer for TDMA scheduling. */
void SlotTimer::handle(Event *e)
{
	busy_ = 0;
	paused_ = 0;
	stime = 0.0;
	rtime = 0.0;

	mac->slotHandler(e);
}

FramePDU::FramePDU(AppDataType type, int n): AppData(type) 
{
	size_ = n++;
	pks_ = new Packet *[n];
	bzero(pks_, n*sizeof(Packet*));

}

FramePDU::~FramePDU()
{
	for(int i=0; pks_[i]!=NULL; i++)
		Packet::free(pks_[i]);
	delete [] pks_;
}

AppData* FramePDU::copy()
{

	FramePDU *fp = new FramePDU(PACKET_DATA, size_);
	for(int i=0; pks_[i]!=NULL; i++)  
		fp->pks_[i] = pks_[i]->refcopy();

	return fp;	
}


// Mac Tdma definitions
MacTdmaDama::MacTdmaDama() : Mac(), mhSlot_(this) 
{

	/* Global variables setting. */
	// Setup the phy specs.
	static int slot_pointer = 0;

	// bind parameters of MacTdmaDama object
	bind("slot_packet_len_", &slot_packet_len_);
	bind("dump_alloc_", &dump_alloc_);
	bind("used_slots_",&used_slots_);
	bind("total_slots_",&total_slots_);
	bind("slot_num_", &slot_num_);

	active_node_++;
	slot_num_ = slot_pointer++;

	if ( active_node_>MAX_TERM ) {
		fprintf(stderr, "MacTdmaDama(): too many terminals %d\n", active_node_);
		abort();
	}

	allocator_ = NULL;
	em_ = NULL;
	bod_log = NULL;
	dump_alloc_ = NULL;

}

/* similar to 802.11, no cached node lookup. */
int MacTdmaDama::command(int argc, const char*const* argv)
{
	if (argc == 2) 
	{
		if (strcmp(argv[1], "reset") == 0)
		{
			used_slots_ = 0;
			total_slots_ = 0;
			return TCL_OK;
		}
	} else
	if (argc == 3)
	{
		if (strcmp(argv[1], "log-target") == 0)
		{
			logtarget_ = (NsObject*) TclObject::lookup(argv[2]);
			if(logtarget_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
                if (strcmp(argv[1], "eventtrace") == 0)
                {
                        bod_log = (EventTrace *)TclObject::lookup(argv[2]);
                        if (!bod_log)
                                return TCL_ERROR;
                        return (TCL_OK);
                }
		if (strcmp(argv[1], "set-allocator") == 0)
		{
			allocator_ = (TdmaAllocator*) TclObject::lookup(argv[2]);	
			if (allocator_ == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
		if (strcmp(argv[1], "errormac") == 0)
		{
			em_ = (ErrorModel *)TclObject::lookup(argv[2]);
			if (!em_)
				return TCL_ERROR;
			return (TCL_OK);
		}
                if (strcmp(argv[1], "setmon") == 0)
                {
                        qMon_ = (Queue*)TclObject::lookup(argv[2]);
                        if (!qMon_)
                                return TCL_ERROR;

			qMon_->block();
                        return (TCL_OK);
                }
	}
	return Mac::command(argc, argv);
}



void MacTdmaDama::btp_decoding(Packet* p)
{
Packet* burst;
FramePDU* framep;
int cellno;
struct hdr_tdmadama *btp, *burst_hdr;
struct burst_des* bdes;
double offset;
int size;
Scheduler& s = Scheduler::instance();
int scount;
int i;

	btp = HDR_TDMADAMA(p);
	bdes = (struct burst_des*)(p->accessdata());
	
	scount = 0;
	for(i=0; i<btp->slot_num_; i++) {
		
		if ( bdes[i].assignment_id == slot_num_ ) {

			offset = bdes[i].offset;
			size = bdes[i].size;
			cellno = size/slot_packet_len_;

			/* Schedule timeslot */
			burst = Packet::alloc();
			framep = new FramePDU(PACKET_DATA, cellno); 
			burst->setdata(framep);

			burst_hdr = HDR_TDMADAMA(burst);
			burst_hdr->size_ = size;
			burst_hdr->cellno_ = cellno;

			s.schedule(&mhSlot_, burst, offset);
			scount++;	
		}
	}

        /* log CR transmission */
        if (bod_log) {
                sprintf(bod_log->buffer(), "%-3d %7.04lf BTP %5d",
                        slot_num_, NOW, scount);
                bod_log->dump();
        }

	Packet::free(p);

}



void MacTdmaDama::recv_control(Packet* p)
{
struct hdr_tdmadama* dh;

	dh = HDR_TDMADAMA(p);

	if ( dh->type() == TBTP ){ 

		btp_decoding(p);
		return;
	}

	if ( allocator_ && dh->type() == CR ) {

		allocator_->rcv_req(p);
		return;
	}
		
	Packet::free(p);

}


/* MAC entry point */
void MacTdmaDama::recv(Packet* p, Handler* h)
{
	struct hdr_cmn *ch = HDR_CMN(p);

	u_int32_t dst;
	struct hdr_mac* dh;
	dh = HDR_MAC(p);

	/* Incoming packets from phy layer, 
		send traffic UP to ll layer. 
		and process control messages */
	if (ch->direction() == hdr_cmn::UP)
	{

		dst = dh->macDA();

		if ( dh->ftype() == MF_CONTROL ) {
			recv_control(p);
			return;
		}

		if (dst != (u_int32_t) index_ && 
			dst != MAC_BROADCAST) {
			Packet::free(p);
			return;
		}

		sendUp(p);
		return;

	}

	/* Control packets sent immediately, 
	   no need to unlock IFQ.            */
	if ( dh->ftype() == MF_CONTROL ) {
		downtarget_->recv(p, this);
		return;
	}

	/* Packets coming down from ll layer (from ifq actually),
	   send them to phy layer. */
	callback_ = h;

	/* buffer packet */
	if (pktTx_ != NULL)
		printf("<%d> warning: buffer is NOT empty\n",index_);

	pktTx_ = p;

}


void MacTdmaDama::sendUp(Packet* pkt_)
{
struct hdr_tdmadama* burst;
Packet** pc;
int i;

	struct hdr_cmn *ch = HDR_CMN(pkt_);

	/* Now forward packet upwards. */
	ch->num_forwards() += 1;
	
	burst = HDR_TDMADAMA(pkt_);
	pc = ((FramePDU*)(pkt_->userdata()))->pks_;

	for(i=0; pc[i] != NULL; i++) {

		/* No need to destroy the inner fragments cause */
		/* this is done at the end to the function      */
		if ( HDR_MAC(pc[i])->macDA() != index_ ) 
			continue;

		HDR_CMN(pc[i])->direction() = hdr_cmn::UP;

		/* We need to send a refcopy because the fragment is about */
		/* to be destroyed at the end to the function              */
		uptarget_->recv(pc[i]->refcopy(), (Handler*) 0);
	}

	Packet::free(pkt_);
}

/* Actually send the packet. */
void MacTdmaDama::sendDown()
{

	/* Check if there is any packet buffered. */
	if (!pktTx_)
		return;

	downtarget_->recv(pktTx_, this);

	pktTx_ = NULL;

}


/* Handles the transmission of a packet in a timeslot */
void MacTdmaDama::slotHandler(Event *e)
{
Packet *pp, **pc;
FramePDU* fp;
int i, cn, totp;
struct hdr_tdmadama *burst;
	
	qMon_->unblock();

	pp = (Packet*) e;
	fp = (FramePDU*)pp->userdata();
	burst  = HDR_TDMADAMA(pp);
	HDR_MAC(pp)->macDA() = MAC_BROADCAST;

	pc = fp->pks_;
	cn = burst->cellno_;
	total_slots_ += burst->size_;

	totp = 0;
	for(i=0; i<cn; i++)
	{
		qMon_->resume();
		if (!pktTx_) 
			break;

		pc[i] = pktTx_;
		pktTx_ = NULL;
		totp++;
		used_slots_ += slot_packet_len_;
	}

	if (totp>0) {
		burst->cellno_ = totp;
		downtarget_->recv(pp, this);
	} else 
		Packet::free(pp);

	qMon_->block();
	return;

}



