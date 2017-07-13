#!/usr/bin/env python
#
# Copyright (c) 2016 Cisco and/or its affiliates.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
from string import Template

import dto_gen
import util

jvpp_facade_callback_template = Template("""
package $plugin_package.$future_package;

/**
 * <p>Async facade callback setting values to future objects
 * <br>It was generated by jvpp_future_facade_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public final class FutureJVpp${plugin_name}FacadeCallback implements $plugin_package.$callback_package.JVpp${plugin_name}GlobalCallback {

    private final java.util.Map<java.lang.Integer, java.util.concurrent.CompletableFuture<? extends $base_package.$dto_package.JVppReply<?>>> requests;
    private final $plugin_package.$notification_package.Global${plugin_name}NotificationCallback notificationCallback;

    public FutureJVpp${plugin_name}FacadeCallback(
        final java.util.Map<java.lang.Integer, java.util.concurrent.CompletableFuture<? extends $base_package.$dto_package.JVppReply<?>>> requestMap,
        final $plugin_package.$notification_package.Global${plugin_name}NotificationCallback notificationCallback) {
        this.requests = requestMap;
        this.notificationCallback = notificationCallback;
    }

    @Override
    @SuppressWarnings("unchecked")
    public void onError($base_package.VppCallbackException reply) {
        final java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>> completableFuture;

        synchronized(requests) {
            completableFuture = (java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>>) requests.get(reply.getCtxId());
        }

        if(completableFuture != null) {
            completableFuture.completeExceptionally(reply);

            synchronized(requests) {
                requests.remove(reply.getCtxId());
            }
        }
    }

    @Override
    @SuppressWarnings("unchecked")
    public void onControlPingReply($base_package.$dto_package.ControlPingReply reply) {
        final java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>> completableFuture;

        synchronized(requests) {
            completableFuture = (java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>>) requests.get(reply.context);
        }

        if(completableFuture != null) {
            // Finish dump call
            if (completableFuture instanceof $base_package.$future_package.AbstractFutureJVppInvoker.CompletableDumpFuture) {
                completableFuture.complete((($base_package.$future_package.AbstractFutureJVppInvoker.CompletableDumpFuture) completableFuture).getReplyDump());
                // Remove future mapped to dump call context id
                synchronized(requests) {
                    requests.remove((($base_package.$future_package.AbstractFutureJVppInvoker.CompletableDumpFuture) completableFuture).getContextId());
                }
            } else {
                completableFuture.complete(reply);
            }

            synchronized(requests) {
                requests.remove(reply.context);
            }
        }
    }

$methods
}
""")

jvpp_facade_callback_method_template = Template("""
    @Override
    @SuppressWarnings("unchecked")
    public void on$callback_dto($plugin_package.$dto_package.$callback_dto reply) {
        final java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>> completableFuture;

        synchronized(requests) {
            completableFuture = (java.util.concurrent.CompletableFuture<$base_package.$dto_package.JVppReply<?>>) requests.get(reply.context);
        }

        if(completableFuture != null) {
            completableFuture.complete(reply);

            synchronized(requests) {
                requests.remove(reply.context);
            }
        }
    }
""")

jvpp_facade_callback_notification_method_template = Template("""
    @Override
    public void on$callback_dto($plugin_package.$dto_package.$callback_dto notification) {
        notificationCallback.on$callback_dto(notification);
    }
""")

jvpp_facade_details_callback_method_template = Template("""
    @Override
    @SuppressWarnings("unchecked")
    public void on$callback_dto($plugin_package.$dto_package.$callback_dto reply) {
        final $base_package.$future_package.AbstractFutureJVppInvoker.CompletableDumpFuture<$plugin_package.$dto_package.$callback_dto_reply_dump> completableFuture;

        synchronized(requests) {
            completableFuture = ($base_package.$future_package.AbstractFutureJVppInvoker.CompletableDumpFuture<$plugin_package.$dto_package.$callback_dto_reply_dump>) requests.get(reply.context);
        }

        if(completableFuture != null) {
            completableFuture.getReplyDump().$callback_dto_field.add(reply);
        }
    }
""")


def generate_jvpp(func_list, base_package, plugin_package, plugin_name, dto_package, callback_package, notification_package, future_facade_package, inputfile):
    """ Generates JVpp interface and JNI implementation """
    print "Generating JVpp future facade"

    if not os.path.exists(future_facade_package):
        raise Exception("%s folder is missing" % future_facade_package)

    methods = []
    methods_impl = []
    callbacks = []
    for func in func_list:
        camel_case_name_with_suffix = util.underscore_to_camelcase_upper(func['name'])

        if util.is_ignored(func['name']) or util.is_control_ping(camel_case_name_with_suffix):
            continue

        if not util.is_reply(camel_case_name_with_suffix) and not util.is_notification(func['name']):
            continue

        camel_case_method_name = util.underscore_to_camelcase(func['name'])

        if not util.is_notification(func["name"]):
            camel_case_request_method_name = util.remove_reply_suffix(util.underscore_to_camelcase(func['name']))
            if util.is_details(camel_case_name_with_suffix):
                camel_case_reply_name = get_standard_dump_reply_name(util.underscore_to_camelcase_upper(func['name']),
                                                                     func['name'])
                callbacks.append(jvpp_facade_details_callback_method_template.substitute(base_package=base_package,
                                                                                         plugin_package=plugin_package,
                                                                                         dto_package=dto_package,
                                                                                         callback_dto=camel_case_name_with_suffix,
                                                                                         callback_dto_field=camel_case_method_name,
                                                                                         callback_dto_reply_dump=camel_case_reply_name + dto_gen.dump_dto_suffix,
                                                                                         future_package=future_facade_package))

                methods.append(future_jvpp_method_template.substitute(plugin_package=plugin_package,
                                                                      dto_package=dto_package,
                                                                      method_name=camel_case_request_method_name +
                                                                                  util.underscore_to_camelcase_upper(util.dump_suffix),
                                                                      reply_name=camel_case_reply_name + dto_gen.dump_dto_suffix,
                                                                      request_name=util.remove_reply_suffix(camel_case_reply_name) +
                                                                                   util.underscore_to_camelcase_upper(util.dump_suffix)))
                methods_impl.append(future_jvpp_dump_method_impl_template.substitute(plugin_package=plugin_package,
                                                                                     dto_package=dto_package,
                                                                                     method_name=camel_case_request_method_name +
                                                                                                 util.underscore_to_camelcase_upper(util.dump_suffix),
                                                                                     reply_name=camel_case_reply_name + dto_gen.dump_dto_suffix,
                                                                                     request_name=util.remove_reply_suffix(camel_case_reply_name) +
                                                                                                  util.underscore_to_camelcase_upper(util.dump_suffix)))
            else:
                request_name = util.underscore_to_camelcase_upper(util.unconventional_naming_rep_req[func['name']]) \
                    if func['name'] in util.unconventional_naming_rep_req else util.remove_reply_suffix(camel_case_name_with_suffix)

                methods.append(future_jvpp_method_template.substitute(plugin_package=plugin_package,
                                                                      dto_package=dto_package,
                                                                      method_name=camel_case_request_method_name,
                                                                      reply_name=camel_case_name_with_suffix,
                                                                      request_name=request_name))
                methods_impl.append(future_jvpp_method_impl_template.substitute(plugin_package=plugin_package,
                                                                                dto_package=dto_package,
                                                                                method_name=camel_case_request_method_name,
                                                                                reply_name=camel_case_name_with_suffix,
                                                                                request_name=request_name))

                callbacks.append(jvpp_facade_callback_method_template.substitute(base_package=base_package,
                                                                                 plugin_package=plugin_package,
                                                                                 dto_package=dto_package,
                                                                                 callback_dto=camel_case_name_with_suffix))

        if util.is_notification(func["name"]):
            callbacks.append(jvpp_facade_callback_notification_method_template.substitute(plugin_package=plugin_package,
                                                                                          dto_package=dto_package,
                                                                                          callback_dto=util.add_notification_suffix(camel_case_name_with_suffix)))

    jvpp_file = open(os.path.join(future_facade_package, "FutureJVpp%sFacadeCallback.java" % plugin_name), 'w')
    jvpp_file.write(jvpp_facade_callback_template.substitute(inputfile=inputfile,
                                                             base_package=base_package,
                                                             plugin_package=plugin_package,
                                                             plugin_name=plugin_name,
                                                             dto_package=dto_package,
                                                             notification_package=notification_package,
                                                             callback_package=callback_package,
                                                             methods="".join(callbacks),
                                                             future_package=future_facade_package))
    jvpp_file.flush()
    jvpp_file.close()

    jvpp_file = open(os.path.join(future_facade_package, "FutureJVpp%s.java" % plugin_name), 'w')
    jvpp_file.write(future_jvpp_template.substitute(inputfile=inputfile,
                                                    base_package=base_package,
                                                    plugin_package=plugin_package,
                                                    plugin_name=plugin_name,
                                                    notification_package=notification_package,
                                                    methods="".join(methods),
                                                    future_package=future_facade_package))
    jvpp_file.flush()
    jvpp_file.close()

    jvpp_file = open(os.path.join(future_facade_package, "FutureJVpp%sFacade.java" % plugin_name), 'w')
    jvpp_file.write(future_jvpp_facade_template.substitute(inputfile=inputfile,
                                                           base_package=base_package,
                                                           plugin_package=plugin_package,
                                                           plugin_name=plugin_name,
                                                           dto_package=dto_package,
                                                           notification_package=notification_package,
                                                           methods="".join(methods_impl),
                                                           future_package=future_facade_package))
    jvpp_file.flush()
    jvpp_file.close()


future_jvpp_template = Template('''
package $plugin_package.$future_package;

/**
 * <p>Async facade extension adding specific methods for each request invocation
 * <br>It was generated by jvpp_future_facade_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public interface FutureJVpp${plugin_name} extends $base_package.$future_package.FutureJVppInvoker {
$methods

    @Override
    public $plugin_package.$notification_package.${plugin_name}NotificationRegistry getNotificationRegistry();

}
''')

future_jvpp_method_template = Template('''
    java.util.concurrent.CompletionStage<$plugin_package.$dto_package.$reply_name> $method_name($plugin_package.$dto_package.$request_name request);
''')


future_jvpp_facade_template = Template('''
package $plugin_package.$future_package;

/**
 * <p>Implementation of FutureJVpp based on AbstractFutureJVppInvoker
 * <br>It was generated by jvpp_future_facade_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public class FutureJVpp${plugin_name}Facade extends $base_package.$future_package.AbstractFutureJVppInvoker implements FutureJVpp${plugin_name} {

    private final $plugin_package.$notification_package.${plugin_name}NotificationRegistryImpl notificationRegistry = new $plugin_package.$notification_package.${plugin_name}NotificationRegistryImpl();

    /**
     * <p>Create FutureJVpp${plugin_name}Facade object for provided JVpp instance.
     * Constructor internally creates FutureJVppFacadeCallback class for processing callbacks
     * and then connects to provided JVpp instance
     *
     * @param jvpp provided $base_package.JVpp instance
     *
     * @throws java.io.IOException in case instance cannot connect to JVPP
     */
    public FutureJVpp${plugin_name}Facade(final $base_package.JVppRegistry registry, final $base_package.JVpp jvpp) throws java.io.IOException {
        super(jvpp, registry, new java.util.HashMap<>());
        java.util.Objects.requireNonNull(registry, "JVppRegistry should not be null");
        registry.register(jvpp, new FutureJVpp${plugin_name}FacadeCallback(getRequests(), notificationRegistry));
    }

    @Override
    public $plugin_package.$notification_package.${plugin_name}NotificationRegistry getNotificationRegistry() {
        return notificationRegistry;
    }

$methods
}
''')

future_jvpp_method_impl_template = Template('''
    @Override
    public java.util.concurrent.CompletionStage<$plugin_package.$dto_package.$reply_name> $method_name($plugin_package.$dto_package.$request_name request) {
        return send(request);
    }
''')

future_jvpp_dump_method_impl_template = Template('''
    @Override
    public java.util.concurrent.CompletionStage<$plugin_package.$dto_package.$reply_name> $method_name($plugin_package.$dto_package.$request_name request) {
        return send(request, new $plugin_package.$dto_package.$reply_name());
    }
''')


# Returns request name or special one from unconventional_naming_rep_req map
def get_standard_dump_reply_name(camel_case_dto_name, func_name):
    # FIXME this is a hotfix for sub-details callbacks
    # FIXME also for L2FibTableEntry
    # It's all because unclear mapping between
    #  request -> reply,
    #  dump -> reply, details,
    #  notification_start -> reply, notifications

    # vpe.api needs to be "standardized" so we can parse the information and create maps before generating java code
    suffix = func_name.split("_")[-1]
    return util.underscore_to_camelcase_upper(
        util.unconventional_naming_rep_req[func_name]) + util.underscore_to_camelcase_upper(suffix) if func_name in util.unconventional_naming_rep_req \
        else camel_case_dto_name
