#include <stdarg.h>
#include <string.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "xmlrpc_config.h"

#include "bool.h"

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include <xmlrpc-c/client_int.h>
#include <xmlrpc-c/client_global.h>

/*=========================================================================
   Global Client
=========================================================================*/

static struct xmlrpc_client * globalClientP;
static bool globalClientExists = false;


void
xmlrpc_client_init2(xmlrpc_env *                      const envP,
                    int                               const flags,
                    const char *                      const appname,
                    const char *                      const appversion,
                    const struct xmlrpc_clientparms * const clientparmsP,
                    unsigned int                      const parmSize) {
/*----------------------------------------------------------------------------
   This function is not thread-safe.
-----------------------------------------------------------------------------*/
    if (globalClientExists)
        xmlrpc_faultf(
            envP,
            "Xmlrpc-c global client instance has already been created "
            "(need to call xmlrpc_client_cleanup() before you can "
            "reinitialize).");
    else {
        /* The following call is not thread-safe */
        xmlrpc_client_setup_global_const(envP);
        if (!envP->fault_occurred) {
            xmlrpc_client_create(envP, flags, appname, appversion,
                                 clientparmsP, parmSize, &globalClientP);
            if (!envP->fault_occurred)
                globalClientExists = true;

            if (envP->fault_occurred)
                xmlrpc_client_teardown_global_const();
        }
    }
}



void
xmlrpc_client_init(int          const flags,
                   const char * const appname,
                   const char * const appversion) {
/*----------------------------------------------------------------------------
   This function is not thread-safe.
-----------------------------------------------------------------------------*/
    struct xmlrpc_clientparms clientparms;

    /* As our interface does not allow for failure, we just fail silently ! */

    xmlrpc_env env;
    xmlrpc_env_init(&env);

    clientparms.transport = NULL;

    /* The following call is not thread-safe */
    xmlrpc_client_init2(&env, flags,
                        appname, appversion,
                        &clientparms, XMLRPC_CPSIZE(transport));

    xmlrpc_env_clean(&env);
}



void
xmlrpc_client_cleanup() {
/*----------------------------------------------------------------------------
   This function is not thread-safe
-----------------------------------------------------------------------------*/
    XMLRPC_ASSERT(globalClientExists);

    xmlrpc_client_destroy(globalClientP);

    globalClientExists = false;

    /* The following call is not thread-safe */
    xmlrpc_client_teardown_global_const();
}



static void
validateGlobalClientExists(xmlrpc_env * const envP) {

    if (!globalClientExists)
        xmlrpc_faultf(envP,
                      "Xmlrpc-c global client instance "
                      "has not been created "
                      "(need to call xmlrpc_client_init2()).");
}



void
xmlrpc_client_transport_call(
    xmlrpc_env *               const envP,
    void *                     const reserved ATTR_UNUSED,
        /* for client handle */
    const xmlrpc_server_info * const serverP,
    xmlrpc_mem_block *         const callXmlP,
    xmlrpc_mem_block **        const respXmlPP) {

    validateGlobalClientExists(envP);
    if (!envP->fault_occurred)
        xmlrpc_client_transport_call2(envP, globalClientP, serverP,
                                      callXmlP, respXmlPP);
}


//prototype
void
synch_rpc_helper(void *args);

//structure for passing in multiple args to thread
typedef struct arg_struct
{
		xmlrpc_env *		 envP;
	 	xmlrpc_client *		 clientP;
		char *			 serverUrl;
		char *			 methodName;
		char * 			 format;
		xmlrpc_value **		 resultPP;
		va_list *			 args;
		pthread_mutex_t *	 counter_mutex;
		int *			 counter;
} arg_struct_t;


void
synch_rpc_helper(void *args){
	arg_struct_t *arg_struct = (arg_struct_t *)args;

	xmlrpc_client_call2f_va(arg_struct->envP, arg_struct->clientP, arg_struct->serverUrl,
					 arg_struct->methodName, arg_struct->format,
					 arg_struct->resultPP, *arg_struct->args);


	pthread_mutex_lock(arg_struct->counter_mutex);
	*(arg_struct->counter) = *(arg_struct->counter) + 1;
	pthread_mutex_unlock(arg_struct->counter_mutex);

}

//servers come before method args
xmlrpc_value *
xmlrpc_client_call(xmlrpc_env * const envP,
                   char * const methodName,
                   const char * const format,
                   int const server_num,
                   ...) {

    xmlrpc_value * resultP;
    char *servers[server_num];
    pthread_t threads[server_num];

    char arg_format[strlen(format)];
    strcpy (arg_format, "(");
    strcat (arg_format, format + 1 + server_num);

    validateGlobalClientExists(envP);

    pthread_mutex_t counter_mutex;
    pthread_mutex_init(&counter_mutex, NULL);
    int return_counter = 0;
    int threshold = server_num;

    arg_struct_t *rpc_args = malloc(sizeof(arg_struct_t));


    if (!envP->fault_occurred) {
        va_list args;

        va_start(args, server_num);

	int i;
	for(i = 0; i < server_num; i++)
	{
		servers[i] = va_arg(args, char *);
	}

	//allocate arg_struct arguments
	rpc_args->envP = envP;
	rpc_args->clientP = globalClientP;
	rpc_args->methodName = methodName;
	rpc_args->format = arg_format;
	rpc_args->resultPP = &resultP;
	rpc_args->args = &args;
	rpc_args->counter_mutex = &counter_mutex;
	rpc_args->counter = &return_counter;


	for(i = 0; i < server_num; i++)
	{
		rpc_args->serverUrl = servers[i];
		pthread_create(&threads[i], NULL, (void *)synch_rpc_helper, (void *)rpc_args);
        /*	synch_rpc_helper(envP, globalClientP, servers[i],
                                methodName, arg_format, &resultP, args,
				&counter_mutex, &return_counter);
	*/
	}

        va_end(args);
    }
    while(return_counter < threshold)
    {
	sleep(1);
    }

    pthread_mutex_destroy(&counter_mutex);
    free(rpc_args);

    return resultP;
}


xmlrpc_value *
xmlrpc_client_call_server(xmlrpc_env *               const envP,
                          const xmlrpc_server_info * const serverInfoP,
                          const char *               const methodName,
                          const char *               const format,
                          ...) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred) {
        va_list args;

        va_start(args, format);

        xmlrpc_client_call_server2_va(envP, globalClientP, serverInfoP,
                                      methodName, format, args, &resultP);
        va_end(args);
    }
    return resultP;
}



xmlrpc_value *
xmlrpc_client_call_server_params(
    xmlrpc_env *               const envP,
    const xmlrpc_server_info * const serverInfoP,
    const char *               const methodName,
    xmlrpc_value *             const paramArrayP) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred)
        xmlrpc_client_call2(envP, globalClientP,
                            serverInfoP, methodName, paramArrayP,
                            &resultP);

    return resultP;
}



xmlrpc_value *
xmlrpc_client_call_params(xmlrpc_env *   const envP,
                          const char *   const serverUrl,
                          const char *   const methodName,
                          xmlrpc_value * const paramArrayP) {

    xmlrpc_value * resultP;

    validateGlobalClientExists(envP);

    if (!envP->fault_occurred) {
        xmlrpc_server_info * serverInfoP;

        serverInfoP = xmlrpc_server_info_new(envP, serverUrl);

        if (!envP->fault_occurred) {
            xmlrpc_client_call2(envP, globalClientP,
                                serverInfoP, methodName, paramArrayP,
                                &resultP);

            xmlrpc_server_info_free(serverInfoP);
        }
    }
    return resultP;
}



void
xmlrpc_client_call_server_asynch_params(
    xmlrpc_server_info * const serverInfoP,
    const char *         const methodName,
    xmlrpc_response_handler    responseHandler,
    void *               const userData,
    xmlrpc_value *       const paramArrayP) {

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    validateGlobalClientExists(&env);

    if (!env.fault_occurred)
        xmlrpc_client_start_rpc(&env, globalClientP,
                                serverInfoP, methodName, paramArrayP,
                                responseHandler, userData);

    if (env.fault_occurred) {
        /* Unfortunately, we have no way to return an error and the
           regular callback for a failed RPC is designed to have the
           parameter array passed to it.  This was probably an oversight
           of the original asynch design, but now we have to be as
           backward compatible as possible, so we do this:
        */
        (*responseHandler)(serverInfoP->serverUrl,
                           methodName, paramArrayP, userData,
                           &env, NULL);
    }
    xmlrpc_env_clean(&env);
}


//must enter number of servers, as well as a format string with the servers. Servers come first
void
xmlrpc_client_call_asynch(const char * const methodName,
                          xmlrpc_multi_response_handler responseHandler,
                          void *       const userData,
                          const char * const format,
			  			  int  	       const server_num,
                          ...) {

    char *servers[server_num];

    char arg_format[strlen(format)];
    strcpy (arg_format, "(");
    strcat (arg_format, format + 1 + server_num); //reforms format string

    xmlrpc_env env;

    xmlrpc_env_init(&env);

    validateGlobalClientExists(&env);

    if (!env.fault_occurred) {
        va_list args;

        va_start(args, server_num);

	int i;
	for(i = 0; i < server_num; i++)
	{

		servers[i] = va_arg(args, char *);
	}

	//for(i = 0; i < server_num; i++)
	//{
		//to prevent invoking of callback handler, make new function that calls
		//	callback handler once all the servers have returned

        	xmlrpc_client_start_multi_rpcf_va(&env, globalClientP,
                                    servers, methodName,
									,ALL ,
                                    ,responseHandler, userData,
                                    arg_format, args);

    		if (env.fault_occurred)
       			(*responseHandler)(servers, methodName, NULL, userData, &env, NULL);


	//}

	xmlrpc_env_clean(&env);
        va_end(args);
    }
}


void
xmlrpc_client_call_asynch_params(const char *   const serverUrl,
                                 const char *   const methodName,
                                 xmlrpc_response_handler responseHandler,
                                 void *         const userData,
                                 xmlrpc_value * const paramArrayP) {
    xmlrpc_env env;
    xmlrpc_server_info * serverInfoP;

    xmlrpc_env_init(&env);

    serverInfoP = xmlrpc_server_info_new(&env, serverUrl);

    if (!env.fault_occurred) {
        xmlrpc_client_call_server_asynch_params(
            serverInfoP, methodName, responseHandler, userData, paramArrayP);

        xmlrpc_server_info_free(serverInfoP);
    }
    if (env.fault_occurred)
        (*responseHandler)(serverUrl, methodName, paramArrayP, userData,
                           &env, NULL);
    xmlrpc_env_clean(&env);
}



void
xmlrpc_client_call_server_asynch(xmlrpc_server_info * const serverInfoP,
                                 const char *         const methodName,
                                 xmlrpc_response_handler    responseHandler,
                                 void *               const userData,
                                 const char *         const format,
                                 ...) {

    xmlrpc_env env;

    validateGlobalClientExists(&env);

    if (!env.fault_occurred) {
        va_list args;

        xmlrpc_env_init(&env);

        va_start(args, format);

        xmlrpc_client_start_rpcf_server_va(
            &env, globalClientP, serverInfoP, methodName,
            responseHandler, userData, format, args);

        va_end(args);
    }
    if (env.fault_occurred)
        (*responseHandler)(serverInfoP->serverUrl, methodName, NULL,
                           userData, &env, NULL);

    xmlrpc_env_clean(&env);
}



void
xmlrpc_client_event_loop_finish_asynch(void) {

    XMLRPC_ASSERT(globalClientExists);
    xmlrpc_client_event_loop_finish(globalClientP);
}



void
xmlrpc_client_event_loop_finish_asynch_timeout(
    unsigned long const milliseconds) {

    XMLRPC_ASSERT(globalClientExists);
    xmlrpc_client_event_loop_finish_timeout(globalClientP, milliseconds);
}
