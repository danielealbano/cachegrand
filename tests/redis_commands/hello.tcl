#!/usr/bin/tclsh

package require Tcl 8.5

set ::host "127.0.0.1"
set ::base_port 30000


# Execute the specified instance of the server specified by 'type', using
# the provided configuration file. Returns the PID of the process.
proc exec_instance {type dirname cfgfile} {
    if {$type eq "redis"} {
        set prgname cachegrand-server
    } else {
        error "Unknown instance type."
    }


    # /tmp/cachegrand/cmake-build-debug/src/cachegrand-server -c /tmp/cachegrand/cachegrand-conf.yml
    #TODO capire qui
    set errfile [file join $dirname err.txt]
    set pid [exec ../../cmake-build-debug/src/${prgname} -c $cfgfile 2>> $errfile &]

    return $pid
}

# Spawn a redis or sentinel instance, depending on 'type'.
proc spawn_instance {type base_port {conf {}} {base_conf_file ""}} {
    puts "$type $base_port $base_conf_file"

    # Create a directory for this instance.
    set dirname "${type}_instace"
    lappend ::dirs $dirname
    catch {exec rm -rf $dirname}
    file mkdir $dirname

    # Write the instance config file.
    set cfgfile [file join $dirname $type.yaml]
    if {$base_conf_file ne ""} {
        file copy -- $base_conf_file $cfgfile
        set cfg [open $cfgfile a+]
    } else {
        set cfg [open $cfgfile w]
    }

#    puts $cfg "port $port"
#    puts $cfg "dir ./$dirname"
#    puts $cfg "logfile log.txt"
#
#    # Add additional config files
#    foreach directive $conf {
#        puts $cfg $directive
#    }
#    dict for {name val} $::global_config {
#        puts $cfg "$name $val"
#    }
#    close $cfg

    # Finally exec it and remember the pid for later cleanup.
    set retry 2
    while {$retry} {
        set pid [exec_instance $type $dirname $cfgfile]

        # Check availability
        if {[server_is_up 127.0.0.1 $port 100] == 0} {
            puts "Starting $type at port $port failed, try another"
            incr retry -1
            set port [find_available_port $base_port $::redis_port_count]
            set cfg [open $cfgfile a+]
            puts $cfg "port $port"
            close $cfg
        } else {
            puts "Starting $type at port $port"
            lappend ::pids $pid
            break
        }
    }

exit 1;

    # Check availability finally
    if {[server_is_up $::host $port 100] == 0} {
        set logfile [file join $dirname log.txt]
        puts [exec tail $logfile]
        abort_sentinel_test "Problems starting $type #$j: ping timeout, maybe server start failed, check $logfile"
    }

    # Push the instance into the right list
    set link [redis $::host $port 0 $::tls]
    $link reconnect 1
    lappend ::${type}_instances [list \
        pid $pid \
        host $::host \
        port $port \
        plaintext-port $pport \
        link $link \
    ]

}

####################################################################################

# Execute all the units inside the 'tests' directory.
#proc run_tests {} {
#    set tests [lsort [glob ../tests/*]]
#
#while 1 {
#    foreach test $tests {
#        # Remove leaked_fds file before starting
#        if {$::leaked_fds_file != "" && [file exists $::leaked_fds_file]} {
#            file delete $::leaked_fds_file
#        }
#
#        if {[llength $::run_matching] != 0 && ![search_pattern_list $test $::run_matching true]} {
#            continue
#        }
#        if {[file isdirectory $test]} continue
#        puts [colorstr yellow "Testing unit: [lindex [file split $test] end]"]
#        if {[catch { source $test } err]} {
#            puts "FAILED: caught an error in the test $err"
#            puts $::errorInfo
#            incr ::failed
#            # letting the tests resume, so we'll eventually reach the cleanup and report crashes
#
#            if {$::stop_on_failure} {
#                puts -nonewline "(Test stopped, press enter to resume the tests)"
#                flush stdout
#                gets stdin
#            }
#        }
#        check_leaks {redis sentinel}
#
#        # Check if a leaked fds file was created and abort the test.
#        if {$::leaked_fds_file != "" && [file exists $::leaked_fds_file]} {
#            puts [colorstr red "ERROR: Sentinel has leaked fds to scripts:"]
#            puts [exec cat $::leaked_fds_file]
#            puts "----"
#            incr ::failed
#        }
#    }
#
#    if {$::loop == 0} { break }
#} ;# while 1
#}
#
## Print a message and exists with 0 / 1 according to zero or more failures.
#proc end_tests {} {
#    if {$::failed == 0 } {
#        puts [colorstr green "GOOD! No errors."]
#        exit 0
#    } else {
#        puts [colorstr red "WARNING $::failed test(s) failed."]
#        exit 1
#    }
#}
#
#proc cleanup {} {
#    puts "Cleaning up..."
#    foreach pid $::pids {
#        puts "killing stale instance $pid"
#        stop_instance $pid
#    }
#    log_crashes
#    if {$::dont_clean} {
#        return
#    }
#    foreach dir $::dirs {
#        catch {exec rm -rf $dir}
#    }
#}

####################################################################################

proc main {} {
    puts "Starting..."

    # TODO
    #parse_options

    spawn_instance cachengrad $::base_port {
        "cluster-enabled yes"
    } "../../etc/cachegrand.yaml.skel"

#    run_tests
#    cleanup
#    end_tests
}

if {[catch main e]} {
    puts $::errorInfo
    #if {$::pause_on_error} pause_on_error
    #cleanup
    exit 1
}

