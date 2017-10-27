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

import callback_gen
import util
from string import Template

notification_registry_template = Template("""
package $plugin_package.$notification_package;

/**
 * <p>Registry for notification callbacks defined in ${plugin_name}.
 * <br>It was generated by notification_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public interface ${plugin_name}NotificationRegistry extends $base_package.$notification_package.NotificationRegistry {

    $register_callback_methods

    @Override
    void close();
}
""")

global_notification_callback_template = Template("""
package $plugin_package.$notification_package;

/**
 * <p>Aggregated callback interface for notifications only.
 * <br>It was generated by notification_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public interface Global${plugin_name}NotificationCallback$callbacks {

}
""")

notification_registry_impl_template = Template("""
package $plugin_package.$notification_package;

/**
 * <p>Notification registry delegating notification processing to registered callbacks.
 * <br>It was generated by notification_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public final class ${plugin_name}NotificationRegistryImpl implements ${plugin_name}NotificationRegistry, Global${plugin_name}NotificationCallback {

    // TODO add a special NotificationCallback interface and only allow those to be registered
    private final java.util.concurrent.ConcurrentMap<Class<? extends $base_package.$dto_package.JVppNotification>, $base_package.$callback_package.JVppNotificationCallback> registeredCallbacks =
        new java.util.concurrent.ConcurrentHashMap<>();

    $register_callback_methods
    $handler_methods

    @Override
    public void close() {
        registeredCallbacks.clear();
    }
}
""")

register_callback_impl_template = Template("""
    public java.lang.AutoCloseable register$callback(final $plugin_package.$callback_package.$callback callback){
        if(null != registeredCallbacks.putIfAbsent($plugin_package.$dto_package.$notification.class, callback)){
            throw new IllegalArgumentException("Callback for " + $plugin_package.$dto_package.$notification.class +
                "notification already registered");
        }
        return () -> registeredCallbacks.remove($plugin_package.$dto_package.$notification.class);
    }
""")

handler_impl_template = Template("""
    @Override
    public void on$notification(
        final $plugin_package.$dto_package.$notification notification) {
        final $base_package.$callback_package.JVppNotificationCallback jVppNotificationCallback = registeredCallbacks.get($plugin_package.$dto_package.$notification.class);
        if (null != jVppNotificationCallback) {
            (($plugin_package.$callback_package.$callback) registeredCallbacks
                .get($plugin_package.$dto_package.$notification.class))
                .on$notification(notification);
        }
    }
""")

notification_provider_template = Template("""
package $plugin_package.$notification_package;

 /**
 * Provides ${plugin_name}NotificationRegistry.
 * <br>The file was generated by notification_gen.py based on $inputfile
 * <br>(python representation of api file generated by vppapigen).
 */
public interface ${plugin_name}NotificationRegistryProvider extends $base_package.$notification_package.NotificationRegistryProvider {

    @Override
    public ${plugin_name}NotificationRegistry getNotificationRegistry();
}
""")


def generate_notification_registry(func_list, base_package, plugin_package, plugin_name, notification_package, callback_package, dto_package, inputfile):
    """ Generates notification registry interface and implementation """
    print "Generating Notification interfaces and implementation"

    if not os.path.exists(notification_package):
        os.mkdir(notification_package)

    callbacks = []
    register_callback_methods = []
    register_callback_methods_impl = []
    handler_methods = []
    for func in func_list:

        if not util.is_notification(func['name']):
            continue

        camel_case_name_with_suffix = util.underscore_to_camelcase_upper(func['name'])
        notification_dto = util.add_notification_suffix(camel_case_name_with_suffix)
        callback_ifc = notification_dto + callback_gen.callback_suffix
        fully_qualified_callback_ifc = "{0}.{1}.{2}".format(plugin_package, callback_package, callback_ifc)
        callbacks.append(fully_qualified_callback_ifc)

        # TODO create NotificationListenerRegistration and return that instead of AutoCloseable to better indicate
        # that the registration should be closed
        register_callback_methods.append("java.lang.AutoCloseable register{0}({1} callback);"
                                         .format(callback_ifc, fully_qualified_callback_ifc))
        register_callback_methods_impl.append(register_callback_impl_template.substitute(plugin_package=plugin_package,
                                                                                         callback_package=callback_package,
                                                                                         dto_package=dto_package,
                                                                                         notification=notification_dto,
                                                                                         callback=callback_ifc))
        handler_methods.append(handler_impl_template.substitute(base_package=base_package,
                                                                plugin_package=plugin_package,
                                                                callback_package=callback_package,
                                                                dto_package=dto_package,
                                                                notification=notification_dto,
                                                                callback=callback_ifc))


    callback_file = open(os.path.join(notification_package, "%sNotificationRegistry.java" % plugin_name), 'w')
    callback_file.write(notification_registry_template.substitute(inputfile=inputfile,
                                                                register_callback_methods="\n    ".join(register_callback_methods),
                                                                base_package=base_package,
                                                                plugin_package=plugin_package,
                                                                plugin_name=plugin_name,
                                                                notification_package=notification_package))
    callback_file.flush()
    callback_file.close()

    callback_file = open(os.path.join(notification_package, "Global%sNotificationCallback.java" % plugin_name), 'w')

    global_notification_callback_callbacks = ""
    if (callbacks):
        global_notification_callback_callbacks = " extends " + ", ".join(callbacks)

    callback_file.write(global_notification_callback_template.substitute(inputfile=inputfile,
                                                                       callbacks=global_notification_callback_callbacks,
                                                                       plugin_package=plugin_package,
                                                                       plugin_name=plugin_name,
                                                                       notification_package=notification_package))
    callback_file.flush()
    callback_file.close()

    callback_file = open(os.path.join(notification_package, "%sNotificationRegistryImpl.java" % plugin_name), 'w')
    callback_file.write(notification_registry_impl_template.substitute(inputfile=inputfile,
                                                                     callback_package=callback_package,
                                                                     dto_package=dto_package,
                                                                     register_callback_methods="".join(register_callback_methods_impl),
                                                                     handler_methods="".join(handler_methods),
                                                                     base_package=base_package,
                                                                     plugin_package=plugin_package,
                                                                     plugin_name=plugin_name,
                                                                     notification_package=notification_package))
    callback_file.flush()
    callback_file.close()

    callback_file = open(os.path.join(notification_package, "%sNotificationRegistryProvider.java" % plugin_name), 'w')
    callback_file.write(notification_provider_template.substitute(inputfile=inputfile,
                                                                     base_package=base_package,
                                                                     plugin_package=plugin_package,
                                                                     plugin_name=plugin_name,
                                                                     notification_package=notification_package))
    callback_file.flush()
    callback_file.close()

