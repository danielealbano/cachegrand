# Copyright (C) 2018-2022 Vito Castellano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

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
        if {[catch {set fd1 [socket -server $::socket_host $port]}] ||
            [catch {set fd2 [socket -server $::server_host [expr $port+10000]]}]} {
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

proc linespacer {chr} {
  puts [string repeat $chr 50]
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

proc lpop {listVar {count 1}} {
    upvar 1 $listVar l
    set ele [lindex $l 0]
    set l [lrange $l 1 end]
    set ele
}

proc randomInt {max} {
    expr {int(rand()*$max)}
}

proc tags_acceptable {tags err_return} {
    upvar $err_return err

    if {[llength $::allowed_tags] > 0} {
        set matched 0
        foreach tag $::allowed_tags {
            if {[lsearch $tags $tag] >= 0} {
                incr matched
            }
        }

        if {$matched < 1} {
            set err "Tag: none of the tags allowed"
            return 0
        }
    }

    if {[llength $::excluded_tags] > 0} {
        set matched 0
        foreach tag $::excluded_tags {
            if {[lsearch $tags $tag] >= 0} {
                incr matched
            }
        }

        if {$matched >= 1} {
            set err "Found excluded tags"
            return 0
        }
    }

    return 1
}

proc dump_server_log {srv} {
    set pid [dict get $srv "pid"]
    linespacer "="
    puts "STDOUT LOG"
    puts [exec cat [dict get $srv "stdout"]]
    linespacer "="

    linespacer "="
    puts "STDERR LOG"
    puts [exec cat [dict get $srv "stderr"]]
    linespacer "="
}

proc the_end {} {
    linespacer "="
    puts "End Report"
    puts "Execution time of different units:"
    foreach {time name} $::clients_time_history {
        puts "- $time seconds - $name"
    }

    if {[llength $::failed_tests]} {
        puts "\n :( [llength $::failed_tests] tests failed:"
        foreach failed $::failed_tests {
            puts "-> $failed"
        }
        if {!$::dont_clean} cleanup
        linespacer "="
        exit 1
    } else {
        puts "\n :) All tests passed without errors!"
        if {!$::dont_clean} cleanup
        linespacer "="
        exit 0
    }
}