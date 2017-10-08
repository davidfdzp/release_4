
/* MAC block for MFTDMA-DAMA - University of Aberdeen 2009 */

#include "mac-rle.h"
#include "rle-encap.h"


static class RleHeaderClass : public PacketHeaderClass
{
	public:
		RleHeaderClass(): PacketHeaderClass("PacketHeader/RleBurst",
			sizeof(hdr_rleburst))
		{
			bind_offset(&hdr_rleburst::offset_);
		}
} class_hdr_rle;

int hdr_rleburst::offset_;


static class MacRleClass : public TclClass
{
	public:
		MacRleClass() : TclClass("Mac/Rle") {}
		TclObject* create(int, const char*const*)
		{
			return (new MacRle());
		}
} class_mac_rle;


RlePayload::~RlePayload()
{
frag_hdr *hf, *nf;

	hf=head;
	while( hf != NULL ) {
                Packet::free(hf->alpdu);
		nf = hf->next;
		delete hf;
		hf = nf;
	}
}

AppData* RlePayload::copy()
{
	frag_hdr *hf, *nf, **hfp;

        RlePayload* na = new RlePayload;

	hfp = &(na->head);	

        for(hf=head; hf!=NULL; hf=hf->next) {

		nf = new frag_hdr;
		nf->start_end = hf->start_end;
		nf->length = hf->length;
		nf->frag_id = hf->frag_id;
		nf->seqno = hf->seqno;
		
		nf->alpdu = hf->alpdu->refcopy();
	
		*hfp = nf;
		hfp = &(nf->next);

        }

	*hfp = NULL;

        return na;

}

frag_hdr* RlePayload::new_frag(Packet* p)
{
frag_hdr *hf;

	hf = new frag_hdr;
	hf->next = NULL;
	hf->alpdu = p->refcopy();
	hf->seqno = 0;

	if (last) {
		last->next = hf;
		last = hf;
		 
	} else {
		head = last = hf;
	}

	return hf;

}



// Mac Tdma definitions
MacRle::MacRle() : MacTdmaDama()
{
int i;

	bind("used_slots_",&used_slots_);
	bind("total_slots_",&total_slots_);
	bind("sent_up_", &sent_up_);
	bind("sent_down_", &sent_down_);

	for(i=0; i<8; i++)
		next[i] = 0;

	for(i=1; i<MAX_CNTS; i++) {
		cbuffer[i].next = &cbuffer[i-1];
		cbuffer[i].head = NULL;
		cbuffer[i].pkt = NULL;
	}

	cbuffer[0].head = NULL;
	cbuffer[0].next = NULL;
	cbuffer[0].pkt = NULL;

	free = &cbuffer[MAX_CNTS-1];


}

void MacRle::reset() 
{
	// code to reset the agent
	used_slots_ = 0;
	total_slots_ = 0;
	sent_up_ = 0;
	sent_down_ = 0;
	
}

int MacRle::command(int argc, const char*const* argv)
{
	if (argc == 2) {
		if (strcmp(argv[1], "reset") == 0) {
			reset();
			return (TCL_OK);
		}
	}

	return (MacTdmaDama::command(argc, argv));
}

/* This function generates a new burst and transmits */
void MacRle::slotHandler(Event*e)
{
Packet *bp, *qq;
struct hdr_rleburst* burst;
struct hdr_ll_rle* qh;
struct frag_hdr* fh;
RlePayload* pload;

unsigned bs;

	bp = (Packet *)e; 

	pload = new RlePayload();
	bp->setdata(pload);
	burst = HDR_RLE_BURST(bp);
	HDR_MAC(bp)->macSA() = slot_num_;
	HDR_MAC(bp)->macDA() = MAC_BROADCAST;

	bs = HDR_TDMADAMA((Packet*)e)->size_;

	burst->size = bs;
	total_slots_ += bs;

	do {

		if (!(qq = QueueIface::head(qMon_)))
			break;

		qh = HDR_RLE(qq);
		
		if ( qh->psize + FHS > bs) {

			/* Too big for this frame */
			fh = pload->new_frag(qq);

			if (qh->start_end & 0x02) {
				qh->total_length += PHS + TS;
				qh->psize = qh->total_length;
			}	

			qh->start_end &= 0x02;

			fh->start_end = qh->start_end; 
			fh->frag_id = qh->frag_id;
			fh->length = bs - FHS;

			qh->start_end &= 0x01;		
			qh->psize -= fh->length;
			bs = 0;
			break;
		}


		/* Packet fits the burst */
		bs -= qh->psize + FHS;
		fh = pload->new_frag(qq);

		qh->start_end |= 0x01;

		fh->start_end = qh->start_end;
		fh->frag_id = qh->frag_id;
		fh->length = qh->psize;

		if (fh->start_end == 0x01) {
			fh->seqno = next[qh->frag_id];
			next[qh->frag_id]++;
		}

		sent_down_++;
		QueueIface::remove(qMon_, qq);
		//Packet::free(qq);

	} while ( bs >= FHS + PHS );

	if (pload->head) { 
		//printf("sending burst\n");
		used_slots_ += burst->size - bs;
		downtarget_->recv(bp, this);
	} else 
		Packet::free(bp);


}

context* MacRle::get_context(int src, int fid)
{
int k;
context *cp;

	/* hash function */
	k = ((src<<3) + fid) % MAX_CNTS;
	cp = cbuffer[k].head;

	while( cp != NULL 
		&& !(cp->src == src) 
		&& !(cp->fid == fid) )
		cp = cp->next;

	if ( cp == NULL ) {

		if (!free) {
			fprintf(stderr, "MacRle::get_context: ");
			fprintf(stderr, "too many contexts\n");
			abort();
		}

		cp = free;
		free = free->next;
		cp->exp_sn = 0;
		cp->src = src;
		cp->fid = fid;

		cp->next = NULL;
		cbuffer[k].head = cp;
		
	}
	
	return cp;

}


void MacRle::recv(Packet* p, Handler* h)
{
	struct hdr_cmn *ch = HDR_CMN(p);

	u_int32_t dst;
	struct hdr_mac* dh;
	dh = HDR_MAC(p);

	/* Incoming packets from phy layer, 
		send traffic UP to ll layer. 
		and process control messages */
	if (ch->direction() == hdr_cmn::UP) {
	
		dst = dh->macDA();

		if ( dh->ftype() == MF_CONTROL ) {
			recv_control(p);
			return;
		}

		if (dst != (u_int32_t) index_ && 
			dst != MAC_BROADCAST)
		{
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
	
	Packet::free(p);

}

/* Receiver */
void MacRle::sendUp(Packet* p) 
{

struct frag_hdr* fh;
struct context* cp;
struct hdr_mac* hm;
struct hdr_ll_rle* rp;
RlePayload* rpp;

	// printf("\n\n\ndecoding burst\n");
	int measp = 0;	
	rpp = (RlePayload*)p->userdata();
	hm = HDR_MAC(p);

	for(fh = rpp->head; fh!=NULL; fh=fh->next) {

		if ( HDR_MAC(fh->alpdu)->macDA() != index_ )
			continue;

		struct hdr_cmn* ch = HDR_CMN(fh->alpdu);

		//printf("%lf FRAG from source %d - len %d - seqno %d - ch->uid()=%d fh->frag_id=%d\n", 
		// 	NOW, hm->macSA(), fh->length, fh->seqno, ch->uid(), fh->frag_id);

		measp += fh->length + 2;

		if ( fh->start_end == 0x03 ) {
			/* Don't need to create a Context for it */
			/* Just send the packet UP */

			ch = HDR_CMN(fh->alpdu);
			ch->direction() = hdr_cmn::UP;
			ch->num_forwards() += 1;
			
			// printf("%lf packet sent up (NO CONTEXT)\n", NOW);
			sent_up_++;
			uptarget_->recv(fh->alpdu->refcopy(), (Handler*) 0);
			continue;		
		}

		rp = HDR_RLE(fh->alpdu);
		cp = get_context(hm->macSA(), fh->frag_id);
		
		/* the fragment is the first */
		if (fh->start_end & 0x02) {

			/* remove any previous buffered */
			if (cp->pkt) { 
				Packet::free(cp->pkt);
				cp->exp_sn++;
			}	

			cp->pkt = fh->alpdu->refcopy();
			cp->len = 0;
			//printf("resetting\n");
		}

		cp->len += fh->length;

		/* the fragment is the last */
		if (fh->start_end & 0x01) {

			//printf("%lf FRAG islast LEN %d %d SEQNO cp->exp_sn=%d fh->seqno=%d\n", 
			// NOW, cp->len, rp->total_length, cp->exp_sn, fh->seqno);

			/* length */
			if ( !(cp->len == rp->total_length &&
				cp->exp_sn == fh->seqno)) {
				
				//printf("%lf packet dropped\n", NOW);
				cp->exp_sn = fh->seqno + 1;
				if (cp->pkt) {
					Packet::free(cp->pkt);
					cp->pkt = NULL;
				}
				continue;
			}

			ch = HDR_CMN(cp->pkt);
			ch->direction() = hdr_cmn::UP;
			ch->num_forwards() += 1;
		
			// printf("%lf packet sent up\n", NOW);
			sent_up_++;
			uptarget_->recv(cp->pkt, (Handler*) 0);
	
			cp->len = 0;
			cp->pkt = NULL;
			cp->exp_sn++;
			//free_context(cp);
		}	
	}

	Packet::free(p);

	//printf("totalp = %d\n\n\n", measp);
}


