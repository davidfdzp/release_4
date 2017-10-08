# Session emulation

Agent/TCP/FullTcp/Sack set segsize_ 1500

ns-random 0

set num_idle_conn 0
set req_size 320

set objnum 42    ;# total number 
set no_serv [expr [ns-random]%6+1]    ;# serv_tot = main + no_serv = 1 + no_serv
set objnum_server [expr ($objnum+$no_serv+2)/($no_serv+1)]

set num_conn 6

set dns_size 62
set is_delay 0.01

set http_debug 1

set obj_size [new RandomVariable/Pareto]
$obj_size set shape_ 1.2
$obj_size set avg_ 7187

Agent/UDP instproc process_data {from data} {
	global ns dns_size http_debug

	$self instvar peer_ isserver_ id_ n1_ n2_ 

	set query_type [lindex $data 0]
	set server [lindex $data 1]

	if {$http_debug} {
		puts "DNS [format "%.4lf" [$ns now]] $query_type $server"
	}


	if { $query_type == "DNS_query" } {
		$self send $dns_size "DNS_response $server"
	}

	if { $query_type == "DNS_response" } {
		# open all the connections
		$ns at [$ns now] "new-http-session $id_ $n1_ $n2_ $server"
	}


}

proc start-http-session {id n1 n2 {server 0}} {

	global ns http_start_time page_req_time

	set page_req_time($id,$server) [$ns now]

	#start DNS
	set dnsc [new Agent/UDP]
	set dnss [new Agent/UDP]

	$dnsc set peer_ $dnss
	$dnss set peer_ $dnsc

	$dnss set id_ $id
	$dnss set n1_ $n1
	$dnss set n2_ $n2

	$dnsc set id_ $id
	$dnsc set n1_ $n1
	$dnsc set n2_ $n2

	$ns attach-agent $n1 $dnsc
	$ns attach-agent $n2 $dnss
	$ns connect $dnsc $dnss
	
	if { $server == "0" } {
		set http_start_time($id) [$ns now]
	}

	$ns at [$ns now] "$dnsc send 62 \"DNS_query $server\""
	

}


Application/TcpApp instproc http-send-req-index {server} {
	global ns req_size objnum obj_num duration
	global page_req_time page_time num_conn http_debug

	$self instvar apps tcp id



	if {$server == "0" } {
		# first server download index.html first
		if { $http_debug == "1" } {
			puts "i $id $server [format "%.4lf" [$ns now]]"
		}
		$ns at [$ns now] "$self send $req_size \"$apps http-req-recv-index\""
	} else {
		# other servers download objects only
		$ns at [$ns now] "$self new-http-session"
		$ns at [$ns now] "$self http-send-req NULL"

	}


}

Application/TcpApp instproc http-req-recv-index {} {
	global ns obj_size id
	$self instvar appc	
	set size [expr int([$obj_size value])]
	$ns at [$ns now] "$self send $size \"$appc http-recv-index\""
}

Application/TcpApp instproc http-recv-index {} {

	global ns http_debug is_delay no_serv
	$self instvar id n1 n2 server

	# create sessions to other servers
	if { $http_debug == "1" } {
		puts "r $id $server [format "%.4lf" [$ns now]]"
	}


	$ns at [$ns now] "$self new-http-session"
	$ns at [$ns now] "$self http-send-req NULL"

	set dd [$ns now]
	for {set i 1} { $i <= $no_serv} {incr i} {
		set dd [expr $dd + $is_delay]
		$ns at $dd "start-http-session $id $n1 $n2 $i"
	}

}

Application/TcpApp instproc http-send-req {objid} {
	global ns req_size objnum obj_num duration http_start_time
	global http_duration page_req_time page_time num_conn http_debug

	$self instvar apps tcp id server

	if { $objid != "NULL" && $http_debug == "1" } {
		puts "- $id $server [format "%.4lf" [$ns now]] $objid"
	}

	incr obj_num($id,$server) -1
	if { $obj_num($id,$server) >= 0} {
		if { $http_debug == "1" } {
		        puts "+ $id $server [format "%.4lf" [$ns now]] $obj_num($id,$server)"
		}
		$ns at [$ns now] "$self send $req_size \"$apps http-req-recv $obj_num($id,$server)\""
		return
	} 
	
	[$self set tcp] close

	if { $obj_num($id,$server) == [expr -$num_conn-1]} {
		set duration($id,$server) [expr [$ns now] - $page_req_time($id,$server)]
		set http_duration($id) [expr [$ns now] - $http_start_time($id)]
		if { $http_debug == "1" } {
			puts "S $id $server [format "%.4lf" $duration($id,$server)]"
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
	global ns objnum_server tcp num_conn obj_num
	global page_req_time
	$self instvar id n1 n2 server
	
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
		$appc set server $server
		
		$appc set id $id
		$apps set id $id
		$apps set server $server

		$ns at $now "$appc http-send-req NULL"
	
	}

	set obj_num($id,$server) $objnum_server
	
}


proc new-http-session { id n1 n2 {server 0}} {

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
	$appc set server $server
	$appc set n2 $n2
	$apps set id $id
	$apps set server $server

	$ns at $now "$appc http-send-req-index $server"

	
}

