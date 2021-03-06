#!/bin/sh
#
# $Id$
#
# Created 2010
# Author: Mike Ovsiannikov
#
# Copyright 2010-2012,2016 Quantcast Corporation. All rights reserved.
#
# This file is part of Kosmos File System (KFS).
#
# Licensed under the Apache License, Version 2.0
# (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
# implied. See the License for the specific language governing
# permissions and limitations under the License.
#

sort='cat'
if [ $# -gt 0 -a x"$1" = x"-c" ]; then
    sort='sort -n'
    shift
fi

nf=20
if [ $# -gt 1 -a x"$1" = x"-n" ]; then
    shift
    nf=$1
    shift
fi

if [ $# -gt 0 -a -d "$1" -a -f "$1"/current ]; then
    if [ x"$sort" = x'cat' ]; then
        myarg=
    else
        myarg='-c'
    fi
    if head -n 5 "$1"/current | grep -E '^@[0-9a-fA-F]+ ' > /dev/null; then
        stripts='sed -e '\''s/^@[0-9a-fA-F]* //'\'' | '
    else
        stripts=''
    fi
    {
        echo "$1"/@*.s | tr ' ' '\n' | sort | tail -n $nf | xargs gunzip -c 
        cat "$1"/current
    } | "$0" $myarg
    exit $?
fi

# The following can be used to update RPC table:
# awk '/^[ ]+f\([^)]+\)/ { \
#    print "    names[i++] = \"" tolower(substr($1,3,length($1)-3)) "\";"; \
# }' ../cc/meta/MetaRequest.h

grep -h -F  ' ===request=counters: ' ${1+"$@"} \
| sed -e 's/^@[0-9a-fA-F]* //' \
| awk '
BEGIN {
    i=0
    names[i++] = "total";

    names[i++] = "lookup";
    names[i++] = "lookup_path";
    names[i++] = "create";
    names[i++] = "mkdir";
    names[i++] = "remove";
    names[i++] = "rmdir";
    names[i++] = "readdir";
    names[i++] = "readdirplus";
    names[i++] = "getalloc";
    names[i++] = "getlayout";
    names[i++] = "allocate";
    names[i++] = "truncate";
    names[i++] = "rename";
    names[i++] = "setmtime";
    names[i++] = "change_file_replication";
    names[i++] = "coalesce_blocks";
    names[i++] = "retire_chunkserver";
    names[i++] = "toggle_worm";
    names[i++] = "chunk_server_hello";
    names[i++] = "chunk_server_bye";
    names[i++] = "chunk_heartbeat";
    names[i++] = "chunk_allocate";
    names[i++] = "chunk_delete";
    names[i++] = "chunk_stalenotify";
    names[i++] = "begin_make_chunk_stable";
    names[i++] = "chunk_make_stable";
    names[i++] = "chunk_verschange";
    names[i++] = "chunk_replicate";
    names[i++] = "chunk_size";
    names[i++] = "chunk_replication_check";
    names[i++] = "chunk_corrupt";
    names[i++] = "chunk_retire";
    names[i++] = "lease_acquire";
    names[i++] = "lease_renew";
    names[i++] = "lease_relinquish";
    names[i++] = "lease_cleanup";
    names[i++] = "ping";
    names[i++] = "stats";
    names[i++] = "recompute_dirsize";
    names[i++] = "dump_chunktoservermap";
    names[i++] = "dump_chunkreplicationcandidates";
    names[i++] = "fsck";
    names[i++] = "check_leases";
    names[i++] = "open_files";
    names[i++] = "upservers";
    names[i++] = "log_make_chunk_stable";
    names[i++] = "log_make_chunk_stable_done";
    names[i++] = "set_chunk_servers_properties";
    names[i++] = "chunk_server_restart";
    names[i++] = "chunk_set_properties";
    names[i++] = "get_chunk_servers_counters";
    names[i++] = "log_chunk_version_change";
    names[i++] = "get_request_counters";
    names[i++] = "checkpoint";
    names[i++] = "disconnect";
    names[i++] = "getpathname";
    names[i++] = "chunk_evacuate";
    names[i++] = "chmod";
    names[i++] = "chown";
    names[i++] = "chunk_available";
    names[i++] = "chunkdir_info";
    names[i++] = "get_chunk_server_dirs_counters";
    names[i++] = "authenticate";
    names[i++] = "delegate";
    names[i++] = "delegate_cancel";
    names[i++] = "force_chunk_replication";
    names[i++] = "log_group_users";
    names[i++] = "set_group_users";
    names[i++] = "ack";
    names[i++] = "remove_from_dumpster";
    names[i++] = "log_chunk_allocate";
    names[i++] = "log_writer_control";
    names[i++] = "log_clear_obj_store_delete";
    names[i++] = "read_meta_data";
    names[i++] = "chunk_op_log_completion";
    names[i++] = "chunk_op_log_in_flight";
    names[i++] = "hibernated_prune";
    names[i++] = "hibernated_remove";
    names[i++] = "restart_process";
    names[i++] = "crypto_key_new";
    names[i++] = "crypto_key_expired";
    names[i++] = "noop";
    names[i++] = "vr_hello";
    names[i++] = "vr_start_view_change";
    names[i++] = "vr_do_view_change";
    names[i++] = "vr_start_view";
    names[i++] = "vr_reconfiguration";
    names[i++] = "vr_log_start_view";
    names[i++] = "vr_get_status";
    names[i++] = "setatime";

    names[i++] = "other";
    names[i++] = "alloc_reuse";
    names[i++] = "log_writer";
    ncnt = i;
    shv[0] = -1;
    badlines = 0;
}
function showCounters(date, time, dhv, pv, dv)
{
    i = 0;
    for (f = 0 ; f < ncnt; f++) {
        if (pv[i] != 0 || f == 0) {
            printf("%3d %s %32s %11d %6.2f%% %10d %6.2f%% %10.1f %10.1f %6.2f%%\n", \
                f + 1, time, names[f], \
                dv[i],   dv[0] > 0 ? dv[i]   * 100. / dv[0] : 0, \
                dv[i+1], dv[1] > 0 ? dv[i+1] * 100. / dv[1] : 0, \
                dv[i] > 0 ? dv[i+2] / dv[i] : dv[i+2], \
                dv[i] > 0 ? dv[i+3] / dv[i] : dv[i+3], \
                dhv[0] > 0 ? dv[i+3] * 100. / dhv[0] : 0 \
            );
        }
        i += 4;
    }
    printf("%3d %s %32s %11d %6.2f%% %10d %6.2f%% %10d %10d\n", \
        f + 1, time,  date " 1e-6sec us sys", dhv[0], \
        dhv[0] > 0 ? dhv[1] * 100. / dhv[0] : 0, dhv[1], \
        dhv[0] > 0 ? dhv[2] * 100. / dhv[0] : 0, dhv[2], \
        dhv[1] + dhv[2] \
    );
}
/ ===request=counters: / {
    f = 7;
    # time, user, system
    for (i = 0; i < 3; i++) {
        v = $(f++);
        dhv[i] = v - phv[i];
        phv[i] = v;        
    }
    for (i = 0; i < ncnt * 4; i++) {
        v = $(f++);
        dv[i] = v - pv[i];
        pv[i] = v;
    }
    if (NF != f - 1) {
        badlines++;
    }
    if (shv[0] < 0) {
        for (i = 0; i < 3; i++) {
            shv[i] = phv[i];
        }
    }
    date = $1;
    time = $2;
    showCounters(date, time, dhv, pv, dv);
}
END {
    for (i = 0; i < 3; i++) {
        phv[i] -= shv[i];
    }
    showCounters("TOTAL " date, time, phv, pv, pv);
    printf("id time op total %% errors %% time exec_time %%time [time: 1e-6 sec]\n");
    if (badlines) {
        printf("WARNING: unsupported counters format encoutered: %d times\n", \
            badlines);
    }
}' | $sort
