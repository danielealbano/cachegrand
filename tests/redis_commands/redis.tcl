set ::redis_id 0
array set ::redis_fd {}
array set ::redis_addr {}
array set ::redis_blocking {}
array set ::redis_deferred {}
array set ::redis_readraw {}
array set ::redis_attributes {}
array set ::redis_reconnect {}
array set ::redis_tls {}
array set ::redis_callback {}
array set ::redis_state {}
array set ::redis_statestack {}

proc redis {{server 127.0.0.1} {port 6380} {defer 0} {tls 0} {tlsoptions {}} {readraw 0}} {
    set fd [socket $server $port]
    fconfigure $fd -translation binary

    set id [incr ::redis_id]
    set ::redis_fd($id) $fd
    set ::redis_addr($id) [list $server $port]
    set ::redis_blocking($id) 1
    set ::redis_deferred($id) $defer
    set ::redis_readraw($id) $readraw
    set ::redis_reconnect($id) 0
    set ::redis_tls($id) $tls

    redis_reset_state $id
    interp alias {} ::redis_redisHandle$id {} ::redis___dispatch__ $id
}

proc redis_safe_read {fd len} {
    if {$len == -1} {
        set err [catch {set val [read $fd]} msg]
    } else {
        set err [catch {set val [read $fd $len]} msg]
    }

    if {!$err} {
        return $val
    }

    if {[string match "*connection abort*" $msg]} {
        return {}
    }

    error $msg
}

proc redis_safe_gets {fd} {
    if {[catch {set val [gets $fd]} msg]} {
        if {[string match "*connection abort*" $msg]} {
            return {}
        }
        error $msg
    }
    return $val
}


proc redis___dispatch__ {id method args} {
    set errorcode [catch {::redis___dispatch__raw__ $id $method $args} retval]
    if {$errorcode && $::redis_reconnect($id) && $::redis_fd($id) eq {}} {
        set errorcode [catch {::redis___dispatch__raw__ $id $method $args} retval]
    }

    return -code $errorcode $retval
}

proc redis___dispatch__raw__ {id method argv} {
    set fd $::redis_fd($id)

    # Reconnect the link if needed.
    if {$fd eq {} && $method ne {close}} {
        lassign $::redis_addr($id) host port
        set ::redis_fd($id) [socket $host $port]

        fconfigure $::redis_fd($id) -translation binary
        set fd $::redis_fd($id)
    }

    set blocking $::redis_blocking($id)
    set deferred $::redis_deferred($id)
    if {$blocking == 0} {
        if {[llength $argv] == 0} {
            error "Please provide a callback in non-blocking mode"
        }
        set callback [lindex $argv end]
        set argv [lrange $argv 0 end-1]
    }

    if {[info command ::redis___method__$method] eq {}} {
        catch {unset ::redis_attributes($id)}
        set cmd "*[expr {[llength $argv]+1}]\r\n"
        append cmd "$[string length $method]\r\n$method\r\n"
        foreach a $argv {
            append cmd "$[string length $a]\r\n$a\r\n"
        }

        redis_write $fd $cmd
        if {[catch {flush $fd}]} {
            catch {close $fd}
            set ::redis_fd($id) {}
            return -code error "I/O error reading reply"
        }

        if {!$deferred} {
            if {$blocking} {
                redis_read_reply $id $fd
            } else {
                # Every well formed reply read will pop an element from this
                # list and use it as a callback. So pipelining is supported
                # in non blocking mode.
                lappend ::redis_callback($id) $callback
                fileevent $fd readable [list redis_readable $fd $id]
            }
        }
    } else {
        uplevel 1 [list ::redis___method__$method $id $fd] $argv
    }
}

proc redis___method__blocking {id fd val} {
    set ::redis_blocking($id) $val
    fconfigure $fd -blocking $val
}

proc redis___method__reconnect {id fd val} {
    set ::redis_reconnect($id) $val
}

proc redis___method__read {id fd} {
    redis_read_reply $id $fd
}

proc redis___method__rawread {id fd {len -1}} {
    return [redis_safe_read $fd $len]
}

proc redis___method__write {id fd buf} {
    redis_write $fd $buf
}

proc redis___method__flush {id fd} {
    flush $fd
}

proc redis___method__close {id fd} {
    catch {close $fd}
    catch {unset ::redis_fd($id)}
    catch {unset ::redis_addr($id)}
    catch {unset ::redis_blocking($id)}
    catch {unset ::redis_deferred($id)}
    catch {unset ::redis_readraw($id)}
    catch {unset ::redis_attributes($id)}
    catch {unset ::redis_reconnect($id)}
    catch {unset ::redis_tls($id)}
    catch {unset ::redis_state($id)}
    catch {unset ::redis_statestack($id)}
    catch {unset ::redis_callback($id)}
    catch {interp alias {} ::redis_redisHandle$id {}}
}

proc redis___method__channel {id fd} {
    return $fd
}

proc redis___method__deferred {id fd val} {
    set ::redis_deferred($id) $val
}

proc redis___method__readraw {id fd val} {
    set ::redis_readraw($id) $val
}

proc redis___method__readingraw {id fd} {
    return $::redis_readraw($id)
}

proc redis___method__attributes {id fd} {
    set _ $::redis_attributes($id)
}

proc redis_write {fd buf} {
    puts -nonewline $fd $buf
}

proc redis_writenl {fd buf} {
    redis_write $fd $buf
    redis_write $fd "\r\n"
    flush $fd
}

proc redis_readnl {fd len} {
    set buf [redis_safe_read $fd $len]
    redis_safe_read $fd 2 ; # discard CR LF
    return $buf
}

proc redis_bulk_read {fd} {
    set count [redis_read_line $fd]
    if {$count == -1} return {}
    set buf [redis_readnl $fd $count]
    return $buf
}

proc redis_multi_bulk_read {id fd} {
    set count [redis_read_line $fd]
    if {$count == -1} return {}
    set l {}
    set err {}
    for {set i 0} {$i < $count} {incr i} {
        if {[catch {
            lappend l [redis_read_reply $id $fd]
        } e] && $err eq {}} {
            set err $e
        }
    }
    if {$err ne {}} {return -code error $err}
    return $l
}

proc redis_read_map {id fd} {
    set count [redis_read_line $fd]
    if {$count == -1} return {}
    set d {}
    set err {}
    for {set i 0} {$i < $count} {incr i} {
        if {[catch {
            set k [redis_read_reply $id $fd] ; # key
            set v [redis_read_reply $id $fd] ; # value
            dict set d $k $v
        } e] && $err eq {}} {
            set err $e
        }
    }
    if {$err ne {}} {return -code error $err}
    return $d
}

proc redis_read_line fd {
    string trim [redis_safe_gets $fd]
}

proc redis_read_null fd {
    redis_safe_gets $fd
    return {}
}

proc redis_read_bool fd {
    set v [redis_read_line $fd]
    if {$v == "t"} {return 1}
    if {$v == "f"} {return 0}
    return -code error "Bad protocol, '$v' as bool type"
}

proc redis_read_verbatim_str fd {
    set v [redis_bulk_read $fd]
    # strip the first 4 chars ("txt:")
    return [string range $v 4 end]
}

proc redis_read_reply {id fd} {
    if {$::redis_readraw($id)} {
        return [redis_read_line $fd]
    }

    while {1} {
        set type [redis_safe_read $fd 1]
        switch -exact -- $type {
            _ {return [redis_read_null $fd]}
            : -
            ( -
            + {return [redis_read_line $fd]}
            , {return [expr {double([redis_read_line $fd])}]}
            # {return [redis_read_bool $fd]}
            = {return [redis_read_verbatim_str $fd]}
            - {return -code error [redis_read_line $fd]}
            $ {return [redis_bulk_read $fd]}
            > -
            ~ -
            * {return [redis_multi_bulk_read $id $fd]}
            % {return [redis_read_map $id $fd]}
            | {
                set attrib [redis_read_map $id $fd]
                set ::redis_attributes($id) $attrib
                continue
            }
            default {
                if {$type eq {}} {
                    catch {close $fd}
                    set ::redis_fd($id) {}
                    return -code error "I/O error reading reply"
                }
                return -code error "Bad protocol, '$type' as reply type byte"
            }
        }
    }
}

proc redis_reset_state id {
    set ::redis_state($id) [dict create buf {} mbulk -1 bulk -1 reply {}]
    set ::redis_statestack($id) {}
}

proc redis_call_callback {id type reply} {
    set cb [lindex $::redis_callback($id) 0]
    set ::redis_callback($id) [lrange $::redis_callback($id) 1 end]
    uplevel #0 $cb [list ::redis_redisHandle$id $type $reply]
    redis_reset_state $id
}

proc redis_readable {fd id} {
    if {[eof $fd]} {
        redis_call_callback $id eof {}
        ::redis___method__close $id $fd
        return
    }

    if {[dict get $::redis_state($id) bulk] == -1} {
        set line [gets $fd]
        if {$line eq {}} return ;# No complete line available, return
        switch -exact -- [string index $line 0] {
            : -
            + {redis_call_callback $id reply [string range $line 1 end-1]}
            - {redis_call_callback $id err [string range $line 1 end-1]}
            ( {redis_call_callback $id reply [string range $line 1 end-1]}
            $ {
                dict set ::redis_state($id) bulk \
                    [expr [string range $line 1 end-1]+2]
                if {[dict get $::redis_state($id) bulk] == 1} {
                    # We got a $-1, hack the state to play well with this.
                    dict set ::redis_state($id) bulk 2
                    dict set ::redis_state($id) buf "\r\n"
                    redis_readable $fd $id
                }
            }
            * {
                dict set ::redis_state($id) mbulk [string range $line 1 end-1]
                # Handle *-1
                if {[dict get $::redis_state($id) mbulk] == -1} {
                    redis_call_callback $id reply {}
                }
            }
            default {
                redis_call_callback $id err \
                    "Bad protocol, $type as reply type byte"
            }
        }
    } else {
        set totlen [dict get $::redis_state($id) bulk]
        set buflen [string length [dict get $::redis_state($id) buf]]
        set toread [expr {$totlen-$buflen}]
        set data [read $fd $toread]
        set nread [string length $data]
        dict append ::redis_state($id) buf $data
        # Check if we read a complete bulk reply
        if {[string length [dict get $::redis_state($id) buf]] ==
            [dict get $::redis_state($id) bulk]} {
            if {[dict get $::redis_state($id) mbulk] == -1} {
                redis_call_callback $id reply \
                    [string range [dict get $::redis_state($id) buf] 0 end-2]
            } else {
                dict with ::redis_state($id) {
                    lappend reply [string range $buf 0 end-2]
                    incr mbulk -1
                    set bulk -1
                }
                if {[dict get $::redis_state($id) mbulk] == 0} {
                    redis_call_callback $id reply \
                        [dict get $::redis_state($id) reply]
                }
            }
        }
    }
}
