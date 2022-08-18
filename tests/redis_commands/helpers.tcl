proc cleanup {} {
    if {$::verbose} {puts -nonewline "Cleanup: take a seconds... "}
    flush stdout
    catch {exec rm -rf {*}[glob $::tmproot]}
    if {$::verbose} {puts "OK"}
}

set ::last_port_attempted 0
proc find_available_port {start count} {
    set port [expr $::last_port_attempted + 1]
    for {set attempts 0} {$attempts < $count} {incr attempts} {
        if {$port < $start || $port >= $start+$count} {
            set port $start
        }
        set fd1 -1
        if {[catch {set fd1 [socket -server 127.0.0.1 $port]}] ||
            [catch {set fd2 [socket -server 127.0.0.1 [expr $port+10000]]}]} {
            if {$fd1 != -1} {
                close $fd1
            }
        } else {
            close $fd1
            close $fd2
            set ::last_port_attempted $port
            return $port
        }
        incr port
    }
    error "Can't find a non busy port in the $start-[expr {$start+$count-1}] range."
}

proc linespacer {} {
  puts "\n################################################"
}

proc is_alive config {
    set pid [dict get $config pid]
    if {[catch {exec kill -0 $pid} err]} {
        return 0
    } else {
        return 1
    }
}

proc send_data_packet {fd status data {elapsed 0}} {
    set payload [list $status $data $elapsed]
    puts $fd [string length $payload]
    puts -nonewline $fd $payload
    flush $fd
}

###################################### TODO: 👇




# The the_end function gets called when all the test units were already
# executed, so the test finished.
proc the_end {} {
    # TODO: print the status, exit with the right exit code.
    puts "\n                   The End\n"
    puts "Execution time of different units:"
    foreach {time name} $::clients_time_history {
        puts "  $time seconds - $name"
    }
    if {[llength $::failed_tests]} {
        puts "\n{!!! WARNING} The following tests failed:\n"
        foreach failed $::failed_tests {
            puts "*** $failed"
        }
        if {!$::dont_clean} cleanup
        exit 1
    } else {
        puts "\n \o/ All tests passed without errors!\n"
        if {!$::dont_clean} cleanup
        exit 0
    }
}

proc dump_server_log {srv} {
    set pid [dict get $srv "pid"]
    puts "\n===== Start of server log (pid $pid) =====\n"
    puts [exec cat [dict get $srv "stdout"]]
    puts "===== End of server log (pid $pid) =====\n"

    puts "\n===== Start of server stderr log (pid $pid) =====\n"
    puts [exec cat [dict get $srv "stderr"]]
    puts "===== End of server stderr log (pid $pid) =====\n"
}

proc lpop {listVar {count 1}} {
    upvar 1 $listVar l
    set ele [lindex $l 0]
    set l [lrange $l 1 end]
    set ele
}

proc randomInt {max} {
    expr {int(rand()*$max)}
}