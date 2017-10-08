#ifndef ns_phy_tdmadama_h
#define ns_phy_tdmadama_h
#include "phy.h"

class TdmaDamaPhy : public Phy {
 public:
	TdmaDamaPhy();
	void sendDown(Packet *p);
	int sendUp(Packet *p);
 protected:
	int command(int argc, const char*const* argv);

	double bdrop_rate_;
	int drop_burst;
	int current_burst;
};



#endif							 /* __mac_tdmadama_h__ */
