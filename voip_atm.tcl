#!/home/raffaello/ns-allinone-2.34/ns-2.34/ns
# ERG-UoA Aberdeen (UK), May 2008

# CONFIGURATION VARIABLES ###########################################

;# testing == 1  enables tracing for debugging purposes

if { $argc != 5 } {
	puts "usage: ns example.tcl <CRA kbps> <RBDC=0/1> <VBDC=0/1> <# streams/term> <smoothing par.>"
	exit 0
}


ns-random 0

Allocator/MFTDMA set testing_ 0

Allocator/MFTDMA set layout_ 0            ;# First-Fit=0, Best-Fit=1
Allocator/MFTDMA set mode_ 1              ;# Slot-based=0, Continuous=1
Allocator/MFTDMA set forget_debit_ 1      ;# Carry-Next-Frame=0, Not-Carry-Next-Frame=1

Allocator/MFTDMA set hwin_ 400
Requester/Combiner set req_period_ 1.0
Requester/Combiner set alpha_ [lindex $argv 4]
Requester/Combiner set win_ [expr int(0.6/[Requester/Combiner set req_period_]+1)]

set testing            1
Allocator/MFTDMA set frame_duration_ 0.0265

set terrestrial_delay          0ms
set terrestrial_capacity      100Mb

set bdrop_rate 0.0

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

set duration [expr $stop+5]

##########################################################

proc new-voip { i } {
	global ns voip rcst hq 
	global start reset stop no_terminals rcst

	set voip(s$i) [new Application/Traffic/Voice]
	set voip(r$i) [new Application/Traffic/Voice]
	set udp_s [new Agent/UDP]
	set udp_r [new Agent/UDP]

	$voip(s$i) attach-agent $udp_s
	$voip(s$i) set interval_ $voip(interval)
	$voip(s$i) set burst_time_ $voip(burst_time)
	$voip(s$i) set idle_time_ $voip(idle_time)
	$voip(s$i) set packetSize_ $voip(plen)
	$voip(r$i) attach-agent $udp_r
	
	$udp_s set index $i
	$udp_s set fid_ 0
	$udp_s set prio_ 0 
	$udp_r set index $i

	set n [expr [ns-random] % $no_terminals]

	$ns attach-agent $rcst($n) $udp_s
	$ns attach-agent $hq $udp_r	
	$ns connect $udp_s $udp_r

	$ns at $start "$voip(s$i) start"
	$ns at $reset "$voip(r$i) reset"
	$ns at $stop "$voip(s$i) stop"
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
		-macType Mac/TdmaDama \
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
	[$rcst(0) set phy_tx_(0)] set bdrop_rate_ $bdrop_rate
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

	$ns flush-trace

	set used [$ter_mac(0) set used_slots_]
	set total [$ter_mac(0) set total_slots_]

	set eff [expr double($used)/$total]
	$voip(r0) update_score
	puts "[$voip(r0) set delay_] [$voip(r0) set rscore_] [$voip(r0) set mos_] $eff"

	$ns halt
}


for {set i 0} { $i < $voip(no_voip)} {incr i} {
	$ns at $start "new-voip $i"
}

$ns at $reset "$ter_mac(0) reset"
$ns at $duration "finish-sim"

$ns run 

