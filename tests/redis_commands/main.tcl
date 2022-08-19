#!/usr/bin/tclsh

# Copyright (C) 2018-2022 Vito Castellano
# All rights reserved.
#
# This software may be modified and distributed under the terms
# of the BSD license.  See the LICENSE file for details.

package require Tcl 8.5
source helpers.tcl
source server.tcl
source client.tcl
source redis.tcl
source test.tcl

########################
# Suite CFG
########################
set ::tclsh [info nameofexecutable]
set ::dont_clean 0
set ::dump_logs 0
set ::verbose 1 ; # Use this static value for the moment
set ::tmproot "./tmp"
file mkdir $::tmproot

########################
# Server
########################
set ::tls 0
set ::srv {}
set ::server_host 127.0.0.1
set ::server_port 6380;
set ::server_pid 0;

set ::socket_host 127.0.0.1
set ::socket_port 21111

set ::server_path "../../cmake-build-debug/src/cachegrand-server"
set ::server_cfg "../../etc/cachegrand.yaml.skel"

########################
# Client
########################
set ::client 0
set ::numclients 1; # Use static value for the moment
set ::timeout 1200; # If this limit will reach quit the test.
set ::portcount 8000

########################
# Tests
########################
set ::test_path "./tests"
set ::num_tests 0
set ::num_passed 0
set ::num_failed 0
set ::num_skipped 0
set ::tests_failed {}
set ::cur_test ""
set ::curfile "";
set ::durable 1
set ::skiptests {}
set ::run_solo_tests {}
set ::last_progress [clock seconds]

set ::tags {}
set ::allowed_tags {}

set ::next_test 0
set ::all_tests {}

for {set j 0} {$j < [llength $argv]} {incr j} {
    set opt [lindex $argv $j]
    set arg [lindex $argv [expr $j+1]]
    if {$opt eq {--server_host}} {
        set ::server_host $arg
        incr j
    } elseif {$opt eq {--server_path}} {
        set ::server_path $arg
        incr j
    } elseif {$opt eq {--server_cfg}} {
        set ::server_cfg $arg
        incr j
    } elseif {$opt eq {--server_port}} {
        set ::server_port $arg
        incr j
    } elseif {$opt eq {--client}} {
        set ::client 1
        set ::socket_port $arg
        incr j
    } elseif {$opt eq {--test_path}} {
        set ::test_path $arg
        incr j
    } elseif {$opt eq {--tests}} {
        foreach test $arg {
            lappend ::all_tests $test
        }
        incr j
    } elseif {$opt eq {--allowed_tags}} {
        foreach tag $arg {
            lappend ::allowed_tags $tag
        }
        incr j
    } elseif {$opt eq {--dont_clean}} {
        set ::dont_clean 1
        incr j
    } else {
        puts "Wrong argument: $opt"
        exit 1
    }
}

if {$::client} {
    if {[catch { test_launcher $::socket_port } err]} {
        set estr "Executing test client: $err.\n$::errorInfo"
        if {[catch {send_data_packet $::test_server_fd exception $estr}]} {
            puts $estr
        }
         exit 1
    }

    exit 0
}

puts "************************************************"
puts "** Cachegrand - Commands Redis Tests Launcher **"
puts "- Founded [llength $::all_tests] test files to run!"
puts "************************************************"
if {[catch { spawn_client } err]} {
    if {[string length $err] > 0} {
        if {$err ne "exception"} {
            puts $::errorInfo
        }
        exit 1
    }
}
