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

###################################### TODO: 👇










# Test if TERM looks like to support colors
proc color_term {} {
    expr {[info exists ::env(TERM)] && [string match *xterm* $::env(TERM)]}
}

proc colorstr {color str} {
    if {[color_term]} {
        set b 0
        if {[string range $color 0 4] eq {bold-}} {
            set b 1
            set color [string range $color 5 end]
        }
        switch $color {
            red {set colorcode {31}}
            green {set colorcode {32}}
            yellow {set colorcode {33}}
            blue {set colorcode {34}}
            magenta {set colorcode {35}}
            cyan {set colorcode {36}}
            white {set colorcode {37}}
            default {set colorcode {37}}
        }
        if {$colorcode ne {}} {
            return "\033\[$b;${colorcode};49m$str\033\[0m"
        }
    } else {
        return $str
    }
}

# Check if current ::tags match requested tags. If ::allowtags are used,
# there must be some intersection. If ::denytags are used, no intersection
# is allowed. Returns 1 if tags are acceptable or 0 otherwise, in which
# case err_return names a return variable for the message to be logged.
proc tags_acceptable {tags err_return} {
    upvar $err_return err

    # If tags are whitelisted, make sure there's match
    if {[llength $::allowtags] > 0} {
        set matched 0
        foreach tag $::allowtags {
            if {[lsearch $tags $tag] >= 0} {
                incr matched
            }
        }
        if {$matched < 1} {
            set err "Tag: none of the tags allowed"
            return 0
        }
    }

    foreach tag $::denytags {
        if {[lsearch $tags $tag] >= 0} {
            set err "Tag: $tag denied"
            return 0
        }
    }

#    if {$::external && [lsearch $tags "external:skip"] >= 0} {
#        set err "Not supported on external server"
#        return 0
#    }

#    if {$::singledb && [lsearch $tags "singledb:skip"] >= 0} {
#        set err "Not supported on singledb"
#        return 0
#    }

#    if {$::cluster_mode && [lsearch $tags "cluster:skip"] >= 0} {
#        set err "Not supported in cluster mode"
#        return 0
#    }

#    if {$::tls && [lsearch $tags "tls:skip"] >= 0} {
#        set err "Not supported in tls mode"
#        return 0
#    }

#    if {!$::large_memory && [lsearch $tags "large-memory"] >= 0} {
#        set err "large memory flag not provided"
#        return 0
#    }

    return 1
}

# returns the number of times a line with that pattern appears in a file
proc count_message_lines {file pattern} {
    set res 0
    # exec fails when grep exists with status other than 0 (when the patter wasn't found)
    catch {
        set res [string trim [exec grep $pattern $file 2> /dev/null | wc -l]]
    }
    return $res
}

proc is_alive config {
    set pid [dict get $config pid]
    if {[catch {exec kill -0 $pid} err]} {
        return 0
    } else {
        return 1
    }
}

# Return all log lines starting with the first line that contains a warning.
# Generally, this will be an assertion error with a stack trace.
proc crashlog_from_file {filename} {
    set lines [split [exec cat $filename] "\n"]
    set matched 0
    set logall 0
    set result {}
    foreach line $lines {
        if {[string match {*REDIS BUG REPORT START*} $line]} {
            set logall 1
        }
        if {[regexp {^\[\d+\]\s+\d+\s+\w+\s+\d{2}:\d{2}:\d{2} \#} $line]} {
            set matched 1
        }
        if {$logall || $matched} {
            lappend result $line
        }
    }
    join $result "\n"
}

# Return sanitizer log lines
proc sanitizer_errors_from_file {filename} {
    set log [exec cat $filename]
    set lines [split [exec cat $filename] "\n"]

    foreach line $lines {
        # Ignore huge allocation warnings
        if ([string match {*WARNING: AddressSanitizer failed to allocate*} $line]) {
            continue
        }

        # GCC UBSAN output does not contain 'Sanitizer' but 'runtime error'.
        if {[string match {*runtime error*} $log] ||
            [string match {*Sanitizer*} $log]} {
            return $log
        }
    }

    return ""
}

proc check_sanitizer_errors stderr {
    set res [sanitizer_errors_from_file $stderr]
    if {$res != ""} {
        send_data_packet $::test_server_fd err "Sanitizer error: $res\n"
    }
}

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
        puts "\n[colorstr bold-red {!!! WARNING}] The following tests failed:\n"
        foreach failed $::failed_tests {
            puts "*** $failed"
        }
        if {!$::dont_clean} cleanup
        exit 1
    } else {
        puts "\n[colorstr bold-white {\o/}] [colorstr bold-green {All tests passed without errors!}]\n"
        if {!$::dont_clean} cleanup
        exit 0
    }
}

proc send_data_packet {fd status data {elapsed 0}} {
    set payload [list $status $data $elapsed]
    puts $fd [string length $payload]
    puts -nonewline $fd $payload
    flush $fd
}

# try to match a value to a list of patterns that are either regex (starts with "/") or plain string.
# The caller can specify to use only glob-pattern match
proc search_pattern_list {value pattern_list {glob_pattern false}} {
    foreach el $pattern_list {
        if {[string length $el] == 0} { continue }
        if { $glob_pattern } {
            if {[string match $el $value]} {
                return 1
            }
            continue
        }
        if {[string equal / [string index $el 0]] && [regexp -- [string range $el 1 end] $value]} {
            return 1
        } elseif {[string equal $el $value]} {
            return 1
        }
    }
    return 0
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

proc lremove {listVar value} {
    upvar 1 $listVar var
    set idx [lsearch -exact $var $value]
    set var [lreplace $var $idx $idx]
}