/* MAC basic functions */

#ifndef ns_mac_rle_h
#define ns_mac_rle_h

#define FHS  2       // Base Hdr Frag + Label 
#define PHS  2       // Base PHS PPDU + Label
#define TS   1       // Size of RLE trailer
#define MAX_CNTS  512

#include "mac-tdmadama.h"

#define HDR_RLE_BURST(p)  (hdr_rleburst::access(p))

struct context {

	int src, fid;
	Packet* pkt;

	unsigned char exp_sn;
	unsigned len;

	context* head;
	context* next;

	/* Freeing the context is not 
	   needed when seqno are used */

	// context** phook;

};

struct frag_hdr {

	unsigned start_end;
	unsigned length;
	unsigned frag_id;
	unsigned char seqno;
	
	Packet* alpdu;

	frag_hdr* next;	
};

/* Burst packet (Frame) */
struct hdr_rleburst {

	int size;

	static int offset_;
        inline static int& offset() { return offset_; }
	inline static hdr_rleburst* access(const Packet*p){
		return (struct hdr_rleburst*) p->access(offset_);
	}


};


class RlePayload: public AppData
{

        public:

                RlePayload() : AppData(PACKET_DATA), 
			head(NULL), last(NULL) {}
                ~RlePayload();
		AppData* copy();
                frag_hdr* new_frag(Packet*);


                frag_hdr *head, *last;

};


class QueueIface: public Queue
{
	public:
	static Packet* head(Queue* q) { 
		return ((QueueIface*)q)->pq_->head(); 
	}

	static void remove(Queue* q, Packet* p) {
		((QueueIface*)q)->pq_->remove(p);	
		((QueueIface*)q)->target_->recv(p, (Handler*)NULL);
	}

};

/* TDMA Mac layer. */
class MacRle : public MacTdmaDama
{

	public:
		MacRle();
		//void virtual recv(Packet *p, Handler *h);

		void virtual sendUp(Packet*);
		void virtual recv(Packet*, Handler*);

		void reset();
		int command(int argc, const char*const* argv);

	protected:

		int sent_up_;
		int sent_down_;
		context* get_context(int, int);

		/* Freeing the context is not 
		   needed when seqno are used */
		// void free_context(context*);

		void virtual slotHandler(Event*);

	private:

		context cbuffer[MAX_CNTS];


		unsigned char next[8];
	
		context* free;

		

};
#endif							 /* __mac_tdmadama_h__ */
