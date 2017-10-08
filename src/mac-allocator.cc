/* Raffaello Secchi - University of Aberdeen (UK) */

#include "mac-allocator.h"

static class AllocatorMFTDMAClass : public TclClass
{
	public:
		AllocatorMFTDMAClass() : TclClass("Allocator/MFTDMA") {}
		TclObject* create(int, const char*const*)
		{
			return (new AllocatorMFTDMA());
		}
} class_allocator_mftdma;

void PollAllocator::expire(Event* e)
{
	allocator_->allocation();
	resched(allocator_->frame_duration_);
}


char AllocatorMFTDMA::wrk[1024];

/* Reinitialize Allocator new nodes are created */
void TdmaAllocator::init()
{
int i;

	for(i=0; i<MAX_TERM; i++)
		requestv_[i] = 0.0;

	active_node_ = mac_->active_node_;

}


TdmaAllocator::TdmaAllocator() :  Connector(), poll_(this)
{
	bind("hub_",&hub_);
	bind("active_node_",&active_node_);
	bind("bytes_superframe_",&bytes_superframe_);
	bind("frame_duration_",&frame_duration_);
	bind("slot_time_",&slot_time_);

	poll_.sched(0.0);

}


void AllocatorMFTDMA::printreq()
{

	printf("Request Table: %.4lf\n", NOW);
	for(int i=0; i<active_node_ ; i++ )
		printf("%.3lf RBDC=%d VBDC=%d\n", 
			NOW, reqv_rate[i], reqv_volume[i]);

	printf("\n---------------------------\n\n\n");

}



void Frame::reset()
{
	int i,j;

	for(i=0; i<carriers_; i++)
		for(j=0; j<timeslots_; j++)
			frame[i][j] = NOTHING_TO_SEND;

}


void Frame::draw(BaseTrace *bt, 
int xs, int ys, int xi, int yi)
{
	char *fp;
	int i, j, k;
	int x0, x1, y0, y1;
	double w, h;
	double R, G, B;

	w = double(xs-xi)/timeslots_;
	h = double(ys-yi)/carriers_;
	fp = bt->buffer();

	for(i=0; i<carriers_; i++)
	{
		for(j=0; j<timeslots_; j++)
		{

			x0 = w*(j+1) + xi;
			y0 = h*(i+1) + yi;
			x1 = w*j + xi;
			y1 = h*i + yi;

			if (frame[i][j] != NOTHING_TO_SEND)
			{
				k = (frame[i][j]*19+18) % 31;
				R = 1-.333*(k & 0x0003);
				B = 1-.333*((k & 0x000C) >> 2);
				G = 1-.333*((k & 0x0030) >> 3);

				sprintf(fp, "%.03lf %.3lf %.03lf %d %d %d %d BoxF\n",
					R, B, G, x0-marg, y0+marg, x1+marg, y1-marg);
				bt->dump();

				sprintf(fp, "%d %d moveto\n",
					int(x1+w/2-5), int(y1+h/2-5));
				bt->dump();
				sprintf(fp,"(%d) show\n", frame[i][j]);
				bt->dump();
			}

			sprintf(fp, "%d %d %d %d Box\n",
				x0-marg, y0+marg, x1+marg, y1-marg);
			bt->dump();

		}
	}
}


AllocatorMFTDMA::AllocatorMFTDMA(): TdmaAllocator()
{
int i;

	deficit = NULL;
	slot_index = 0;
	cur_term = 0;

	bind("utilization_", &utilization_);
	bind("utility_", &utility_);
	bind("feedback_", &feedback_);
	bind("tolerance_", &tolerance_);
	bind("load_", &load_);
	bind("testing_", &testing_);
	bind("slot_count_", &slot_count_);
	bind("f_count_", &nf);
	bind("layout_", &layout_);
	bind("mode_", &mode_);
	bind("forget_debit_", &forget_debit_);
	bind("hwin_", &hwin_);
	bind("wwin_", &wwin_);
	bind("sl_threshold_", &sl_threshold_);
	bind("max_buf_req_", &max_req_buf_);

	masksize = 1;

	bs = 1;
	ns = 6;

	page = 0;
	bt = NULL;

	for(i=0; i<MAX_TERM; i++) {
		cnst_rate[i] = 0.0;
		reqv_rate[i] = 0.0;
		reqv_volume[i] = 0.0;
	}

}


void AllocatorMFTDMA::init()
{

	if (deficit)
		delete[] deficit;

	active_node_ = mac_->active_node_;

	deficit = new int[active_node_];
	for(int i=0; i<active_node_; i++)
		deficit[i] = 0;

	TdmaAllocator::init();
}


int AllocatorMFTDMA::command(int argc, const char*const* argv)
{
	int t,f,r;
	double h;
	char buffer[4096];

	if (argc == 2)
	{
		if (strcmp(argv[1], "draw-superframe") == 0)
		{
			trace(NULL, 0);
			return TCL_OK;
		} else
		if (strcmp(argv[1], "get-requests") == 0)
		{
			Tcl* tcl = &(Tcl::instance());
			t = 0;
			for( r=0; r<active_node_; r++)
				if ( hub_ != r )
					t += sprintf(buffer+t, "%.3lf ", requestv_[r]);
			tcl->evalf("%s set requestv {%s}\n",name(), buffer);
			return TCL_OK;

		}
	}
	else if (argc == 3)
	{
		if (strcmp(argv[1], "attach") == 0)
		{
			mac_ = (MacTdmaDama*) TclObject::lookup(argv[2]);
			if(mac_ == 0)
				return TCL_ERROR;
			init();
			return TCL_OK;
		} else
		if (strcmp(argv[1], "trace-file") == 0)
		{

			bt = (BaseTrace*) TclObject::lookup(argv[2]);
			if(bt == 0)
				return TCL_ERROR;
			return TCL_OK;
		}
	} else if (argc == 4)
	{
		if (strcmp(argv[1], "cra_") == 0)
		{
			t = atoi(argv[2]);
			if ( t < 0 || t > MAX_TERM )
				return TCL_ERROR;
			
			h = atof(argv[3]); 

			if ( h < 0 )
				return TCL_ERROR;

			cnst_rate[t] = frame_duration_*h*125;	
			return TCL_OK;
		}
	}
	else if (argc == 5)
	{
		if (strcmp(argv[1], "add-new-frame") == 0)
		{

			/* initialise a new frame*/
			sf[nf].carriers_ = atoi(argv[2]);
			sf[nf].timeslots_ = atoi(argv[3]);
			sf[nf].burstsize_ = atoi(argv[4]);
			sf[nf].slots = sf[nf].carriers_ * sf[nf].timeslots_;
			masksize *= sf[nf].timeslots_;
			sf[nf].reset();
			nf++;

			if (masksize > MSK_LEN*8) {
				fprintf(stderr, "superframe too complex (masksize exceeded)\n");
				return TCL_ERROR;
			}


			return TCL_OK;
		} else
		if (strcmp(argv[1], "new-rule") == 0)
		{

			/* initialise a new rule */
			t = atoi(argv[2]);
			f = atoi(argv[4]);
			r = tr[t].rulenum;
			tr[t].rules[r].rule = atoi(argv[3]);
			tr[t].rules[r].fp = &sf[f];
			tr[t].id_ = t;

			tr[t].rulenum++;

			return TCL_OK;
		}
	}

	return TdmaAllocator::command(argc, argv);

}

Terminal::Terminal()
{
	rulenum = 0;
}

void Frame::print()
{
	int i,j;

	for(i=0; i<carriers_; i++)
	{
		for(j=0; j<timeslots_; j++)
			printf("[%2d]",frame[i][j]);
		printf("\n");
	}

}


double AllocatorMFTDMA::firstfit(Frame* fp, Terminal* tp)
{
	int i,j;

	for(i=0; i<fp->carriers_; i++)
	{
		for(j=0; j<fp->timeslots_; j++)
		{
			if (fp->frame[i][j] == NOTHING_TO_SEND &&
				tp->testmask(j, masksize/fp->timeslots_) )
			{
				fp->frame[i][j] = tp->id_;
				tp->setmask(j, masksize/fp->timeslots_);
				return double(j)*frame_duration_/fp->timeslots_;
			}
		}
	}

	return -1;

}


double AllocatorMFTDMA::bestfit(Frame* fp, Terminal* tp)
{
	int i,j;
	int i_opt, j_opt;

	i_opt = -1;
	j_opt = masksize;

	for(i=0; i<fp->carriers_; i++)
	{
		for(j=0; j<fp->timeslots_; j++)
		{

			if (fp->frame[i][j] == NOTHING_TO_SEND &&
				tp->testmask(j, masksize/fp->timeslots_) )
			{
				if ( j < j_opt )
				{
					i_opt = i;
					j_opt = j;
				}
			}
		}
	}

	if ( i_opt >= 0 )
	{
		fp->frame[i_opt][j_opt] = tp->id_;
		tp->setmask(j_opt, masksize/fp->timeslots_);
		return double(j_opt)*frame_duration_/fp->timeslots_;
	}
	return -1;

}

void AllocatorMFTDMA::allocator_report()
{
int i;

	feedback_ = 0;
	utility_ = 0;
	for(i=0; i<active_node_; i++)
	{
		if ( hub_ != i )		 //ignore hub
		{
			if ( deficit[i] <= 0 )
			{
				/* the terminal is happy */
				feedback_++;
				utility_++;
			}
			else
			{
				/* evaluating residual request */
				utilization_ -= deficit[i];
				if ( tolerance_*(deficit[i]+requestv_[i]) <= a[i])
					utility_++;
			}
		}
	}

	/* throughput */
	utilization_ += load_;

	if ( feedback_ >= (active_node_ - 1))
		feedback_ = 1;
	else
		feedback_ = 0;


}


void AllocatorMFTDMA::rcv_req(Packet* p) 
{
struct hdr_tdmadama* dh;
int slot_num;

	dh = HDR_TDMADAMA(p);
	slot_num = dh->slot_num();
	reqv_rate[slot_num] = dh->rate() * frame_duration_ * 125;
	reqv_volume[slot_num] += dh->volume();
	Packet::free(p);
	
}

void AllocatorMFTDMA::init_request_queues(unsigned* req_vec)
{
int i;

	for(i=0; i<active_node_; i++)
	{
		utilization_ += deficit[i];
		deficit[i] += req_vec[i];
		load_ += req_vec[i];
	}

	left_to_check = active_node_;
	/* start from a random terminal */
	// cur_term = RNG::defaultrng()->next() % active_node_;

}

void AllocatorMFTDMA::assign_timeslot(Packet* p)
{
Frame* frame;
Terminal* tp;
int i, t;
double offset;
struct burst_des* bdes;
struct hdr_tdmadama* btp;

	btp = HDR_TDMADAMA(p);
	t = btp->slot_num_;
			
	/* build assingnment in TBTP */
	bdes = (struct burst_des*)(p->accessdata());

	while( freeslots && left_to_check ) {

		/* the superframe has slots free.
		   If current terminal is satisfied,
		   look for another terminal.       */
		if ( deficit[cur_term]<=0) {
			/* search for the next positive request */
			do
			{
				left_to_check--;
				cur_term = (cur_term + 1) % active_node_;
			} while(deficit[cur_term] <=0 && left_to_check);

			/* terminal not found (the algorithm stops)*/
			if (!left_to_check)
				break;

		}

		/* found the terminal, now try allocating */
		tp = &tr[cur_term];		 // this is the terminal

		/* Seeking for a frame in the rule list */
		for(i=0; i<tp->rulenum; i++) {

			/* the SMALL rule */
			if (tp->rules[i].rule == 1) {
				if (deficit[cur_term] > sl_threshold_)
					continue;
			}

			/* try this framea */
			frame = tp->rules[i].fp;

			/* now tries allocation */
			if ( layout_ == FIRST_FIT &&
				(offset = firstfit(frame, tp))>=.0)
				break;

			if ( layout_ == BEST_FIT &&
				(offset = bestfit(frame, tp))>=.0)
				break;

			frame = NULL;

		}

		if ( frame ) {

			/* found a positive match */
			freeslots--;
			deficit[cur_term] -= frame->burstsize_;
			bdes[t].assignment_id = cur_term;
			bdes[t].size = frame->burstsize_;
			bdes[t].offset = offset;
			t++;
			a[cur_term] += frame->burstsize_;

			/* one slot allocated the cycle restart */
			if ( mode_ == SLOT_MODE )
				left_to_check = active_node_;
		}
		else {
			/* no luck, next customer please ... */
			if ( mode_ == CONT_MODE ) {
				left_to_check--;
				cur_term = (cur_term + 1) % active_node_;
			}
		}

		/* in SLOT mode go to the next */
		if ( mode_ == SLOT_MODE )
			cur_term = (cur_term + 1) % active_node_;

	}

	/* TBTP ready */
	btp->slot_num_ = t;

}


void AllocatorMFTDMA::reset_allocator()
{
int i;

	for(i=0; i<active_node_; i++)
		a[i] = 0.0;

	for(i=0; i<nf; i++)
		sf[i].reset();

	utilization_ = load_ = 0.0;

	for(i=0; i<active_node_; i++)
		tr[i].reset(masksize);

	freeslots = slot_count_;


}

void AllocatorMFTDMA::allocation()
{
int i;
char buf[1000];
struct hdr_tdmadama* btp;
Packet* p;

	/* create BTP packet */
        p = Packet::alloc(sizeof(struct burst_des)*MAX_SLOTS);

        btp = HDR_TDMADAMA(p);
       	btp->type() = TBTP; 
	btp->time() = NOW;
	btp->slot_num_ = 0;
        HDR_MAC(p)->ftype() = MF_CONTROL;

	/* Init Allocator */
	reset_allocator();

	/* Allocate CRA */
	init_request_queues(cnst_rate);
	assign_timeslot(p);

	/* Allocate RBDC */
	init_request_queues(reqv_rate);
	assign_timeslot(p);

	/* Allocate VBDC */
	init_request_queues(reqv_volume);
	for(i=0; i<active_node_; i++)
		reqv_volume[i] = 0.0;
	assign_timeslot(p);

	/* congestion feedback */
	allocator_report();

	/* Do not carry forward requests > max_req_buf_ */
	for(i=0; i<active_node_; i++)
		if ( deficit[i] > max_req_buf_ )
			deficit[i] = max_req_buf_;

	if (forget_debit_ == 1) {
		/* removes negative deficits */
		for(i=0; i<active_node_; i++)
			if ( deficit[i] < 0 )
				deficit[i] = 0;
	}

	/* draw superframe on eps */
	sprintf(buf, "time=%.04lf\n", NOW);
	trace(buf, cur_term);

       	mac_->recv(p, NULL);

}


void AllocatorMFTDMA::trace(char* sp, int t)
{
	Tcl* tcl;
	char* fp;
	int p;
	int w, h;

	if (!bt || page>1500)
		return;

	if (page == 0)
	{
		tcl = &(Tcl::instance());
		tcl->eval("set eps_header");
		sprintf(bt->buffer(), "%s",tcl->result());
		bt->dump();
	}

	fp = bt->buffer();

	page++;
	sprintf(fp, "%%%%Page: (%d) %d\n"
		"90 rotate 0 -595 translate\n"
		"2 setlinewidth\n",
		page,page);
	bt->dump();

	/* Drawing frames */
	h = 550;
	for(p=0; p<nf; p++)
	{
		w = (hwin_*sf[p].carriers_*sf[p].timeslots_*sf[p].burstsize_)
			/bytes_superframe_;
		h -= w;

		sf[p].draw(bt, wwin_+50, h, 50, h+w);
	}

	/* print title (top) */
	if ( sp )
	{
		sprintf(fp, "50 560 moveto\n"
			"(%s) show\n", sp);
		bt->dump();
	}

	sprintf(fp, "showpage\n");

	bt->dump();
	Tcl_Flush(bt->channel());

}


char* AllocatorMFTDMA::print_bits(unsigned char* bp, int num)
{
	unsigned char c;
	int i,j,k;

	k = 0;
	for(j=0; j<(num+7)/8; j++)
	{
		c = bp[j];
		for(i=0; i<8; i++)
		{
			if ( c & 0x01 )
				wrk[k] = '1';
			else
				wrk[k] = '0';
			c >>= 1;
			k++;
			if (k==num)
			{
				wrk[k] = '\0';
				return wrk;
			}
		}
	}

	return wrk;
}


void Terminal::reset(int masksize)
{
	int i;

	for(i=0; i<(masksize+7)/8; i++)
		_mask[i] = 0x00;

}


void Terminal::setmask(int p, int fieldsize)
{
	int firstbit, i, j1, j2;

	firstbit = p * fieldsize;

	for(i=0; i<fieldsize; i++)
	{

		j1 = (firstbit + i)/8;
		j2 = (firstbit + i)%8;

		_mask[j1] |= (unsigned char)(1<<j2);

	}

}


int Terminal::testmask(int p, int fieldsize)
{
	int firstbit;				 // slotsize in bits
	int i, j1, j2;

	firstbit = p * fieldsize;

	for(i=0; i<fieldsize; i++)
	{
		j1 = (firstbit + i)/8;
		j2 = (firstbit + i)%8;

		if (_mask[j1] & (unsigned char)(1<<j2))
			return 0;

	}

	return 1;
}


void AllocatorMFTDMA::reset()
{
	int i;

	for(i=0; i<active_node_; i++)
		a[i] = 0.0;

	for(i=0; i<nf; i++)
		sf[i].reset();

}
