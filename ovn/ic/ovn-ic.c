/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <config.h>

#include <getopt.h>
#include <stdlib.h>
#include <stdio.h>

#include "bitmap.h"
#include "command-line.h"
#include "daemon.h"
#include "dirs.h"
#include "openvswitch/dynamic-string.h"
#include "fatal-signal.h"
#include "hash.h"
#include "openvswitch/hmap.h"
#include "openvswitch/json.h"
#include "ovn/lex.h"
#include "ovn/lib/chassis-index.h"
#include "ovn/lib/ip-mcast-index.h"
#include "ovn/lib/mcast-group-index.h"
#include "ovn/lib/ovn-inb-idl.h"
#include "ovn/lib/ovn-isb-idl.h"
#include "ovn/lib/ovn-l7.h"
#include "ovn/lib/ovn-nb-idl.h"
#include "ovn/lib/ovn-sb-idl.h"
#include "ovn/lib/ovn-util.h"
#include "ovn/actions.h"
#include "ovn/logical-fields.h"
#include "packets.h"
#include "openvswitch/poll-loop.h"
#include "smap.h"
#include "sset.h"
#include "svec.h"
#include "stream.h"
#include "stream-ssl.h"
#include "unixctl.h"
#include "util.h"
#include "uuid.h"
#include "openvswitch/vlog.h"

VLOG_DEFINE_THIS_MODULE(ovn_ic);

static unixctl_cb_func ovn_ic_exit;
static unixctl_cb_func ovn_ic_pause;
static unixctl_cb_func ovn_ic_resume;
static unixctl_cb_func ovn_ic_is_paused;

struct ic_context {
    struct ovsdb_idl *ovnnb_idl;
    struct ovsdb_idl *ovnsb_idl;
    struct ovsdb_idl *ovninb_idl;
    struct ovsdb_idl *ovnisb_idl;
    struct ovsdb_idl_txn *ovnnb_txn;
    struct ovsdb_idl_txn *ovnsb_txn;
    struct ovsdb_idl_txn *ovninb_txn;
    struct ovsdb_idl_txn *ovnisb_txn;
};

static const char *ovnnb_db;
static const char *ovnsb_db;
static const char *ovninb_db;
static const char *ovnisb_db;
static const char *unixctl_path;


static void
usage(void)
{
    printf("\
%s: OVN interconnection management daemon\n\
usage: %s [OPTIONS]\n\
\n\
Options:\n\
  --ovnnb-db=DATABASE       connect to ovn-nb database at DATABASE\n\
                            (default: %s)\n\
  --ovnsb-db=DATABASE       connect to ovn-sb database at DATABASE\n\
                            (default: %s)\n\
  --unixctl=SOCKET          override default control socket name\n\
  -h, --help                display this help message\n\
  -o, --options             list available options\n\
  -V, --version             display version information\n\
", program_name, program_name, default_nb_db(), default_sb_db());
    daemon_usage();
    vlog_usage();
    stream_usage("database", true, true, false);
}

static const struct isbrec_availability_zone *
az_run(struct ic_context *ctx)
{
    /* XXX: handle AZ deletion. */
    const struct nbrec_nb_global *nb_global =
        nbrec_nb_global_first(ctx->ovnnb_idl);

    if (!nb_global) {
        VLOG_INFO("NB Global not exist.");
        return NULL;
    }

    if (!nb_global->name[0]) {
        return NULL;
    }

    const char *az_name = nb_global->name;
    const struct isbrec_availability_zone *az;
    ISBREC_AVAILABILITY_ZONE_FOR_EACH (az, ctx->ovnisb_idl) {
        if (!strcmp(az->name, az_name)) {
            return az;
        }
    }

    /* Create AZ in ISB */
    if (ctx->ovnisb_txn) {
        VLOG_INFO("Register AZ %s to interconnection DB.", az_name);
        az = isbrec_availability_zone_insert(ctx->ovnisb_txn);
        isbrec_availability_zone_set_name(az, az_name);
        return az;
    }
    return NULL;
}

static uint32_t
allocate_ts_dp_key(struct hmap *dp_tnlids)
{
    static uint32_t hint = OVN_MIN_DP_KEY_GLOBAL;
    return ovn_allocate_tnlid(dp_tnlids, "transit switch datapath",
                              OVN_MIN_DP_KEY_GLOBAL, OVN_MAX_DP_KEY_GLOBAL,
                              &hint);
}

static void
ts_run(struct ic_context *ctx)
{
    const struct inbrec_transit_switch *ts;

    /* Sync INB TS to AZ NB */
    if (ctx->ovnnb_txn) {
        struct shash nb_tses = SHASH_INITIALIZER(&nb_tses);
        const struct nbrec_logical_switch *ls;

        /* Get current NB Logical_Switch with other_config:interconn-ts */
        NBREC_LOGICAL_SWITCH_FOR_EACH (ls, ctx->ovnnb_idl) {
            const char *ts_name = smap_get(&ls->other_config, "interconn-ts");
            if (ts_name) {
                shash_add(&nb_tses, ts_name, ls);
            } 
        }

        /* Create NB Logical_Switch for each TS */
        INBREC_TRANSIT_SWITCH_FOR_EACH (ts, ctx->ovninb_idl) {
            ls = shash_find_and_delete(&nb_tses, ts->name);
            if (!ls) {
                ls = nbrec_logical_switch_insert(ctx->ovnnb_txn);
                nbrec_logical_switch_set_name(ls, ts->name);
                nbrec_logical_switch_update_other_config_setkey(ls,
                                                                "interconn-ts",
                                                                ts->name);
            }
        }
        
        /* Delete extra NB Logical_Switch with other_config:interconn-ts */
        struct shash_node *node;
        SHASH_FOR_EACH (node, &nb_tses) {
            nbrec_logical_switch_delete(node->data);
        }
        shash_destroy(&nb_tses);
    }

    struct hmap dp_tnlids = HMAP_INITIALIZER(&dp_tnlids);
    struct shash isb_dps = SHASH_INITIALIZER(&isb_dps);
    const struct isbrec_datapath_binding *isb_dp;
    ISBREC_DATAPATH_BINDING_FOR_EACH (isb_dp, ctx->ovnisb_idl) {
        shash_add(&isb_dps, isb_dp->transit_switch, isb_dp);
        ovn_add_tnlid(&dp_tnlids, isb_dp->tunnel_key);
    }

    /* Sync ISB TS tunnel key to AZ SB datapath.  (AZ SB datapath is created by
     * northd.) */
    if (ctx->ovnsb_txn) {
        const struct sbrec_datapath_binding *sb_dp;
        SBREC_DATAPATH_BINDING_FOR_EACH (sb_dp, ctx->ovnsb_idl) {
            const char *ts_name = smap_get(&sb_dp->external_ids,
                                           "interconn-ts");
            if (ts_name) {
                isb_dp = shash_find_data(&isb_dps, ts_name);
                if (!isb_dp) {
                    VLOG_DBG("SB datapath "UUID_FMT" with interconn-ts %s not "
                             "found in ISB, ignore.",
                             UUID_ARGS(&sb_dp->header_.uuid),
                             ts_name);
                    continue;
                }
                sbrec_datapath_binding_set_tunnel_key(sb_dp,
                                                      isb_dp->tunnel_key);
            }
        }
    }

    /* Sync TS between INB and ISB.  This is performed after syncing with AZ
     * SB, to avoid uncommitted ISB datapath tunnel key to be synced back to
     * AZ. */
    if (ctx->ovnisb_txn) {
        /* Create ISB Datapath_Binding */
        INBREC_TRANSIT_SWITCH_FOR_EACH (ts, ctx->ovninb_idl) {
            isb_dp = shash_find_and_delete(&isb_dps, ts->name);
            if (!isb_dp) {
                /* Allocate tunnel key */
                int64_t dp_key = allocate_ts_dp_key(&dp_tnlids);
                if (!dp_key) {
                    continue;
                }

                isb_dp = isbrec_datapath_binding_insert(ctx->ovnisb_txn);
                isbrec_datapath_binding_set_transit_switch(isb_dp, ts->name);
                isbrec_datapath_binding_set_tunnel_key(isb_dp, dp_key);
            }
        }

        /* Delete extra ISB Datapath_Binding */
        struct shash_node *node;
        SHASH_FOR_EACH (node, &isb_dps) {
            isbrec_datapath_binding_delete(node->data);
        }
    }
    ovn_destroy_tnlids(&dp_tnlids);
    shash_destroy(&isb_dps);
}

static void
gateway_run(struct ic_context *ctx, const struct isbrec_availability_zone *az)
{
    if (!ctx->ovnisb_txn || !ctx->ovnsb_txn) {
        return;
    }

    struct shash local_gws = SHASH_INITIALIZER(&local_gws);
    struct shash remote_gws = SHASH_INITIALIZER(&remote_gws);
    const struct isbrec_gateway *gw;
    ISBREC_GATEWAY_FOR_EACH (gw, ctx->ovnisb_idl) {
        if (gw->availability_zone == az) {
            shash_add(&local_gws, gw->name, gw);
        } else {
            shash_add(&remote_gws, gw->name, gw);
        }
    }

    const struct sbrec_chassis *chassis;
    SBREC_CHASSIS_FOR_EACH (chassis, ctx->ovnsb_idl) {
        if (chassis->is_interconn) {
            gw = shash_find_and_delete(&local_gws, chassis->name);
            if (!gw) {
                gw = isbrec_gateway_insert(ctx->ovnisb_txn);
                isbrec_gateway_set_availability_zone(gw, az);
                isbrec_gateway_set_name(gw, chassis->name);
                isbrec_gateway_set_hostname(gw, chassis->hostname);

                /* Sync encaps used by this chassis. */
                ovs_assert(chassis->n_encaps);
                struct isbrec_encap *isb_encap;
                struct isbrec_encap **isb_encaps =
                    xmalloc(chassis->n_encaps * sizeof *isb_encap);
                size_t i;
                for (i = 0; i < chassis->n_encaps; i++) {
                    isb_encap = isbrec_encap_insert(ctx->ovnisb_txn);
                    isbrec_encap_set_gateway_name(isb_encap,
                                                  chassis->name);
                    isbrec_encap_set_ip(isb_encap, chassis->encaps[i]->ip);
                    isbrec_encap_set_type(isb_encap,
                                          chassis->encaps[i]->type);
                    isbrec_encap_set_options(isb_encap,
                                             &chassis->encaps[i]->options);
                    isb_encaps[i] = isb_encap;
                }
                isbrec_gateway_set_encaps(gw, isb_encaps,
                                          chassis->n_encaps);
                free(isb_encaps);
            } else {
                /* XXX Update ISB gateway/encaps, if any changes. */

            }
        } else if (chassis->is_remote) {
            gw = shash_find_and_delete(&remote_gws, chassis->name);
            if (!gw) {
                sbrec_chassis_delete(chassis);
                continue;
            }
            /* XXX Update SB chassis/encaps, if any changes. */
        }
    }

    /* Delete extra gateways from ISB for the local AZ */
    struct shash_node *node;
    SHASH_FOR_EACH (node, &local_gws) {
        isbrec_gateway_delete(node->data);
    }
    shash_destroy(&local_gws);

    /* Create SB chassis for remote gateways in ISB */
    SHASH_FOR_EACH (node, &remote_gws) {
        gw = node->data;
        chassis = sbrec_chassis_insert(ctx->ovnsb_txn);
        sbrec_chassis_set_name(chassis, gw->name);
        sbrec_chassis_set_hostname(chassis, gw->hostname);
        sbrec_chassis_set_is_remote(chassis, true);
        /* Sync encaps used by this gateway. */
        ovs_assert(gw->n_encaps);
        struct sbrec_encap *sb_encap;
        struct sbrec_encap **sb_encaps =
            xmalloc(gw->n_encaps * sizeof *sb_encap);
        size_t i;
        for (i = 0; i < gw->n_encaps; i++) {
            sb_encap = sbrec_encap_insert(ctx->ovnsb_txn);
            sbrec_encap_set_chassis_name(sb_encap, gw->name);
            sbrec_encap_set_ip(sb_encap, gw->encaps[i]->ip);
            sbrec_encap_set_type(sb_encap, gw->encaps[i]->type);
            sbrec_encap_set_options(sb_encap, &gw->encaps[i]->options);
            sb_encaps[i] = sb_encap;
        }
        sbrec_chassis_set_encaps(chassis, sb_encaps, gw->n_encaps);
        free(sb_encaps);
    }
    shash_destroy(&remote_gws);
}

static void
ovn_db_run(struct ic_context *ctx)
{
    const struct isbrec_availability_zone *az = az_run(ctx);
    VLOG_DBG("Availability zone: %s", az ? az->name : "not created yet.");

    if (!az) {
        return;
    }
    
    ts_run(ctx);
    gateway_run(ctx, az);
}

static void
parse_options(int argc OVS_UNUSED, char *argv[] OVS_UNUSED)
{
    enum {
        DAEMON_OPTION_ENUMS,
        VLOG_OPTION_ENUMS,
        SSL_OPTION_ENUMS,
    };
    static const struct option long_options[] = {
        {"ovnsb-db", required_argument, NULL, 'd'},
        {"ovnnb-db", required_argument, NULL, 'D'},
        {"ovnisb-db", required_argument, NULL, 'i'},
        {"ovninb-db", required_argument, NULL, 'I'},
        {"unixctl", required_argument, NULL, 'u'},
        {"help", no_argument, NULL, 'h'},
        {"options", no_argument, NULL, 'o'},
        {"version", no_argument, NULL, 'V'},
        DAEMON_LONG_OPTIONS,
        VLOG_LONG_OPTIONS,
        STREAM_SSL_LONG_OPTIONS,
        {NULL, 0, NULL, 0},
    };
    char *short_options = ovs_cmdl_long_options_to_short_options(long_options);

    for (;;) {
        int c;

        c = getopt_long(argc, argv, short_options, long_options, NULL);
        if (c == -1) {
            break;
        }

        switch (c) {
        DAEMON_OPTION_HANDLERS;
        VLOG_OPTION_HANDLERS;
        STREAM_SSL_OPTION_HANDLERS;

        case 'd':
            ovnsb_db = optarg;
            break;

        case 'D':
            ovnnb_db = optarg;
            break;

        case 'u':
            unixctl_path = optarg;
            break;

        case 'h':
            usage();
            exit(EXIT_SUCCESS);

        case 'o':
            ovs_cmdl_print_options(long_options);
            exit(EXIT_SUCCESS);

        case 'V':
            ovs_print_version(0, 0);
            exit(EXIT_SUCCESS);

        default:
            break;
        }
    }

    if (!ovnsb_db) {
        ovnsb_db = default_sb_db();
    }

    if (!ovnnb_db) {
        ovnnb_db = default_nb_db();
    }

    if (!ovnisb_db) {
        ovnisb_db = default_isb_db();
    }

    if (!ovninb_db) {
        ovninb_db = default_inb_db();
    }

    free(short_options);
}

static void OVS_UNUSED
add_column_noalert(struct ovsdb_idl *idl,
                   const struct ovsdb_idl_column *column)
{
    ovsdb_idl_add_column(idl, column);
    ovsdb_idl_omit_alert(idl, column);
}

int
main(int argc, char *argv[])
{
    int res = EXIT_SUCCESS;
    struct unixctl_server *unixctl;
    int retval;
    bool exiting;
    bool paused;

    fatal_ignore_sigpipe();
    ovs_cmdl_proctitle_init(argc, argv);
    set_program_name(argv[0]);
    service_start(&argc, &argv);
    parse_options(argc, argv);

    daemonize_start(false);

    retval = unixctl_server_create(unixctl_path, &unixctl);
    if (retval) {
        exit(EXIT_FAILURE);
    }
    unixctl_command_register("exit", "", 0, 0, ovn_ic_exit, &exiting);
    unixctl_command_register("pause", "", 0, 0, ovn_ic_pause, &paused);
    unixctl_command_register("resume", "", 0, 0, ovn_ic_resume, &paused);
    unixctl_command_register("is-paused", "", 0, 0, ovn_ic_is_paused,
                             &paused);

    daemonize_complete();

    /* ovn-inb db. */
    struct ovsdb_idl_loop ovninb_idl_loop = OVSDB_IDL_LOOP_INITIALIZER(
        ovsdb_idl_create(ovninb_db, &inbrec_idl_class, true, true));

    /* ovn-isb db. */
    struct ovsdb_idl_loop ovnisb_idl_loop = OVSDB_IDL_LOOP_INITIALIZER(
        ovsdb_idl_create(ovnisb_db, &isbrec_idl_class, true, true));

    /* ovn-nb db. XXX: add only needed tables and columns */
    struct ovsdb_idl_loop ovnnb_idl_loop = OVSDB_IDL_LOOP_INITIALIZER(
        ovsdb_idl_create(ovnnb_db, &nbrec_idl_class, true, true));

    /* ovn-sb db. XXX: add only needed tables and columns */
    struct ovsdb_idl_loop ovnsb_idl_loop = OVSDB_IDL_LOOP_INITIALIZER(
        ovsdb_idl_create(ovnsb_db, &sbrec_idl_class, true, true));

    /* Ensure that only a single ovn-ic is active in the deployment by
     * acquiring a lock called "ovn_ic" on the southbound database
     * and then only performing DB transactions if the lock is held. */
    ovsdb_idl_set_lock(ovnsb_idl_loop.idl, "ovn_ic");
    bool had_lock = false;

    /* Main loop. */
    exiting = false;
    paused = false;
    while (!exiting) {
        if (!paused) {
            struct ic_context ctx = {
                .ovnnb_idl = ovnnb_idl_loop.idl,
                .ovnnb_txn = ovsdb_idl_loop_run(&ovnnb_idl_loop),
                .ovnsb_idl = ovnsb_idl_loop.idl,
                .ovnsb_txn = ovsdb_idl_loop_run(&ovnsb_idl_loop),
                .ovninb_idl = ovninb_idl_loop.idl,
                .ovninb_txn = ovsdb_idl_loop_run(&ovninb_idl_loop),
                .ovnisb_idl = ovnisb_idl_loop.idl,
                .ovnisb_txn = ovsdb_idl_loop_run(&ovnisb_idl_loop),
            };

            if (!had_lock && ovsdb_idl_has_lock(ovnsb_idl_loop.idl)) {
                VLOG_INFO("ovn-ic lock acquired. "
                        "This ovn-ic instance is now active.");
                had_lock = true;
            } else if (had_lock && !ovsdb_idl_has_lock(ovnsb_idl_loop.idl)) {
                VLOG_INFO("ovn-ic lock lost. "
                        "This ovn-ic instance is now on standby.");
                had_lock = false;
            }

            if (ovsdb_idl_has_lock(ovnsb_idl_loop.idl)) {
                ovn_db_run(&ctx);
            }

            ovsdb_idl_loop_commit_and_wait(&ovnnb_idl_loop);
            ovsdb_idl_loop_commit_and_wait(&ovnsb_idl_loop);
            ovsdb_idl_loop_commit_and_wait(&ovninb_idl_loop);
            ovsdb_idl_loop_commit_and_wait(&ovnisb_idl_loop);
        } else {
            /* ovn-ic is paused
             *    - we still want to handle any db updates and update the
             *      local IDL. Otherwise, when it is resumed, the local IDL
             *      copy will be out of sync.
             *    - but we don't want to create any txns.
             * */
            ovsdb_idl_run(ovnnb_idl_loop.idl);
            ovsdb_idl_run(ovnsb_idl_loop.idl);
            ovsdb_idl_run(ovninb_idl_loop.idl);
            ovsdb_idl_run(ovnisb_idl_loop.idl);
            ovsdb_idl_wait(ovnnb_idl_loop.idl);
            ovsdb_idl_wait(ovnsb_idl_loop.idl);
            ovsdb_idl_wait(ovninb_idl_loop.idl);
            ovsdb_idl_wait(ovnisb_idl_loop.idl);
        }

        unixctl_server_run(unixctl);
        unixctl_server_wait(unixctl);
        if (exiting) {
            poll_immediate_wake();
        }

        poll_block();
        if (should_service_stop()) {
            exiting = true;
        }
    }

    unixctl_server_destroy(unixctl);
    ovsdb_idl_loop_destroy(&ovnnb_idl_loop);
    ovsdb_idl_loop_destroy(&ovnsb_idl_loop);
    ovsdb_idl_loop_destroy(&ovninb_idl_loop);
    ovsdb_idl_loop_destroy(&ovnisb_idl_loop);
    service_stop();

    exit(res);
}

static void
ovn_ic_exit(struct unixctl_conn *conn, int argc OVS_UNUSED,
                const char *argv[] OVS_UNUSED, void *exiting_)
{
    bool *exiting = exiting_;
    *exiting = true;

    unixctl_command_reply(conn, NULL);
}

static void
ovn_ic_pause(struct unixctl_conn *conn, int argc OVS_UNUSED,
                const char *argv[] OVS_UNUSED, void *pause_)
{
    bool *pause = pause_;
    *pause = true;

    unixctl_command_reply(conn, NULL);
}

static void
ovn_ic_resume(struct unixctl_conn *conn, int argc OVS_UNUSED,
                  const char *argv[] OVS_UNUSED, void *pause_)
{
    bool *pause = pause_;
    *pause = false;

    unixctl_command_reply(conn, NULL);
}

static void
ovn_ic_is_paused(struct unixctl_conn *conn, int argc OVS_UNUSED,
                     const char *argv[] OVS_UNUSED, void *paused_)
{
    bool *paused = paused_;
    if (*paused) {
        unixctl_command_reply(conn, "true");
    } else {
        unixctl_command_reply(conn, "false");
    }
}
