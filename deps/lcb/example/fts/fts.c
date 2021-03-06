/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 *     Copyright 2018 Couchbase, Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *       http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

/**

   CFLAGS="-I$(realpath ../../include) -I$(realpath ../../build/generated)"
   LDFLAGS="-L$(realpath ../../build/lib) -lcouchbase -Wl,-rpath=$(realpath ../../build/lib)"
   make fts

   The example assumes the existence of "travel-sample" bucket and three
   specific Full Text Indexes, defined for it. These are:

   - "travel-sample-index-unstored": Uses only the default settings.

   - "travel-sample-index-stored": Uses default settings, with one exception:
     dynamic fields are stored, for the whole index.

   - "travel-sample-index-hotel-description": Indexes only the description
     fields of hotel documents, and disables the default type mapping. The index
     has a custom analyzer named myUnicodeAnalyzer defined on it: the analyzer's
     main characteristic is that it uses the unicode tokenizer.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libcouchbase/couchbase.h>
#include <libcouchbase/cbft.h>

#include "queries.h"

static void fail(const char *msg)
{
    printf("[\x1b[31mERROR\x1b[0m] %s\n", msg);
    exit(EXIT_FAILURE);
}

static void check(lcb_error_t err, const char *msg)
{
    if (err != LCB_SUCCESS) {
        char buf[1024] = {0};
        snprintf(buf, sizeof(buf), "%s: %s\n", msg, lcb_strerror_short(err));
        fail(buf);
    }
}

static int err2color(lcb_error_t err)
{
    switch (err) {
        case LCB_SUCCESS:
            return 49;
        default:
            return 31;
    }
}

static void ln2space(const void *buf, size_t nbuf)
{
    size_t ii;
    char *str = (char *)buf;
    for (ii = 0; ii < nbuf; ii++) {
        if (str[ii] == '\n') {
            str[ii] = ' ';
        }
    }
}

static void row_callback(lcb_t instance, int type, const lcb_RESPFTS *resp)
{
    ln2space(resp->row, resp->nrow);
    if (resp->rc != LCB_SUCCESS) {
        printf("\x1b[31m%s\x1b[0m: ", lcb_strerror_short(resp->rc));
    }
    printf("%.*s\n", (int)resp->nrow, (char *)resp->row);
    if (resp->rflags & LCB_RESP_F_FINAL) {
        printf("\n");
    }
}

int main(int argc, char *argv[])
{
    lcb_error_t err;
    lcb_t instance;
    char *bucket = NULL;
    size_t ii;

    if (argc < 2) {
        printf("Usage: %s couchbase://host/bucket [ password [ username ] ]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    {
        struct lcb_create_st create_options = {0};
        create_options.version = 3;
        create_options.v.v3.connstr = argv[1];
        if (argc > 2) {
            create_options.v.v3.passwd = argv[2];
        }
        if (argc > 3) {
            create_options.v.v3.username = argv[3];
        }
        check(lcb_create(&instance, &create_options), "create couchbase handle");
        check(lcb_connect(instance), "schedule connection");
        lcb_wait(instance);
        check(lcb_get_bootstrap_status(instance), "bootstrap from cluster");
        check(lcb_cntl(instance, LCB_CNTL_GET, LCB_CNTL_BUCKETNAME, &bucket), "get bucket name");
        if (strcmp(bucket, "travel-sample") != 0) {
            fail("expected bucket to be \"travel-sample\"");
        }
    }

    for (ii = 0; ii < num_queries; ii++) {
        lcb_CMDFTS cmd = {0};
        cmd.callback = row_callback;
        cmd.query = queries[ii].query;
        cmd.nquery = queries[ii].query_len;
        check(lcb_fts_query(instance, NULL, &cmd), "schedule FTS index creation operation");
        printf("----> \x1b[1m%s\x1b[0m\n", queries[ii].comment);
        printf("----> \x1b[32m%.*s\x1b[0m\n", (int)queries[ii].query_len, queries[ii].query);
        lcb_wait(instance);
    }

    /* Now that we're all done, close down the connection handle */
    lcb_destroy(instance);
    return 0;
}
