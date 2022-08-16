proc tests_launcher socket_port {
    puts "** Test Launcher"

    if {$::verbose} { puts "Trying to comunicate using socket client... "}
    set ::test_server_fd [socket $::socket_host $socket_port]
    fconfigure $::test_server_fd -encoding binary

    if {$::verbose} { puts "Sending ready status... "}
    send_data_packet $::test_server_fd ready [pid]

    while 1 {
        set bytes [gets $::test_server_fd]
        set payload [read $::test_server_fd $bytes]
        foreach {cmd data} $payload break

        if {$cmd eq {run}} {
            execute_test_file $data
        } elseif {$cmd eq {run_code}} {
            foreach {name filename code} $data break
            execute_test_code $name $filename $code
        } else {
            error "Unknown test client command: $cmd"
        }
    }
}

proc execute_test_file __testname {
    set path "$::test_path/$__testname.tcl"
    set ::curfile $path

    puts "\n** Executing test file: $::curfile"
    source $path
    send_data_packet $::test_server_fd done "$__testname"
}

proc execute_test_code {__testname filename code} {
    set ::curfile $filename

    puts "\n** Executing test code..."
    eval $code
    send_data_packet $::test_server_fd done "$__testname"
}









###################################### TODO: 👇


proc test {name code {okpattern undefined} {tags {}}} {
    # abort if test name in skiptests
    if {[search_pattern_list $name $::skiptests]} {
        incr ::num_skipped
        send_data_packet $::test_server_fd skip $name
        return
    }

    # TODO da ripristinare
    # abort if only_tests was set but test name is not included
#    if {[llength $::only_tests] > 0 && ![search_pattern_list $name $::only_tests]} {
#        incr ::num_skipped
#        send_data_packet $::test_server_fd skip $name
#        return
#    }

    set tags [concat $::tags $tags]
    if {![tags_acceptable $tags err]} {
        incr ::num_aborted
        send_data_packet $::test_server_fd ignore "$name: $err"
        return
    }

    incr ::num_tests
    set details {}
    lappend details "$name in $::curfile"

    # set a cur_test global to be logged into new servers that are spawn
    # and log the test name in all existing servers
    set prev_test $::cur_test
    set ::cur_test "$name in $::curfile"
    if {$::external} {
        catch {
            set r [redis [srv 0 host] [srv 0 port] 0 $::tls]
            catch {
                $r debug log "### Starting test $::cur_test"
            }
            $r close
        }
    } else {
        foreach srv $::servers {
            set stdout [dict get $srv stdout]
            set fd [open $stdout "a+"]
            puts $fd "### Starting test $::cur_test"
            close $fd
        }
    }



    send_data_packet $::test_server_fd testing $name


    set test_start_time [clock milliseconds]
    if {[catch {set retval [uplevel 1 $code]} error]} {
        set assertion [string match "assertion:*" $error]
        if {$assertion || $::durable} {

            # QUI ENTRO SE FALLISCO

            # durable prevents the whole tcl test from exiting on an exception.
            # an assertion is handled gracefully anyway.
            set msg [string range $error 10 end]
            lappend details $msg
            if {!$assertion} {
                lappend details $::errorInfo
            }
            lappend ::tests_failed $details

            incr ::num_failed
            send_data_packet $::test_server_fd err [join $details "\n"]

#            if {$::stop_on_failure} {
#                puts "Test error (last server port:[srv port], log:[srv stdout]), press enter to teardown the test."
#                flush stdout
#                gets stdin
#            }
        } else {
            # Re-raise, let handler up the stack take care of this.
            error $error $::errorInfo
        }
    } else {

        if {$okpattern eq "undefined" || $okpattern eq $retval || [string match $okpattern $retval]} {
            incr ::num_passed
            set elapsed [expr {[clock milliseconds]-$test_start_time}]
            send_data_packet $::test_server_fd ok $name $elapsed
        } else {
            set msg "Expected '$okpattern' to equal or match '$retval'"
            lappend details $msg
            lappend ::tests_failed $details

            incr ::num_failed
            send_data_packet $::test_server_fd err [join $details "\n"]
        }
    }

#    if {$::traceleaks} {
#        set output [exec leaks redis-server]
#        if {![string match {*0 leaks*} $output]} {
#            send_data_packet $::test_server_fd err "Detected a memory leak in test '$name': $output"
#        }
#    }

    set ::cur_test $prev_test
}

# Provide easy access to the client for the inner server. It's possible to
# prepend the argument list with a negative level to access clients for
# servers running in outer blocks.
proc r {args} {
    set level 0
    if {[string is integer [lindex $args 0]]} {
        set level [lindex $args 0]
        set args [lrange $args 1 end]
    }

    [srv $level "client"] {*}$args
}



