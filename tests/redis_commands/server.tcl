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

    # Skip some tags
    if {![tags_acceptable $::tags err]} {
        incr ::num_aborted
        send_data_packet $::test_server_fd ignore $err
        set ::tags [lrange $::tags 0 end-[llength $tags]]
        return
    }

    #TODO: sistemare externali in questo caso
    if {$::external} {
        run_external_server_test $code $overrides

        set ::tags [lrange $::tags 0 end-[llength $tags]]
        return
    }

    set stdout [format "%s/%s" $::tmproot "stdout"]
    set stderr [format "%s/%s" $::tmproot "stderr"]

    # Check form stdout if server isn't started. In this case this value should be 0
    set previous_ready_count [count_message_lines $stdout "Ready to accept connections"]

    # Starting server
    set server_started 0
    while {$server_started == 0} {
        set server_started [spawn_server $tags $code $stdout $stderr]
    }

    # setup new cfg
    dict set srv "pid"    $::server_pid
    dict set srv "host"   $::server_host
    dict set srv "port"   $::server_port
    dict set srv "stdout" $stdout
    dict set srv "stderr" $stderr

    # if a block of code is supplied, we wait for the server to become
    # available, create a client object and kill the server afterwards
    if {$code ne "undefined"} {

        set line [exec head -n1 $stdout]
        if {[string match {*already in use*} $line]} {
            error_and_quit $config_file $line
        }

        while 1 {
            # check that the server actually started and is ready for connections
            if {[count_message_lines $stdout "Ready to accept connections"] > $previous_ready_count} {
                break
            }
            after 10
        }

        # append the server to the stack
        lappend ::servers $srv

        # connect client (after server dict is put on the stack)
        reconnect

        # remember previous num_failed to catch new errors
        set prev_num_failed $::num_failed



        # execute provided block
        set num_tests $::num_tests
        if {[catch { uplevel 1 $code } error]} {
            set backtrace $::errorInfo
            set assertion [string match "assertion:*" $error]



            # fetch srv back from the server list, in case it was restarted by restart_server (new PID)
            set srv [lindex $::servers end]

            # pop the server object
            set ::servers [lrange $::servers 0 end-1]

            # Kill the server without checking for leaks
            dict set srv "skipleaks" 1
            #TODO capire perchè bisogna killare server qui
#            kill_server $srv



            if {$::dump_logs && $assertion} {
                # if we caught an assertion ($::num_failed isn't incremented yet)
                # this happens when the test spawns a server and not the other way around
                dump_server_log $srv
            } else {



                # Print crash report from log
                set crashlog [crashlog_from_file [dict get $srv "stdout"]]
                if {[string length $crashlog] > 0} {
                    puts [format "\nLogged crash report (pid %d):" [dict get $srv "pid"]]
                    puts "$crashlog"
                    puts ""
                }



                set sanitizerlog [sanitizer_errors_from_file [dict get $srv "stderr"]]
                if {[string length $sanitizerlog] > 0} {
                    puts [format "\nLogged sanitizer errors (pid %d):" [dict get $srv "pid"]]
                    puts "$sanitizerlog"
                    puts ""
                }
            }

            if {!$assertion && $::durable} {
                # durable is meant to prevent the whole tcl test from exiting on
                # an exception. an assertion will be caught by the test proc.
                set msg [string range $error 10 end]
                lappend details $msg
                lappend details $backtrace
                lappend ::tests_failed $details

                incr ::num_failed
                send_data_packet $::test_server_fd err [join $details "\n"]
            } else {
                # Re-raise, let handler up the stack take care of this.
                error $error $backtrace
            }
        } else {
            if {$::dump_logs && $prev_num_failed != $::num_failed} {
                dump_server_log $srv
            }
        }

        # fetch srv back from the server list, in case it was restarted by restart_server (new PID)
        set srv [lindex $::servers end]

        # Don't do the leak check when no tests were run
        if {$num_tests == $::num_tests} {
            dict set srv "skipleaks" 1
        }



        # pop the server object
        set ::servers [lrange $::servers 0 end-1]

        set ::tags [lrange $::tags 0 end-[llength $tags]]


        kill_server $srv

        #TODO DA FARE ASSOLUTAMENTE!!
#        if {!$keep_persistence} {
#            clean_persistence $srv
#        }

        set _ ""
    } else {
        set ::tags [lrange $::tags 0 end-[llength $tags]]
        set _ $srv
    }
}

proc spawn_server {tags code stdout stderr} {
    puts "** Spawn server \[$::server_host@$::server_port\]"

    # Emit event server-spawning
    send_data_packet $::test_server_fd "server-spawning" "port $::server_port"

    # TODO pass binary and conf via conf
    if {$::verbose} {puts -nonewline "Spawing... "}
    set pid [exec ../../cmake-build-debug/src/cachegrand-server -c /tmp/cachegrand/cachegrand-conf.yml >> $stdout 2>> $stderr &]
    if {$::verbose} {puts "OK Server spawned with pid \[$pid\]"}

    # Emit event server-spawned
    send_data_packet $::test_server_fd server-spawned $pid

    # Check if server is truly up
    set retrynum 100
    if {$code ne "undefined"} {
        set serverisup [server_is_up $::server_host $::server_port $retrynum]
    } else {
        set serverisup 1
    }

    if {!$serverisup} {
        error "Probably there are some errors with server :("
    }

    set $::server_pid $pid
    return 1
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




#################################################### TODO 👇





proc run_external_server_test {code overrides} {
    set srv {}
    dict set srv "host" $::server_host
    dict set srv "port" $::server_port
    set client [redis $::server_host $::server_port 0 $::tls]
    dict set srv "client" $client
#    if {!$::singledb} {
#        $client select 9
#    }

    set config {}
    dict set config "port" $::server_port
    dict set srv "config" $config

    # append the server to the stack
    lappend ::servers $srv

    if {[llength $::servers] > 1} {
        if {$::verbose} {
            puts "Notice: nested start_server statements in external server mode, test must be aware of that!"
        }
    }

#    r flushall
#    r function flush

    # store overrides
    set saved_config {}
    foreach {param val} $overrides {
        dict set saved_config $param [lindex [r config get $param] 1]
        r config set $param $val

        # If we enable appendonly, wait for for rewrite to complete. This is
        # required for tests that begin with a bg* command which will fail if
        # the rewriteaof operation is not completed at this point.
        if {$param == "appendonly" && $val == "yes"} {
            waitForBgrewriteaof r
        }
    }

    if {[catch {set retval [uplevel 2 $code]} error]} {
        if {$::durable} {
            set msg [string range $error 10 end]
            lappend details $msg
            lappend details $::errorInfo
            lappend ::tests_failed $details

            incr ::num_failed
            send_data_packet $::test_server_fd err [join $details "\n"]
        } else {
            # Re-raise, let handler up the stack take care of this.
            error $error $::errorInfo
        }
    }

    # restore overrides
    dict for {param val} $saved_config {
        r config set $param $val
    }

    set srv [lpop ::servers]

    if {[dict exists $srv "client"]} {
        [dict get $srv "client"] close
    }
}

proc kill_server config {
    # nothing to kill when running against external server
    if {$::external} return

    # Close client connection if exists
    if {[dict exists $config "client"]} {
        [dict get $config "client"] close
    }

    # nevermind if its already dead
    if {![is_alive $config]} {
        # Check valgrind errors if needed
#        if {$::valgrind} {
#            check_valgrind_errors [dict get $config stderr]
#        }

        check_sanitizer_errors [dict get $config stderr]
        return
    }
    set pid [dict get $config pid]

    # check for leaks
    if {![dict exists $config "skipleaks"]} {
        catch {
            if {[string match {*Darwin*} [exec uname -a]]} {
                tags {"leaks"} {
                    test "Check for memory leaks (pid $pid)" {
                        set output {0 leaks}
                        catch {exec leaks $pid} output option
                        # In a few tests we kill the server process, so leaks will not find it.
                        # It'll exits with exit code >1 on error, so we ignore these.
                        if {[dict exists $option -errorcode]} {
                            set details [dict get $option -errorcode]
                            if {[lindex $details 0] eq "CHILDSTATUS"} {
                                  set status [lindex $details 2]
                                  if {$status > 1} {
                                      set output "0 leaks"
                                  }
                            }
                        }
                        set output
                    } {*0 leaks*}
                }
            }
        }
    }

    # kill server and wait for the process to be totally exited
    send_data_packet $::test_server_fd server-killing $pid
    catch {exec kill $pid}
    # Node might have been stopped in the test
    catch {exec kill -SIGCONT $pid}
#    if {$::valgrind} {
#        set max_wait 120000
#    } else {
        set max_wait 10000
#    }

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

    # Check valgrind errors if needed
#    if {$::valgrind} {
#        check_valgrind_errors [dict get $config stderr]
#    }

    check_sanitizer_errors [dict get $config stderr]

    # Remove this pid from the set of active pids in the test server.
    send_data_packet $::test_server_fd server-killed $pid
}

# Wait for actual startup, return 1 if port is busy, 0 otherwise
proc wait_server_started {stdout pid} {
    set checkperiod 100; # Milliseconds
    set maxiter [expr {120*1000/$checkperiod}] ; # Wait up to 2 minutes.
    set port_busy 0
    while 1 {
        if {[regexp -- " PID: $pid" [exec cat $stdout]]} {
            break
        }

        after $checkperiod
        incr maxiter -1
        if {$maxiter == 0} {
            puts "No PID detected in log $stdout"
            puts "--- LOG CONTENT ---"
            puts [exec cat $stdout]
            puts "-------------------"
            break
        }

        # Check if the port is actually busy and the server failed
        # for this reason.
        if {[regexp {Failed listening on port} [exec cat $stdout]]} {
            set port_busy 1
            break
        }
    }

    return $port_busy
}



# Setup a list to hold a stack of server configs. When calls to start_server
# are nested, use "srv 0 pid" to get the pid of the inner server. To access
# outer servers, use "srv -1 pid" etcetera.
set ::servers {}
proc srv {args} {
    set level 0
    if {[string is integer [lindex $args 0]]} {
        set level [lindex $args 0]
        set property [lindex $args 1]
    } else {
        set property [lindex $args 0]
    }

    set srv [lindex $::servers end+$level]

    dict get $srv $property
}


proc reconnect {args} {
    set level [lindex $args 0]
    if {[string length $level] == 0 || ![string is integer $level]} {
        set level 0
    }

    set srv [lindex $::servers end+$level]
    set host [dict get $srv "host"]
    set port [dict get $srv "port"]
#    set config [dict get $srv "config"]
    set client [redis $host $port 0 $::tls]
    if {[dict exists $srv "client"]} {
        set old [dict get $srv "client"]
        $old close
    }
    dict set srv "client" $client

    # re-set $srv in the servers list
    lset ::servers end+$level $srv
}

proc force_kill_all_servers {} {
    foreach p $::active_servers {
        puts "Killing still running Redis server $p"
        catch {exec kill -9 $p}
    }
}