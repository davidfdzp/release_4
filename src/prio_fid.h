#ifndef ns_prio_fid_h
#define ns_prio_fid_h
#include "drop-tail.h"

class PacketQueueFid : public PacketQueue
{
public:

	virtual Packet* enque(Packet*);

};


class PrioFid : public DropTail 
{
public:

	PrioFid(){

		q_ = new PacketQueueFid();
        	pq_ = q_;
                bind_bool("drop_front_", &drop_front_);
	        bind_bool("summarystats_", &summarystats);
        	bind_bool("queue_in_bytes_", &qib_);  // boolean: q in bytes?
                bind("mean_pktsize_", &mean_pktsize_);
	}


};

#endif

