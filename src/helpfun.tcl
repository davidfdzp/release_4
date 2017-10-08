# ================= Generic Helpers ===============================

proc get-random {} {
        set RS [open random r]
        set rnd [gets $RS]
        set rnd [expr $rnd + 1]
        close $RS
        set RS [open random w]
        puts $RS $rnd
	return $rnd
}

# ================= Mac Multicarrier defaults ===============================
Simulator instproc requesterType {val} {$self set requesterType_ $val}
Simulator instproc allocatorType {val} {$self set allocatorType_ $val}

add-packet-header MPEG ATM RleBurst Rle TdmaDama
		
# Helper function for superframe tracing
Allocator/MFTDMA instproc trace-sf { filedes {index_ 0}} {
	$self instvar mac_ bt_
	set bt_ [new BaseTrace]
	$bt_ attach $filedes
	$self trace-file $bt_
	return $bt_	
}

# new-frame-atm adds an ATM-based frame in the MFTDMA superframe.
# Its arguments are:
#   car: the number of carriers in the frame
#    ts: the number of timeslots 
#    bs: the burst size in bytes 
Allocator/MFTDMA instproc new-frame { car ts bs } {

	$self instvar mac_ bytes_superframe_ 
	$self instvar slot_count_ f_count_ mac_
	$self instvar frame_duration_ slot_time_

	# check inputs
	if { $car<1 || $car>20 } {
	   puts stderr "$self new-frame: too many carriers for a single frame"
	   exit 
	}
		
	if { $ts<1 || $ts>256 } {
	   puts stderr "$self new-frame: too many timeslots per carrier"
	   exit	
	}

	set minlen [$mac_ set slot_packet_len_]
	if { $bs<$minlen } {
	   puts stderr "$self new-frame: burstsize smaller than $minlen bytes"
	   exit	
	}
	
	if { $bs>64000 } {
	   puts stderr "$self new-frame: burstsize larger than 64kB"
	   exit	
	}

	$self add-new-frame $car $ts $bs
	set slot_count_ [expr $slot_count_ + $car*$ts]
	set bytes_superframe_ [expr $bytes_superframe_ + $car*$ts*$bs]
	set slot_time_ [expr  $frame_duration_/$ts]
	$self attach $mac_

	return [expr $f_count_ - 1]
}

# add-rule adds a rule for allocation.
# Its arguments are:  <term> <rule1> <rule2> ... <ruleN>
#
# +  Each rule is in the form:    "<condition> <frame>"
# +  <condition> must be satisfied so that allocation in <frame>
#    is attempted
Allocator/MFTDMA instproc add-rule args {

	set tt [lindex $args 0]

	set mac_class [$tt info class]
	

	if { $mac_class != "Mac/TdmaDama" && 
		$mac_class != "Mac/Rle" } {
		puts "add-rule: $tt not a Mac/TdmaDama or Mac/Rle"
		exit
	}

	set args [lreplace $args 0 0]

	foreach arg $args {


		set ll [split $arg " "]
		set arg1 [lindex $ll 0]
		set arg2 [lindex $ll 1]

		# Decoding rules
		if { $arg2 == "" } {
			$self new-rule [$tt set slot_num_] 0 $arg1
			continue
		}
		if { $arg1 == "NULL" } {
			$self new-rule [$tt set slot_num_] 0 $arg2
			continue
		}
		
		if { $arg1 == "SMALL" } {
			$self new-rule [$tt set slot_num_] 1 $arg2
			continue
		}
	}

}


Allocator/MFTDMA instproc cra { mac_ter rate } {

	set mac_class [$mac_ter info class]

	if { $mac_class != "Mac/TdmaDama" && 
		$mac_class != "Mac/Rle" } {
		puts "cra: $tt not a Mac/TdmaDama or Mac/Rle"
		exit
	}

	#if { [$mac_ter info class] != "Mac/TdmaDama" } {
	#	puts "cra: $mac_ter not Mac/TdmaDama"
	#	exit
	#}

	$self cra_ [$mac_ter set slot_num_] $rate
		

}


# ================= Tcl Interface & Defaults ================================
Simulator instproc setup-geolink {ternode satnode} {

	$self instvar llType_ ifqlen_ requesterType_
	$self instvar macType_ ifqType_ channelType_ phyType_ wiredRouting_


	# Configure default variables
	if { [info exists llType_] } {
		set opt_ll $llType_
	} else {
		set opt_ll LL/Atm
	}

	if { [info exists ifqType_] } {
		set opt_ifq $ifqType_
	} else {
		set opt_ifq Queue/DropTail/PrioFid
	}

	if { [info exists ifqlen_] } {
		set opt_qlim $ifqlen_
	} else {
		set opt_qlim [Queue set limit_]
	} 

        if { [info exists macType_] } {
		set opt_mac $macType_ 
	} else {
		set opt_mac Mac/TdmaDama
	}

	set opt_bw 3072000.0

	if { [info exists phyType_] } {
		set opt_phy $phyType_
	} else {
		else opt_phy Phy/TdmaDama
	}


	if { [info exists requestType_] } {
		set opt_req $requesterType_
	} else {
		set opt_req Requester/Combiner
	}

        $ternode add-interface geo $opt_ll $opt_ifq $opt_qlim $opt_mac $opt_bw $opt_phy 
        $ternode attach-to-inlink [$satnode set downlink_]
	$ternode attach-to-outlink [$satnode set uplink_]
	$ternode install-requester $opt_req

	[$ternode set ll_(0)] set limit_ $opt_qlim

	if { $opt_ll == "LL/Atm" } {
		[$ternode set mac_(0)] set slot_packet_len_ 53
	}

	if { $opt_ll == "LL/Mpeg" } {
		[$ternode set mac_(0)] set slot_packet_len_ 188
	}

	if { $opt_ll == "LL/Rle" } {
		[$ternode set mac_(0)] set slot_packet_len_ 2
	}

	if { ($opt_ll == "LL/Rle" && $opt_mac != "Mac/Rle") || \
		($opt_ll != "LL/Rle" && $opt_mac == "Mac/Rle") } {

		puts stderr "setup-geolink: must be both LL/Rle and Mac/Rle"
		exit

		# [$ternode set mac_(0)] setmon $ifq_ 
	}
	$ternode setifqueue

}

Node/SatNode instproc start-req {} {
	$self instvar requester_
	$requester_ start
}

Node/SatNode instproc stop-req {} {
	$self instvar requester_
	$requester_ stop
}


Node/SatNode instproc setifqueue {} {
	$self instvar mac_ ifq_

	$mac_(0) setmon $ifq_(0)

}

# Trace element between mac and ll tracing packets between node and node
Node/SatNode instproc trace-inlink-queue {f {index_ 0} } {
        $self instvar id_ rcvT_ mac_ ll_ phy_rx_ em_ errT_

        set ns [Simulator instance]
        set toNode_ $id_
        set fromNode_ -1

        if {[info exists em_($index_)]} {
                # if error model, then chain mac -> em -> rcvT -> ll
                # First, set up an error trace on the ErrorModule
                set errT_($index_) [$ns create-trace Sat/Error $f $fromNode_ $toNode_]
                $errT_($index_) target [$em_($index_) drop-target]
                $em_($index_) drop-target $errT_($index_)
                set rcvT_($index_) [$ns create-trace Sat/Recv $f $fromNode_ $toNode_]
                $rcvT_($index_) target [$em_($index_) target]
                $em_($index_) target $rcvT_($index_)
        } else {
                # if no error model, then insert between mac and ll
                set rcvT_($index_) [$ns create-trace Sat/Recv $f $fromNode_ $toNode_]
                $rcvT_($index_) target [$mac_($index_) up-target]
                $mac_($index_) up-target $rcvT_($index_)
        }

}


Node/SatNode instproc trace-outlink-queue {f {index_ 0} } {
	$self instvar id_ enqT_ deqT_ drpT_ mac_ ll_ ifq_ drophead_
	$self instvar linkhead_ lhT_

	set ns [Simulator instance]
	set fromNode_ $id_
	set toNode_ -1

	set enqT_($index_) [$ns create-trace Enque $f $fromNode_ $toNode_]
	$enqT_($index_) target [$ll_($index_) down-target]
	$ll_($index_) down-target $enqT_($index_)

	set deqT_($index_) [$ns create-trace Deque $f $fromNode_ $toNode_]
	$deqT_($index_) target [$ifq_($index_) target]
	$ifq_($index_) target $deqT_($index_)

	set drpT_($index_) [$ns create-trace Drop $f $fromNode_ $toNode_]
	$drpT_($index_) target [$drophead_($index_) target]
	$drophead_($index_) target $drpT_($index_)
	$ifq_($index_) drop-target $drophead_($index_)

	set lhT_($index_) [$ns create-trace Hop $f $fromNode_ $toNode_]
	$lhT_($index_) target [$linkhead_($index_) target] 
	$linkhead_($index_) target $lhT_($index_)

}


Node/SatNode instproc init args {
        eval $self next $args           ;# parent class constructor


        $self instvar nifs_
        $self instvar phy_tx_ phy_rx_ mac_ ifq_ ll_ pos_ hm_ id_ ifqType_

        set nifs_       0               ;# number of network interfaces
        set ns_ [Simulator instance]


        # Create a drop trace to log packets for which no route exists
        set trace_ [$ns_ get-ns-traceall]
        if {$trace_ != ""} {
                set dropT_ [$ns_ create-trace Sat/Drop $trace_ $self $self ""]
                $self set_trace $dropT_
        }
        $self cmd set_address $id_ ; # Used to indicate satellite node in array
}


#  Attaches error model to interface "index" (by default, the first one)
Node/SatNode instproc interface-errormodel { em { index 0 } } {
	$self instvar mac_ ll_ em_ linkhead_
	set pp [$mac_($index) up-target]
	$mac_($index) up-target $em
	$em target $pp
	$em drop-target [new Agent/Null]; # otherwise, packet is only marked
	set em_($index) $em
	$linkhead_($index) seterrmodel $em
} 

Node/SatNode instproc insert-monitor { {index_ 0}} {
	$self instvar qMonitor_ snoopIn_ snoopOut_ snoopDrop_
	$self instvar ll_ ifq_ drophead_ bytesInt_ pktsInt_ requester_
	$self instvar snoopIP_

	$self instvar linkhead_

	set snoopIn_($index_)   [new SnoopQueue/In]
	set snoopOut_($index_)  [new SnoopQueue/Out]
	set snoopDrop_($index_) [new SnoopQueue/Drop]

	$snoopIn_($index_) target [$ll_($index_) down-target]	
	$ll_($index_) down-target $snoopIn_($index_)

	$snoopOut_($index_) target [$ifq_($index_) target]
	$ifq_($index_) target $snoopOut_($index_)

	$snoopDrop_($index_) target [$drophead_($index_) target]
	$drophead_($index_) target $snoopDrop_($index_)
	$ifq_($index_) drop-target $snoopDrop_($index_)

	set bytesInt_($index_) [new Integrator]
	$qMonitor_($index_) set-bytes-integrator $bytesInt_($index_)

	set pktsInt_($index_) [new Integrator]
	$qMonitor_($index_) set-pkts-integrator $pktsInt_($index_)

	$requester_ set qMonitor_ $qMonitor_($index_)

	$snoopIn_($index_) set-monitor $qMonitor_($index_)
	$snoopOut_($index_) set-monitor $qMonitor_($index_)
	$snoopDrop_($index_) set-monitor $qMonitor_($index_)

} 

Simulator instproc satRequester {type_} {
	$self set satRequester_ $type_
	return $type_
}

Simulator instproc satAllocator {type_} {
	$self set satAllocator_ $type_
	return $type_
}

Simulator instproc satPositionLat {type_} {
	$self set satPositionLat_ $type_
	return $type_
}

Simulator instproc satPositionLon {type_} {
	$self set satPositionLon_ $type_
	return $type_
}


Node/SatNode instproc install-requester { type {index_ 0} } {
	$self instvar mac_ ifq_ qMonitor_ requester_ id_ ll_

	set qMonitor_($index_) [new QueueMonitor]	
	if {[info exist mac_($index_)]} {

		set requester_ [new $type]
		$requester_ attach $mac_($index_)
		$requester_ setifq $ifq_($index_)
		$requester_ set ifq_ $ifq_($index_)
		$requester_ set id_ $id_
		$requester_ set node_ $self
		$self insert-monitor $index_

		$requester_ setmon $qMonitor_($index_)
		$ll_($index_) setmon $ifq_($index_) 

		return $requester_
	}

}

Node/SatNode instproc install-allocator {type {index_ 0} } {
	$self instvar mac_ allocator_
	if {[info exist mac_($index_)]} {
		set allocator_ [new $type]
		$allocator_ attach $mac_($index_)
		#$mac_($index_) set hub_ 1
		$mac_($index_) set-allocator $allocator_
		$allocator_ set hub_ [$mac_($index_) set slot_num_]
		$allocator_ set mac_ $mac_($index_)
	}
	return $allocator_
}

Node/SatNode instproc trace-event { filedes {index_ 0}} {
	$self instvar mac_ et_ requester_ allocator_
	if {[info exist mac_($index_)]} {
		set et_ [new BaseTrace/Event]
		$et_ attach $filedes
		$mac_($index_) eventtrace $et_
		$requester_ eventtrace $et_
	}
	return $et_	
}

Node/SatNode instproc insert-sat-ttl { {index_ 0} } {
        $self instvar ifq_ ttl_
        if {[info exist ifq_($index_)] } {
                set ttl_($index_) [new TTLChecker]
                set tgt [$ifq_($index_) target]
                $ifq_($index_) target $ttl_($index_)
                $ttl_($index_) target $tgt
        }
}



