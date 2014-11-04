/* A simple synchronous XML-RPC client program written in C, as an example of
   an Xmlrpc-c client.  This invokes the sample.add procedure that the
   Xmlrpc-c example xmlrpc_sample_add_server.c server provides.  I.e. it adds
   two numbers together, the hard way.

   This sends the RPC to the server running on the local system ("localhost"),
   HTTP Port 8080.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>

#include "config.h"  /* information about this build environment */

#define NAME "Xmlrpc-c Test Client"
#define VERSION "1.0"

static void 
dieIfFaultOccurred (xmlrpc_env * const envP) {
    if (envP->fault_occurred) {
        fprintf(stderr, "ERROR: %s (%d)\n",
                envP->fault_string, envP->fault_code);
        exit(1);
    }
}



int 
main(int           const argc, 
     const char ** const argv) {

    xmlrpc_env env;
    xmlrpc_value * resultP;
    xmlrpc_int32 sum;
	xmlrpc_multi_wait_type wait_type;
    const char * const serverUrl = "http://localhost:8080/RPC2";
    const char * const serverUrl2 = "http://localhost:8081/RPC2";
    const char * const serverUrl3 = "http://localhost:8082/RPC2";
    const char * const methodName = "sample.add";

    if (argc == 2) {
        if (strcmp(argv[1], "ANY") == 0) {
			wait_type = ANY;
		} else if (strcmp(argv[1], "MAJORITY") == 0) {
			wait_type = MAJORITY;
		} else if (strcmp(argv[1], "ALL") == 0) {
			wait_type = ALL;
		} else {
			fprintf(stderr, "This program take 1 argument. This " 
			"is the number of servers to wait for responses from " 
			"before returning. Options are: ANY, MAJORITY, and ALL\n");
       		exit(1);
		}
    } else {
		fprintf(stderr, "This program take 1 argument. This " 
		"is the number of servers to wait for responses from " 
		"before returning. Options are: ANY, MAJORITY, and ALL\n");
        exit(1);
	}

    /* Initialize our error-handling environment. */
    xmlrpc_env_init(&env);

    /* Start up our XML-RPC client library. */
    xmlrpc_client_init2(&env, XMLRPC_CLIENT_NO_FLAGS, NAME, VERSION, NULL, 0);
    dieIfFaultOccurred(&env);

    printf("Making XMLRPC call to servers, method '%s' "
           "to request the sum "
           "of 5 and 7...\n", methodName);

    /* Make the remote procedure call */
    resultP = xmlrpc_client_call(&env, methodName, "(sssii)", wait_type, 3, serverUrl, serverUrl2, serverUrl3, 
				(xmlrpc_int32) 5, (xmlrpc_int32) 7);
    dieIfFaultOccurred(&env);
    
    /* Get our sum and print it out. */
    xmlrpc_read_int(&env, resultP, &sum);
    dieIfFaultOccurred(&env);
    printf("The sum is %d\n", sum);
    
    /* Dispose of our result value. */
    xmlrpc_DECREF(resultP);

    /* Clean up our error-handling environment. */
    xmlrpc_env_clean(&env);
    
    /* Shutdown our XML-RPC client library. */
    xmlrpc_client_cleanup();

    return 0;
}

