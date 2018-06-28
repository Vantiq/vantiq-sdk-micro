# vantiq-sdk-micro

###### This is new and subject to wild changes until the dust from construction settles. Exercise caution...
![Image of Construction](Under_construction.gif)

## Overview

The VANTIQ micro edition sdk (VME) exists to enable the system to participate in extremely restrictive environments 
where running a VM much less an edge node is not possible. The target are those systems that can only accomodate compact
C-based applications. The example used in the design for the SDK is a set-top box where you might have as little as 1MB
of memory available.

## Repository Structure

* **src/vme** - contains the c sources and header files that comprise the entire library. the compile down to both a
libvme.a as well as its dynamically loaded equivalent (libvme.dylib on OS X)
* **src/vipo** - contains the sources for a prototype application built atop libvme that connects to a simulated Deep
Packet Inspector (DPI) to request any / all device discovery data and then publishes that resulting JSON discovery data
to the VANTIQ server. It can use either inet or local sockets to fetch the data, and relies on libvme to leverage HTTPS
to connect to the VANTIQ system.
* **src/dpisim** - a very simple minded DPI application. it reads "discovery" data from a file and sends whenever a
connection is opened, then waits for an acknowledgement. currently, will send the same data 3 times before transmitting
an EOT to end the transmission / close the connection.
* **testFiles** - files used in unit and integration testing. there are some configuration files and generated datasets
that help drive regression tests.

## Building

The library currently builds on a Mac / Mac OS X. The SDK uses gradle to build and run CUnit framework tests. Gradle is
not currently adept at working with native applications and libraries. This choice might change. For now the usual set
of gradle targets will work as expected:
* gradlew clean
* gradlew assemble
* gradelw build

the latter builds the library and runs the CUnit tests. Note that "gradle tests" does not work.

the build.gradle script uses 'c' and 'cunit-test-suite' gradle
plugins. these force the build script into heavy use of the newish "model space" gradle methodology making typically
easy things difficult. this is in part why "gradle tests" is not yet supported.

At present we lack the ability cross compile / test the code on the
targetted "micro" environemnts. We expect we will work with the field to address the inevitable issues as the SDK sees
use. The first order of business will likely to have a Makefile based build in place.

### Build Dependencies 

It depends on libcurl to handle the lion's share of HTTP protocol work. libcurl works with OPENSSL to deal with one-way
SSL / TLS requirements imposed by connecting to the VANTIQ system. We assume that targetted micro environments will have 
access to these widely used tools.

## Examples

### Authentication
* authenticate to the VANTIQ server, then tear down the connection:
```c
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