proc test_launcher socket_port {
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

    linespacer
    puts  "** Executing test file: $::curfile"
    source $path
    send_data_packet $::test_server_fd done "$__testname"
}

proc execute_test_code {__testname filename code} {
    set ::curfile $filename

    linespacer
    puts "\n** Executing test code..."
    eval $code
    send_data_packet $::test_server_fd done "$__testname"
}

proc test_controller {code tags} {
    if {$code ne "undefined"} {
        set prev_num_failed $::num_failed
        set num_tests $::num_tests

        # Run test and catch!
        puts "\n** Ready to test!"
        if {[catch { uplevel 1 $code } error]} {
            set backtrace $::errorInfo
            if {$::durable} {
              lappend error_details $error
              lappend error_details $backtrace

              # Emit event error with details
              send_data_packet $::test_server_fd err [join $error_details "\n"]
            } else {
              error $error $backtrace
            }
        } else {
            if {$::dump_logs && $prev_num_failed != $::num_failed} {
                dump_server_log $::srv
            }
        }

        set ::tags [lrange $::tags 0 end-[llength $tags]]

        kill_server $::srv
        set _ ""
    } else {
        set ::tags [lrange $::tags 0 end-[llength $tags]]
        set _ $::srv
    }

    puts ""
}

proc test {name code {okpattern undefined} {tags {}}} {
    incr ::num_tests
    set details {}
    lappend details "$name in $::curfile"

    set prev_test $::cur_test
    set ::cur_test "$name in $::curfile"

    if {$::verbose} {
      set stdout [dict get $::srv stdout]
      set fd [open $stdout "a+"]
      puts $fd "### Starting test $::cur_test"
      close $fd
    }

    # Emit event testing
    send_data_packet $::test_server_fd testing $name

    set test_start_time [clock milliseconds]
    if {[catch {set retval [uplevel 1 $code]} error]} {
        # We are here if for example a command is not implemented
        set assertion [string match "assertion:*" $error]
        # Durable permit to continue to run other tests in case of error
        if {$assertion || $::durable} {
            lappend details $error
            lappend ::tests_failed $details
            incr ::num_failed

            # Emit event error
            send_data_packet $::test_server_fd err [join $details "\n"]
        } else {
            error $error $::errorInfo
        }
    } else {
        if {$okpattern eq "undefined" || $okpattern eq $retval || [string match $okpattern $retval]} {
            incr ::num_passed
            set elapsed [expr {[clock milliseconds]-$test_start_time}]

            # Emit event ok
            send_data_packet $::test_server_fd ok $name $elapsed
        } else {
            set msg "Expected '$okpattern' to equal or match '$retval'"
            lappend details $msg
            lappend ::tests_failed $details

            incr ::num_failed

            # Emit event error
            send_data_packet $::test_server_fd err [join $details "\n"]
        }
    }

    set ::cur_test $prev_test
}

proc r {args} {
    set level 0
    if {[string is integer [lindex $args 0]]} {
        set level [lindex $args 0]
        set args [lrange $args 1 end]
    }

    [srv $level "client"] {*}$args
}

proc tags {tags code} {
    set tags [string map { \" "" } $tags]
    set ::tags [concat $::tags $tags]

    uplevel 1 $code
    set ::tags [lrange $::tags 0 end-[llength $tags]]
}

########################################### TODO 👇

