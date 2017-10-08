#include <limits.h>
#include "phy-tdmadama.h"
#include "satnode.h"
#include "sattrace.h"
#include "mac-rle.h"

static class TdmaDamaPhyClass: public TclClass {
public:
	TdmaDamaPhyClass() : TclClass("Phy/TdmaDama") {}
	TclObject* create(int, const char*const*) {
		return (new TdmaDamaPhy);
	}
} class_TdmaDamaPhy;


TdmaDamaPhy::TdmaDamaPhy(){

	drop_burst = 42;
	current_burst = 0;
	bind("bdrop_rate_", &bdrop_rate_);

}

void TdmaDamaPhy::sendDown(Packet *p)
{
#if 0
RlePayload* rpp;
frag_hdr* fh;
int count;

	current_burst++;

	printf("current_burst %d %x\n", current_burst, (int)this);

	if ( current_burst % 50 == drop_burst ) {

		rpp = (RlePayload*)p->userdata();
		if (!rpp)
			return;
		count = 0;
        	for(fh = rpp->head; fh!=NULL; fh=fh->next)
			count++;
			
		printf("COUNT +%d %x %lf\n", count, (int)this, NOW);

		Packet::free(p);
		return;

	}
#endif

	//printf("PHY RCV CELL PACKET %x SIZE %d\n", (int)pc[0], HDR_CMN(pc[0])->size());
	// printf("PHY RCV CELL PACKET %x SIZE %d\n", (int)pc[0], HDR_TDMADAMA(p)->size_);


	if ((double(RNG::defaultrng()->next())/INT_MAX)<bdrop_rate_) {
		Packet::free(p);
		return;
	}

	if (channel_)
		channel_->recv(p, this);
	else {
		// it is possible for routing to change (and a channel to
		// be disconnected) while a packet
		// is moving down the stack.  Therefore, just log a drop
		// if there is no channel
		if ( ((SatNode*) head()->node())->trace() )
			((SatNode*) head()->node())->trace()->traceonly(p);
		Packet::free(p);
	}
}

// Note that this doesn't do that much right now.  If you want to incorporate
// an error model, you could insert a "propagation" object like in the
// wireless case.
int TdmaDamaPhy::sendUp(Packet * /* pkt */)
{
	return TRUE;
}

int
TdmaDamaPhy::command(int argc, const char*const* argv) {
	if (argc == 2) {
	} else if (argc == 3) {
		TclObject *obj;

		if( (obj = TclObject::lookup(argv[2])) == 0) {
			fprintf(stderr, "%s lookup failed\n", argv[1]);
			return TCL_ERROR;
		}
	}
	return Phy::command(argc, argv);
}



