#!/home/raffaello/ns-allinone-2.34/ns-2.34/ns
# ERG-UoA Aberdeen (UK), May 2008

# CONFIGURATION VARIABLES ###########################################

;# testing == 1  enables tracing for debugging purposes

if { $argc != 5 } {
	puts "usage: ns example.tcl <CRA> <RBDC=0/1> <VBDC=0/1> <connection_rate> <smoothing par.>"
	exit 0
}


ns-random 0

Allocator/MFTDMA set testing_ 0

Allocator/MFTDMA set layout_ 0            ;# First-Fit=0, Best-Fit=1
Allocator/MFTDMA set mode_ 1              ;# Slot-based=0, Continuous=1
Allocator/MFTDMA set no_carry 1           ;# Carry-Next-Frame=0, Not-Carry-Next-Frame=1
Requester/Combiner set req_period_ 1.0
Requester/Combiner set alpha_ [lindex $argv 4]

set testing            0
set frame_duration    0.0265

set terrestrial_delay          0ms
set terrestrial_capacity      100Mb

set no_terminals         1

# VoIP Flows (IPsec/CRTP/G729)
# max tolerable delay (ms)

set voip(interval)      0.04
set voip(burst_time)    0.46
set voip(idle_time)     0.54
set voip(plen)            76
set voip(no_voip)          [lindex $argv 3] 
set voip(index)            0
 
set start 1.0
set reset 20.0
set stop  30.0

set duration $stop 

######### Web Traffic ######################################


Agent/TCP/FullTcp/Sack set segsize_ 1500


set num_conn 7
set req_size 320
set objnum 43
set obj_size [new RandomVariable/Pareto]
set last_web_done -1
$obj_size set shape_ 1.2
$obj_size set avg_ 7187


Application/TcpApp instproc http-send-req-index {} {
	global ns req_size objnum obj_num duration testing
	global page_req_time page_time num_conn  page_req_time

	$self instvar apps tcp id

	set page_req_time($id) [$ns now]

	if { $testing == "1" } {
		puts "[$ns now] $objnum $id + INDEX"
	}
	$ns at [$ns now] "$self send $req_size \"$apps http-req-recv-index\""

}

Application/TcpApp instproc http-req-recv-index { } {
	global ns obj_size
	$self instvar appc	
	set size [expr int([$obj_size value])]
	$ns at [$ns now] "$self send $size \"$appc http-recv-index\""
}

Application/TcpApp instproc http-recv-index {} {

	global ns objnum  testing
	$self instvar id
	if { $testing  == "1" } {
		puts "[$ns now] $objnum $id - INDEX"
	}
	$ns at [$ns now] "$self new-http-session"
	$ns at [$ns now] "$self http-send-req NULL"

}

Application/TcpApp instproc http-send-req {objid} {
	global ns req_size objnum obj_num web_duration testing
	global page_req_time page_time num_conn last_web_done 

	$self instvar apps tcp id

	if { $objid != "NULL" && $testing == "1" } {
		puts "[$ns now] $objid $id - $obj_num($id)"
	}  

	incr obj_num($id) -1
	if { $obj_num($id) >= 0} {
		if { $testing == "1" } {
	        	puts "[$ns now] $obj_num($id) $id +"
		}
		$ns at [$ns now] "$self send $req_size \"$apps http-req-recv $obj_num($id)\""
		return
	} 
	
	[$self set tcp] close

	if { $obj_num($id) == [expr -$num_conn-1]} {
		set last_web_done $id
		set web_duration($id) [expr [$ns now] - $page_req_time($id)]
		if { $testing == "1" } {
			puts "end $id $web_duration($id) [$ns now] $page_req_time($id)"
		}
	}

}

Application/TcpApp instproc http-req-recv {obj_id} {
	global ns obj_size  obj_num
	$self instvar appc id	
	set size [expr int([$obj_size value])]
	$ns at [$ns now] "$self send $size \"$appc http-send-req $obj_id\""
}


Application/TcpApp instproc new-http-session { } {
	global ns objnum tcp num_conn obj_num
	global page_req_time
	$self instvar id n1 n2
	
	set now [$ns now]
	
	for {set i 0} {$i< $num_conn} {incr i} {

		set tcpc [new Agent/TCP/FullTcp/Sack]
		set tcps [new Agent/TCP/FullTcp/Sack]
		set appc [new Application/TcpApp $tcpc]
		set apps [new Application/TcpApp $tcps]
		$ns attach-agent $n1 $tcpc
		$ns attach-agent $n2 $tcps
		$ns connect $tcpc $tcps
		$tcps listen
		$appc connect $apps

		$appc set apps $apps
		$apps set appc $appc
		$appc set tcp $tcpc
		
		$appc set id $id
		$apps set id $id

		$ns at $now "$appc http-send-req NULL"
	
	}

	set obj_num($id) $objnum
	
}


proc new-http-session { id n1 n2 } {

	global ns
	set now [$ns now]
	
	set tcpc [new Agent/TCP/FullTcp/Sack]
	set tcps [new Agent/TCP/FullTcp/Sack]
	set appc [new Application/TcpApp $tcpc]
	set apps [new Application/TcpApp $tcps]

	$ns attach-agent $n1 $tcpc
	$ns attach-agent $n2 $tcps
	$ns connect $tcpc $tcps
	$tcps listen
	$appc connect $apps

	$appc set apps $apps
	$apps set appc $appc
	$appc set tcp $tcpc
		
	$appc set id $id
	$appc set n1 $n1
	$appc set n2 $n2
	$apps set id $id

	$ns at $now "$appc http-send-req-index"
	
}

# Creating scenario  ##########################

set ns [new Simulator]

if { $testing == 1 } {
	# Tracing enabling must 
	# precede link and node creation 
	set outfile [open out.tr w]
	$ns trace-all $outfile
}

# Head-Quarter node
set hq    [$ns node]
 
# Configure bent-pipe satellite
$ns node-config -wiredRouting ON \
                -satNodeType geo-repeater \
                -phyType Phy/Repeater \
                -channelType Channel/Sat


# GEO satellite at 13 degrees longitude East (Hotbird 6)
set sat [$ns node]
$sat set-position 13

$ns node-config -satNodeType terminal \
                -llType LL/Atm \
		-ifqLen 250 \
		-requesterType Requester/Combiner \
		-phyType Phy/Sat


set hub   [$ns node]
$hub set-position 43.71 10.38
$ns setup-geolink $hub $sat
set hub_mac [$hub set mac_(0)]

for {set i 0} { $i < $no_terminals } {incr i} {
	set rcst($i) [$ns node]
	$rcst($i) set-position 43.71 10.38
	$ns at 0.0 "$rcst($i) start-req"
	$ns setup-geolink $rcst($i) $sat
	set ter_mac($i) [$rcst($i) set mac_(0)]
}


if { $testing == 1 } {
        set ev_file [open event.tr w]
        $hub trace-event $ev_file
        for {set i 0} { $i < $no_terminals } {incr i} {
                $rcst($i) trace-event $ev_file
        }
        $rcst(0) trace-event $ev_file
	$ns trace-all-satlinks $outfile

}


# Network Control Center
set rrm [$hub install-allocator Allocator/MFTDMA]

################### RRM CONF ######################

#### Forward Link 

set DL_frame [$rrm new-frame 1 32 53]
$rrm add-rule $hub_mac $DL_frame
$rrm cra $hub_mac 32

#### Return Link 

set f0 [$rrm new-frame 1 16 53]

for {set i 0} {$i<$no_terminals} {incr i} {
	$rrm add-rule $ter_mac($i) $f0
	$rrm cra $ter_mac($i) [lindex $argv 0]
	[$rcst($i) set requester_] set rbdc_ [lindex $argv 1]
	[$rcst($i) set requester_] set vbdc_ [lindex $argv 2]
}


if { $testing == 1 } {
	$ns at 19.0 {
		set ps_anim [open "superframe.ps" w]
		$rrm trace-sf $ps_anim
	}
}

###################################################

# We use centralized routing
set satrouteobject_ [new SatRouteObject]
$satrouteobject_ compute_routes

$ns duplex-link $hq $hub $terrestrial_capacity $terrestrial_delay DropTail


proc finish-sim {} {
	global testing ns rrm ter_mac voip
	global web_duration last_web_done

	$ns flush-trace

	set used [$ter_mac(0) set used_slots_]
	set total [$ter_mac(0) set total_slots_]

	set eff [expr double($used)/$total]
	if { $last_web_done != "-1" } {
		puts "$web_duration($last_web_done) $eff"
		$ns halt
	}
	
	$ns at [expr [$ns now] + 10] "finish-sim"	

}



$ns at $start "new-http-session $i $rcst(0) $hub"

$ns at $start "$ter_mac(0) reset"
$ns at $duration "finish-sim"

$ns run 

