# vantiq-sdk-micro

## Overview

The VANTIQ micro edition sdk (VME) exists to enable the system to participate in restrictive environments 
where running an edge node is not possible. The targets are those systems that can only accomodate compact
C-based applications. The example used in the design for the SDK is a set-top box where you might have as little as 1MB
of memory available.

### Support Platforms

Currently, the library has been built / tested on Mac OSX and Linux. We believe it should compile and run on Windows
with the Windows subsystem for Linux though this has not been verified.

## Repository Structure

* **src/vme** - contains the c sources and header files that comprise the entire library. the compile down to both a
libvme.a as well as its dynamically loaded equivalent (libvme.dylib on OS X, libvme.so on Linux)
* **src/vipo** - contains the sources for a prototype application built atop libvme that connects to a simulated Deep
Packet Inspector (DPI) to request any / all device discovery data and then publishes that resulting JSON discovery data
to the VANTIQ server. It can use either inet or local sockets to fetch the data, and relies on libvme to leverage HTTPS
to connect to the VANTIQ system.
* **testFiles** - files used in unit and integration testing. there are some configuration files and generated datasets
that help drive regression tests.

## Building

The library currently builds on Linux and Mac / Mac OS X. The SDK uses make to build and run CUnit framework tests.
The set of targets includes:
* make clean
* make all
* make test

the latter first builds the library and then runs the CUnit tests.

At present we lack the ability cross compile / test the code on the
targetted "micro" environemnts. We expect we will work with the field to address issues arising as the SDK sees
use.

### Build Dependencies 

libvme depends on libcurl to handle the HTTP/S protocol work. libcurl works with OPENSSL to deal with one-way
SSL / TLS requirements in connecting to the VANTIQ system. We assume that targeted micro environments will have
access to these widely used tools.

Further, in order to run the tests, the project expects the CUnit framework to be installed on the machine. 

All dependencies can be installed for Mac OS X via `brew install curl` and `brew install CUnit`. Similarly on Linux
environments you should be able to use the appropriate package installer for this.

## Configuration File

libvme initialization allows for a path to a config file. At present the configuration lets you specify
* **VANTIQ_BASEURL** - the URL to the VANTIQ server
* **VANTIQTOKEN** - the VANTIQ access token that enables libvme applications to authenticate and login to a specific namespace
* **LOG_LEVEL** - the logging level (one of TRACE, DEBUG, INFO, WARN, or ERROR)

## Testing
The regressions defined for libvme are all integration tests. i.e. they require a running VANTIQ server as well as some
types and rules. Here are the steps required:
* import the project defined in the vme.zip file under testFiles/input.
* generate a long lived access token in the namespace where the project was imported. [Create Access Token](https://dev.vantiq.com/docs/system/resourceguide/index.html#create-access-token)
in the documentation.
* edit the file config.properties under testFiles/input to set the values for the VANTIQ server URL as well as the
generated access token.
* run the command `make test` at the root of the project.
## Examples

### Authentication
* authenticate to the VANTIQ server, then tear down the connection:
```c
    vmeconfig_t config;
    if (vme_parse_config("config.properties", &config) == -1) {
        ...
    }
    VME vme = vme_init(config.vantiq_url, config.vantiq_token, 1);
    ...
    vme_teardown(vme);
```

every application should book-end their use of VANTIQ with these two calls. the token is a long-lived access token that
must be generated from with the desired namespace via the VANTIQ UI. See: [Create Access Token](https://dev.vantiq.com/docs/system/resourceguide/index.html#create-access-token)
in the documentation.

### selects
* select 1 instance from the Employees type where dept == Marketing:
```c
    result = vme_select_one(vme, rsURI, "[\"id\"]", "{ \"dept\" : \"Marketing\"}");
```
* select full name and SSN for 100 instances from the Employees type:
```c
    char *rsURI = vme_build_custom_rsuri(vme, "Employees", NULL);
    result = vme_select(vme, rsURI, "[\"last_name\", \"first_name\", \"ssn\"]", NULL, NULL, 0, 100);`
    if (result->vme_error_msg == NULL) {
        ... results handling
    }
```
* select all employees who make more than $200,000 paging through results 1000 instances at a time:
```c
    int page = 0;
    do {
        if (result != NULL)
            vme_free_result(result);

        page++; // pages are 1 - based
        result = vme_select(vme, rsURI, NULL, "{\"salary\" : { \"$gt\" : 200000.0}}", NULL, page, 1000);
        ... // process results
    } while(result->vme_size > 2); // "[]" empty array of instances
    vme_free_result(result);
```
* select employees who make more than $245,000 order by salary largest to smallest:
```c
    result = vme_select(vme, rsURI, "[\"salary\", \"id\"]", "{\"salary\" : {\"$gt\":245000.0}}", "{\"salary\":-1}", 0, 0);
```
### inserts
* insert instances from a dataset file 500 at a time.
```c
    vmebuf_t *msg = vmebuf_alloc();
    vmebuf_push(msg, '[');

    int count = 1;
    while (fgets(buffer, sizeof(buffer), jsonFile) != NULL) {
        if (msg->len > 1) {
            vmebuf_push(msg, ',');
        vmebuf_concat(msg, buffer, strlen(buffer));

        if ((count % 500) == 0) {
            vmebuf_push(msg, ']');
            vme_result_t *result = vme_insert(vme, rsURI, msg->data, msg->len);
            if (result->vme_error_msg != NULL) {
                fprintf(stderr, "insert resulted in err: %s", result->vme_error_msg);
                fflush(stderr);
            }
            vme_free_result(result);
            vmebuf_truncate(msg);
            vmebuf_push(msg, '[');
        }
        count++;
    }
```
### execute procedure
```c
    vme_result_t *result = vme_execute(vme, "MyProc", "{\"empSSN\": \"655-71-9041\", \"newSalary\": 500000.00}");
```

### updates
```c
     char *rsURI = vme_build_custom_rsuri(vme, "Employees", idStr); // <--- must specify instance ID
     char *expr = "{ \"salary\": 251000.00 }";
     vme_result_t *result = vme_update(vme, rsURI, expr, strlen(expr));
```
### aggregates
* for all employess in a given department making more than $200K get the total (sum), max, min, average salary, restrict to departments having an average > $225,000. Further, parse the resulting JSON string into an object tree and examine the results.
```c
        const char *pipeline = "[{\"$match\": { \"salary\" : { \"$gt\" : 200000.0} }}, { \"$group\": { \"_id\": \"$dept\", \"total\": { \"$sum\": \"$salary\"}, \"average\": { \"$avg\" : \"$salary\" }, \"max\": { \"$max\" : \"$salary\" }, \"min\": { \"$min\" : \"$salary\" }}}, { \"$match\": { \"average\": { \"$gte\": 225000.0 }}} ]";

        vme_result_t *result = vme_aggregate(vme, rsURI, pipeline);

        if (result->vme_error_msg == NULL) {
            cJSON *json = cJSON_Parse(result->vme_json_data);
            char *pretty = cJSON_Print(json);
            printf("Aggregate pipeline on Employees:\n%s\n",  pretty);
            paw_over_aggreates(json);
            free(pretty);
            cJSON_Delete(json);
        }
```
### deletes
* lay-off all employees that make more than $225,000
```c
    vme_result_t *result = vme_delete_count(vme, rsURI, "{ \"salary\" : { \"$gt\": 225000.0 }}");
    if (result->vme_count < 10) {
        // institute pay-cuts as well
    }
    vme_free_result(result);
```
### publish events
* publish a discovery json document to a topic
```c
        /* the topic can be anything--no need to pre-define */
        vme_result_t *result = vme_publish(vme, "/GiantTelco/Smarthome/Discovery", fullMsg->data, fullMsg->len);
        vme_free_result(result);
```
