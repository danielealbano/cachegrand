proc spawn_client {} {
    puts "\n** Spawn Client"
    cleanup

    # Set global keys
    set ::idle_clients {}
    set ::active_clients {}
    array set ::active_clients_task {}
    array set ::clients_start_time {}
    set ::clients_time_history {}
    set ::failed_tests {}
    set ::clients_pids {}

    # Finding free socketport
    set client_socket_port [find_available_port [expr {$::socket_port - 32}] 32]

    if {$::verbose} { puts -nonewline "Starting socket server at port $client_socket_port... "}
    socket -server accept_test_clients -myaddr $::socket_host $client_socket_port
    if {$::verbose} {puts "OK"}

    if {$::verbose} { puts "Will be spawned: $::numclients clients... "}
    set start_port $::socket_port
    set port_count [expr {$::portcount / $::numclients}]
    for {set j 0} {$j < $::numclients} {incr j} {
        set p [exec $::tclsh [info script] {*}$::argv --client $client_socket_port --portcount $port_count &]
        if {$::verbose} { puts "- Creating instances with PID: $p"}
        lappend ::clients_pids $p
        incr start_port $port_count
    }
    if {$::verbose} {puts "Spawning Clients... OK \n"}

    # Enter the event loop to handle clients I/O
    after 100 client_watcher
    vwait forever
}

proc client_watcher {} {
    set elapsed [expr {[clock seconds]-$::last_progress}]

    if {$elapsed > $::timeout} {
        puts "\[TIMEOUT\]: clients state report follows."
        kill_clients
        the_end
    }

    after 100 client_watcher
}

proc accept_test_clients {fd addr port} {
    fconfigure $fd -encoding binary
    fileevent $fd readable [list read_from_test_client $fd]
}

proc read_from_test_client fd {
    set bytes [gets $fd]
    set payload [read $fd $bytes]
    foreach {status data elapsed} $payload break
    set ::last_progress [clock seconds]

    if {$status eq {ready}} {
        if {$::verbose} { puts "Client with pid \[$data\] recieve \[$status\] status" }
        signal_idle_client $fd

    } elseif {$status eq {done}} {
        set elapsed [expr {[clock seconds]-$::clients_start_time($fd)}]
        set all_tests_count [llength $::all_tests]
        set running_tests_count [expr {[llength $::active_clients]-1}]
        set completed_tests_count [expr {$::next_test-$running_tests_count}]

        puts "\[$completed_tests_count/$all_tests_count $status\]: $data ($elapsed seconds)"
        lappend ::clients_time_history $elapsed $data

        signal_idle_client $fd
        set ::active_clients_task($fd) "(DONE) $data"

    } elseif {$status eq {ok}} {
        if {$::verbose} { puts "\[$status\]: $data ($elapsed ms)" }
        set ::active_clients_task($fd) "(OK) $data"

    } elseif {$status eq {skip}} {
        if {$::verbose} { puts "\[$status\]: $data" }

    } elseif {$status eq {ignore}} {
        if {$::verbose} { puts "\[$status\]: $data" }

    } elseif {$status eq {err}} {
        set err "\[$status\]: $data"
        puts $err
        lappend ::failed_tests $err
        set ::active_clients_task($fd) "(ERR) $data"

    } elseif {$status eq {exception}} {
        puts "\[$status\]: $data"

        kill_clients
        kill_server $::srv
        exit 1

    } elseif {$status eq {testing}} {
        set ::active_clients_task($fd) "(IN PROGRESS) $data"

    } elseif {$status eq {server-spawning}} {
        set ::active_clients_task($fd) "(SPAWNING SERVER) $data"

    } elseif {$status eq {server-spawned}} {
        lappend ::active_servers $data
        set ::active_clients_task($fd) "(SPAWNED SERVER) pid:$data"

    } elseif {$status eq {server-killing}} {
        set ::active_clients_task($fd) "(KILLING SERVER) pid:$data"

    } elseif {$status eq {server-killed}} {
        set ::active_servers [lsearch -all -inline -not -exact $::active_servers $data]
        set ::active_clients_task($fd) "(KILLED SERVER) pid:$data"

    } elseif {$status eq {run_solo}} {
        lappend ::run_solo_tests $data

    } else {
        if {$::verbose} { puts "\[$status\]: $data" }
    }
}

proc signal_idle_client fd {
    # Remove this fd from the list of active clients.
    set ::active_clients \
        [lsearch -all -inline -not -exact $::active_clients $fd]

    # New unit to process?
    if {$::next_test != [llength $::all_tests]} {
        if {$::verbose} {
            puts "Testing \[[lindex $::all_tests $::next_test]\] and ASSIGNED to client : $fd"
            set ::active_clients_task($fd) "ASSIGNED: $fd ([lindex $::all_tests $::next_test])"
        }

        set ::clients_start_time($fd) [clock seconds]
        send_data_packet $fd run [lindex $::all_tests $::next_test]
        lappend ::active_clients $fd
        incr ::next_test
    } elseif {[llength $::run_solo_tests] != 0 && [llength $::active_clients] == 0} {
        if {$::verbose} {
            puts "Testing solo test and ASSIGNED to client : $fd"
            set ::active_clients_task($fd) "ASSIGNED: $fd solo test"
        }

        set ::clients_start_time($fd) [clock seconds]
        send_data_packet $fd run_code [lpop ::run_solo_tests]
        lappend ::active_clients $fd
    } else {
        lappend ::idle_clients $fd
        set ::active_clients_task($fd) "SLEEPING, no more units to assign"
        if {[llength $::active_clients] == 0} {
            linespacer "#"
            the_end
        }
    }
}

proc kill_clients {} {
    foreach p $::clients_pids {
        catch {exec kill $p}
    }
}