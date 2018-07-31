//
//  utils.c
//
//  common utility functionality
//
//  Copyright © 2018 VANTIQ. All rights reserved.
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "vme.h"


const char *vme_system_types[] = {
    "users",
    "types",
    "namespaces",
    "profiles",
    "scalars",
    "logs",
    "documents",
    "sources",
    "sourceimpls",
    "topics",
    "rules",
    "configurations",
    "nodes",
    "nodeconfigs",
    "components",
    "procedures",
    "events",
    "tokens",
    "organizations",
    "series",
    "ArsProductMap",
    "audits",
    "situations",
    "projects",
    "groups",
    "activitypatterns",
    "collaborations",
    "collaborationtypes",
    "pagesets",
    "services",
    "custom",
    "debugconfigs",
    "scheduledevents",
    "eventstreams",
    "delegatedrequests",
    "eventdesigns",
    "scripts"
};
#define N_SYSTEM_TYPES (sizeof (vme_system_types) / sizeof (const char *))

// up to 999 api versions
#define LONGEST_PREFIX "/api/vxxx/resources/"
#define RSURI_PATH_PREFIX "/api/v%d/resources/%s/%s/%s"
#define RSURI_PATH_NOID_NONAME "/api/v%d/resources/%s/%s"

int is_sys_type(const char *rs)
{
    if (rs != NULL) {
        for (int i = 0; i < N_SYSTEM_TYPES; i++) {
            if (strcmp(rs, vme_system_types[i]) == 0)
                return i;
        }
    }
    return -1;
}

const char *vme_mapto_rsname(vme_rstype_t type)
{
    assert(type >= 0 && type < N_SYSTEM_TYPES);
    return vme_system_types[type];
}

/*
 * build up a path to the specified resource.
 * RETURN: allocated string, must be freed
 */
char *build_rsuri(const int apiVers, const char *ns, const char *rsName, const char *rsID)
{
    assert(apiVers < 1000);
    assert(ns != NULL);
    assert(rsName != NULL || rsID != NULL); // they cannot both be NULL;
    const char *optionalName = (rsName != NULL && strlen(rsName) == 0) ? NULL : rsName;
    const char *optionalID = (rsID != NULL && strlen(rsID) == 0) ? NULL : rsID;
    
    size_t len =
        sizeof(LONGEST_PREFIX)                          // prfx(/api/v1/resources/)
        + strlen(ns) + 1                                // + ns(custom) + /
        + (optionalName == NULL ? 0 : strlen(optionalName) + 1) // + rsName(/a/b/c) + /
        + (optionalID == NULL ? 0 : strlen(optionalID)) // + id(<uuid>)
        + 1;
    char *path = malloc(len);
    
    // make sure we don't leave a trailing <slash> that causes a 404 :^(
    const char *fmt =
        (rsName == NULL || rsID == NULL) ? RSURI_PATH_NOID_NONAME : RSURI_PATH_PREFIX;

    int needed = (rsName == NULL ?
                  snprintf(path, len, fmt, apiVers, ns, rsID) :
                  snprintf(path, len, fmt, apiVers, ns, rsName, rsID)) ;
    assert(needed <= len);
    return path;
}
