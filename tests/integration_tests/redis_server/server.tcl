# Copyright (C) 2018-2022 Vito Castellano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

proc start_server {options {code undefined}} {
    puts "\n** Start Server"

    # setup defaults
    set overrides {}
    set tags {}
    set args {}
    set config_lines {}

    # parse options
    foreach {option value} $options {
        switch $option {
            "overrides" {
                set overrides $value
            }
            "args" {
                set args $value
            }
            "tags" {
                set tags [string map { \" "" } $value]
                set ::tags [concat $::tags $tags]
            }
            default {
                error "Unknown option $option"
            }
        }
    }

    # Starting server
    set server_started 0
    while {$server_started == 0} {
        if {$code ne "undefined"} {
          set server_started [spawn_server]
        } else {
          set server_started 1
        }
    }

    # Run Test
    test_controller $code $tags
}

proc spawn_server {} {
    puts "\n** Spawn server \[$::server_host@$::server_port\]"

    set stdout [format "%s/%s" $::tmproot "stdout"]
    set stderr [format "%s/%s" $::tmproot "stderr"]

    # Emit event server-spawning
    send_data_packet $::test_server_fd "server-spawning" "port $::server_port"

    if {$::verbose} {puts -nonewline "Spawing... "}
    set pid [exec $::server_path -c $::server_cfg >> $stdout 2>> $stderr &]
    set ::server_pid $pid
    if {$::verbose} {puts "OK Server spawned with pid \[$pid\]"}

    # Emit event server-spawned
    send_data_packet $::test_server_fd server-spawned $pid

    # Check if server is truly up
    set serverisup [server_is_up $::server_host $::server_port 100]
    if {!$serverisup} {
        error "Probably there are some errors with server :("
    }

    # Struct server's info
    set client [redis $::server_host $::server_port]
    dict set ::srv "client" $client
    dict set ::srv "pid" $::server_pid
    dict set ::srv "host" $::server_host
    dict set ::srv "port" $::server_port
    dict set ::srv "stdout" $stdout
    dict set ::srv "stderr" $stderr

    puts ""
    return $serverisup
}

proc server_is_up {host port retrynum} {
    if {$::verbose} {puts -nonewline "Check if server is up..."}
    after 10 ;
    set retval 0
    while {[incr retrynum -1]} {
        if {[catch {ping_server $host $port} ping]} {
            set ping 0
        }
        if {$ping} {return 1}
        after 50
    }
    return 0
}

proc ping_server {host port} {
    set retval 0
    if {[catch {
        set fd [socket $host $port]
        fconfigure $fd -translation binary
        puts $fd "PING\r\n"
        flush $fd

        set reply [gets $fd]
        if {[string range $reply 0 0] eq {+} ||
            [string range $reply 0 0] eq {-}} {
            set retval 1
        }
        close $fd
    } e]} {
        if {$::verbose} {
            puts -nonewline "."
        }
    } else {
        if {$::verbose} {
            puts -nonewline " Communication was good.\n"
        }
    }

    return $retval
}

proc kill_server config {
    if {[dict exists $config "client"]} {
        [dict get $config "client"] close
    }

    if {![dict exists $config "pid"]} {
        return
    }

    if {![is_alive $config]} {
        return
    }

    set pid [dict get $config pid]

    # Emit event server-killing
    if {$::verbose} {puts -nonewline "Server killing... "}
    send_data_packet $::test_server_fd server-killing $pid
    catch {exec kill $pid}

    # Node might have been stopped in the test
    catch {exec kill -SIGCONT $pid}

    set max_wait 10000
    while {[is_alive $config]} {
        incr wait 10
        if {$wait == $max_wait} {
            puts "Forcing process $pid to crash..."
            catch {exec kill -SEGV $pid}
        } elseif {$wait >= $max_wait * 2} {
            puts "Forcing process $pid to exit..."
            catch {exec kill -KILL $pid}
        } elseif {$wait % 1000 == 0} {
            puts "Waiting for process $pid to exit..."
        }
        after 10
    }

    # Emit event server killed
    send_data_packet $::test_server_fd server-killed $pid
    if {$::verbose} {puts "OK"}
}

proc srv {args} {
    set level 0
    if {[string is integer [lindex $args 0]]} {
        set level [lindex $args 0]
        set property [lindex $args 1]
    } else {
        set property [lindex $args 0]
    }

    dict get $::srv $property
}