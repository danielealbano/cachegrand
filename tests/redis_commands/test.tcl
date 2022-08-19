# Copyright (C) 2018-2022 Vito Castellano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

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

    linespacer "#"
    puts  "** Executing test file: $::curfile"
    source $path
    send_data_packet $::test_server_fd done "$__testname"
}

proc execute_test_code {__testname filename code} {
    set ::curfile $filename

    linespacer "#"
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
              if {$::dump_logs} {
                  dump_server_log $::srv
              }

              error $error $backtrace
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
    set tags [concat $::tags $tags]
    if {![tags_acceptable $tags err]} {
        incr ::num_aborted
        # Emit event ingore for logging [skipped at the moment]
#        send_data_packet $::test_server_fd ignore "$name: $err"
        return
    }

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
        set cmd_not_implemented [string match "Command not implemented!" $error]

        # Durable permit to continue to run other tests in case of error
        if {$assertion || $::durable} {
            lappend details $error
            lappend ::tests_failed $details
            incr ::num_failed

            # Emit event error
            send_data_packet $::test_server_fd err [join $details "\n"]

            if {!$cmd_not_implemented} {
                linespacer "+"
                puts "Owhh No! Bad error occurred!"
                puts "Check if the server is still alive..."
                set serverisup [server_is_up $::server_host $::server_port 10]
                if {!$serverisup} {
                    puts "Server goes away"
                    set server_started 0
                    while {$server_started == 0} {
                        if {$code ne "undefined"} {
                          set server_started [spawn_server]
                        } else {
                          set server_started 1
                        }
                    }
                }
                linespacer "+"
            }
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

#######################################################################
# Asserts Check List
#######################################################################
proc fail {msg} {
    error "assertion:$msg"
}

proc assert {condition} {
    if {![uplevel 1 [list expr $condition]]} {
        set context "(context: [info frame -1])"
        error "assertion:Expected [uplevel 1 [list subst -nocommands $condition]] $context"
    }
}

proc assert_no_match {pattern value} {
    if {[string match $pattern $value]} {
        set context "(context: [info frame -1])"
        error "assertion:Expected '$value' to not match '$pattern' $context"
    }
}

proc assert_match {pattern value {detail ""}} {
    if {![string match $pattern $value]} {
        set context "(context: [info frame -1])"
        error "assertion:Expected '$value' to match '$pattern' $context $detail"
    }
}

proc assert_failed {expected_err detail} {
     if {$detail ne ""} {
        set detail "(detail: $detail)"
     } else {
        set detail "(context: [info frame -2])"
     }
     error "assertion:$expected_err $detail"
}

proc assert_not_equal {value expected {detail ""}} {
    if {!($expected ne $value)} {
        assert_failed "Expected '$value' not equal to '$expected'" $detail
    }
}

proc assert_equal {value expected {detail ""}} {
    if {$expected ne $value} {
        assert_failed "Expected '$value' to be equal to '$expected'" $detail
    }
}

proc assert_lessthan {value expected {detail ""}} {
    if {!($value < $expected)} {
        assert_failed "Expected '$value' to be less than '$expected'" $detail
    }
}

proc assert_lessthan_equal {value expected {detail ""}} {
    if {!($value <= $expected)} {
        assert_failed "Expected '$value' to be less than or equal to '$expected'" $detail
    }
}

proc assert_morethan {value expected {detail ""}} {
    if {!($value > $expected)} {
        assert_failed "Expected '$value' to be more than '$expected'" $detail
    }
}

proc assert_morethan_equal {value expected {detail ""}} {
    if {!($value >= $expected)} {
        assert_failed "Expected '$value' to be more than or equal to '$expected'" $detail
    }
}

proc assert_range {value min max {detail ""}} {
    if {!($value <= $max && $value >= $min)} {
        assert_failed "Expected '$value' to be between to '$min' and '$max'" $detail
    }
}

proc assert_error {pattern code {detail ""}} {
    if {[catch {uplevel 1 $code} error]} {
        assert_match $pattern $error $detail
    } else {
        assert_failed "Expected an error matching '$pattern' but got '$error'" $detail
    }
}

proc assert_encoding {enc key} {
    set val [r object encoding $key]
    assert_match $enc $val
}

proc assert_type {type key} {
    assert_equal $type [r type $key]
}

proc assert_refcount {ref key} {
    set val [r object refcount $key]
    assert_equal $ref $val
}