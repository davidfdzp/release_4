#include "prio_fid.h"


static class PrioFidClass : public TclClass {
 public:
        PrioFidClass() : TclClass("Queue/DropTail/PrioFid") {}
        TclObject* create(int, const char*const*) {
                return (new PrioFid);
        }
} class_prio_fid;



Packet* PacketQueueFid::enque(Packet*p)
{
Packet* pt_;
int prio;
Packet *prev, *curr;

	++len_;
	bytes_ += hdr_cmn::access(p)->size();

	pt_ = tail_;

	// no queue? just insert the packet
	if (!tail_) {
		head_= tail_= p;
		return pt_;	
	}
	
	// should insert at last?
	prio = HDR_IP(p)->prio();
	if (HDR_IP(tail_)->prio() <= prio){
		tail_->next_ = p;
		tail_ = p;
		return pt_;
	}


	prev = 0;
	curr = head_;
	while(HDR_IP(curr)->prio() <= prio)
	{
		prev = curr;
		curr = curr->next_;
	}

	if (prev) 
		prev->next_ = p;
	else 
		head_ = p;
	
	p->next_ = curr;

	return pt_;

}



