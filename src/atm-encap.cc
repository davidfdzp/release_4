/* ATM/AAL5 multiprotcol encapsulator (RFC 2684) for MFTDMA-DAMA
 * This code considers the VC encapsulation (Sec 3) rather than LLC */

#ifndef ns_atm_encap_h
#define ns_atm_encap_h

#include "atm-encap.h"

static class AtmLLClass : public TclClass
{
	public:
		AtmLLClass() : TclClass("LL/Atm") {}
		TclObject* create(int, const char*const*)
		{
			return (new AtmLL);
		}
} sat_class_atm_ll;


static class ATMHeaderClass : public PacketHeaderClass
{
	public:
		ATMHeaderClass(): PacketHeaderClass("PacketHeader/ATM",
			sizeof(hdr_ll_atm))
		{
			bind_offset(&hdr_ll_atm::offset_);
		}
} class_hdr_ll_atm;



int AtmLL::cid_ = 0;
int hdr_ll_atm::offset_;

AtmLL::AtmLL() : SatLL()
{

	bind("id_",&id_);
	bind("sent_up_",&sent_up_);
	bind("sent_down_",&sent_down_);

	sent_up_=0;
	sent_down_=0;
	buffer_ = NULL;
	id_ = cid_++;

}


void AtmLL::dump(Packet* p )
{
	struct hdr_cmn* ch = HDR_CMN(p);
	struct hdr_ll_atm* llh = HDR_ATM(p);

	printf("%0.4lf, (%d) :: direction =%d, size() = %d, addr() = %d,"
		"pt() = %d\n",
		NOW,
		id_,
		ch->direction(),
		llh->length(),
		llh->addr(),
		llh->pt()
		);
}


int AtmLL::command(int argc, const char*const* argv)
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


void AtmLL::send_frags(Packet* p)
{
	Packet* pp;
	struct hdr_cmn* ch;
	struct hdr_cmn* fch;
	struct hdr_ll_atm* ah;
	struct hdr_ll_atm* fah;
	struct hdr_ip* ip;
	int frags;
	int psize;
	int i;

	ch = HDR_CMN(p);
	ah = HDR_ATM(p);
	ip = HDR_IP(p);

	psize = ch->size() + ATM_TRL_LEN;

	frags = (psize + 47) / 48;

        if ( qMon_->length() + frags >= qMon_->limit() ) {
		qMon_->drop(p);
                return;
        }

	sent_down_++;

	for( i=0; i<frags; i++ )
	{

		pp = p->copy();

		fch = HDR_CMN(pp);
		fah = HDR_ATM(pp);

		fch->size() = 53;
		fah->length() = ch->size();
		fah->addr() = id_ * ATM_PRIO_NO + ip->flowid();
		fah->pt() = 0;

		/* last ATM */
		if ( i == frags - 1 )
			fah->pt() = 1;

		sendDown(pp);
	}

	Packet::free(p);

}


void AtmLL::dump_buffer()
{
	struct hdr_ll_atm* ah;

	printf("%d %.4lf: ", id_, NOW );

	for(Packet* p=buffer_; p != NULL; p=p->next_)
	{
		ah = HDR_ATM(p);
		printf("[%d %d]", ah->addr(), ah->pt());
	}

	printf("\n");

}


void AtmLL::recv(Packet* p, Handler* /*h*/)
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

	ch->direction() = hdr_cmn::DOWN;

	send_frags(p);

}


/* restarting agent */
void AtmLL::reset()
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


void AtmLL::sendDown(Packet* p)
{
	hdr_cmn *ch = HDR_CMN(p);

	char *mh = (char*)p->access(hdr_mac::offset_);
	int peer_mac_;
	SatChannel* satchannel_;

	getRoute(p);

	// Set mac src, type, and dst
	mac_->hdr_src(mh, mac_->addr());
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

	downtarget_->recv(p);

}


void AtmLL::sendUp(Packet* p)
{
	Packet *q, *qq;
	struct hdr_ll_atm *ah, *qh;
	int paddr;
	int len,frags;

	Scheduler& s = Scheduler::instance();

	ah = HDR_ATM(p);

	if ( ah->pt() == 0 )
	{

		/* PT==0  => enqueue */
		p->next_ = buffer_;
		buffer_ = p;
		return;

	}

	/* PT==1 => dequeue */
	paddr = ah->addr();
	len = 1;
	q = buffer_;
	qq = NULL;

	/* dequeue */
	while( q != NULL )
	{

		qh = HDR_ATM(q);

		if ( qh->addr() == paddr ) {
			len++;
			q = q->next_;

			if ( qq == NULL ) {
				drop(buffer_);
				buffer_ = q;
			} else {
				drop(qq->next_);
				qq->next_ = q;
			}
			continue;
		}

		qq = q;
		q = q->next_;

	}

	frags = (ah->length() + ATM_TRL_LEN + 47) / 48;

	/* packet complete => send it up */
	if( len == frags )
	{
		sent_up_++;
		HDR_CMN(p)->size() = ah->length();
		s.schedule(uptarget_, p, 0.0);
		return;

	}

	/* This happens if a previous PT==1 was lost */
	drop(p);

}
#endif							 /* ns_atm_encap_h */
