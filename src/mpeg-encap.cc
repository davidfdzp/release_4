/* ULE/Mpeg2 encapsulator for MFTDMA-DAMA */

#include "mpeg-encap.h"

static class MpegLLClass : public TclClass
{
	public:
		MpegLLClass() : TclClass("LL/Mpeg") {}
		TclObject* create(int, const char*const*)
		{
			return (new MpegLL);
		}
} sat_class_mpeg_ll;

static class MPEGHeaderClass : public PacketHeaderClass
{
	public:
		MPEGHeaderClass(): PacketHeaderClass("PacketHeader/MPEG",
			sizeof(hdr_ll_mpeg))
		{
			bind_offset(&hdr_ll_mpeg::offset_);
		}
} class_hdr_ll_mpeg;

int MpegLL::cid_ = 0;
int hdr_ll_mpeg::offset_;

MpegLL::MpegLL() : SatLL()
{

	bind("limit_",&limit_);

	bind("id_",&id_);
	bind("sent_up_",&sent_up_);
	bind("sent_down_",&sent_down_);
	bind("pack_thresh",&pack_thresh);

	sent_up_ = 0;
	sent_down_ = 0;
	rx_len_ = 0;

	buffer_ = NULL;

	id_ = cid_++;

	tx_timer = new MpegTimer(this);

	for(int i=0; i<N_QUEUES; i++) {
		tx_buffer[i] = NULL;
		rx_buffer[i] = NULL;
	}

}


DvbPayload::~DvbPayload()
{
	int i;

	for(i=0; i<index_; i++)
		Packet::free(frag[i]);

}

AppData* DvbPayload::copy() 
{

	DvbPayload* na = new DvbPayload;
	na->index_ = index_;
	for(int i=0; i<index_; i++) { 
		na->frag[i] = frag[i]->refcopy();
		na->frag_size[i] = frag_size[i]; 
	}

	return na;

}

void DvbPayload::insert_frag(Packet* p, int fsize)
{

	frag[index_] = p->refcopy();
	frag_size[index_] = fsize;
	

	index_++;
}


int MpegLL::get_queue(Packet* buffer[], int addr)
{
	int k, l;
	
	k = l = ( addr % N_QUEUES );

	do {
		if (!(buffer[l]) || 
			HDR_MPEG(buffer[l])->addr_ == addr) 
		   return l;

		l = ( l + 1 ) % N_QUEUES;

	} while (k != l);

	printf("Not enough queues at Mpeg encaps\n");
	abort();

}

void MpegLL::dump(Packet* p)
{
	DvbPayload *dp;
	Packet *k;
	int i;
	int fl;
	struct hdr_cmn* ch = HDR_CMN(p);
	struct hdr_ll_mpeg* llh = HDR_MPEG(p);

	printf("%0.4lf, (%d) :: direction =%d, size() = %d, addr() = %d, pid_ = %d\n",
		NOW,
		id_,
		ch->direction(),
		llh->length(),
		llh->addr(),
		llh->pid_
	);

	/* show fragmensts */

	dp = (DvbPayload*)p->userdata();

	for(i=0; i<dp->index(); i++)
	{
		k = dp->frag[i];
		fl = dp->frag_size[i];
		printf("frag[%d] size=%d\n", i, fl);
	}

}


int MpegLL::command(int argc, const char*const* argv)
{
	if (argc == 2)
	{
		if (strcmp(argv[1], "reset") == 0)
		{
			reset();
			return (TCL_OK);
		}
	} else
	if (argc == 3)
        {

                if (strcmp(argv[1], "setmon") == 0)
                {
                        qMon_ = (Queue*)TclObject::lookup(argv[2]);
                        if (!qMon_)
                                return TCL_ERROR;
                        return (TCL_OK);
                }

	}

	return (SatLL::command(argc, argv));
}



void MpegLL::new_dvb(int queue, Packet* p)
{
	struct hdr_cmn* ch;
	struct hdr_ll_mpeg* mh;


	tx_buffer[queue] = p->copy();

	ch = HDR_CMN(tx_buffer[queue]);
	mh = HDR_MPEG(tx_buffer[queue]);

	ch->size() = 188;
	mh->pusi_ = 0;
	mh->addr_ = id_;
	mh->pid_ = id_;
	mh->status_ = 0;
	mh->queue_ = queue;
	mh->dvb_space_ = 184;

	payload = new DvbPayload();
	tx_buffer[queue]->setdata(payload);

}

void MpegLL::setpusi(Packet* p)
{

	struct hdr_ll_mpeg* mh;
	mh = HDR_MPEG(p);
	if (mh->pusi_ == 0) {
		mh->pusi_ = 1;
		mh->dvb_space_--;
	}
}


void MpegLL::send_frags(Packet* p)
{

	struct hdr_cmn* ch;
	struct hdr_ll_mpeg* mh;
	int psize;
	int queue;

	ch = HDR_CMN(p);
	ch->size() += ULE_HDR_AND_CRC;
	psize = ch->size();

	/* Select the queue */
	getRoute(p);
	nsaddr_t dst = ch->next_hop();

	queue = get_queue(tx_buffer, dst);

	if (!tx_buffer[queue]) {
		new_dvb(queue, p);
		setpusi(tx_buffer[queue]);
	}
	
	mh = HDR_MPEG(tx_buffer[queue]);
	mh->addr_ = dst;


	while (psize > mh->dvb_space_) {

		/* single fragment */
	        tx_timer->cancel(tx_buffer[queue]);
	        psize -= mh->dvb_space_;
	        payload->insert_frag(p,mh->dvb_space_);
	        sendDown(tx_buffer[queue]);

		/* empty buffer and preparing new cell */
		tx_buffer[queue] = NULL;
	        new_dvb(queue, p);
		mh = HDR_MPEG(tx_buffer[queue]);
		mh->addr_ = dst;

	}
	
	payload->insert_frag(p, psize);
	mh->dvb_space_ -= psize;
	
	if ( mh->dvb_space_ < ULE_HDR + ((mh->pusi_==0)?1:0)) {
	
	        tx_timer->cancel(tx_buffer[queue]);
	        sendDown(tx_buffer[queue]);
		tx_buffer[queue] = NULL;
	
	} else {
	
	        if (mh->status_ == 0) {
			mh->status_ = 1;
	                tx_timer->start(tx_buffer[queue], pack_thresh);
		}
	
	}


}

void MpegTimer::handle(Event *e) {
	dvbp->handle_timer((Packet*)e);
}


void MpegLL::recv(Packet* p, Handler* /*h*/)
{

	hdr_cmn *ch = HDR_CMN(p);

	assert(initialized());

	// If direction = UP, then pass it up the stack
	// Otherwise, set direction to DOWN and pass it down the stack
	if(ch->direction() == hdr_cmn::UP)
	{

		//dump_buffer();

		uptarget_ ? sendUp(p) : drop(p);
		return;
	}

	sent_down_++;
	ch->direction() = hdr_cmn::DOWN;

	send_frags(p);

}


/* restarting agent */
void MpegLL::reset()
{
	Packet *p,*pp;

	p = buffer_;
	while( p != NULL )
	{
		pp = p->next_;
		drop(p);
		p = pp;
	}
	buffer_ = NULL;

	sent_up_=0;
	sent_down_=0;

}


void MpegLL::sendDown(Packet* p)
{
	SatChannel* satchannel_;
	hdr_cmn *ch = HDR_CMN(p);

	char *mh = (char*)p->access(hdr_mac::offset_);
	int peer_mac_;


	if ( qMon_->length() > limit_ ) {
		Packet::free(p);
		return;
	}
		
	//DvbPayload dp = p->userdata();	


	getRoute(p);


	// Set mac src, type, and dst
	mac_->hdr_src(mh, mac_->addr());

	// We'll just use ETHERTYPE_IP
	mac_->hdr_type(mh, ETHERTYPE_IP);

	((hdr_mac*)mh)->ftype() = MF_DATA;


	nsaddr_t dst = ch->next_hop();


	// a value of -1 is IP_BROADCAST
	if (dst < -1)
	{
		printf("Error:  next_hop_ field not set by routing agent\n");
		abort();
	}

	switch(ch->addr_type())
	{

		case NS_AF_INET:
		case NS_AF_NONE:
			if (IP_BROADCAST == (u_int32_t) dst)
			{
				mac_->hdr_dst((char*) HDR_MAC(p), MAC_BROADCAST);
				break;
			}
			/* 
			 * Here is where arp would normally occur.  In the satellite
			 * case, we don't arp (for now).  Instead, use destination
			 * address to find the mac address corresponding to the
			 * peer connected to this channel.  If someone wants to
			 * add arp, look at how the wireless code does it.
			 */
			// Cache latest value used
			if (dst == arpcachedst_)
			{
				mac_->hdr_dst((char*) HDR_MAC(p), arpcache_);
				break;
			}
			// Search for peer's mac address (this is the pseudo-ARP)
			satchannel_ = (SatChannel*) channel();
			peer_mac_ = satchannel_->find_peer_mac_addr(dst);
			if (peer_mac_ < 0 )
			{
				printf("Error:  couldn't find dest mac on channel ");
				printf("for src/dst %d %d at NOW %f\n",
					ch->last_hop_, dst, NOW);
				abort();
			}
			else
			{
				mac_->hdr_dst((char*) HDR_MAC(p), peer_mac_);
				arpcachedst_ = dst;
				arpcache_ = peer_mac_;
				break;
			}

		default:
			printf("Error:  addr_type not set to NS_AF_INET or NS_AF_NONE\n");
			abort();
	}

	
	Scheduler& s = Scheduler::instance();
	s.schedule(downtarget_, p, delay_);

}



void MpegLL::handle_timer(Packet* p)
{
	 int queue;
	 struct hdr_ll_mpeg* mh;

	 mh = HDR_MPEG(p);
	 
	 queue = mh->queue_; 

         sendDown(tx_buffer[queue]);
	 tx_buffer[queue] = NULL;


}


void MpegLL::sendUp(Packet* p)
{
	DvbPayload *dp;
	Packet *inp;
	int frag_len, pid;
	int i;
	int queue;

	struct hdr_cmn *ch;
	struct hdr_ll_mpeg *mh;

	dp = (DvbPayload*)p->userdata();

	/* find rx queue */
	pid = HDR_MPEG(p)->pid_;

	queue = get_queue(rx_buffer, pid);

	/* process fragments */
	for(i=0; i<dp->index(); i++)
	{
		inp = dp->frag[i];
		frag_len = dp->frag_size[i];

		if  ( rx_buffer[queue] && 
			HDR_CMN(rx_buffer[queue])->uid_ == HDR_CMN(inp)->uid_ ) {

			mh = HDR_MPEG(rx_buffer[queue]);

		} else {

			if (rx_buffer[queue])
				Packet::free(rx_buffer[queue]);

			rx_buffer[queue] = inp->copy();
			mh = HDR_MPEG(rx_buffer[queue]);
			mh->length_ = 0;

		}

		mh->length_ += frag_len;
		mh->addr_ = pid;

		ch = HDR_CMN(rx_buffer[queue]);

		if ( ch->size() == mh->length_ ) {

			ch->size() -= ULE_HDR_AND_CRC;
			Scheduler& s = Scheduler::instance();

			s.schedule(uptarget_, rx_buffer[queue], 0.0);
			rx_buffer[queue] = NULL;
		}
	

	}


	Packet::free(p);
	return;	

}

