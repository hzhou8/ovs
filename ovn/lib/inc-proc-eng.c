//TODO copyright
#include <config.h>

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "openvswitch/dynamic-string.h"
#include "openvswitch/hmap.h"
#include "openvswitch/vlog.h"
#include "inc-proc-eng.h"

VLOG_DEFINE_THIS_MODULE(inc_proc_eng);

void
engine_run(struct engine_node *node, uint64_t run_id)
{
    if (node->run_id == run_id) {
        return;
    }
    node->run_id = run_id;

    if (node->changed) {
        node->changed = false;
        if (node->reset_old_data) {
            node->reset_old_data(node);
        }
    }
    if (!node->n_inputs) {
        node->run(node);
        VLOG_DBG("node: %s, changed: %d", node->name, node->changed);
        return;
    }
    
    size_t i;

    for (i = 0; i < node->n_inputs; i++) {
        engine_run(node->inputs[i], run_id);
    }

    bool need_compute = false;
    bool need_recompute = false;
    for (i = 0; i < node->n_inputs; i++) {
        if (node->inputs[i]->changed) {
            need_compute = true;
            if (!node->change_handler[i]) {
                need_recompute = true;
                break;
            }
        }
    }

    if (need_recompute) {
        VLOG_DBG("node: %s, recompute", node->name);
        node->run(node);
    } else if (need_compute) {
        for (i = 0; i < node->n_inputs; i++) {
            if (node->inputs[i]->changed) {
                VLOG_DBG("node: %s, handle change for input %s",
                         node->name, node->inputs[i]->name);
                node->change_handler[i](node->inputs[i], node);
            }
        }
    }

    VLOG_DBG("node: %s, changed: %d", node->name, node->changed);

}

