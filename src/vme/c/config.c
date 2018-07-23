#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

#include "vme.h"
#include "log.h"

#define MAXLINELENGTH 1024
#define MAXKEYLENGTH 80

#define DPI_PORT "DPIPORT"
#define VANTIQ_URL "VANTIQ_BASEURL"
#define VANTIQ_TOKEN "VANTIQTOKEN"
#define DPI_SOCKET_PATH "DPISOCKETPATH"
#define LOG_LEVEL "LOG_LEVEL"

int set_config_param(vmeconfig_t *config, const char *key, const char *value);

char *remove_spaces(char *str) {
	while (isspace(*str))
		str++;
	char *end = str;
	while (!isspace(*end) && *end != '\0')
		end++;
	*end = '\0';
	return str;
}

char *find_first(char *confLine, char sep)
{
    assert(confLine != NULL);
    while (*confLine != sep && *confLine != '\0')
        confLine++;
    if (*confLine == '\0')
        return NULL;
    return confLine;
}

int parseKeysVals(FILE *in, vmeconfig_t *config)
{
	char buffer[MAXLINELENGTH];
	int lines = 0;

	while (fgets(buffer, sizeof buffer, in) != NULL) {
        // ignore commented lines
        if (buffer[0] == '#')
            continue;
        char *sep = find_first(buffer, '=');
		/* maybe a blank line  or comment*/
        if (sep == NULL || *sep != '=')
			continue;
        *sep++ = '\0';
        char *key = &buffer[0];
        char *value = sep;
        /* misformatted or empty line, just skip it */
		if (value == NULL) {
            continue;
		}
		key = remove_spaces(key);
		value = remove_spaces(value);
        if (strlen(value) == 0)
            continue;
		if (set_config_param(config, key, value) == 1)
            lines++;
	}
	return lines;
}

int vme_parse_config(const char *path, vmeconfig_t *config) {
    FILE *configFile;

    int parsedVals = 0;
    memset(config, 0, sizeof(vmeconfig_t));
    configFile = fopen(path, "r");
    if (configFile) {
        parsedVals = parseKeysVals(configFile, config);
        /* this is a global VME setting at present. should be per client */
        if (config->log_level) {
            vme_set_log_level(config->log_level);
        } else {
            log_set_level(LOG_WARN);
        }
        fclose(configFile);
    } else {
        return -1;
    }
    return parsedVals;
}

bool cmp_strings(const char* str1, const char* str2)
{
    if (strlen(str1) != strlen(str2))
        return false;

    do {
        if (tolower(*str1++) != tolower(*str2++))
            return false;
    } while(*str1 || *str2);

    return true;
}

int set_config_param(vmeconfig_t *config, const char *key, const char *value)
{
	if (cmp_strings(key, DPI_PORT)) {
		config->dpi_port = strdup(value);
    } else if (cmp_strings(key, DPI_SOCKET_PATH)) {
        config->dpi_socket_path = strdup(value);
    } else if (cmp_strings(key, VANTIQ_URL)) {
        size_t len = strlen(value);
        if (value[len-1] != '/') {
            config->vantiq_url = (char *) malloc(len + 2);
            strcpy(config->vantiq_url, value);
            config->vantiq_url[len] = '/';
            config->vantiq_url[len+1] = '\0';
        } else
            config->vantiq_url = strdup(value);
	} else if (cmp_strings(key, VANTIQ_TOKEN)) {
		config->vantiq_token = strdup(value);
    } else if (cmp_strings(key, LOG_LEVEL)) {
        config->log_level = strdup(value);
	} else {
        return 0;
	}
    return 1;
}
